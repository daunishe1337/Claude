package com.neorig.pcbuilder.ui.common

import android.content.Context
import android.content.res.Configuration
import android.content.res.Resources
import android.os.LocaleList
import com.neorig.pcbuilder.data.repo.AppLanguage
import java.util.Locale

/**
 * Смена языка внутри приложения без AppCompat:
 * Activity оборачивает базовый контекст конфигурацией с нужной локалью.
 */
object LocaleHelper {

    fun wrap(base: Context, language: AppLanguage): Context {
        val tag = language.tag
        if (tag == null) {
            // «Системный»: возвращаем локаль системы (могла остаться от ручного выбора)
            Locale.setDefault(Resources.getSystem().configuration.locales.get(0))
            return base
        }
        val locale = Locale.forLanguageTag(tag)
        Locale.setDefault(locale)
        val config = Configuration(base.resources.configuration)
        config.setLocale(locale)
        config.setLocales(LocaleList(locale))
        return base.createConfigurationContext(config)
    }
}
