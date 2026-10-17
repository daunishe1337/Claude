package com.neorig.pcbuilder.di

import android.content.Context
import android.util.Log
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import com.neorig.pcbuilder.data.db.AppDatabase
import com.neorig.pcbuilder.data.repo.CatalogRepository
import com.neorig.pcbuilder.data.repo.SavedBuildsRepository
import com.neorig.pcbuilder.data.repo.SettingsRepository
import com.neorig.pcbuilder.render.scene.SceneComposer
import com.neorig.pcbuilder.ui.build.BuilderViewModel
import com.neorig.pcbuilder.ui.saved.SavedBuildsViewModel
import com.neorig.pcbuilder.ui.settings.SettingsViewModel
import java.io.IOException

/**
 * Ручное внедрение зависимостей: один контейнер на всё приложение.
 * (Для приложения такого размера Hilt/Koin избыточны.)
 */
class AppContainer(context: Context) {

    private val appContext = context.applicationContext

    val catalogRepository = CatalogRepository(appContext.assets)
    val settingsRepository = SettingsRepository(appContext)
    private val database by lazy { AppDatabase.create(appContext) }
    val savedBuildsRepository by lazy { SavedBuildsRepository(database.savedBuildDao(), catalogRepository) }

    /** Сборщик 3D-сцены: .glb читаются из assets/models, при отсутствии — упрощённая геометрия. */
    val sceneComposer = SceneComposer(
        modelSource = { path ->
            try {
                appContext.assets.open(path).use { it.readBytes() }
            } catch (e: IOException) {
                null
            }
        },
        log = { Log.w("NeoRig3D", it) },
    )

    /** Фабрика всех ViewModel приложения. */
    val viewModelFactory: ViewModelProvider.Factory = viewModelFactory {
        initializer { BuilderViewModel(catalogRepository, savedBuildsRepository, settingsRepository) }
        initializer { SavedBuildsViewModel(savedBuildsRepository) }
        initializer { SettingsViewModel(settingsRepository, savedBuildsRepository, catalogRepository) }
    }
}
