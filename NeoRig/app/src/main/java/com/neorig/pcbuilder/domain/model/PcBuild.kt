package com.neorig.pcbuilder.domain.model

/**
 * Сборка — по одной детали на категорию.
 * Неизменяемый объект: любые замены создают новую сборку.
 */
data class PcBuild(val parts: Map<Category, Part>) {

    operator fun get(category: Category): Part? = parts[category]

    /** Итоговая стоимость всех деталей, включая монитор. */
    val totalPrice: Int get() = parts.values.sumOf { it.price }

    /** Все ли категории заполнены. */
    val isComplete: Boolean get() = Category.entries.all { it in parts }

    /** Новая сборка с заменённой деталью (категория берётся из самой детали). */
    fun with(part: Part): PcBuild = PcBuild(parts + (part.category to part))

    /** Детали в порядке категорий (для списков и экспорта). */
    fun orderedParts(): List<Part> = Category.entries.mapNotNull { parts[it] }

    /** Траты по категориям — для полосы бюджета. */
    fun spendingByCategory(): Map<Category, Int> =
        Category.entries.associateWith { parts[it]?.price ?: 0 }

    companion object {
        val EMPTY = PcBuild(emptyMap())
    }
}

/** Сохранённая сборка (то, что лежит в Room, уже связанное с базой деталей). */
data class SavedBuild(
    val id: Long,
    val name: String,
    val createdAt: Long,
    val request: BuildRequest,
    val build: PcBuild,
)
