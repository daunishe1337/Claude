package com.neorig.pcbuilder.domain.engine

import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.Purpose
import kotlin.math.min

/** Правила оценки «полезности» деталей при подборе. */
internal object Scoring {

    /**
     * Насколько категория влияет на итоговую производительность.
     * Корпус и блок питания важны, но деньги в первую очередь должны идти в CPU/GPU/память.
     */
    fun importance(category: Category): Double = when (category) {
        Category.CPU, Category.GPU, Category.RAM, Category.SSD, Category.MONITOR -> 1.0
        Category.MOTHERBOARD, Category.COOLER -> 0.5
        Category.PSU -> 0.45
        Category.CASE -> 0.35
    }

    /**
     * Потолок «разумной» производительности для назначения.
     * Офисному ПК не нужна флагманская видеокарта: выше потолка апгрейд не даёт пользы,
     * и неиспользованные деньги остаются в «остатке бюджета».
     */
    fun cap(purpose: Purpose, category: Category): Int = when (purpose) {
        Purpose.STUDY -> when (category) {
            Category.GPU -> 40
            Category.CPU -> 70
            Category.RAM -> 80
            Category.SSD -> 82
            Category.MOTHERBOARD -> 64
            Category.PSU -> 60
            Category.COOLER -> 58
            Category.CASE -> 62
            Category.MONITOR -> 100
        }
        else -> 100
    }

    /** Индекс детали с учётом назначения и потолка. */
    fun effective(part: Part, purpose: Purpose): Int =
        min(part.scoreFor(purpose), cap(purpose, part.category))
}
