package com.neorig.pcbuilder.ui.saved

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.neorig.pcbuilder.data.repo.SavedBuildsRepository
import com.neorig.pcbuilder.domain.model.SavedBuild
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

/** Список сохранённых сборок и выбор двух сборок для сравнения. */
class SavedBuildsViewModel(
    private val repository: SavedBuildsRepository,
) : ViewModel() {

    /** null — ещё загружаются. */
    val builds: StateFlow<List<SavedBuild>?> = repository.observe()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), null)

    private val _selection = MutableStateFlow<List<Long>>(emptyList())

    /** Отмеченные для сравнения (не больше двух, в порядке выбора). */
    val selection: StateFlow<List<Long>> = _selection.asStateFlow()

    fun toggleSelection(id: Long) {
        _selection.update { current ->
            when {
                id in current -> current - id
                current.size >= 2 -> current.drop(1) + id
                else -> current + id
            }
        }
    }

    fun clearSelection() = _selection.update { emptyList() }

    fun delete(id: Long) {
        _selection.update { it - id }
        viewModelScope.launch { repository.delete(id) }
    }

    fun find(id: Long): SavedBuild? = builds.value?.firstOrNull { it.id == id }
}
