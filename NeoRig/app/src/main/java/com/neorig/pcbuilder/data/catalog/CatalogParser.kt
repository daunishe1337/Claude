package com.neorig.pcbuilder.data.catalog

import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.PartCatalog
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.InputStream

/** Формат одного файла базы: категория + список деталей. */
@Serializable
private data class CategoryFile(
    val category: Category,
    val items: List<Part>,
)

/**
 * Разбор JSON-базы деталей. Не зависит от Android:
 * источник файлов передаётся функцией [open] (в приложении — AssetManager::open,
 * в тестах — чтение с диска).
 */
class CatalogParser(private val open: (path: String) -> InputStream) {

    private val json = Json {
        ignoreUnknownKeys = true // новые поля в JSON не ломают старую версию приложения
        isLenient = true
    }

    fun load(): PartCatalog {
        val parts = Category.entries.associateWith { category ->
            val text = open("$DB_DIR/${category.fileName}.json").bufferedReader(Charsets.UTF_8).use { it.readText() }
            val file = json.decodeFromString(CategoryFile.serializer(), text)
            require(file.category == category) {
                "Файл ${category.fileName}.json содержит категорию ${file.category}"
            }
            // Категория задаётся файлом, поэтому в каждой детали её можно не указывать
            file.items.map { it.copy(category = category) }
        }
        return PartCatalog(parts)
    }

    companion object {
        const val DB_DIR = "db"
    }
}
