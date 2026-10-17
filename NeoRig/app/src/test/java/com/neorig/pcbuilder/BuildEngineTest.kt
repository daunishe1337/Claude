package com.neorig.pcbuilder

import com.neorig.pcbuilder.domain.compat.CompatibilityChecker
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.engine.AssembleResult
import com.neorig.pcbuilder.domain.engine.BuildEngine
import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.GpuBrand
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.domain.model.RefreshRate
import com.neorig.pcbuilder.domain.model.Resolution
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class BuildEngineTest {

    private val catalog = TestCatalog.catalog
    private val checker = CompatibilityChecker()
    private val engine = BuildEngine(catalog, checker)

    private fun success(budget: Int, prefs: BuildPreferences = BuildPreferences()): AssembleResult.Success {
        val result = engine.assemble(BuildRequest(budget, prefs))
        return result as? AssembleResult.Success ?: error("Ожидали сборку для $budget / $prefs, получили $result")
    }

    @Test
    fun everyCombinationProducesValidResult() {
        val budgets = listOf(30_000, 45_000, 60_000, 80_000, 100_000, 150_000, 200_000, 300_000, 500_000, 1_000_000)
        for (purpose in Purpose.entries) for (cpu in CpuBrand.entries) for (gpu in GpuBrand.entries)
            for (res in Resolution.entries) for (hz in RefreshRate.entries) for (budget in budgets) {
                val prefs = BuildPreferences(cpu, gpu, purpose, res, hz)
                when (val result = engine.assemble(BuildRequest(budget, prefs))) {
                    is AssembleResult.Success -> {
                        val build = result.build
                        assertTrue("неполная сборка $prefs $budget", build.isComplete)
                        assertTrue("перерасход $prefs $budget: ${build.totalPrice}", build.totalPrice <= budget)
                        assertTrue("несовместимо $prefs $budget: ${checker.check(build)}", checker.isClean(build))
                        if (cpu != CpuBrand.ANY) assertEquals(cpu.vendor, build[Category.CPU]!!.brand)
                        val g = build[Category.GPU]!!
                        if (gpu != GpuBrand.ANY) assertEquals(gpu.vendor, g.brand)
                        if (g.isIntegratedGraphics) assertEquals(Purpose.STUDY, purpose)
                        assertEquals(res.code, build[Category.MONITOR]!!.specs.resolution)
                    }
                    is AssembleResult.InsufficientBudget -> {
                        assertTrue(result.minimumBudget > budget)
                        // С минимальным бюджетом сборка обязана получиться
                        val retry = engine.assemble(BuildRequest(result.minimumBudget, prefs))
                        if (retry !is AssembleResult.Success) fail("минимум ${result.minimumBudget} не сработал для $prefs")
                    }
                    AssembleResult.NoCompatibleParts -> fail("нет деталей для $prefs")
                }
            }
    }

    @Test
    fun cheapGamingBudgetReportsMinimum() {
        val result = engine.assemble(BuildRequest(30_000, BuildPreferences(purpose = Purpose.GAMING)))
        assertTrue(result is AssembleResult.InsufficientBudget)
        val minimum = (result as AssembleResult.InsufficientBudget).minimumBudget
        assertTrue("минимум $minimum", minimum in 35_000..60_000)
    }

    @Test
    fun officeBuildFitsIntoMinimalBudget() {
        val build = success(30_000, BuildPreferences(purpose = Purpose.STUDY, resolution = Resolution.FHD, refreshRate = RefreshRate.HZ_60)).build
        assertTrue(build[Category.GPU]!!.isIntegratedGraphics)
    }

    @Test
    fun gamingSpendsMostOnGpu() {
        for (budget in listOf(100_000, 150_000, 250_000, 400_000)) {
            val build = success(budget, BuildPreferences(purpose = Purpose.GAMING, resolution = Resolution.QHD)).build
            val spending = build.spendingByCategory().filterKeys { it != Category.MONITOR }
            assertEquals("бюджет $budget: $spending", Category.GPU, spending.maxBy { it.value }.key)
        }
    }

    @Test
    fun workBuildPrefersCpuAndRam() {
        val build = success(250_000, BuildPreferences(purpose = Purpose.WORK, resolution = Resolution.QHD)).build
        assertTrue(build[Category.RAM]!!.specs.capacityGb!! >= 32)
        assertTrue(build[Category.CPU]!!.specs.cores!! >= 8)
    }

    @Test
    fun budgetIsUsedEfficiently() {
        for (budget in listOf(80_000, 150_000, 300_000)) {
            val build = success(budget).build
            val leftover = budget - build.totalPrice
            assertTrue("бюджет $budget, остаток $leftover", leftover < budget * 0.08)
        }
    }

    @Test
    fun monitorMatchesResolutionAndRefresh() {
        val build = success(200_000, BuildPreferences(resolution = Resolution.QHD, refreshRate = RefreshRate.HZ_240)).build
        val monitor = build[Category.MONITOR]!!
        assertEquals("1440p", monitor.specs.resolution)
        assertTrue(monitor.specs.refreshHz!! >= 240)
    }

    @Test
    fun manualReplacementMarksIncompatibleOptions() {
        val build = success(150_000, BuildPreferences(cpuBrand = CpuBrand.AMD)).build
        val options = engine.options(build, Category.CPU)
        val intel = options.first { it.part.brand == "Intel" }
        assertTrue(intel.hasErrors)
        assertTrue(intel.issues.any { it is Incompatibility.SocketMismatch })
        assertTrue(options.single { it.isCurrent }.issues.isEmpty())
    }

    @Test
    fun autoFixRepairsPlatformChange() {
        val build = success(150_000, BuildPreferences(cpuBrand = CpuBrand.AMD)).build
        val intelCpu = catalog.of(Category.CPU).first { it.id == "cpu-i5-14600kf" }
        val broken = build.with(intelCpu)
        assertTrue(checker.hasErrors(broken))
        val fixed = engine.autoFix(broken, locked = Category.CPU)
        assertEquals(intelCpu, fixed[Category.CPU])
        assertTrue(checker.check(fixed).toString(), checker.isClean(fixed))
    }

    @Test
    fun printSampleBuilds() {
        // Не проверка, а наглядный вывод для настройки алгоритма
        for (purpose in Purpose.entries) for (budget in listOf(50_000, 100_000, 150_000, 250_000, 500_000, 1_000_000)) {
            val prefs = BuildPreferences(purpose = purpose, resolution = Resolution.QHD)
            val started = System.nanoTime()
            val result = engine.assemble(BuildRequest(budget, prefs))
            val ms = (System.nanoTime() - started) / 1_000_000
            println("== $purpose $budget (${ms} мс)")
            if (result is AssembleResult.Success) {
                result.build.orderedParts().forEach { println("   ${it.category.name.padEnd(12)} ${it.price.toString().padStart(7)}  ${it.name}") }
                println("   ИТОГО ${result.build.totalPrice}, остаток ${budget - result.build.totalPrice}")
            } else println("   $result")
        }
    }
}
