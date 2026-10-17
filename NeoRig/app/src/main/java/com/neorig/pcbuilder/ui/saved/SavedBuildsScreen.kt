package com.neorig.pcbuilder.ui.saved

import android.content.Intent
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.fps.FpsEstimator
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.SavedBuild
import com.neorig.pcbuilder.ui.common.Texts
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.components.BudgetBar
import com.neorig.pcbuilder.ui.components.EmptyState
import com.neorig.pcbuilder.ui.components.GradientButton
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.OutlineAction
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons
import java.text.DateFormat
import java.util.Date

/** «Мои сборки»: список сохранённых, открыть / поделиться / удалить, выбор двух для сравнения. */
@Composable
fun SavedBuildsScreen(
    viewModel: SavedBuildsViewModel,
    onOpen: (SavedBuild) -> Unit,
    onCompare: (Long, Long) -> Unit,
) {
    val builds by viewModel.builds.collectAsStateWithLifecycle()
    val selection by viewModel.selection.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val res = context.resources
    var toDelete by remember { mutableStateOf<SavedBuild?>(null) }

    val list = builds
    when {
        list == null -> Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            CircularProgressIndicator(color = NeoColors.Teal)
        }
        list.isEmpty() -> EmptyState(
            icon = NeoIcons.Bookmark,
            title = stringResource(R.string.saved_empty_title),
            body = stringResource(R.string.saved_empty_body),
            modifier = Modifier.fillMaxSize(),
        )
        else -> LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item {
                Text(stringResource(R.string.saved_title), style = MaterialTheme.typography.headlineMedium, color = NeoColors.TextPrimary)
                Text(stringResource(R.string.saved_compare_hint), style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
                Spacer(Modifier.height(8.dp))
                GradientButton(
                    text = stringResource(R.string.action_compare, selection.size),
                    onClick = { if (selection.size == 2) onCompare(selection[0], selection[1]) },
                    enabled = selection.size == 2,
                    icon = NeoIcons.Compare,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            items(list, key = { it.id }) { saved ->
                SavedBuildCard(
                    saved = saved,
                    selected = saved.id in selection,
                    onToggle = { viewModel.toggleSelection(saved.id) },
                    onOpen = { onOpen(saved) },
                    onShare = {
                        val fps = FpsEstimator().estimate(saved.build, saved.request.preferences.resolution)
                        val text = Texts.shareText(res, saved.name, saved.request, saved.build, fps)
                        val send = Intent(Intent.ACTION_SEND).apply {
                            type = "text/plain"
                            putExtra(Intent.EXTRA_TEXT, text)
                        }
                        context.startActivity(Intent.createChooser(send, res.getString(R.string.share_chooser)))
                    },
                    onDelete = { toDelete = saved },
                )
            }
        }
    }

    toDelete?.let { saved ->
        AlertDialog(
            onDismissRequest = { toDelete = null },
            containerColor = NeoColors.SurfaceHigh,
            title = { Text(stringResource(R.string.delete_confirm_title)) },
            text = { Text(stringResource(R.string.delete_confirm_body, saved.name)) },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.delete(saved.id)
                    toDelete = null
                }) { Text(stringResource(R.string.action_delete), color = NeoColors.Error) }
            },
            dismissButton = {
                TextButton(onClick = { toDelete = null }) { Text(stringResource(R.string.action_cancel), color = NeoColors.TextMuted) }
            },
        )
    }
}

@Composable
private fun SavedBuildCard(
    saved: SavedBuild,
    selected: Boolean,
    onToggle: () -> Unit,
    onOpen: () -> Unit,
    onShare: () -> Unit,
    onDelete: () -> Unit,
) {
    val res = LocalContext.current.resources
    val date = remember(saved.createdAt) {
        DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT).format(Date(saved.createdAt))
    }
    NeonCard(Modifier.fillMaxWidth(), accent = if (selected) NeoColors.Teal else null) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(saved.name, style = MaterialTheme.typography.titleMedium, color = NeoColors.TextPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(date, style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted)
            }
            Checkbox(
                checked = selected,
                onCheckedChange = { onToggle() },
                colors = CheckboxDefaults.colors(checkedColor = NeoColors.Teal, uncheckedColor = NeoColors.Outline),
            )
        }
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = Alignment.Bottom) {
            Text(formatRub(saved.build.totalPrice), style = MaterialTheme.typography.titleLarge, color = NeoColors.Teal)
            Spacer(Modifier.width(8.dp))
            Text("/ ${formatRub(saved.request.budget)}", style = MaterialTheme.typography.bodySmall, color = NeoColors.TextMuted, modifier = Modifier.padding(bottom = 3.dp))
        }
        val cpu = saved.build[Category.CPU]?.let { Texts.partName(res, it) }
        val gpu = saved.build[Category.GPU]?.let { Texts.partName(res, it) }
        Text(
            listOfNotNull(cpu, gpu).joinToString(" · "),
            style = MaterialTheme.typography.bodySmall,
            color = NeoColors.TextPrimary,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(Modifier.height(10.dp))
        BudgetBar(saved.build.spendingByCategory(), saved.request.budget, showLegend = false)
        Spacer(Modifier.height(12.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlineAction(stringResource(R.string.action_open), onClick = onOpen, icon = NeoIcons.Tune, color = NeoColors.Teal, modifier = Modifier.weight(1f))
            Spacer(Modifier.width(8.dp))
            IconButton(onClick = onShare) {
                Icon(Icons.Filled.Share, contentDescription = stringResource(R.string.action_share), tint = NeoColors.TextPrimary)
            }
            IconButton(onClick = onDelete) {
                Icon(Icons.Filled.Delete, contentDescription = stringResource(R.string.action_delete), tint = NeoColors.Error)
            }
        }
    }
}
