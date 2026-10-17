package com.neorig.pcbuilder.domain.model

/**
 * Вся локальная база деталей в памяти.
 * Списки в каждой категории отсортированы по цене (от дешёвых к дорогим).
 */
class PartCatalog(parts: Map<Category, List<Part>>) {

    private val byCategory: Map<Category, List<Part>> =
        Category.entries.associateWith { category ->
            (parts[category] ?: emptyList()).sortedWith(compareBy({ it.price }, { it.id }))
        }

    private val byId: Map<String, Part> = byCategory.values.flatten().associateBy { it.id }

    /** Детали категории, отсортированные по цене. */
    fun of(category: Category): List<Part> = byCategory[category].orEmpty()

    /** Поиск детали по id (например, при загрузке сохранённой сборки). */
    fun byId(id: String): Part? = byId[id]

    /** Количество деталей в базе. */
    val size: Int get() = byId.size
}
