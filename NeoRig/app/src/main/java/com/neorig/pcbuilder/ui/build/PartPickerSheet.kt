package com.neorig.pcbuilder.ui.build

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.compat.Severity
import com.neorig.pcbuilder.domain.engine.PartOption
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.ui.common.Texts
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.common.formatRubDelta
import com.neorig.pcbuilder.ui.components.CategoryBadge
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.ScoreBar
import com.neorig.pcbuilder.ui.components.SegmentedSelector
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.categoryColor

private enum class SortMode { PRICE, PERFORMANCE }

/**
 * Нижняя шторка ручной замены детали.
 * Несовместимые варианты помечены красным с причиной; выбрать их всё равно можно —
 * тогда на экране сборки появится предупреждение и кнопка автоисправления.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PartPickerSheet(
    category: Category,
    options: List<PartOption>,
    onSelect: (Part) -> Unit,
    onDismiss: () -> Unit,
) {
    val res = LocalContext.current.resources
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    var onlyCompatible by remember { mutableStateOf(false) }
    var sort by remember { mutableStateOf(SortMode.PRICE) }

    val visible = options
        .filter { !onlyCompatible || !it.hasErrors || it.isCurrent }
        .let { list ->
            when (sort) {
                SortMode.PRICE -> list.sortedBy { it.part.price }
                SortMode.PERFORMANCE -> list.sortedByDescending { it.part.score }
            }
        }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = NeoColors.Surface,
    ) {
        Column(Modifier.padding(horizontal = 16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                CategoryBadge(category, size = 36.dp)
                Spacer(Modifier.width(12.dp))
                Text(
                    stringResource(R.string.picker_title, Texts.categoryName(res, category)),
                    style = MaterialTheme.typography.titleLarge,
                    color = NeoColors.TextPrimary,
                    modifier = Modifier.weight(1f),
                )
            }
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.picker_only_compatible), style = MaterialTheme.typography.bodyMedium, color = NeoColors.TextPrimary, modifier = Modifier.weight(1f))
                Switch(
                    checked = onlyCompatible,
                    onCheckedChange = { onlyCompatible = it },
                    colors = SwitchDefaults.colors(checkedTrackColor = NeoColors.Violet, checkedThumbColor = NeoColors.Teal),
                )
            }
            Spacer(Modifier.height(8.dp))
            SegmentedSelector(
                options = SortMode.entries,
                selected = sort,
                label = { stringResource(if (it == SortMode.PRICE) R.string.sort_price else R.string.sort_performance) },
                onSelect = { sort = it },
            )
            Spacer(Modifier.height(12.dp))
        }
        LazyColumn(
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 32.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            items(visible, key = { it.part.id }) { option ->
                OptionRow(option, category, onClick = { onSelect(option.part) })
            }
        }
    }
}

@Composable
private fun OptionRow(option: PartOption, category: Category, onClick: () -> Unit) {
    val res = LocalContext.current.resources
    val part = option.part
    val accent = when {
        option.isCurrent -> NeoColors.Teal
        option.hasErrors -> NeoColors.Error
        option.hasWarnings -> NeoColors.Warning
        else -> null
    }
    NeonCard(Modifier.fillMaxWidth(), accent = accent, onClick = onClick, contentPadding = 12.dp) {
        Row(verticalAlignment = Alignment.Top) {
            Column(Modifier.weight(1f)) {
                Text(Texts.partName(res, part), style = MaterialTheme.typography.titleSmall, color = NeoColors.TextPrimary, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Text(Texts.specLine(res, part), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Spacer(Modifier.height(6.dp))
                ScoreBar(part.score, Modifier.fillMaxWidth(0.7f), color = categoryColor(category))
            }
            Spacer(Modifier.width(10.dp))
            Column(horizontalAlignment = Alignment.End) {
                Text(formatRub(part.price), style = MaterialTheme.typography.titleSmall, color = NeoColors.TextPrimary)
                if (!option.isCurrent) {
                    Text(
                        formatRubDelta(option.priceDelta),
                        style = MaterialTheme.typography.labelSmall,
                        color = if (option.priceDelta > 0) NeoColors.Warning else NeoColors.Success,
                    )
                }
            }
        }
        Spacer(Modifier.height(8.dp))
        when {
            option.isCurrent -> StatusLine(Icons.Filled.CheckCircle, stringResource(R.string.picker_current), NeoColors.Teal)
            option.issues.isEmpty() -> StatusLine(Icons.Filled.CheckCircle, stringResource(R.string.picker_compatible), NeoColors.Success)
            else -> option.issues.forEach { issue ->
                val error = issue.severity == Severity.ERROR
                StatusLine(
                    if (error) Icons.Filled.Close else Icons.Filled.Warning,
                    Texts.issue(res, issue),
                    if (error) NeoColors.Error else NeoColors.Warning,
                )
            }
        }
    }
}

@Composable
private fun StatusLine(icon: androidx.compose.ui.graphics.vector.ImageVector, text: String, color: androidx.compose.ui.graphics.Color) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(vertical = 1.dp)) {
        Icon(icon, contentDescription = null, tint = color, modifier = Modifier.size(14.dp))
        Spacer(Modifier.width(6.dp))
        Text(text, style = MaterialTheme.typography.bodySmall, color = color)
    }
}
