package com.neorig.pcbuilder.ui.components

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithCache
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.ui.common.Texts
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.categoryColor
import kotlin.math.max
import kotlin.math.roundToInt

/**
 * Анимированная полоса бюджета: цветные сегменты — сколько потрачено на каждую категорию.
 * При первом показе сегменты «вырастают» слева направо, при замене детали плавно меняют ширину.
 */
@Composable
fun BudgetBar(
    spending: Map<Category, Int>,
    budget: Int,
    modifier: Modifier = Modifier,
    showLegend: Boolean = true,
) {
    val spent = spending.values.sum()
    val scale = max(budget, spent).coerceAtLeast(1).toFloat()
    val appear = remember { Animatable(0f) }
    LaunchedEffect(Unit) { appear.animateTo(1f, tween(durationMillis = 900, easing = FastOutSlowInEasing)) }

    // Анимируем долю каждой категории отдельно (порядок категорий фиксирован)
    val fractions = Category.entries.map { category ->
        val target = (spending[category] ?: 0) / scale
        val animated by animateFloatAsState(target, animationSpec = tween(600), label = "budget-${category.name}")
        category to animated
    }

    Column(modifier) {
        Canvas(
            Modifier
                .fillMaxWidth()
                .height(16.dp)
                .clip(CircleShape)
        ) {
            drawRoundRect(color = NeoColors.Outline, cornerRadius = CornerRadius(size.height / 2))
            val gap = 2.dp.toPx()
            var x = 0f
            for ((category, fraction) in fractions) {
                val w = size.width * fraction * appear.value
                if (w <= 0.5f) continue
                drawRoundRect(
                    color = categoryColor(category),
                    topLeft = Offset(x, 0f),
                    size = Size(max(w - gap, 1f), size.height),
                    cornerRadius = CornerRadius(3.dp.toPx()),
                )
                x += w
            }
        }
        if (showLegend) {
            Spacer(Modifier.height(12.dp))
            val res = LocalContext.current.resources
            val visible = Category.entries.filter { (spending[it] ?: 0) > 0 }
            visible.chunked(2).forEach { row ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    row.forEach { category ->
                        val price = spending[category] ?: 0
                        val percent = (price * 100f / max(budget, 1)).roundToInt()
                        Row(Modifier.weight(1f).height(26.dp), verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                Modifier
                                    .size(10.dp)
                                    .clip(CircleShape)
                                    .background(categoryColor(category))
                            )
                            Spacer(Modifier.width(6.dp))
                            Text(
                                Texts.categoryName(res, category),
                                style = MaterialTheme.typography.bodySmall,
                                color = NeoColors.TextMuted,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                                modifier = Modifier.weight(1f),
                            )
                            Text(
                                "$percent%",
                                style = MaterialTheme.typography.labelMedium,
                                color = NeoColors.TextPrimary,
                            )
                        }
                    }
                    if (row.size == 1) Spacer(Modifier.weight(1f))
                }
            }
        }
    }
}

/** Заливка текста фирменным градиентом (без экспериментальных API). */
fun Modifier.neonText(brush: Brush = NeoColors.NeonGradient): Modifier = this
    .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
    .drawWithCache {
        onDrawWithContent {
            drawContent()
            drawRect(brush = brush, blendMode = BlendMode.SrcAtop)
        }
    }
