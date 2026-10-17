package com.neorig.pcbuilder.ui.build

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.fps.Bottleneck
import com.neorig.pcbuilder.domain.fps.FpsEstimate
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.SectionHeader
import com.neorig.pcbuilder.ui.theme.NeoColors

/** Цвет оценки FPS: красный < 30, жёлтый < 60, зелёный, бирюзовый — упор в частоту монитора. */
fun fpsColor(fps: Int, refreshHz: Int): Color = when {
    fps < 30 -> NeoColors.Error
    fps < 60 -> NeoColors.Warning
    fps >= refreshHz -> NeoColors.Teal
    else -> NeoColors.Success
}

/** Карточка «Оценка FPS» в популярных играх (условно, по тиру видеокарты). */
@Composable
fun FpsCard(estimates: List<FpsEstimate>, resolutionLabel: String, refreshHz: Int, modifier: Modifier = Modifier) {
    if (estimates.isEmpty()) return
    val maxFps = maxOf(estimates.maxOf { it.fps }, refreshHz, 120).toFloat()
    NeonCard(modifier.fillMaxWidth()) {
        SectionHeader(
            stringResource(R.string.fps_title),
            subtitle = stringResource(R.string.fps_subtitle, resolutionLabel),
        )
        Spacer(Modifier.height(12.dp))
        estimates.forEach { estimate ->
            val color = fpsColor(estimate.fps, refreshHz)
            val fraction by animateFloatAsState((estimate.fps / maxFps).coerceIn(0.02f, 1f), tween(700), label = "fps-${estimate.game}")
            Column(Modifier.padding(vertical = 5.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        estimate.game,
                        style = MaterialTheme.typography.bodyMedium,
                        color = NeoColors.TextPrimary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    val note = when {
                        estimate.limitedBy == Bottleneck.CPU -> stringResource(R.string.fps_limited_cpu)
                        estimate.limitedBy == Bottleneck.ENGINE_CAP -> stringResource(R.string.fps_limited_cap)
                        estimate.fps >= refreshHz -> stringResource(R.string.fps_above_refresh, refreshHz)
                        else -> null
                    }
                    if (note != null) {
                        Text(note, style = MaterialTheme.typography.labelSmall, color = NeoColors.TextMuted)
                        Spacer(Modifier.width(8.dp))
                    }
                    Text("~${estimate.fps}", style = MaterialTheme.typography.titleSmall, color = color)
                }
                Spacer(Modifier.height(4.dp))
                Box(
                    Modifier
                        .fillMaxWidth()
                        .height(6.dp)
                        .clip(CircleShape)
                        .background(NeoColors.Outline)
                ) {
                    Box(
                        Modifier
                            .fillMaxWidth(fraction)
                            .height(6.dp)
                            .clip(CircleShape)
                            .background(Brush.horizontalGradient(listOf(color.copy(alpha = 0.55f), color)))
                    )
                }
            }
        }
    }
}
