package com.neorig.pcbuilder.domain.engine

import com.neorig.pcbuilder.domain.compat.CompatibilityChecker
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.compat.Severity
import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.GpuBrand
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.PartCatalog
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Purpose
import kotlin.math.abs
import kotlin.math.min

/** Результат автоматической сборки. */
sealed interface AssembleResult {
    /**
     * Сборка найдена.
     * @property monitorReserve сколько изначально зарезервировали на монитор
     */
    data class Success(val build: PcBuild, val monitorReserve: Int) : AssembleResult

    /** Бюджета не хватает даже на самую дешёвую совместимую сборку. */
    data class InsufficientBudget(val requested: Int, val minimumBudget: Int) : AssembleResult

    /** Под выбранные предпочтения в базе нет совместимых деталей. */
    data object NoCompatibleParts : AssembleResult
}

/** Вариант для ручной замены детали: сама деталь + её проблемы в текущей сборке. */
data class PartOption(
    val part: Part,
    val issues: List<Incompatibility>,
    val priceDelta: Int,
    val isCurrent: Boolean,
) {
    val hasErrors: Boolean get() = issues.any { it.severity == Severity.ERROR }
    val hasWarnings: Boolean get() = issues.any { it.severity == Severity.WARNING }
}

/**
 * Движок подбора сборки.
 *
 * Алгоритм:
 * 1. Считаем минимальную совместимую сборку. Если бюджет меньше — сообщаем минимальную сумму.
 * 2. Резервируем на монитор 15–25 % бюджета (по разрешению) и берём лучший подходящий монитор.
 * 3. Для каждого подходящего процессора (и каждого поддерживаемого им типа памяти)
 *    делим остаток по долям назначения ([BudgetShares]) и подбираем лучшие детали в рамках доли,
 *    затем «чиним» перерасход и раздаём остаток на самые полезные апгрейды.
 * 4. Из всех кандидатов выбираем сборку с максимальной «полезностью»
 *    (взвешенная сумма индексов производительности).
 * 5. Если после этого остались деньги — пробуем улучшить монитор.
 */
class BuildEngine(
    private val catalog: PartCatalog,
    private val checker: CompatibilityChecker = CompatibilityChecker(),
) {

    // ---------------------------------------------------------------------------------------
    // Автосборка
    // ---------------------------------------------------------------------------------------

    fun assemble(request: BuildRequest): AssembleResult {
        val prefs = request.preferences
        val budget = request.budget
        val monitors = monitorCandidates(prefs)
        val cheapestSystem = cheapestSystem(prefs)
        if (monitors.isEmpty() || cheapestSystem == null) return AssembleResult.NoCompatibleParts

        val cheapestMonitor = monitors.minBy { it.price }
        val minimum = cheapestSystem.totalPrice + cheapestMonitor.price
        if (budget < minimum) return AssembleResult.InsufficientBudget(budget, minimum)

        // 1. Монитор: резерв по разрешению, но так, чтобы на системный блок точно хватило
        val reserve = (budget * BudgetShares.monitorShare(prefs.resolution)).toInt()
        val monitorCap = min(reserve, budget - cheapestSystem.totalPrice)
        var monitor = monitors
            .filter { it.price <= monitorCap }
            .maxWithOrNull(compareBy<Part>({ it.score }, { -it.price }))
            ?: cheapestMonitor

        // 2. Системный блок: перебираем процессоры
        val systemBudget = budget - monitor.price
        val shares = BudgetShares.systemShares(prefs)
        var best: PcBuild? = null
        var bestUtility = Double.NEGATIVE_INFINITY
        for (cpu in cpuCandidates(prefs)) {
            for (memType in cpu.compat.memoryTypes) {
                val candidate = fillForCpu(cpu, memType, systemBudget, shares, prefs) ?: continue
                val u = utility(candidate, prefs, shares)
                if (u > bestUtility + 1e-9 ||
                    (abs(u - bestUtility) < 1e-9 && candidate.totalPrice < (best?.totalPrice ?: Int.MAX_VALUE))
                ) {
                    best = candidate
                    bestUtility = u
                }
            }
        }
        val system = best ?: cheapestSystem

        // 3. Остаток бюджета — на монитор получше
        val leftover = budget - monitor.price - system.totalPrice
        if (leftover > 0) {
            monitors
                .filter { it.score > monitor.score && it.price <= monitor.price + leftover }
                .maxWithOrNull(compareBy<Part>({ it.score }, { -it.price }))
                ?.let { monitor = it }
        }
        return AssembleResult.Success(system.with(monitor), reserve)
    }

    /** Минимальный бюджет (с монитором) для заданных предпочтений; null — собрать нельзя. */
    fun minimumBudget(prefs: BuildPreferences): Int? {
        val system = cheapestSystem(prefs) ?: return null
        val monitor = monitorCandidates(prefs).minByOrNull { it.price } ?: return null
        return system.totalPrice + monitor.price
    }

    // ---------------------------------------------------------------------------------------
    // Ручная замена деталей
    // ---------------------------------------------------------------------------------------

    /** Все варианты категории с пометками совместимости относительно текущей сборки. */
    fun options(build: PcBuild, category: Category): List<PartOption> {
        val current = build[category]
        return catalog.of(category).map { part ->
            val candidate = build.with(part)
            PartOption(
                part = part,
                issues = checker.issuesFor(candidate, category),
                priceDelta = part.price - (current?.price ?: 0),
                isCurrent = part.id == current?.id,
            )
        }
    }

    /**
     * Автоисправление: заменяет несовместимые детали (кроме [locked]) на совместимые,
     * максимально близкие по цене к текущим.
     */
    fun autoFix(build: PcBuild, locked: Category? = null): PcBuild {
        var result = build
        repeat(MAX_FIX_STEPS) {
            val issues = checker.check(result)
            if (issues.isEmpty()) return result
            val target = issues
                .sortedBy { if (it.severity == Severity.ERROR) 0 else 1 }
                .flatMap { it.categories }
                .filter { it != locked }
                .minByOrNull { FIX_ORDER.indexOf(it) }
                ?: return result
            val current = result[target]
            val replacement = catalog.of(target)
                .filter { checker.issuesFor(result.with(it), target).isEmpty() }
                .minWithOrNull(
                    compareBy<Part>({ abs(it.price - (current?.price ?: 0)) }, { -it.score })
                )
                ?: catalog.of(target)
                    .filter { candidate ->
                        checker.issuesFor(result.with(candidate), target).none { it.severity == Severity.ERROR }
                    }
                    .minByOrNull { abs(it.price - (current?.price ?: 0)) }
                ?: return result
            if (replacement.id == current?.id) return result
            result = result.with(replacement)
        }
        return result
    }

    // ---------------------------------------------------------------------------------------
    // Кандидаты с учётом предпочтений
    // ---------------------------------------------------------------------------------------

    private fun cpuCandidates(prefs: BuildPreferences): List<Part> =
        catalog.of(Category.CPU).filter { cpu ->
            prefs.cpuBrand == CpuBrand.ANY || cpu.brand.equals(prefs.cpuBrand.vendor, ignoreCase = true)
        }

    /** Видеокарты: фильтр по бренду; встроенная графика — только для учёбы/офиса при «Неважно». */
    private fun gpuCandidates(prefs: BuildPreferences, cpu: Part): List<Part> =
        catalog.of(Category.GPU).filter { gpu ->
            if (gpu.isIntegratedGraphics) {
                prefs.purpose == Purpose.STUDY &&
                    prefs.gpuBrand == GpuBrand.ANY &&
                    cpu.specs.integratedGraphics == true
            } else {
                prefs.gpuBrand == GpuBrand.ANY || gpu.brand.equals(prefs.gpuBrand.vendor, ignoreCase = true)
            }
        }

    /** Мониторы нужного разрешения с частотой не ниже выбранной (с мягким откатом). */
    private fun monitorCandidates(prefs: BuildPreferences): List<Part> {
        val all = catalog.of(Category.MONITOR)
        val byResolution = all.filter { it.specs.resolution == prefs.resolution.code }
        val exact = byResolution.filter { (it.specs.refreshHz ?: 0) >= prefs.refreshRate.hz }
        return exact.ifEmpty { byResolution.ifEmpty { all } }
    }

    /** Пулы деталей для конкретного процессора и типа памяти (статические фильтры). */
    private fun poolsFor(cpu: Part, memType: String, prefs: BuildPreferences): Map<Category, List<Part>>? {
        val socket = cpu.compat.socket
        val cpuTdp = cpu.compat.tdpW ?: 0
        val pools = mapOf(
            Category.GPU to gpuCandidates(prefs, cpu),
            Category.MOTHERBOARD to catalog.of(Category.MOTHERBOARD).filter {
                it.compat.socket == socket && it.compat.memoryType == memType
            },
            Category.RAM to catalog.of(Category.RAM).filter { it.compat.memoryType == memType },
            Category.COOLER to catalog.of(Category.COOLER).filter {
                socket in it.compat.sockets && (it.compat.maxTdpW ?: 0) >= cpuTdp
            },
            Category.CASE to catalog.of(Category.CASE),
            Category.PSU to catalog.of(Category.PSU),
            Category.SSD to catalog.of(Category.SSD),
        )
        return if (pools.values.any { it.isEmpty() }) null else pools
    }

    // ---------------------------------------------------------------------------------------
    // Самая дешёвая совместимая сборка (для минимального бюджета)
    // ---------------------------------------------------------------------------------------

    private fun cheapestSystem(prefs: BuildPreferences): PcBuild? {
        var best: PcBuild? = null
        for (cpu in cpuCandidates(prefs)) {
            for (memType in cpu.compat.memoryTypes) {
                val pools = poolsFor(cpu, memType, prefs) ?: continue
                val build = cheapestFor(cpu, pools) ?: continue
                if (best == null || build.totalPrice < best.totalPrice) best = build
            }
        }
        return best
    }

    private fun cheapestFor(cpu: Part, pools: Map<Category, List<Part>>): PcBuild? {
        var build = PcBuild(mapOf(Category.CPU to cpu))
        // Видеокарта, плата, память, SSD — независимы: берём самые дешёвые совместимые
        for (category in listOf(Category.GPU, Category.MOTHERBOARD, Category.RAM, Category.SSD)) {
            val part = pools.getValue(category).firstOrNull { checker.isClean(build.with(it)) } ?: return null
            build = build.with(part)
        }
        // Кулер и корпус зависят друг от друга — перебираем пары
        var bestPair: Pair<Part, Part>? = null
        for (cooler in pools.getValue(Category.COOLER)) {
            val withCooler = build.with(cooler)
            if (!checker.isClean(withCooler)) continue
            val case = pools.getValue(Category.CASE).firstOrNull { checker.isClean(withCooler.with(it)) } ?: continue
            if (bestPair == null || cooler.price + case.price < bestPair.first.price + bestPair.second.price) {
                bestPair = cooler to case
            }
        }
        val (cooler, case) = bestPair ?: return null
        build = build.with(cooler).with(case)
        val psu = pools.getValue(Category.PSU).firstOrNull { checker.isClean(build.with(it)) } ?: return null
        return build.with(psu)
    }

    // ---------------------------------------------------------------------------------------
    // Подбор под процессор: доли → починка перерасхода → апгрейды на остаток
    // ---------------------------------------------------------------------------------------

    private fun fillForCpu(
        cpu: Part,
        memType: String,
        systemBudget: Int,
        shares: Map<Category, Double>,
        prefs: BuildPreferences,
    ): PcBuild? {
        val rest = systemBudget - cpu.price
        if (rest <= 0) return null
        val pools = poolsFor(cpu, memType, prefs) ?: return null

        // Доли остальных категорий нормируем так, чтобы в сумме было 1
        val otherShareSum = FILL_ORDER.sumOf { shares[it] ?: 0.0 }
        val targets = FILL_ORDER.associateWith { ((shares[it] ?: 0.0) / otherShareSum * rest).toInt() }

        // Шаг 1: в каждой категории — лучшая деталь в рамках своей доли
        var build = PcBuild(mapOf(Category.CPU to cpu))
        for (category in FILL_ORDER) {
            val compatible = pools.getValue(category).filter { checker.isClean(build.with(it)) }
            if (compatible.isEmpty()) return null
            val target = targets.getValue(category)
            val pick = compatible
                .filter { it.price <= target }
                .maxWithOrNull(compareBy<Part>({ Scoring.effective(it, prefs.purpose) }, { -it.price }))
                ?: compatible.first() // самая дешёвая совместимая
            build = build.with(pick)
        }

        // Шаг 2: если вышли за бюджет — понижаем детали с наименьшей потерей полезности на рубль
        var guard = 0
        while (build.totalPrice > systemBudget && guard++ < MAX_STEPS) {
            build = cheapestLossDowngrade(build, pools, shares, prefs) ?: return null
        }
        if (build.totalPrice > systemBudget) return null

        // Шаг 3: раздаём остаток на апгрейды с наибольшим приростом полезности
        guard = 0
        while (guard++ < MAX_STEPS) {
            build = bestUpgrade(build, systemBudget, pools, shares, prefs) ?: break
        }
        return build
    }

    /** Один шаг понижения: деталь, замена которой экономит больше всего «на единицу потерь». */
    private fun cheapestLossDowngrade(
        build: PcBuild,
        pools: Map<Category, List<Part>>,
        shares: Map<Category, Double>,
        prefs: BuildPreferences,
    ): PcBuild? {
        var bestBuild: PcBuild? = null
        var bestRatio = Double.POSITIVE_INFINITY
        for (category in FILL_ORDER) {
            val current = build[category] ?: continue
            val weight = (shares[category] ?: 0.0) * Scoring.importance(category)
            for (option in pools.getValue(category)) {
                if (option.price >= current.price) continue
                val candidate = build.with(option)
                if (!checker.isClean(candidate)) continue
                val saved = current.price - option.price
                val loss = weight * (Scoring.effective(current, prefs.purpose) - Scoring.effective(option, prefs.purpose))
                val ratio = loss / saved
                if (ratio < bestRatio) {
                    bestRatio = ratio
                    bestBuild = candidate
                }
            }
        }
        return bestBuild
    }

    /** Один шаг улучшения: апгрейд с максимальным приростом полезности, который влезает в остаток. */
    private fun bestUpgrade(
        build: PcBuild,
        systemBudget: Int,
        pools: Map<Category, List<Part>>,
        shares: Map<Category, Double>,
        prefs: BuildPreferences,
    ): PcBuild? {
        val leftover = systemBudget - build.totalPrice
        if (leftover <= 0) return null
        val baseUtility = utility(build, prefs, shares)
        var bestBuild: PcBuild? = null
        var bestGain = 1e-9
        var bestCost = Int.MAX_VALUE
        for (category in FILL_ORDER) {
            val current = build[category] ?: continue
            // Пулы отсортированы по цене, поэтому дальше «дорогих» вариантов можно не смотреть
            for (option in pools.getValue(category)) {
                if (option.price - current.price > leftover) break
                if (Scoring.effective(option, prefs.purpose) <= Scoring.effective(current, prefs.purpose)) continue
                // Апгрейд может потребовать заменить зависимые детали (БП, корпус, кулер)
                val raw = build.with(option)
                val candidate = if (checker.isClean(raw)) raw else resolveDependencies(raw, category, pools) ?: continue
                val cost = candidate.totalPrice - build.totalPrice
                if (cost > leftover) continue
                val gain = utility(candidate, prefs, shares) - baseUtility
                if (gain > bestGain + 1e-9 || (abs(gain - bestGain) < 1e-9 && cost < bestCost)) {
                    bestGain = gain
                    bestCost = cost
                    bestBuild = candidate
                }
            }
        }
        return bestBuild
    }

    /** Подгоняет зависимые детали после замены [changed]: самые дешёвые подходящие варианты. */
    private fun resolveDependencies(
        build: PcBuild,
        changed: Category,
        pools: Map<Category, List<Part>>,
    ): PcBuild? {
        var result = build
        repeat(4) {
            val issues = checker.check(result)
            if (issues.isEmpty()) return result
            val target = issues.flatMap { it.categories }
                .filter { it != changed && it != Category.CPU && it in pools }
                .minByOrNull { FIX_ORDER.indexOf(it) }
                ?: return null
            val current = result[target]
            val fitting = pools.getValue(target).filter { checker.issuesFor(result.with(it), target).isEmpty() }
            val replacement = fitting
                .filter { current == null || it.score >= current.score }
                .minByOrNull { it.price }
                ?: fitting.minByOrNull { it.price }
                ?: return null
            result = result.with(replacement)
        }
        return if (checker.isClean(result)) result else null
    }

    // ---------------------------------------------------------------------------------------
    // Полезность сборки
    // ---------------------------------------------------------------------------------------

    /** Взвешенная сумма индексов + штраф за дисбаланс CPU/GPU в играх. */
    internal fun utility(build: PcBuild, prefs: BuildPreferences, shares: Map<Category, Double>): Double {
        var sum = 0.0
        for ((category, share) in shares) {
            val part = build[category] ?: continue
            sum += share * Scoring.importance(category) * Scoring.effective(part, prefs.purpose)
        }
        if (prefs.purpose == Purpose.GAMING) {
            val cpu = build[Category.CPU]?.score ?: 0
            val gpu = build[Category.GPU]?.score ?: 0
            val gap = gpu - cpu - BOTTLENECK_TOLERANCE
            if (gap > 0) sum -= gap * BOTTLENECK_PENALTY
        }
        return sum
    }

    companion object {
        /** Порядок подбора деталей после процессора (от независимых к зависимым). */
        private val FILL_ORDER = listOf(
            Category.GPU,
            Category.MOTHERBOARD,
            Category.RAM,
            Category.COOLER,
            Category.CASE,
            Category.PSU,
            Category.SSD,
        )

        /** Порядок, в котором меняем детали при исправлении совместимости. */
        private val FIX_ORDER = listOf(
            Category.PSU,
            Category.CASE,
            Category.COOLER,
            Category.RAM,
            Category.MOTHERBOARD,
            Category.SSD,
            Category.GPU,
            Category.CPU,
            Category.MONITOR,
        )

        private const val MAX_STEPS = 60
        private const val MAX_FIX_STEPS = 8
        private const val BOTTLENECK_TOLERANCE = 30
        private const val BOTTLENECK_PENALTY = 0.25
    }
}
