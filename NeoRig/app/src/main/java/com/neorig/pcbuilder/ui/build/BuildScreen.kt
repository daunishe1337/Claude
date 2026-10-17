package com.neorig.pcbuilder.ui.build

import android.content.Intent
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateIntAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.data.repo.AppSettings
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.compat.Severity
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.render.scene.Rgba
import com.neorig.pcbuilder.render.scene.RgbState
import com.neorig.pcbuilder.ui.common.Texts
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.components.BudgetBar
import com.neorig.pcbuilder.ui.components.CategoryBadge
import com.neorig.pcbuilder.ui.components.EmptyState
import com.neorig.pcbuilder.ui.components.GradientButton
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.OutlineAction
import com.neorig.pcbuilder.ui.components.SectionHeader
import com.neorig.pcbuilder.ui.components.neonText
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons
import com.neorig.pcbuilder.ui.viewer.Pc3DView

/** Экран «Сборка»: итог, полоса бюджета, совместимость, 3D-превью, детали, FPS. */
@Composable
fun BuildScreen(
    viewModel: BuilderViewModel,
    settings: AppSettings,
    onOpen3d: () -> Unit,
    onChangePreferences: () -> Unit,
    onStart: () -> Unit,
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val build = state.build
    val context = LocalContext.current
    val res = context.resources
    var pickerCategory by remember { mutableStateOf<Category?>(null) }
    var showSaveDialog by remember { mutableStateOf(false) }

    if (build == null) {
        EmptyState(
            icon = NeoIcons.Cube,
            title = stringResource(R.string.build_empty_title),
            body = stringResource(R.string.build_empty_body),
            modifier = Modifier.fillMaxSize(),
            action = { GradientButton(stringResource(R.string.action_start), onClick = onStart, modifier = Modifier.fillMaxWidth()) },
        )
        return
    }

    val prefs = state.request.preferences
    val share = {
        val text = Texts.shareText(res, null, state.request, build, state.fps)
        val send = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_TEXT, text)
        }
        context.startActivity(Intent.createChooser(send, res.getString(R.string.share_chooser)))
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        // Заголовок
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(stringResource(R.string.build_title), style = MaterialTheme.typography.headlineMedium, color = NeoColors.TextPrimary)
                    Text(
                        "${Texts.purposeName(res, prefs.purpose)} · ${prefs.resolution.label} · ${stringResource(R.string.hz, prefs.refreshRate.hz)}",
                        style = MaterialTheme.typography.bodySmall,
                        color = NeoColors.TextMuted,
                    )
                }
                IconButton(onClick = { showSaveDialog = true }) {
                    Icon(NeoIcons.Bookmark, contentDescription = stringResource(R.string.action_save), tint = NeoColors.TextPrimary)
                }
                IconButton(onClick = share) {
                    Icon(Icons.Filled.Share, contentDescription = stringResource(R.string.action_share), tint = NeoColors.TextPrimary)
                }
            }
        }

        // Итог и полоса бюджета
        item {
            NeonCard(Modifier.fillMaxWidth()) {
                val total by animateIntAsState(state.total, tween(500), label = "total")
                Row(verticalAlignment = Alignment.Bottom) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.total), style = MaterialTheme.typography.labelMedium, color = NeoColors.TextMuted)
                        Text(formatRub(total), style = MaterialTheme.typography.headlineMedium, modifier = Modifier.neonText())
                    }
                    Column(horizontalAlignment = Alignment.End) {
                        val over = state.remaining < 0
                        Text(
                            stringResource(if (over) R.string.over_budget else R.string.remaining),
                            style = MaterialTheme.typography.labelMedium,
                            color = NeoColors.TextMuted,
                        )
                        Text(
                            formatRub(if (over) -state.remaining else state.remaining),
                            style = MaterialTheme.typography.titleLarge,
                            color = if (over) NeoColors.Error else NeoColors.Teal,
                        )
                    }
                }
                Text(
                    "${stringResource(R.string.summary_budget)}: ${formatRub(state.request.budget)}",
                    style = MaterialTheme.typography.bodySmall,
                    color = NeoColors.TextMuted,
                )
                Spacer(Modifier.height(14.dp))
                SectionHeader(stringResource(R.string.spending_title))
                Spacer(Modifier.height(10.dp))
                BudgetBar(build.spendingByCategory(), state.request.budget)
            }
        }

        // Совместимость
        item { CompatibilityCard(state.issues, onAutoFix = { viewModel.autoFix(viewModel.lastReplaced) }) }

        // 3D-превью корпуса
        item {
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(240.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(NeoColors.Surface)
            ) {
                Pc3DView(
                    build = build,
                    rgb = settings.toRgbState(),
                    interactive = false,
                    autoRotate = settings.autoRotatePreview,
                    antialiasing = settings.antialiasing,
                    onTap = onOpen3d,
                    modifier = Modifier.fillMaxSize(),
                )
                Text(
                    stringResource(R.string.preview_hint),
                    style = MaterialTheme.typography.labelMedium,
                    color = Color.White,
                    modifier = Modifier
                        .align(Alignment.BottomStart)
                        .padding(10.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color.Black.copy(alpha = 0.45f))
                        .padding(horizontal = 10.dp, vertical = 6.dp),
                )
            }
        }

        // Энергопотребление
        item {
            NeonCard(Modifier.fillMaxWidth(), contentPadding = 14.dp) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(NeoIcons.Bolt, contentDescription = null, tint = NeoColors.Warning, modifier = Modifier.size(22.dp))
                    Spacer(Modifier.width(10.dp))
                    Text(
                        stringResource(R.string.power_line, state.powerDraw, state.requiredPsu),
                        style = MaterialTheme.typography.bodyMedium,
                        color = NeoColors.TextPrimary,
                    )
                }
            }
        }

        // Комплектующие
        item { SectionHeader(stringResource(R.string.parts_title), subtitle = stringResource(R.string.parts_hint)) }
        items(Category.entries, key = { it.name }) { category ->
            val part = build[category]
            if (part != null) {
                PartRow(
                    part = part,
                    issues = state.issues.filter { category in it.categories },
                    onClick = { pickerCategory = category },
                )
            }
        }

        // FPS
        item { FpsCard(state.fps, prefs.resolution.label, prefs.refreshRate.hz) }

        // Действия
        item {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                GradientButton(
                    text = stringResource(R.string.action_save),
                    onClick = { showSaveDialog = true },
                    icon = NeoIcons.Bookmark,
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    OutlineAction(stringResource(R.string.action_share), onClick = share, icon = Icons.Filled.Share, modifier = Modifier.weight(1f))
                    OutlineAction(stringResource(R.string.action_rebuild), onClick = onChangePreferences, icon = NeoIcons.Tune, modifier = Modifier.weight(1f))
                }
                Spacer(Modifier.height(8.dp))
            }
        }
    }

    pickerCategory?.let { category ->
        PartPickerSheet(
            category = category,
            options = remember(category, build) { viewModel.options(category) },
            onSelect = { part ->
                viewModel.replacePart(part)
                pickerCategory = null
            },
            onDismiss = { pickerCategory = null },
        )
    }

    if (showSaveDialog) {
        SaveBuildDialog(
            defaultName = stringResource(R.string.default_build_name, formatRub(state.request.budget)),
            onSave = { name ->
                viewModel.save(name)
                showSaveDialog = false
            },
            onDismiss = { showSaveDialog = false },
        )
    }
}

/** Настройки подсветки → состояние для рендерера. */
fun AppSettings.toRgbState() = RgbState(enabled = rgbEnabled, color = Rgba.fromArgb(rgbColor), mode = rgbMode)

@Composable
private fun CompatibilityCard(issues: List<Incompatibility>, onAutoFix: () -> Unit) {
    val res = LocalContext.current.resources
    val hasErrors = issues.any { it.severity == Severity.ERROR }
    val accent = when {
        issues.isEmpty() -> NeoColors.Success
        hasErrors -> NeoColors.Error
        else -> NeoColors.Warning
    }
    NeonCard(Modifier.fillMaxWidth().animateContentSize(), accent = accent.copy(alpha = 0.8f), contentPadding = 14.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                if (issues.isEmpty()) Icons.Filled.CheckCircle else Icons.Filled.Warning,
                contentDescription = null,
                tint = accent,
            )
            Spacer(Modifier.width(10.dp))
            Text(
                if (issues.isEmpty()) stringResource(R.string.compat_ok) else stringResource(R.string.compat_issues, issues.size),
                style = MaterialTheme.typography.titleSmall,
                color = accent,
            )
        }
        if (issues.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            issues.forEach { issue ->
                Text(
                    "• " + Texts.issue(res, issue),
                    style = MaterialTheme.typography.bodySmall,
                    color = if (issue.severity == Severity.ERROR) NeoColors.Error else NeoColors.Warning,
                    modifier = Modifier.padding(vertical = 2.dp),
                )
            }
            Spacer(Modifier.height(10.dp))
            OutlineAction(stringResource(R.string.action_autofix), onClick = onAutoFix, icon = NeoIcons.Wand, color = NeoColors.Teal, modifier = Modifier.fillMaxWidth())
        }
    }
}

/** Строка детали в списке: иконка, название, характеристики, цена; красная рамка при проблемах. */
@Composable
private fun PartRow(part: Part, issues: List<Incompatibility>, onClick: () -> Unit) {
    val res = LocalContext.current.resources
    val accent = when {
        issues.any { it.severity == Severity.ERROR } -> NeoColors.Error
        issues.isNotEmpty() -> NeoColors.Warning
        else -> null
    }
    NeonCard(Modifier.fillMaxWidth(), accent = accent, onClick = onClick, contentPadding = 12.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            CategoryBadge(part.category)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(Texts.categoryName(res, part.category), style = MaterialTheme.typography.labelSmall, color = NeoColors.TextMuted)
                Text(
                    Texts.partName(res, part),
                    style = MaterialTheme.typography.titleSmall,
                    color = NeoColors.TextPrimary,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    Texts.specLine(res, part),
                    style = MaterialTheme.typography.bodySmall,
                    color = NeoColors.TextMuted,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                if (accent != null) {
                    Text(
                        Texts.issue(res, issues.first()),
                        style = MaterialTheme.typography.bodySmall,
                        color = accent,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
            Spacer(Modifier.width(8.dp))
            Text(formatRub(part.price), style = MaterialTheme.typography.titleSmall, color = NeoColors.TextPrimary)
        }
    }
}

@Composable
private fun SaveBuildDialog(defaultName: String, onSave: (String) -> Unit, onDismiss: () -> Unit) {
    var name by remember { mutableStateOf(defaultName) }
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = NeoColors.SurfaceHigh,
        title = { Text(stringResource(R.string.save_dialog_title)) },
        text = {
            OutlinedTextField(
                value = name,
                onValueChange = { name = it.take(60) },
                label = { Text(stringResource(R.string.save_dialog_hint)) },
                singleLine = true,
            )
        },
        confirmButton = {
            TextButton(onClick = { onSave(name) }, enabled = name.isNotBlank()) {
                Text(stringResource(R.string.action_save), color = NeoColors.Teal)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text(stringResource(R.string.action_cancel), color = NeoColors.TextMuted) }
        },
    )
}
