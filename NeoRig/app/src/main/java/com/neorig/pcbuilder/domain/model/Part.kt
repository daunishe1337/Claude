package com.neorig.pcbuilder.domain.model

import kotlinx.serialization.Serializable

/**
 * Одна деталь из локальной базы (assets/db/\*.json).
 *
 * @property score     условный индекс производительности / качества (1..100) внутри категории;
 *                     для процессора — игровая производительность.
 * @property workScore индекс для рабочих задач (рендер, монтаж). Если не задан — берётся [score].
 */
@Serializable
data class Part(
    val id: String,
    val category: Category = Category.CPU,
    val name: String,
    val brand: String,
    val price: Int,
    val score: Int,
    val workScore: Int? = null,
    val specs: Specs = Specs(),
    val compat: Compat = Compat(),
    val visual: Visual = Visual(),
) {
    /** Индекс производительности с учётом назначения сборки. */
    fun scoreFor(purpose: Purpose): Int = when (purpose) {
        Purpose.GAMING -> score
        Purpose.WORK -> workScore ?: score
        // Для учёбы и офиса важна общая отзывчивость: среднее игрового и рабочего индексов
        Purpose.STUDY -> (score + (workScore ?: score)) / 2
    }

    /** Встроенная графика — «виртуальная» видеокарта за 0 ₽. */
    val isIntegratedGraphics: Boolean get() = id == INTEGRATED_GPU_ID

    companion object {
        const val INTEGRATED_GPU_ID = "gpu-integrated"
    }
}

/** Характеристики для отображения пользователю. Все поля необязательные. */
@Serializable
data class Specs(
    // Процессор
    val cores: Int? = null,
    val threads: Int? = null,
    val baseGhz: Double? = null,
    val boostGhz: Double? = null,
    val cacheMb: Int? = null,
    val integratedGraphics: Boolean? = null,
    // Материнская плата
    val chipset: String? = null,
    val m2Slots: Int? = null,
    val wifi: Boolean? = null,
    // Видеокарта
    val vramGb: Int? = null,
    // ОЗУ / SSD
    val capacityGb: Int? = null,
    val modules: Int? = null,
    val speedMhz: Int? = null,
    val readMbps: Int? = null,
    val writeMbps: Int? = null,
    val pcie: String? = null,
    // Блок питания
    val efficiency: String? = null,
    val modular: Boolean? = null,
    // Кулер
    val coolerType: String? = null,
    val fans: Int? = null,
    // Корпус
    val size: String? = null,
    val fansIncluded: Int? = null,
    // Монитор
    val sizeInch: Double? = null,
    val resolution: String? = null,
    val refreshHz: Int? = null,
    val panel: String? = null,
)

/**
 * Параметры совместимости.
 * Какие поля заполнены, зависит от категории (см. комментарии).
 */
@Serializable
data class Compat(
    /** Сокет: CPU, материнская плата. */
    val socket: String? = null,
    /** Поддерживаемые сокеты: кулер. */
    val sockets: List<String> = emptyList(),
    /** Поддерживаемые типы памяти: CPU (например, LGA1700 умеет и DDR4, и DDR5). */
    val memoryTypes: List<String> = emptyList(),
    /** Тип памяти: материнская плата, ОЗУ. */
    val memoryType: String? = null,
    /** Форм-фактор: материнская плата (ATX / mATX / ITX). */
    val formFactor: String? = null,
    /** Поддерживаемые форм-факторы плат: корпус. */
    val supportedFormFactors: List<String> = emptyList(),
    /** Потребляемая мощность (TDP), Вт: CPU, видеокарта. */
    val tdpW: Int? = null,
    /** Мощность блока питания, Вт. */
    val powerW: Int? = null,
    /** Максимальный отводимый TDP, Вт: кулер. */
    val maxTdpW: Int? = null,
    /** Длина видеокарты, мм. */
    val lengthMm: Int? = null,
    /** Максимальная длина видеокарты, мм: корпус. */
    val maxGpuLengthMm: Int? = null,
    /** Высота башенного кулера, мм. */
    val heightMm: Int? = null,
    /** Максимальная высота кулера, мм: корпус. */
    val maxCoolerHeightMm: Int? = null,
    /** Размер радиатора СЖО, мм (для воздушных кулеров — null). */
    val radiatorMm: Int? = null,
    /** Максимальный размер радиатора, мм: корпус (0 — СЖО не ставится). */
    val maxRadiatorMm: Int? = null,
)

/**
 * Внешний вид детали для 3D-сцены.
 * @property color  основной цвет (#RRGGBB)
 * @property accent акцентный цвет (#RRGGBB)
 * @property style  стиль модели: "dual-fan", "triple-fan", "tower", "aio", "mid", "panoramic" и т.д.
 * @property model  имя .glb-файла в assets/models (если нет — рисуется упрощённая геометрия)
 * @property rgb    есть ли у детали RGB-подсветка
 */
@Serializable
data class Visual(
    val color: String = "#2B2F3A",
    val accent: String = "#8B5CF6",
    val style: String = "default",
    val model: String? = null,
    val rgb: Boolean = false,
)
