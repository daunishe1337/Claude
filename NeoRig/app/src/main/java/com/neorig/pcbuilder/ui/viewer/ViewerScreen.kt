package com.neorig.pcbuilder.ui.viewer

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.render.scene.RgbMode
import com.neorig.pcbuilder.ui.build.BuilderViewModel
import com.neorig.pcbuilder.ui.build.toRgbState
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.components.EmptyState
import com.neorig.pcbuilder.ui.components.GradientButton
import com.neorig.pcbuilder.ui.components.OutlineAction
import com.neorig.pcbuilder.ui.components.SegmentedSelector
import com.neorig.pcbuilder.ui.settings.SettingsViewModel
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons

/** Палитра RGB-подсветки. */
val RgbPalette = listOf(
    Color(0xFFA855F7), // фиолетовый
    Color(0xFF22D3EE), // бирюзовый
    Color(0xFFEC4899), // розовый
    Color(0xFFEF4444), // красный
    Color(0xFFF97316), // оранжевый
    Color(0xFF84CC16), // салатовый
    Color(0xFF3B82F6), // синий
    Color(0xFFF5F5F5), // белый
)

/** Вкладка «3D»: полноэкранная сцена и панель управления подсветкой. */
@Composable
fun ViewerScreen(
    builderViewModel: BuilderViewModel,
    settingsViewModel: SettingsViewModel,
    onAssemble: () -> Unit,
) {
    val state by builderViewModel.state.collectAsStateWithLifecycle()
    val settings by settingsViewModel.settings.collectAsStateWithLifecycle()
    val build = state.build
    var resetSignal by remember { mutableIntStateOf(0) }
    var panelExpanded by remember { mutableStateOf(true) }

    if (build == null) {
        EmptyState(
            icon = NeoIcons.Cube,
            title = stringResource(R.string.viewer_empty_title),
            body = stringResource(R.string.viewer_empty_body),
            modifier = Modifier.fillMaxSize(),
            action = { GradientButton(stringResource(R.string.action_assemble), onClick = onAssemble, modifier = Modifier.fillMaxWidth()) },
        )
        return
    }

    Box(Modifier.fillMaxSize()) {
        Pc3DView(
            build = build,
            rgb = settings.toRgbState(),
            interactive = true,
            antialiasing = settings.antialiasing,
            resetSignal = resetSignal,
            modifier = Modifier.fillMaxSize(),
        )

        // Плашка с итогом сверху
        Row(
            Modifier
                .align(Alignment.TopStart)
                .padding(12.dp)
                .clip(RoundedCornerShape(14.dp))
                .background(Color.Black.copy(alpha = 0.45f))
                .border(1.dp, NeoColors.Violet.copy(alpha = 0.4f), RoundedCornerShape(14.dp))
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(NeoIcons.Cube, contentDescription = null, tint = NeoColors.Teal, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text(formatRub(build.totalPrice), style = MaterialTheme.typography.titleSmall, color = Color.White)
        }
        IconButton(
            onClick = { resetSignal++ },
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(8.dp)
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.45f)),
        ) {
            Icon(Icons.Filled.Refresh, contentDescription = stringResource(R.string.viewer_reset), tint = Color.White)
        }

        // Панель подсветки снизу
        Column(
            Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .padding(12.dp)
                .clip(RoundedCornerShape(16.dp))
                .background(NeoColors.Surface.copy(alpha = 0.88f))
                .border(1.dp, NeoColors.BorderGradient, RoundedCornerShape(16.dp))
                .padding(14.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.viewer_rgb), style = MaterialTheme.typography.titleMedium, color = NeoColors.TextPrimary, modifier = Modifier.weight(1f))
                Switch(
                    checked = settings.rgbEnabled,
                    onCheckedChange = settingsViewModel::setRgbEnabled,
                    colors = SwitchDefaults.colors(checkedTrackColor = NeoColors.Violet, checkedThumbColor = NeoColors.Teal),
                )
                IconButton(onClick = { panelExpanded = !panelExpanded }) {
                    Icon(
                        if (panelExpanded) Icons.Filled.KeyboardArrowDown else Icons.Filled.KeyboardArrowUp,
                        contentDescription = null,
                        tint = NeoColors.TextMuted,
                    )
                }
            }
            AnimatedVisibility(
                visible = panelExpanded,
                enter = expandVertically() + fadeIn(),
                exit = shrinkVertically() + fadeOut(),
            ) {
                Column {
                    Spacer(Modifier.height(8.dp))
                    SegmentedSelector(
                        options = RgbMode.entries,
                        selected = settings.rgbMode,
                        label = {
                            stringResource(
                                when (it) {
                                    RgbMode.STATIC -> R.string.rgb_static
                                    RgbMode.RAINBOW -> R.string.rgb_rainbow
                                    RgbMode.BREATHING -> R.string.rgb_breathing
                                }
                            )
                        },
                        onSelect = settingsViewModel::setRgbMode,
                    )
                    Spacer(Modifier.height(12.dp))
                    ColorSwatches(
                        selected = settings.rgbColor,
                        enabled = settings.rgbEnabled && settings.rgbMode != RgbMode.RAINBOW,
                        onSelect = settingsViewModel::setRgbColor,
                    )
                    Spacer(Modifier.height(12.dp))
                    OutlineAction(
                        text = stringResource(R.string.viewer_reset),
                        onClick = { resetSignal++ },
                        icon = Icons.Filled.Refresh,
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Spacer(Modifier.height(8.dp))
                    Text(stringResource(R.string.viewer_hint), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
                }
            }
        }
    }
}

/** Ряд цветных кружков для выбора цвета подсветки. */
@Composable
fun ColorSwatches(selected: Int, enabled: Boolean, onSelect: (Int) -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier.horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        RgbPalette.forEach { color ->
            val argb = color.toArgb()
            val isSelected = argb == selected
            Box(
                Modifier
                    .size(36.dp)
                    .clip(CircleShape)
                    .background(Brush.radialGradient(listOf(color, color.copy(alpha = if (enabled) 0.7f else 0.25f))))
                    .border(if (isSelected) 3.dp else 1.dp, if (isSelected) Color.White else NeoColors.Outline, CircleShape)
                    .clickable(enabled = enabled) { onSelect(argb) },
            )
        }
    }
}
