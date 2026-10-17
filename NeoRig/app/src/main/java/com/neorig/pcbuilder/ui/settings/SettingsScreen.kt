package com.neorig.pcbuilder.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.data.repo.AppLanguage
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.OutlineAction
import com.neorig.pcbuilder.ui.components.SectionHeader
import com.neorig.pcbuilder.ui.components.SegmentedSelector
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.viewer.ColorSwatches

/** Настройки: язык, 3D, подсветка по умолчанию, данные, о приложении. */
@Composable
fun SettingsScreen(viewModel: SettingsViewModel, onLanguageChanged: () -> Unit) {
    val settings by viewModel.settings.collectAsStateWithLifecycle()
    val partsCount by viewModel.partsCount.collectAsStateWithLifecycle()
    var confirmClear by remember { mutableStateOf(false) }
    val context = LocalContext.current
    val version = remember {
        runCatching { context.packageManager.getPackageInfo(context.packageName, 0).versionName }.getOrNull() ?: "1.0"
    }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Text(stringResource(R.string.settings_title), style = MaterialTheme.typography.headlineMedium, color = NeoColors.TextPrimary)

        NeonCard(Modifier.fillMaxWidth()) {
            SectionHeader(stringResource(R.string.settings_language))
            Spacer(Modifier.height(10.dp))
            SegmentedSelector(
                options = AppLanguage.entries,
                selected = settings.language,
                label = {
                    stringResource(
                        when (it) {
                            AppLanguage.SYSTEM -> R.string.lang_system
                            AppLanguage.RU -> R.string.lang_ru
                            AppLanguage.EN -> R.string.lang_en
                        }
                    )
                },
                onSelect = { if (viewModel.setLanguage(it)) onLanguageChanged() },
            )
        }

        NeonCard(Modifier.fillMaxWidth()) {
            SectionHeader(stringResource(R.string.settings_3d))
            Spacer(Modifier.height(6.dp))
            SwitchRow(
                title = stringResource(R.string.settings_antialiasing),
                subtitle = stringResource(R.string.settings_antialiasing_desc),
                checked = settings.antialiasing,
                onChange = viewModel::setAntialiasing,
            )
            SwitchRow(
                title = stringResource(R.string.settings_autorotate),
                subtitle = stringResource(R.string.settings_autorotate_desc),
                checked = settings.autoRotatePreview,
                onChange = viewModel::setAutoRotate,
            )
            SwitchRow(
                title = stringResource(R.string.viewer_rgb),
                subtitle = null,
                checked = settings.rgbEnabled,
                onChange = viewModel::setRgbEnabled,
            )
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.settings_rgb_default), style = MaterialTheme.typography.bodyMedium, color = NeoColors.TextPrimary)
            Spacer(Modifier.height(8.dp))
            ColorSwatches(selected = settings.rgbColor, enabled = true, onSelect = viewModel::setRgbColor)
        }

        NeonCard(Modifier.fillMaxWidth()) {
            SectionHeader(stringResource(R.string.settings_data), subtitle = stringResource(R.string.settings_parts_count, partsCount))
            Spacer(Modifier.height(10.dp))
            OutlineAction(
                text = stringResource(R.string.settings_clear),
                onClick = { confirmClear = true },
                icon = Icons.Filled.Delete,
                color = NeoColors.Error,
                modifier = Modifier.fillMaxWidth(),
            )
        }

        NeonCard(Modifier.fillMaxWidth()) {
            SectionHeader(stringResource(R.string.settings_about))
            Spacer(Modifier.height(6.dp))
            Text(stringResource(R.string.settings_about_body, version), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
        }
        Spacer(Modifier.height(8.dp))
    }

    if (confirmClear) {
        AlertDialog(
            onDismissRequest = { confirmClear = false },
            containerColor = NeoColors.SurfaceHigh,
            title = { Text(stringResource(R.string.settings_clear_confirm)) },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.clearSavedBuilds()
                    confirmClear = false
                }) { Text(stringResource(R.string.action_delete), color = NeoColors.Error) }
            },
            dismissButton = {
                TextButton(onClick = { confirmClear = false }) { Text(stringResource(R.string.action_cancel), color = NeoColors.TextMuted) }
            },
        )
    }
}

@Composable
private fun SwitchRow(title: String, subtitle: String?, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge, color = NeoColors.TextPrimary)
            if (subtitle != null) Text(subtitle, style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
        }
        Spacer(Modifier.width(12.dp))
        Switch(
            checked = checked,
            onCheckedChange = onChange,
            colors = SwitchDefaults.colors(checkedTrackColor = NeoColors.Violet, checkedThumbColor = NeoColors.Teal),
        )
    }
}
