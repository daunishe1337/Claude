package com.neorig.pcbuilder.domain.fps

import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Resolution
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.roundToInt

/** Что ограничивает FPS в конкретной игре. */
enum class Bottleneck { GPU, CPU, ENGINE_CAP }

/** Оценка FPS в одной игре. */
data class FpsEstimate(
    val game: String,
    val fps: Int,
    val limitedBy: Bottleneck,
)

/**
 * Профиль игры для условной оценки.
 * @property gpuFps100 FPS на высоких настройках в 1080p при индексе видеокарты 100
 * @property cpuFps100 предел FPS, который «вытягивает» процессор с индексом 100
 * @property resolutionScale падение производительности GPU при 1440p и 4K
 * @property engineCap ограничение движка (если есть)
 */
data class GameProfile(
    val name: String,
    val gpuFps100: Int,
    val cpuFps100: Int,
    val resolutionScale: Pair<Double, Double> = 0.70 to 0.40,
    val engineCap: Int? = null,
)

/**
 * Примерная (условная!) оценка FPS по «тиру» видеокарты и процессора.
 * Модель: FPS = min(предел GPU, предел CPU, предел движка), где предел GPU зависит
 * от индекса видеокарты и разрешения, а предел CPU — от игрового индекса процессора.
 */
class FpsEstimator(private val games: List<GameProfile> = DEFAULT_GAMES) {

    fun estimate(build: PcBuild, resolution: Resolution): List<FpsEstimate> {
        val gpuScore = (build[Category.GPU]?.score ?: 0).coerceAtLeast(1)
        val cpuScore = (build[Category.CPU]?.score ?: 0).coerceAtLeast(1)
        return games.map { game -> estimateGame(game, gpuScore, cpuScore, resolution) }
    }

    private fun estimateGame(game: GameProfile, gpuScore: Int, cpuScore: Int, resolution: Resolution): FpsEstimate {
        val resFactor = when (resolution) {
            Resolution.FHD -> 1.0
            Resolution.QHD -> game.resolutionScale.first
            Resolution.UHD -> game.resolutionScale.second
        }
        val gpuLimit = game.gpuFps100 * (gpuScore / 100.0).pow(GPU_EXPONENT) * resFactor
        val cpuLimit = game.cpuFps100 * (cpuScore / 100.0).pow(CPU_EXPONENT)
        val cap = game.engineCap?.toDouble() ?: Double.MAX_VALUE
        val fps = min(gpuLimit, min(cpuLimit, cap))
        val limitedBy = when (fps) {
            cap -> Bottleneck.ENGINE_CAP
            cpuLimit -> Bottleneck.CPU
            else -> Bottleneck.GPU
        }
        return FpsEstimate(game.name, fps.roundToInt().coerceAtLeast(1), limitedBy)
    }

    companion object {
        private const val GPU_EXPONENT = 0.95
        private const val CPU_EXPONENT = 0.75

        /** Популярные игры (только названия, без логотипов). */
        val DEFAULT_GAMES = listOf(
            GameProfile("Cyberpunk 2077", gpuFps100 = 175, cpuFps100 = 190, resolutionScale = 0.66 to 0.36),
            GameProfile("Counter-Strike 2", gpuFps100 = 620, cpuFps100 = 560, resolutionScale = 0.80 to 0.55),
            GameProfile("Dota 2", gpuFps100 = 380, cpuFps100 = 300, resolutionScale = 0.85 to 0.60),
            GameProfile("Fortnite", gpuFps100 = 330, cpuFps100 = 320, resolutionScale = 0.72 to 0.42),
            GameProfile("Red Dead Redemption 2", gpuFps100 = 180, cpuFps100 = 210, resolutionScale = 0.70 to 0.42),
            GameProfile("Apex Legends", gpuFps100 = 340, cpuFps100 = 300, resolutionScale = 0.74 to 0.45, engineCap = 300),
            GameProfile("Baldur's Gate 3", gpuFps100 = 210, cpuFps100 = 160, resolutionScale = 0.72 to 0.44),
            GameProfile("GTA V", gpuFps100 = 260, cpuFps100 = 200, resolutionScale = 0.78 to 0.50, engineCap = 188),
        )
    }
}
