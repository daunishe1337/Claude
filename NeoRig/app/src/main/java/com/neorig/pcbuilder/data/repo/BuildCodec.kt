package com.neorig.pcbuilder.data.repo

import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.PartCatalog
import com.neorig.pcbuilder.domain.model.PcBuild
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json

/** Перевод сборки и запроса в JSON-строки и обратно (для Room и SharedPreferences). */
object BuildCodec {

    private val json = Json { ignoreUnknownKeys = true }
    private val mapSerializer = MapSerializer(String.serializer(), String.serializer())

    fun encodeParts(build: PcBuild): String =
        json.encodeToString(mapSerializer, build.parts.entries.associate { (category, part) -> category.name to part.id })

    /** Детали, которых больше нет в базе, просто пропускаются. */
    fun decodeParts(text: String, catalog: PartCatalog): PcBuild {
        val ids = runCatching { json.decodeFromString(mapSerializer, text) }.getOrDefault(emptyMap())
        val parts = ids.mapNotNull { (categoryName, id) ->
            val category = Category.entries.firstOrNull { it.name == categoryName } ?: return@mapNotNull null
            val part = catalog.byId(id)?.takeIf { it.category == category } ?: return@mapNotNull null
            category to part
        }.toMap()
        return PcBuild(parts)
    }

    fun encodeRequest(request: BuildRequest): String = json.encodeToString(BuildRequest.serializer(), request)

    fun decodeRequest(text: String?): BuildRequest? =
        text?.let { runCatching { json.decodeFromString(BuildRequest.serializer(), it) }.getOrNull() }
}
