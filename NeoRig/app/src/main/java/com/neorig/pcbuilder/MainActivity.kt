package com.neorig.pcbuilder

import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.neorig.pcbuilder.data.repo.SettingsRepository
import com.neorig.pcbuilder.ui.common.LocaleHelper
import com.neorig.pcbuilder.ui.navigation.NeoRigRoot
import com.neorig.pcbuilder.ui.theme.NeoRigTheme

class MainActivity : ComponentActivity() {

    override fun attachBaseContext(newBase: Context) {
        // Язык из настроек применяется до создания интерфейса
        super.attachBaseContext(LocaleHelper.wrap(newBase, SettingsRepository.readLanguage(newBase)))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        // Тёмные системные панели под тёмную тему
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
        )
        super.onCreate(savedInstanceState)
        val container = (application as NeoRigApplication).container
        setContent {
            NeoRigTheme {
                NeoRigRoot(container = container, onLanguageChanged = { recreate() })
            }
        }
    }
}
