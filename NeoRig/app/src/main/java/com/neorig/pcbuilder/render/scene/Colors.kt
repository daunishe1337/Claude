package com.neorig.pcbuilder.render.scene

import kotlin.math.abs
import kotlin.math.pow

/**
 * Цвет RGBA в линейном пространстве (0..1) — именно в нём считается освещение.
 * Цвета из JSON (#RRGGBB, sRGB) переводятся в линейные через [fromHex].
 */
data class Rgba(val r: Float, val g: Float, val b: Float, val a: Float = 1f) {

    fun withAlpha(alpha: Float) = copy(a = alpha)

    operator fun times(k: Float) = Rgba(r * k, g * k, b * k, a)

    fun mix(other: Rgba, t: Float) = Rgba(
        r + (other.r - r) * t,
        g + (other.g - g) * t,
        b + (other.b - b) * t,
        a + (other.a - a) * t,
    )

    /** Яркость (для выбора «светлых» и «тёмных» вариантов). */
    val luminance: Float get() = 0.2126f * r + 0.7152f * g + 0.0722f * b

    companion object {
        val BLACK = Rgba(0f, 0f, 0f)
        val WHITE = Rgba(1f, 1f, 1f)

        /** «#RRGGBB» (sRGB) → линейный цвет. При ошибке — серый, чтобы сцена не падала. */
        fun fromHex(hex: String, alpha: Float = 1f): Rgba {
            val clean = hex.removePrefix("#")
            val value = clean.toLongOrNull(16) ?: return Rgba(0.2f, 0.2f, 0.2f, alpha)
            return fromSrgb(
                ((value shr 16) and 0xFF) / 255f,
                ((value shr 8) and 0xFF) / 255f,
                (value and 0xFF) / 255f,
                alpha,
            )
        }

        /** ARGB-цвет Android (Int) → линейный. */
        fun fromArgb(argb: Int): Rgba = fromSrgb(
            ((argb shr 16) and 0xFF) / 255f,
            ((argb shr 8) and 0xFF) / 255f,
            (argb and 0xFF) / 255f,
            1f,
        )

        fun fromSrgb(r: Float, g: Float, b: Float, a: Float = 1f) =
            Rgba(srgbToLinear(r), srgbToLinear(g), srgbToLinear(b), a)

        private fun srgbToLinear(c: Float): Float = c.toDouble().pow(2.2).toFloat()

        /** HSV → линейный RGB (для режима «Радуга»). [h] в градусах. */
        fun fromHsv(h: Float, s: Float, v: Float): Rgba {
            val hh = ((h % 360f) + 360f) % 360f / 60f
            val c = v * s
            val x = c * (1f - abs(hh % 2f - 1f))
            val m = v - c
            val (r, g, b) = when (hh.toInt()) {
                0 -> Triple(c, x, 0f)
                1 -> Triple(x, c, 0f)
                2 -> Triple(0f, c, x)
                3 -> Triple(0f, x, c)
                4 -> Triple(x, 0f, c)
                else -> Triple(c, 0f, x)
            }
            return fromSrgb(r + m, g + m, b + m)
        }
    }
}
