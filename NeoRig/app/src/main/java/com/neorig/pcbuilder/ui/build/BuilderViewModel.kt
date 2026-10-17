package com.neorig.pcbuilder.ui.build

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.neorig.pcbuilder.data.repo.BuildCodec
import com.neorig.pcbuilder.data.repo.CatalogRepository
import com.neorig.pcbuilder.data.repo.SavedBuildsRepository
import com.neorig.pcbuilder.data.repo.SettingsRepository
import com.neorig.pcbuilder.domain.compat.CompatibilityChecker
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.engine.AssembleResult
import com.neorig.pcbuilder.domain.engine.BuildEngine
import com.neorig.pcbuilder.domain.engine.PartOption
import com.neorig.pcbuilder.domain.fps.FpsEstimate
import com.neorig.pcbuilder.domain.fps.FpsEstimator
import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.GpuBrand
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.PartCatalog
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.domain.model.RefreshRate
import com.neorig.pcbuilder.domain.model.Resolution
import com.neorig.pcbuilder.domain.model.SavedBuild
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Почему не получилось собрать ПК. */
sealed interface AssembleError {
    data class Insufficient(val minimumBudget: Int) : AssembleError
    data object NoParts : AssembleError
}

/** Одноразовые события для UI (навигация, сообщения). */
sealed interface BuilderEvent {
    data object OpenBuild : BuilderEvent
    data class Saved(val name: String) : BuilderEvent
}

/** Состояние конструктора: запрос, текущая сборка и всё, что из неё вычисляется. */
data class BuilderUiState(
    val loading: Boolean = true,
    val request: BuildRequest = BuildRequest(),
    val build: PcBuild? = null,
    val issues: List<Incompatibility> = emptyList(),
    val fps: List<FpsEstimate> = emptyList(),
    val powerDraw: Int = 0,
    val requiredPsu: Int = 0,
    val assembling: Boolean = false,
    val error: AssembleError? = null,
    val minimumBudget: Int? = null,
    val partsCount: Int = 0,
) {
    val total: Int get() = build?.totalPrice ?: 0
    val remaining: Int get() = request.budget - total
}

/**
 * ViewModel мастера «Бюджет → Предпочтения → Сборка» и экрана сборки.
 * Живёт на уровне Activity: её же используют вкладки «3D» и «Мои сборки».
 */
class BuilderViewModel(
    private val catalogRepository: CatalogRepository,
    private val savedBuildsRepository: SavedBuildsRepository,
    private val settingsRepository: SettingsRepository,
) : ViewModel() {

    private val checker = CompatibilityChecker()
    private val fpsEstimator = FpsEstimator()
    private var catalog: PartCatalog? = null
    private var engine: BuildEngine? = null

    /** Последняя категория, заменённая вручную: её автоисправление не трогает. */
    var lastReplaced: Category? = null
        private set

    private val _state = MutableStateFlow(BuilderUiState())
    val state: StateFlow<BuilderUiState> = _state.asStateFlow()

    private val _events = Channel<BuilderEvent>(Channel.BUFFERED)
    val events: Flow<BuilderEvent> = _events.receiveAsFlow()

    init {
        viewModelScope.launch {
            val loaded = catalogRepository.catalog()
            catalog = loaded
            engine = BuildEngine(loaded, checker)
            // Восстанавливаем прошлую сессию: запрос и сборку
            val request = BuildCodec.decodeRequest(settingsRepository.lastRequestJson) ?: BuildRequest()
            val build = settingsRepository.lastBuildJson
                ?.let { BuildCodec.decodeParts(it, loaded) }
                ?.takeIf { it.parts.isNotEmpty() }
            _state.update { it.copy(loading = false, request = request, partsCount = loaded.size) }
            setBuild(build)
            refreshMinimum()
        }
    }

    // ---- Бюджет и предпочтения -------------------------------------------------------------

    fun setBudget(value: Int) {
        val budget = value.coerceIn(BuildRequest.MIN_BUDGET, BuildRequest.MAX_BUDGET)
        updateRequest { it.copy(budget = budget) }
    }

    fun setCpuBrand(brand: CpuBrand) = updatePrefs { it.copy(cpuBrand = brand) }
    fun setGpuBrand(brand: GpuBrand) = updatePrefs { it.copy(gpuBrand = brand) }
    fun setPurpose(purpose: Purpose) = updatePrefs { it.copy(purpose = purpose) }
    fun setResolution(resolution: Resolution) = updatePrefs { it.copy(resolution = resolution) }
    fun setRefreshRate(rate: RefreshRate) = updatePrefs { it.copy(refreshRate = rate) }

    private fun updatePrefs(transform: (BuildPreferences) -> BuildPreferences) {
        updateRequest { it.copy(preferences = transform(it.preferences)) }
        refreshMinimum()
    }

    private fun updateRequest(transform: (BuildRequest) -> BuildRequest) {
        _state.update { it.copy(request = transform(it.request), error = null) }
        settingsRepository.lastRequestJson = BuildCodec.encodeRequest(_state.value.request)
        // FPS зависит от разрешения монитора
        _state.value.build?.let { b ->
            _state.update { it.copy(fps = fpsEstimator.estimate(b, it.request.preferences.resolution)) }
        }
    }

    private fun refreshMinimum() {
        val engine = engine ?: return
        val prefs = _state.value.request.preferences
        viewModelScope.launch {
            val minimum = withContext(Dispatchers.Default) { engine.minimumBudget(prefs) }
            _state.update { it.copy(minimumBudget = minimum) }
        }
    }

    // ---- Автосборка --------------------------------------------------------------------------

    fun assemble() {
        val engine = engine ?: return
        if (_state.value.assembling) return
        val request = _state.value.request
        _state.update { it.copy(assembling = true, error = null) }
        viewModelScope.launch {
            val result = withContext(Dispatchers.Default) { engine.assemble(request) }
            when (result) {
                is AssembleResult.Success -> {
                    _state.update { it.copy(assembling = false) }
                    setBuild(result.build)
                    _events.send(BuilderEvent.OpenBuild)
                }
                is AssembleResult.InsufficientBudget -> _state.update {
                    it.copy(assembling = false, error = AssembleError.Insufficient(result.minimumBudget))
                }
                AssembleResult.NoCompatibleParts -> _state.update {
                    it.copy(assembling = false, error = AssembleError.NoParts)
                }
            }
        }
    }

    /** «Установить минимальный бюджет и собрать» — округляем вверх до тысячи. */
    fun applyMinimumBudgetAndAssemble() {
        val minimum = (_state.value.error as? AssembleError.Insufficient)?.minimumBudget ?: return
        setBudget(((minimum + 999) / 1000) * 1000)
        assemble()
    }

    fun dismissError() = _state.update { it.copy(error = null) }

    // ---- Ручная замена ------------------------------------------------------------------------

    /** Варианты замены с пометками совместимости (быстро — можно вызывать из UI). */
    fun options(category: Category): List<PartOption> {
        val build = _state.value.build ?: return emptyList()
        return engine?.options(build, category).orEmpty()
    }

    fun replacePart(part: Part) {
        val build = _state.value.build ?: return
        lastReplaced = part.category
        setBuild(build.with(part))
    }

    /** Подогнать остальные детали под выбранную вручную. */
    fun autoFix(locked: Category? = null) {
        val build = _state.value.build ?: return
        val engine = engine ?: return
        setBuild(engine.autoFix(build, locked))
    }

    private fun setBuild(build: PcBuild?) {
        val resolution = _state.value.request.preferences.resolution
        _state.update {
            if (build == null) {
                it.copy(build = null, issues = emptyList(), fps = emptyList(), powerDraw = 0, requiredPsu = 0)
            } else {
                it.copy(
                    build = build,
                    issues = checker.check(build),
                    fps = fpsEstimator.estimate(build, resolution),
                    powerDraw = checker.estimatedPowerDraw(build),
                    requiredPsu = checker.requiredPsuWatts(build),
                )
            }
        }
        settingsRepository.lastBuildJson = build?.let { BuildCodec.encodeParts(it) }
    }

    // ---- Сохранение и загрузка ------------------------------------------------------------

    fun save(name: String) {
        val build = _state.value.build ?: return
        val request = _state.value.request
        val clean = name.trim().ifEmpty { return }
        viewModelScope.launch {
            savedBuildsRepository.save(clean, request, build)
            _events.send(BuilderEvent.Saved(clean))
        }
    }

    /** Открыть сохранённую сборку в конструкторе. */
    fun load(saved: SavedBuild) {
        _state.update { it.copy(request = saved.request, error = null) }
        settingsRepository.lastRequestJson = BuildCodec.encodeRequest(saved.request)
        setBuild(saved.build)
        refreshMinimum()
    }
}
