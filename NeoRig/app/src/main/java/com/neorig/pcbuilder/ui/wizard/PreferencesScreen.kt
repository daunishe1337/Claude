package com.neorig.pcbuilder.ui.wizard

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.layout.Arrangement
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
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.GpuBrand
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.domain.model.RefreshRate
import com.neorig.pcbuilder.domain.model.Resolution
import com.neorig.pcbuilder.ui.build.AssembleError
import com.neorig.pcbuilder.ui.build.BuilderViewModel
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.components.GradientButton
import com.neorig.pcbuilder.ui.components.KeyValueRow
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.OutlineAction
import com.neorig.pcbuilder.ui.components.SectionHeader
import com.neorig.pcbuilder.ui.components.SegmentedSelector
import com.neorig.pcbuilder.ui.components.StepIndicator
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons

/** Шаг 2: бренды, назначение и монитор; кнопка «Собрать». */
@Composable
fun PreferencesScreen(viewModel: BuilderViewModel, onBack: () -> Unit) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val prefs = state.request.preferences

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 20.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(18.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.action_back), tint = NeoColors.TextPrimary)
            }
            Spacer(Modifier.width(4.dp))
            StepIndicator(step = 2, total = 2, label = stringResource(R.string.step_of, 2, 2))
        }
        Text(stringResource(R.string.prefs_title), style = MaterialTheme.typography.headlineMedium, color = NeoColors.TextPrimary)

        // Процессор
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            SectionHeader(stringResource(R.string.prefs_cpu))
            SegmentedSelector(
                options = listOf(CpuBrand.INTEL, CpuBrand.AMD, CpuBrand.ANY),
                selected = prefs.cpuBrand,
                label = { it.vendor ?: stringResource(R.string.any) },
                onSelect = viewModel::setCpuBrand,
            )
        }

        // Видеокарта
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            SectionHeader(stringResource(R.string.prefs_gpu))
            SegmentedSelector(
                options = listOf(GpuBrand.NVIDIA, GpuBrand.AMD, GpuBrand.INTEL, GpuBrand.ANY),
                selected = prefs.gpuBrand,
                label = { it.vendor ?: stringResource(R.string.any) },
                onSelect = viewModel::setGpuBrand,
            )
        }

        // Назначение
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            SectionHeader(stringResource(R.string.prefs_purpose))
            PurposeCard(Purpose.GAMING, prefs.purpose, NeoIcons.Gpu, R.string.purpose_gaming, R.string.purpose_gaming_desc, viewModel::setPurpose)
            PurposeCard(Purpose.WORK, prefs.purpose, NeoIcons.Cpu, R.string.purpose_work, R.string.purpose_work_desc, viewModel::setPurpose)
            PurposeCard(Purpose.STUDY, prefs.purpose, NeoIcons.Monitor, R.string.purpose_study, R.string.purpose_study_desc, viewModel::setPurpose)
        }

        // Монитор
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            SectionHeader(stringResource(R.string.prefs_monitor))
            Text(stringResource(R.string.prefs_resolution), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
            SegmentedSelector(
                options = Resolution.entries,
                selected = prefs.resolution,
                label = { it.label },
                onSelect = viewModel::setResolution,
            )
            Text(stringResource(R.string.prefs_refresh), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
            SegmentedSelector(
                options = RefreshRate.entries,
                selected = prefs.refreshRate,
                label = { stringResource(R.string.hz, it.hz) },
                onSelect = viewModel::setRefreshRate,
            )
        }

        // Итог по бюджету
        NeonCard(Modifier.fillMaxWidth()) {
            KeyValueRow(stringResource(R.string.summary_budget), formatRub(state.request.budget))
            state.minimumBudget?.let {
                Spacer(Modifier.height(6.dp))
                KeyValueRow(
                    stringResource(R.string.summary_minimum),
                    formatRub(it),
                    valueColor = if (state.request.budget < it) NeoColors.Warning else NeoColors.TextMuted,
                )
            }
        }

        // Ошибка: не хватает бюджета / нет деталей
        AnimatedVisibility(visible = state.error != null) {
            when (val error = state.error) {
                is AssembleError.Insufficient -> InsufficientBudgetCard(
                    budget = state.request.budget,
                    minimum = error.minimumBudget,
                    onApply = viewModel::applyMinimumBudgetAndAssemble,
                )
                AssembleError.NoParts -> NeonCard(Modifier.fillMaxWidth(), accent = NeoColors.Error) {
                    Text(stringResource(R.string.error_no_parts), color = NeoColors.Error, style = MaterialTheme.typography.bodyMedium)
                }
                null -> Unit
            }
        }

        GradientButton(
            text = stringResource(if (state.assembling) R.string.assembling else R.string.action_assemble),
            onClick = viewModel::assemble,
            loading = state.assembling,
            enabled = !state.loading,
            icon = NeoIcons.Cube,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun PurposeCard(
    purpose: Purpose,
    selected: Purpose,
    icon: ImageVector,
    titleRes: Int,
    descRes: Int,
    onSelect: (Purpose) -> Unit,
) {
    val isSelected = purpose == selected
    NeonCard(
        modifier = Modifier.fillMaxWidth(),
        accent = if (isSelected) NeoColors.Teal else null,
        onClick = { onSelect(purpose) },
        contentPadding = 14.dp,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(icon, contentDescription = null, tint = if (isSelected) NeoColors.Teal else NeoColors.TextMuted, modifier = Modifier.size(28.dp))
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text(stringResource(titleRes), style = MaterialTheme.typography.titleMedium, color = NeoColors.TextPrimary)
                Text(stringResource(descRes), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
            }
        }
    }
}

/** Понятное сообщение о нехватке бюджета с минимальной суммой. */
@Composable
private fun InsufficientBudgetCard(budget: Int, minimum: Int, onApply: () -> Unit) {
    val rounded = ((minimum + 999) / 1000) * 1000
    NeonCard(Modifier.fillMaxWidth(), accent = NeoColors.Warning) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.Warning, contentDescription = null, tint = NeoColors.Warning)
            Spacer(Modifier.width(10.dp))
            Text(stringResource(R.string.error_insufficient_title), style = MaterialTheme.typography.titleMedium, color = NeoColors.Warning)
        }
        Spacer(Modifier.height(8.dp))
        Text(
            stringResource(R.string.error_insufficient_body, formatRub(minimum), formatRub(minimum - budget)),
            style = MaterialTheme.typography.bodyMedium,
            color = NeoColors.TextPrimary,
        )
        Spacer(Modifier.height(12.dp))
        OutlineAction(
            text = stringResource(R.string.action_set_budget, formatRub(rounded)),
            onClick = onApply,
            color = NeoColors.Teal,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}
