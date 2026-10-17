package com.neorig.pcbuilder.data.repo

import android.content.Context
import android.content.SharedPreferences
import com.neorig.pcbuilder.render.scene.RgbMode
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/** Язык интерфейса. [tag] = null — как в системе. */
enum class AppLanguage(val tag: String?) {
    SYSTEM(null),
    RU("ru"),
    EN("en"),
}

/** Настройки приложения. */
data class AppSettings(
    val language: AppLanguage = AppLanguage.SYSTEM,
    val rgbEnabled: Boolean = true,
    val rgbColor: Int = DEFAULT_RGB_COLOR,
    val rgbMode: RgbMode = RgbMode.STATIC,
    val antialiasing: Boolean = true,
    val autoRotatePreview: Boolean = true,
) {
    companion object {
        const val DEFAULT_RGB_COLOR = 0xFFA855F7.toInt()
    }
}

/**
 * Настройки и «сессия» (последний запрос и текущая сборка) в SharedPreferences.
 * Хранилище простое и синхронное — язык нужно прочитать ещё до создания Activity.
 */
class SettingsRepository(context: Context) {

    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val state = MutableStateFlow(read())

    val settings: StateFlow<AppSettings> = state.asStateFlow()

    fun update(transform: (AppSettings) -> AppSettings) {
        state.update { old ->
            transform(old).also { write(it) }
        }
    }

    // ---- Сессия конструктора ----------------------------------------------------------------

    var lastRequestJson: String?
        get() = prefs.getString(KEY_REQUEST, null)
        set(value) = prefs.edit().putString(KEY_REQUEST, value).apply()

    var lastBuildJson: String?
        get() = prefs.getString(KEY_BUILD, null)
        set(value) = prefs.edit().putString(KEY_BUILD, value).apply()

    private fun read() = AppSettings(
        language = enumOrDefault(prefs.getString(KEY_LANGUAGE, null), AppLanguage.SYSTEM),
        rgbEnabled = prefs.getBoolean(KEY_RGB_ENABLED, true),
        rgbColor = prefs.getInt(KEY_RGB_COLOR, AppSettings.DEFAULT_RGB_COLOR),
        rgbMode = enumOrDefault(prefs.getString(KEY_RGB_MODE, null), RgbMode.STATIC),
        antialiasing = prefs.getBoolean(KEY_AA, true),
        autoRotatePreview = prefs.getBoolean(KEY_AUTO_ROTATE, true),
    )

    private fun write(s: AppSettings) {
        prefs.edit()
            .putString(KEY_LANGUAGE, s.language.name)
            .putBoolean(KEY_RGB_ENABLED, s.rgbEnabled)
            .putInt(KEY_RGB_COLOR, s.rgbColor)
            .putString(KEY_RGB_MODE, s.rgbMode.name)
            .putBoolean(KEY_AA, s.antialiasing)
            .putBoolean(KEY_AUTO_ROTATE, s.autoRotatePreview)
            .apply()
    }

    companion object {
        private const val PREFS = "neorig_settings"
        private const val KEY_LANGUAGE = "language"
        private const val KEY_RGB_ENABLED = "rgb_enabled"
        private const val KEY_RGB_COLOR = "rgb_color"
        private const val KEY_RGB_MODE = "rgb_mode"
        private const val KEY_AA = "antialiasing"
        private const val KEY_AUTO_ROTATE = "auto_rotate"
        private const val KEY_REQUEST = "last_request"
        private const val KEY_BUILD = "last_build"

        /** Язык читается в attachBaseContext, когда контейнер зависимостей ещё не создан. */
        fun readLanguage(context: Context): AppLanguage = enumOrDefault(
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY_LANGUAGE, null),
            AppLanguage.SYSTEM,
        )

        private inline fun <reified T : Enum<T>> enumOrDefault(name: String?, default: T): T =
            enumValues<T>().firstOrNull { it.name == name } ?: default
    }
}
