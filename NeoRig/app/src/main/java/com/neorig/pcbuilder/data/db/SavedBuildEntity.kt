package com.neorig.pcbuilder.data.db

import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * Сохранённая сборка в Room.
 * Детали хранятся как id (JSON-словарь «категория → id детали»), а не целиком:
 * при обновлении базы цен сохранённые сборки подтянут актуальные данные.
 */
@Entity(tableName = "saved_builds")
data class SavedBuildEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val name: String,
    val createdAt: Long,
    val budget: Int,
    val totalPrice: Int,
    /** BuildRequest в JSON. */
    val requestJson: String,
    /** Map<Category.name, partId> в JSON. */
    val partIdsJson: String,
)
