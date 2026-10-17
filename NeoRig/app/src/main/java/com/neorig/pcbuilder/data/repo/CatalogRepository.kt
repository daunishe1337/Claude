package com.neorig.pcbuilder.data.repo

import android.content.res.AssetManager
import com.neorig.pcbuilder.data.catalog.CatalogParser
import com.neorig.pcbuilder.domain.model.PartCatalog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/** Загружает базу деталей из assets один раз и держит её в памяти. */
class CatalogRepository(private val assets: AssetManager) {

    private val mutex = Mutex()
    @Volatile private var cached: PartCatalog? = null

    suspend fun catalog(): PartCatalog {
        cached?.let { return it }
        return mutex.withLock {
            cached ?: withContext(Dispatchers.IO) {
                CatalogParser { path -> assets.open(path) }.load()
            }.also { cached = it }
        }
    }
}
