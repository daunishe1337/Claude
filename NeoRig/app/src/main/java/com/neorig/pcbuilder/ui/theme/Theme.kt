package com.neorig.pcbuilder.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.neorig.pcbuilder.domain.model.Category

/** Палитра приложения: тёмный фон и неоновые акценты. */
object NeoColors {
    val Background = Color(0xFF090B12)
    val Surface = Color(0xFF121624)
    val SurfaceHigh = Color(0xFF1A1F32)
    val SurfaceHighest = Color(0xFF232A42)
    val Outline = Color(0xFF2B3252)
    val Violet = Color(0xFFA855F7)
    val VioletDeep = Color(0xFF7C3AED)
    val Teal = Color(0xFF22D3EE)
    val Mint = Color(0xFF2DD4BF)
    val TextPrimary = Color(0xFFE8EAF6)
    val TextMuted = Color(0xFF8E94AE)
    val Error = Color(0xFFFB7185)
    val Warning = Color(0xFFFBBF24)
    val Success = Color(0xFF34D399)

    /** Фирменный градиент: фиолетовый → бирюзовый. */
    val NeonGradient: Brush get() = Brush.horizontalGradient(listOf(Violet, Teal))

    /** Мягкая неоновая рамка карточек. */
    val BorderGradient: Brush get() = Brush.linearGradient(listOf(Violet.copy(alpha = 0.55f), Teal.copy(alpha = 0.25f), Outline))
}

/** Цвет категории — для сегментов полосы бюджета и иконок. */
fun categoryColor(category: Category): Color = when (category) {
    Category.CPU -> Color(0xFFA855F7)
    Category.MOTHERBOARD -> Color(0xFF6366F1)
    Category.GPU -> Color(0xFF22D3EE)
    Category.RAM -> Color(0xFFEC4899)
    Category.SSD -> Color(0xFFF59E0B)
    Category.PSU -> Color(0xFF84CC16)
    Category.COOLER -> Color(0xFF38BDF8)
    Category.CASE -> Color(0xFF94A3B8)
    Category.MONITOR -> Color(0xFF2DD4BF)
}

private val NeoColorScheme = darkColorScheme(
    primary = NeoColors.Violet,
    onPrimary = Color.White,
    primaryContainer = NeoColors.VioletDeep,
    onPrimaryContainer = Color.White,
    secondary = NeoColors.Teal,
    onSecondary = Color(0xFF00151A),
    secondaryContainer = Color(0xFF0E3A44),
    onSecondaryContainer = NeoColors.Teal,
    tertiary = NeoColors.Mint,
    background = NeoColors.Background,
    onBackground = NeoColors.TextPrimary,
    surface = NeoColors.Surface,
    onSurface = NeoColors.TextPrimary,
    surfaceVariant = NeoColors.SurfaceHigh,
    onSurfaceVariant = NeoColors.TextMuted,
    surfaceContainer = NeoColors.Surface,
    surfaceContainerHigh = NeoColors.SurfaceHigh,
    surfaceContainerHighest = NeoColors.SurfaceHighest,
    surfaceContainerLow = NeoColors.Surface,
    surfaceContainerLowest = NeoColors.Background,
    outline = NeoColors.Outline,
    outlineVariant = NeoColors.Outline,
    error = NeoColors.Error,
    onError = Color.Black,
)

private val NeoTypography = Typography(
    displaySmall = TextStyle(fontSize = 34.sp, fontWeight = FontWeight.ExtraBold, letterSpacing = (-0.5).sp),
    headlineMedium = TextStyle(fontSize = 26.sp, fontWeight = FontWeight.Bold),
    headlineSmall = TextStyle(fontSize = 22.sp, fontWeight = FontWeight.Bold),
    titleLarge = TextStyle(fontSize = 20.sp, fontWeight = FontWeight.SemiBold),
    titleMedium = TextStyle(fontSize = 16.sp, fontWeight = FontWeight.SemiBold),
    titleSmall = TextStyle(fontSize = 14.sp, fontWeight = FontWeight.SemiBold),
    bodyLarge = TextStyle(fontSize = 16.sp, lineHeight = 22.sp),
    bodyMedium = TextStyle(fontSize = 14.sp, lineHeight = 20.sp),
    bodySmall = TextStyle(fontSize = 12.sp, lineHeight = 16.sp),
    labelLarge = TextStyle(fontSize = 14.sp, fontWeight = FontWeight.SemiBold, letterSpacing = 0.3.sp),
    labelMedium = TextStyle(fontSize = 12.sp, fontWeight = FontWeight.Medium),
    labelSmall = TextStyle(fontSize = 11.sp, fontWeight = FontWeight.Medium, letterSpacing = 0.4.sp),
)

/** Скругления: карточки — 16dp, как в ТЗ. */
val NeoShapes = Shapes(
    small = RoundedCornerShape(10.dp),
    medium = RoundedCornerShape(16.dp),
    large = RoundedCornerShape(20.dp),
)

@Composable
fun NeoRigTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = NeoColorScheme,
        typography = NeoTypography,
        shapes = NeoShapes,
        content = content,
    )
}
