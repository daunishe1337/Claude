package com.neorig.pcbuilder.domain.model

import kotlinx.serialization.Serializable

/** Предпочтение по производителю процессора. */
enum class CpuBrand(val vendor: String?) {
    INTEL("Intel"),
    AMD("AMD"),
    ANY(null),
}

/** Предпочтение по производителю видеочипа. */
enum class GpuBrand(val vendor: String?) {
    NVIDIA("NVIDIA"),
    AMD("AMD"),
    INTEL("Intel"),
    ANY(null),
}

/** Назначение компьютера. */
enum class Purpose { GAMING, WORK, STUDY }

/** Разрешение монитора. */
enum class Resolution(val code: String, val label: String) {
    FHD("1080p", "1080p"),
    QHD("1440p", "1440p"),
    UHD("2160p", "4K"),
}

/** Частота обновления монитора. */
enum class RefreshRate(val hz: Int) {
    HZ_60(60),
    HZ_144(144),
    HZ_240(240),
}

/** Пожелания пользователя к сборке. */
@Serializable
data class BuildPreferences(
    val cpuBrand: CpuBrand = CpuBrand.ANY,
    val gpuBrand: GpuBrand = GpuBrand.ANY,
    val purpose: Purpose = Purpose.GAMING,
    val resolution: Resolution = Resolution.FHD,
    val refreshRate: RefreshRate = RefreshRate.HZ_144,
)

/** Запрос на автоматическую сборку: бюджет (включая монитор) + пожелания. */
@Serializable
data class BuildRequest(
    val budget: Int = DEFAULT_BUDGET,
    val preferences: BuildPreferences = BuildPreferences(),
) {
    companion object {
        const val MIN_BUDGET = 30_000
        const val MAX_BUDGET = 1_000_000
        const val DEFAULT_BUDGET = 150_000
    }
}
