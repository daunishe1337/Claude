package com.neorig.pcbuilder.ui.common

import kotlin.math.abs

/** Форматирование денег: «150 000 ₽» (неразрывные пробелы между разрядами). */
fun formatRub(amount: Int): String {
    val sign = if (amount < 0) "−" else ""
    val digits = abs(amount).toString()
    val grouped = digits.reversed().chunked(3).joinToString(" ").reversed()
    return "$sign$grouped ₽"
}

/** Со знаком: «+3 500 ₽» / «−2 000 ₽». */
fun formatRubDelta(delta: Int): String = when {
    delta > 0 -> "+" + formatRub(delta)
    delta == 0 -> "±0 ₽"
    else -> formatRub(delta)
}

/** Только цифры из строки ввода (для поля бюджета). */
fun parseAmount(text: String): Int? = text.filter { it.isDigit() }.take(7).toIntOrNull()
