package com.neorig.pcbuilder.domain.engine

import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.domain.model.Resolution

/**
 * Доли бюджета.
 * 1. Сначала на монитор резервируется 15–25 % в зависимости от разрешения.
 * 2. Остаток делится между деталями системного блока по приоритетам назначения.
 * Эти же доли служат весами «полезности» при выборе лучшей сборки.
 */
object BudgetShares {

    /** Доля всего бюджета, резервируемая на монитор. */
    fun monitorShare(resolution: Resolution): Double = when (resolution) {
        Resolution.FHD -> 0.15
        Resolution.QHD -> 0.20
        Resolution.UHD -> 0.25
    }

    /** Доли системного блока (сумма = 1.0). */
    fun systemShares(prefs: BuildPreferences): Map<Category, Double> {
        val base: MutableMap<Category, Double> = when (prefs.purpose) {
            // Игры: больше всего — на видеокарту
            Purpose.GAMING -> mutableMapOf(
                Category.GPU to 0.40,
                Category.CPU to 0.18,
                Category.MOTHERBOARD to 0.10,
                Category.RAM to 0.08,
                Category.SSD to 0.07,
                Category.PSU to 0.07,
                Category.COOLER to 0.04,
                Category.CASE to 0.06,
            )
            // Работа и монтаж: процессор, память и быстрый накопитель
            Purpose.WORK -> mutableMapOf(
                Category.CPU to 0.28,
                Category.GPU to 0.22,
                Category.RAM to 0.14,
                Category.MOTHERBOARD to 0.11,
                Category.SSD to 0.10,
                Category.PSU to 0.06,
                Category.COOLER to 0.05,
                Category.CASE to 0.04,
            )
            // Учёба и офис: сбалансированно, видеокарта вторична
            Purpose.STUDY -> mutableMapOf(
                Category.CPU to 0.26,
                Category.GPU to 0.12,
                Category.MOTHERBOARD to 0.14,
                Category.RAM to 0.13,
                Category.SSD to 0.13,
                Category.PSU to 0.08,
                Category.COOLER to 0.05,
                Category.CASE to 0.09,
            )
        }
        // Для игр учитываем разрешение: в 4K упор в видеокарту, в 1080p важнее процессор
        if (prefs.purpose == Purpose.GAMING) {
            when (prefs.resolution) {
                Resolution.UHD -> {
                    base.add(Category.GPU, +0.05)
                    base.add(Category.CPU, -0.03)
                    base.add(Category.MOTHERBOARD, -0.02)
                }
                Resolution.FHD -> {
                    base.add(Category.GPU, -0.03)
                    base.add(Category.CPU, +0.03)
                }
                Resolution.QHD -> Unit
            }
        }
        val sum = base.values.sum()
        return base.mapValues { it.value / sum }
    }

    private fun MutableMap<Category, Double>.add(category: Category, delta: Double) {
        this[category] = (this[category] ?: 0.0) + delta
    }
}
