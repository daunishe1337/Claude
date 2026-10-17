package com.neorig.pcbuilder.domain.model

/**
 * Категории комплектующих.
 * [fileName] — имя JSON-файла в assets/db, где лежат детали этой категории.
 */
enum class Category(val fileName: String) {
    CPU("cpus"),
    MOTHERBOARD("motherboards"),
    GPU("gpus"),
    RAM("rams"),
    SSD("ssds"),
    PSU("psus"),
    COOLER("coolers"),
    CASE("cases"),
    MONITOR("monitors");

    companion object {
        /** Категории системного блока (всё, кроме монитора). */
        val systemUnit: List<Category> = entries.filter { it != MONITOR }
    }
}
