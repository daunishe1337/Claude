package com.neorig.pcbuilder.ui.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.neorig.pcbuilder.data.repo.AppLanguage
import com.neorig.pcbuilder.data.repo.AppSettings
import com.neorig.pcbuilder.data.repo.CatalogRepository
import com.neorig.pcbuilder.data.repo.SavedBuildsRepository
import com.neorig.pcbuilder.data.repo.SettingsRepository
import com.neorig.pcbuilder.render.scene.RgbMode
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/** Настройки: язык, RGB-подсветка, качество 3D, очистка данных. */
class SettingsViewModel(
    private val repository: SettingsRepository,
    private val savedBuildsRepository: SavedBuildsRepository,
    private val catalogRepository: CatalogRepository,
) : ViewModel() {

    val settings: StateFlow<AppSettings> = repository.settings

    private val _partsCount = MutableStateFlow(0)
    val partsCount: StateFlow<Int> = _partsCount.asStateFlow()

    init {
        viewModelScope.launch { _partsCount.value = catalogRepository.catalog().size }
    }

    /** Возвращает true, если язык действительно изменился (тогда Activity пересоздаётся). */
    fun setLanguage(language: AppLanguage): Boolean {
        if (settings.value.language == language) return false
        repository.update { it.copy(language = language) }
        return true
    }

    fun setRgbEnabled(enabled: Boolean) = repository.update { it.copy(rgbEnabled = enabled) }
    fun setRgbColor(argb: Int) = repository.update { it.copy(rgbColor = argb) }
    fun setRgbMode(mode: RgbMode) = repository.update { it.copy(rgbMode = mode) }
    fun setAntialiasing(enabled: Boolean) = repository.update { it.copy(antialiasing = enabled) }
    fun setAutoRotate(enabled: Boolean) = repository.update { it.copy(autoRotatePreview = enabled) }

    fun clearSavedBuilds() {
        viewModelScope.launch { savedBuildsRepository.deleteAll() }
    }
}
