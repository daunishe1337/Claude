package com.neorig.pcbuilder.ui.saved

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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
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
import com.neorig.pcbuilder.domain.fps.FpsEstimator
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.SavedBuild
import com.neorig.pcbuilder.ui.common.Texts
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.components.CategoryBadge
import com.neorig.pcbuilder.ui.components.EmptyState
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.SectionHeader
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons

/** Сравнение двух сохранённых сборок: детали, цены, итог и FPS. */
@Composable
fun CompareScreen(viewModel: SavedBuildsViewModel, firstId: Long, secondId: Long, onBack: () -> Unit) {
    val builds by viewModel.builds.collectAsStateWithLifecycle()
    val a = builds?.firstOrNull { it.id == firstId }
    val b = builds?.firstOrNull { it.id == secondId }

    Column(Modifier.fillMaxSize()) {
        Row(Modifier.padding(horizontal = 4.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.action_back), tint = NeoColors.TextPrimary)
            }
            Text(stringResource(R.string.compare_title), style = MaterialTheme.typography.headlineSmall, color = NeoColors.TextPrimary)
        }
        if (a == null || b == null) {
            EmptyState(
                icon = NeoIcons.Compare,
                title = stringResource(R.string.compare_missing),
                body = "",
                modifier = Modifier.fillMaxSize(),
            )
            return@Column
        }
        CompareContent(a, b)
    }
}

@Composable
private fun CompareContent(a: SavedBuild, b: SavedBuild) {
    val res = LocalContext.current.resources
    val fpsA = remember(a) { FpsEstimator().estimate(a.build, a.request.preferences.resolution) }
    val fpsB = remember(b) { FpsEstimator().estimate(b.build, a.request.preferences.resolution) }

    LazyColumn(
        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        // Названия и итог
        item {
            NeonCard(Modifier.fillMaxWidth()) {
                Row {
                    BuildHeader(a, NeoColors.Violet, Modifier.weight(1f))
                    Spacer(Modifier.width(12.dp))
                    BuildHeader(b, NeoColors.Teal, Modifier.weight(1f))
                }
            }
        }
        // Детали по категориям
        items(Category.entries, key = { it.name }) { category ->
            val pa = a.build[category]
            val pb = b.build[category]
            NeonCard(Modifier.fillMaxWidth(), contentPadding = 12.dp) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CategoryBadge(category, size = 28.dp)
                    Spacer(Modifier.width(8.dp))
                    Text(Texts.categoryName(res, category), style = MaterialTheme.typography.labelLarge, color = NeoColors.TextMuted)
                }
                Spacer(Modifier.height(8.dp))
                Row {
                    PartCell(pa, better = isBetter(pa, pb), modifier = Modifier.weight(1f))
                    Spacer(Modifier.width(12.dp))
                    PartCell(pb, better = isBetter(pb, pa), modifier = Modifier.weight(1f))
                }
            }
        }
        // FPS
        item {
            NeonCard(Modifier.fillMaxWidth()) {
                SectionHeader(stringResource(R.string.compare_fps, a.request.preferences.resolution.label))
                Spacer(Modifier.height(10.dp))
                val max = maxOf((fpsA + fpsB).maxOfOrNull { it.fps } ?: 1, 60).toFloat()
                fpsA.zip(fpsB).forEach { (x, y) ->
                    Text(x.game, style = MaterialTheme.typography.bodyMedium, color = NeoColors.TextPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    FpsBar(x.fps, max, NeoColors.Violet)
                    FpsBar(y.fps, max, NeoColors.Teal)
                    Spacer(Modifier.height(8.dp))
                }
            }
        }
    }
}

@Composable
private fun BuildHeader(saved: SavedBuild, color: Color, modifier: Modifier) {
    Column(modifier) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.width(10.dp).height(10.dp).clip(CircleShape).background(color))
            Spacer(Modifier.width(6.dp))
            Text(saved.name, style = MaterialTheme.typography.titleSmall, color = NeoColors.TextPrimary, maxLines = 2, overflow = TextOverflow.Ellipsis)
        }
        Spacer(Modifier.height(4.dp))
        Text(stringResource(R.string.compare_total), style = MaterialTheme.typography.labelSmall, color = NeoColors.TextMuted)
        Text(formatRub(saved.build.totalPrice), style = MaterialTheme.typography.titleLarge, color = color)
    }
}

@Composable
private fun PartCell(part: Part?, better: Boolean, modifier: Modifier) {
    val res = LocalContext.current.resources
    Column(modifier) {
        if (part == null) {
            Text("—", color = NeoColors.TextMuted)
            return@Column
        }
        Text(
            Texts.partName(res, part),
            style = MaterialTheme.typography.bodyMedium,
            color = if (better) NeoColors.Teal else NeoColors.TextPrimary,
            maxLines = 3,
            overflow = TextOverflow.Ellipsis,
        )
        Text(formatRub(part.price), style = MaterialTheme.typography.labelLarge, color = NeoColors.TextMuted)
        if (better) Text(stringResource(R.string.compare_better), style = MaterialTheme.typography.labelSmall, color = NeoColors.Teal)
    }
}

@Composable
private fun FpsBar(fps: Int, max: Float, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(vertical = 2.dp)) {
        Box(
            Modifier
                .weight(1f)
                .height(6.dp)
                .clip(CircleShape)
                .background(NeoColors.Outline)
        ) {
            Box(
                Modifier
                    .fillMaxWidth((fps / max).coerceIn(0.02f, 1f))
                    .height(6.dp)
                    .clip(CircleShape)
                    .background(color)
            )
        }
        Spacer(Modifier.width(8.dp))
        Text("~$fps", style = MaterialTheme.typography.labelMedium, color = color, modifier = Modifier.width(44.dp))
    }
}

/** Деталь «лучше», если её индекс производительности выше. */
private fun isBetter(part: Part?, other: Part?): Boolean =
    part != null && other != null && part.score > other.score
