package com.neorig.pcbuilder.domain.compat

import com.neorig.pcbuilder.domain.model.Category

/** Насколько серьёзна проблема. */
enum class Severity {
    /** Физически несовместимо: сборка не заработает или не соберётся. */
    ERROR,

    /** Соберётся, но есть риск (например, перегрев). */
    WARNING,
}

/**
 * Найденная проблема совместимости.
 * Тексты не хранятся здесь — UI превращает каждый тип в локализованную строку.
 *
 * @property categories категории, которых касается проблема (для подсветки в списке)
 */
sealed class Incompatibility(
    val categories: Set<Category>,
    val severity: Severity,
) {
    /** Сокет процессора не совпадает с сокетом материнской платы. */
    data class SocketMismatch(val cpuSocket: String, val boardSocket: String) :
        Incompatibility(setOf(Category.CPU, Category.MOTHERBOARD), Severity.ERROR)

    /** Процессор не поддерживает тип памяти платы. */
    data class CpuMemoryUnsupported(val cpuTypes: List<String>, val boardType: String) :
        Incompatibility(setOf(Category.CPU, Category.MOTHERBOARD), Severity.ERROR)

    /** Тип модулей ОЗУ не совпадает с типом слотов платы. */
    data class RamTypeMismatch(val boardType: String, val ramType: String) :
        Incompatibility(setOf(Category.MOTHERBOARD, Category.RAM), Severity.ERROR)

    /** Форм-фактор платы не поддерживается корпусом. */
    data class FormFactorUnsupported(val boardFormFactor: String, val caseSupports: List<String>) :
        Incompatibility(setOf(Category.MOTHERBOARD, Category.CASE), Severity.ERROR)

    /** Видеокарта длиннее, чем позволяет корпус. */
    data class GpuTooLong(val gpuLengthMm: Int, val caseMaxMm: Int) :
        Incompatibility(setOf(Category.GPU, Category.CASE), Severity.ERROR)

    /** Башенный кулер выше, чем позволяет корпус. */
    data class CoolerTooTall(val coolerHeightMm: Int, val caseMaxMm: Int) :
        Incompatibility(setOf(Category.COOLER, Category.CASE), Severity.ERROR)

    /** Радиатор СЖО не помещается в корпус. */
    data class RadiatorUnsupported(val radiatorMm: Int, val caseMaxMm: Int) :
        Incompatibility(setOf(Category.COOLER, Category.CASE), Severity.ERROR)

    /** Кулер не имеет крепления под сокет процессора. */
    data class CoolerSocketUnsupported(val socket: String, val coolerSockets: List<String>) :
        Incompatibility(setOf(Category.COOLER, Category.CPU), Severity.ERROR)

    /** Мощности блока питания не хватает (нужно потребление × 1.3). */
    data class PsuTooWeak(val psuWatts: Int, val requiredWatts: Int) :
        Incompatibility(setOf(Category.PSU, Category.CPU, Category.GPU), Severity.ERROR)

    /** Выбрана встроенная графика, но в процессоре её нет. */
    data class NoIntegratedGraphics(val cpuName: String) :
        Incompatibility(setOf(Category.GPU, Category.CPU), Severity.ERROR)

    /** Кулер рассчитан на меньший TDP, чем у процессора: возможен троттлинг. */
    data class CoolerUnderpowered(val coolerTdpW: Int, val cpuTdpW: Int) :
        Incompatibility(setOf(Category.COOLER, Category.CPU), Severity.WARNING)
}
