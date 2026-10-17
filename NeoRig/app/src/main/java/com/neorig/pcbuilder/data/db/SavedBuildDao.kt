package com.neorig.pcbuilder.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface SavedBuildDao {

    @Query("SELECT * FROM saved_builds ORDER BY createdAt DESC")
    fun observeAll(): Flow<List<SavedBuildEntity>>

    @Query("SELECT * FROM saved_builds WHERE id = :id")
    suspend fun getById(id: Long): SavedBuildEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(entity: SavedBuildEntity): Long

    @Query("DELETE FROM saved_builds WHERE id = :id")
    suspend fun deleteById(id: Long)

    @Query("DELETE FROM saved_builds")
    suspend fun deleteAll()
}
