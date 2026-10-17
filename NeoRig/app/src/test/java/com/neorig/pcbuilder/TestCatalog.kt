package com.neorig.pcbuilder

import com.neorig.pcbuilder.data.catalog.CatalogParser
import com.neorig.pcbuilder.domain.model.PartCatalog
import java.io.File

/** Загружает настоящую базу из src/main/assets — тесты проверяют реальные данные. */
object TestCatalog {

    val assetsDir: File by lazy {
        val candidates = listOfNotNull(
            System.getProperty("neorig.assets")?.let(::File),
            File("src/main/assets"),
            File("app/src/main/assets"),
        )
        candidates.firstOrNull { File(it, "db").isDirectory }
            ?: error("Не найдена папка assets: ${candidates.map { it.absolutePath }}")
    }

    val catalog: PartCatalog by lazy {
        CatalogParser { path -> File(assetsDir, path).inputStream() }.load()
    }
}
