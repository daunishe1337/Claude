package com.neorig.pcbuilder.ui.wizard

import androidx.compose.animation.core.animateIntAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.engine.BudgetShares
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.ui.build.BuilderViewModel
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.common.parseAmount
import com.neorig.pcbuilder.ui.components.GradientButton
import com.neorig.pcbuilder.ui.components.NeonCard
import com.neorig.pcbuilder.ui.components.StepIndicator
import com.neorig.pcbuilder.ui.components.neonText
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.roundToInt

/** Шаг 1: бюджет (ползунок + поле ввода, от 30 000 до 1 000 000 ₽, монитор включён). */
@Composable
fun BudgetScreen(viewModel: BuilderViewModel, onNext: () -> Unit) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val budget = state.request.budget
    val focusManager = LocalFocusManager.current

    Column(
        Modifier
            .fillMaxSize()
            .imePadding() // поле ввода не прячется под клавиатурой
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 20.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        StepIndicator(step = 1, total = 2, label = stringResource(R.string.step_of, 1, 2))
        Column {
            Text(stringResource(R.string.budget_title), style = MaterialTheme.typography.headlineMedium, color = NeoColors.TextPrimary)
            Spacer(Modifier.height(4.dp))
            Text(stringResource(R.string.budget_subtitle), style = MaterialTheme.typography.bodyMedium, color = NeoColors.TextMuted)
        }

        NeonCard(Modifier.fillMaxWidth()) {
            // Крупная анимированная сумма
            val animated by animateIntAsState(budget, animationSpec = tween(350), label = "budget")
            Text(
                formatRub(animated),
                style = MaterialTheme.typography.displaySmall,
                modifier = Modifier.neonText(),
            )
            Spacer(Modifier.height(12.dp))

            // Логарифмическая шкала: на маленьких суммах ползунок точнее
            Slider(
                value = budgetToSlider(budget),
                onValueChange = { viewModel.setBudget(sliderToBudget(it)) },
                colors = SliderDefaults.colors(
                    thumbColor = NeoColors.Teal,
                    activeTrackColor = NeoColors.Violet,
                    inactiveTrackColor = NeoColors.Outline,
                ),
            )
            Row(Modifier.fillMaxWidth()) {
                Text(formatRub(BuildRequest.MIN_BUDGET), style = MaterialTheme.typography.labelSmall, color = NeoColors.TextMuted, modifier = Modifier.weight(1f))
                Text(formatRub(BuildRequest.MAX_BUDGET), style = MaterialTheme.typography.labelSmall, color = NeoColors.TextMuted)
            }
            Spacer(Modifier.height(16.dp))

            // Поле ввода: синхронизировано с ползунком
            var text by remember(budget) { mutableStateOf(budget.toString()) }
            val typed = parseAmount(text)
            val outOfRange = typed == null || typed !in BuildRequest.MIN_BUDGET..BuildRequest.MAX_BUDGET
            OutlinedTextField(
                value = text,
                onValueChange = { value ->
                    text = value.filter { it.isDigit() }.take(7)
                    parseAmount(text)?.takeIf { it in BuildRequest.MIN_BUDGET..BuildRequest.MAX_BUDGET }?.let(viewModel::setBudget)
                },
                label = { Text(stringResource(R.string.budget_input_label)) },
                suffix = { Text("₽") },
                singleLine = true,
                isError = outOfRange,
                supportingText = if (outOfRange) {
                    { Text(stringResource(R.string.budget_out_of_range, formatRub(BuildRequest.MIN_BUDGET), formatRub(BuildRequest.MAX_BUDGET))) }
                } else {
                    null
                },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
                keyboardActions = KeyboardActions(onDone = {
                    viewModel.setBudget(typed ?: budget)
                    text = (typed ?: budget).coerceIn(BuildRequest.MIN_BUDGET, BuildRequest.MAX_BUDGET).toString()
                    focusManager.clearFocus()
                }),
                colors = OutlinedTextFieldDefaults.colors(
                    focusedBorderColor = NeoColors.Teal,
                    unfocusedBorderColor = NeoColors.Outline,
                    focusedLabelColor = NeoColors.Teal,
                    cursorColor = NeoColors.Teal,
                ),
                modifier = Modifier.fillMaxWidth(),
            )
        }

        // Быстрый выбор
        Text(stringResource(R.string.budget_presets), style = MaterialTheme.typography.titleSmall, color = NeoColors.TextMuted)
        Row(
            Modifier.horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            PRESETS.forEach { preset ->
                val selected = preset == budget
                val shape = RoundedCornerShape(12.dp)
                Text(
                    text = formatRub(preset),
                    style = MaterialTheme.typography.labelLarge,
                    color = if (selected) NeoColors.Background else NeoColors.TextPrimary,
                    modifier = Modifier
                        .clip(shape)
                        .background(if (selected) NeoColors.Teal else NeoColors.SurfaceHigh)
                        .border(1.dp, if (selected) NeoColors.Teal else NeoColors.Outline, shape)
                        .clickable { viewModel.setBudget(preset) }
                        .padding(horizontal = 14.dp, vertical = 10.dp),
                )
            }
        }

        // Подсказка: сколько уйдёт на монитор
        val share = BudgetShares.monitorShare(state.request.preferences.resolution)
        NeonCard(Modifier.fillMaxWidth()) {
            Text(
                stringResource(R.string.budget_split_hint, formatRub((budget * share).roundToInt()), (share * 100).roundToInt()),
                style = MaterialTheme.typography.bodyMedium,
                color = NeoColors.TextPrimary,
            )
            state.minimumBudget?.let { minimum ->
                Spacer(Modifier.height(6.dp))
                Text(
                    stringResource(R.string.budget_min_hint, formatRub(minimum)),
                    style = MaterialTheme.typography.bodySmall,
                    color = if (budget < minimum) NeoColors.Warning else NeoColors.TextMuted,
                )
            }
        }

        Spacer(Modifier.height(4.dp))
        GradientButton(
            text = stringResource(R.string.action_next),
            onClick = onNext,
            icon = NeoIcons.Tune,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(8.dp))
    }
}

private val PRESETS = listOf(50_000, 80_000, 120_000, 200_000, 350_000, 600_000)

private val LOG_RANGE = ln(BuildRequest.MAX_BUDGET.toDouble() / BuildRequest.MIN_BUDGET)

/** Ползунок 0..1 → бюджет по логарифмической шкале с «круглым» шагом. */
internal fun sliderToBudget(position: Float): Int {
    val raw = BuildRequest.MIN_BUDGET * (BuildRequest.MAX_BUDGET.toDouble() / BuildRequest.MIN_BUDGET).pow(position.toDouble())
    val step = when {
        raw < 100_000 -> 1_000
        raw < 300_000 -> 5_000
        else -> 10_000
    }
    return ((raw / step).roundToInt() * step).coerceIn(BuildRequest.MIN_BUDGET, BuildRequest.MAX_BUDGET)
}

internal fun budgetToSlider(budget: Int): Float =
    (ln(budget.toDouble() / BuildRequest.MIN_BUDGET) / LOG_RANGE).toFloat().coerceIn(0f, 1f)
