package com.neorig.pcbuilder.domain.compat

import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.PcBuild
import kotlin.math.ceil

/**
 * Проверка совместимости комплектующих.
 * Работает с неполными сборками: проверяются только пары, где обе детали выбраны.
 */
class CompatibilityChecker {

    /** Полный список проблем сборки. */
    fun check(build: PcBuild): List<Incompatibility> {
        val issues = mutableListOf<Incompatibility>()
        val cpu = build[Category.CPU]
        val board = build[Category.MOTHERBOARD]
        val gpu = build[Category.GPU]
        val ram = build[Category.RAM]
        val psu = build[Category.PSU]
        val cooler = build[Category.COOLER]
        val case = build[Category.CASE]

        // 1. Сокет CPU = сокет платы; CPU поддерживает тип памяти платы
        if (cpu != null && board != null) {
            val cpuSocket = cpu.compat.socket
            val boardSocket = board.compat.socket
            if (cpuSocket != null && boardSocket != null && cpuSocket != boardSocket) {
                issues += Incompatibility.SocketMismatch(cpuSocket, boardSocket)
            }
            val boardMem = board.compat.memoryType
            val cpuMem = cpu.compat.memoryTypes
            if (boardMem != null && cpuMem.isNotEmpty() && boardMem !in cpuMem) {
                issues += Incompatibility.CpuMemoryUnsupported(cpuMem, boardMem)
            }
        }

        // 2. Тип памяти платы = тип модулей ОЗУ
        if (board != null && ram != null) {
            val boardMem = board.compat.memoryType
            val ramMem = ram.compat.memoryType
            if (boardMem != null && ramMem != null && boardMem != ramMem) {
                issues += Incompatibility.RamTypeMismatch(boardMem, ramMem)
            }
        }

        // 3. Встроенная графика требует CPU с iGPU
        if (gpu != null && gpu.isIntegratedGraphics && cpu != null &&
            cpu.specs.integratedGraphics != true
        ) {
            issues += Incompatibility.NoIntegratedGraphics(cpu.name)
        }

        // 4. Кулер: крепление под сокет и запас по TDP
        if (cooler != null && cpu != null) {
            val socket = cpu.compat.socket
            if (socket != null && cooler.compat.sockets.isNotEmpty() && socket !in cooler.compat.sockets) {
                issues += Incompatibility.CoolerSocketUnsupported(socket, cooler.compat.sockets)
            }
            val coolerTdp = cooler.compat.maxTdpW
            val cpuTdp = cpu.compat.tdpW
            if (coolerTdp != null && cpuTdp != null && coolerTdp < cpuTdp) {
                issues += Incompatibility.CoolerUnderpowered(coolerTdp, cpuTdp)
            }
        }

        // 5. Корпус: форм-фактор платы, длина видеокарты, высота кулера / радиатор
        if (case != null) {
            if (board != null) {
                val ff = board.compat.formFactor
                val supported = case.compat.supportedFormFactors
                if (ff != null && supported.isNotEmpty() && ff !in supported) {
                    issues += Incompatibility.FormFactorUnsupported(ff, supported)
                }
            }
            if (gpu != null) {
                val len = gpu.compat.lengthMm
                val max = case.compat.maxGpuLengthMm
                if (len != null && max != null && len > max) {
                    issues += Incompatibility.GpuTooLong(len, max)
                }
            }
            if (cooler != null) {
                val radiator = cooler.compat.radiatorMm
                if (radiator != null) {
                    val maxRad = case.compat.maxRadiatorMm ?: 0
                    if (radiator > maxRad) issues += Incompatibility.RadiatorUnsupported(radiator, maxRad)
                } else {
                    val h = cooler.compat.heightMm
                    val maxH = case.compat.maxCoolerHeightMm
                    if (h != null && maxH != null && h > maxH) {
                        issues += Incompatibility.CoolerTooTall(h, maxH)
                    }
                }
            }
        }

        // 6. Блок питания ≥ суммарное потребление × 1.3
        if (psu != null && (cpu != null || gpu != null)) {
            val required = requiredPsuWatts(build)
            val watts = psu.compat.powerW ?: 0
            if (watts < required) issues += Incompatibility.PsuTooWeak(watts, required)
        }
        return issues
    }

    /** Есть ли в сборке ошибки (предупреждения не считаются). */
    fun hasErrors(build: PcBuild): Boolean = check(build).any { it.severity == Severity.ERROR }

    /** Сборка «чистая» — ни ошибок, ни предупреждений. */
    fun isClean(build: PcBuild): Boolean = check(build).isEmpty()

    /** Проблемы, затрагивающие конкретную категорию. */
    fun issuesFor(build: PcBuild, category: Category): List<Incompatibility> =
        check(build).filter { category in it.categories }

    /** Примерное пиковое потребление системы, Вт. */
    fun estimatedPowerDraw(build: PcBuild): Int {
        val cpu = build[Category.CPU]?.compat?.tdpW ?: 0
        val gpu = build[Category.GPU]?.compat?.tdpW ?: 0
        return cpu + gpu + BASE_SYSTEM_DRAW_W
    }

    /** Минимально допустимая мощность БП: потребление × 1.3. */
    fun requiredPsuWatts(build: PcBuild): Int =
        ceil(estimatedPowerDraw(build) * PSU_HEADROOM).toInt()

    companion object {
        /** Плата, память, накопители, вентиляторы, подсветка. */
        const val BASE_SYSTEM_DRAW_W = 75

        /** Запас мощности блока питания. */
        const val PSU_HEADROOM = 1.3
    }
}
