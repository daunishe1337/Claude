package com.neorig.pcbuilder.data.repo

import com.neorig.pcbuilder.data.db.SavedBuildDao
import com.neorig.pcbuilder.data.db.SavedBuildEntity
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.PartCatalog
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.SavedBuild
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

/** Сохранённые сборки: Room ↔ доменные модели. */
class SavedBuildsRepository(
    private val dao: SavedBuildDao,
    private val catalogRepository: CatalogRepository,
) {

    fun observe(): Flow<List<SavedBuild>> = dao.observeAll().map { entities ->
        val catalog = catalogRepository.catalog()
        entities.map { it.toDomain(catalog) }
    }

    suspend fun save(name: String, request: BuildRequest, build: PcBuild): Long =
        dao.insert(
            SavedBuildEntity(
                name = name,
                createdAt = System.currentTimeMillis(),
                budget = request.budget,
                totalPrice = build.totalPrice,
                requestJson = BuildCodec.encodeRequest(request),
                partIdsJson = BuildCodec.encodeParts(build),
            )
        )

    suspend fun delete(id: Long) = dao.deleteById(id)

    suspend fun deleteAll() = dao.deleteAll()

    private fun SavedBuildEntity.toDomain(catalog: PartCatalog) = SavedBuild(
        id = id,
        name = name,
        createdAt = createdAt,
        request = BuildCodec.decodeRequest(requestJson) ?: BuildRequest(budget = budget),
        build = BuildCodec.decodeParts(partIdsJson, catalog),
    )
}
