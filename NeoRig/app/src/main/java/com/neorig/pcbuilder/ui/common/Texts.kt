package com.neorig.pcbuilder.ui.common

import android.content.res.Resources
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.fps.FpsEstimate
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.GpuBrand
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Purpose

/**
 * Локализованные тексты для доменных моделей.
 * Функции принимают Resources, поэтому подходят и для Compose (LocalContext), и для Intent.
 */
object Texts {

    fun categoryName(res: Resources, category: Category): String = res.getString(
        when (category) {
            Category.CPU -> R.string.cat_cpu
            Category.MOTHERBOARD -> R.string.cat_motherboard
            Category.GPU -> R.string.cat_gpu
            Category.RAM -> R.string.cat_ram
            Category.SSD -> R.string.cat_ssd
            Category.PSU -> R.string.cat_psu
            Category.COOLER -> R.string.cat_cooler
            Category.CASE -> R.string.cat_case
            Category.MONITOR -> R.string.cat_monitor
        }
    )

    fun purposeName(res: Resources, purpose: Purpose): String = res.getString(
        when (purpose) {
            Purpose.GAMING -> R.string.purpose_gaming
            Purpose.WORK -> R.string.purpose_work
            Purpose.STUDY -> R.string.purpose_study
        }
    )

    fun cpuBrandName(res: Resources, brand: CpuBrand): String =
        brand.vendor ?: res.getString(R.string.any)

    fun gpuBrandName(res: Resources, brand: GpuBrand): String =
        brand.vendor ?: res.getString(R.string.any)

    /** Название детали (встроенная графика локализуется). */
    fun partName(res: Resources, part: Part): String =
        if (part.isIntegratedGraphics) res.getString(R.string.integrated_graphics) else part.name

    /** Краткие характеристики одной строкой. */
    fun specLine(res: Resources, part: Part): String {
        val s = part.specs
        val c = part.compat
        val items = mutableListOf<String>()
        when (part.category) {
            Category.CPU -> {
                c.socket?.let(items::add)
                if (s.cores != null && s.threads != null) items += res.getString(R.string.spec_cores, s.cores, s.threads)
                s.boostGhz?.let { items += res.getString(R.string.spec_boost, it.toString()) }
                c.tdpW?.let { items += res.getString(R.string.spec_tdp, it) }
                if (s.integratedGraphics == true) items += res.getString(R.string.spec_igpu)
            }
            Category.MOTHERBOARD -> {
                c.socket?.let(items::add)
                s.chipset?.let(items::add)
                c.memoryType?.let(items::add)
                c.formFactor?.let(items::add)
                if (s.wifi == true) items += res.getString(R.string.spec_wifi)
            }
            Category.GPU -> if (!part.isIntegratedGraphics) {
                s.vramGb?.let { items += res.getString(R.string.spec_vram, it) }
                c.lengthMm?.let { items += res.getString(R.string.spec_length, it) }
                c.tdpW?.let { items += res.getString(R.string.spec_tdp, it) }
            } else {
                items += res.getString(R.string.spec_igpu_desc)
            }
            Category.RAM -> {
                s.capacityGb?.let { items += res.getString(R.string.spec_capacity_gb, it) }
                c.memoryType?.let(items::add)
                s.speedMhz?.let { items += res.getString(R.string.spec_speed, it) }
            }
            Category.SSD -> {
                s.capacityGb?.let { items += capacity(res, it) }
                s.pcie?.let(items::add)
                s.readMbps?.let { items += res.getString(R.string.spec_read, it) }
            }
            Category.PSU -> {
                c.powerW?.let { items += res.getString(R.string.spec_tdp, it) }
                s.efficiency?.let(items::add)
                if (s.modular == true) items += res.getString(R.string.spec_modular)
            }
            Category.COOLER -> {
                val radiator = c.radiatorMm
                if (radiator != null) items += res.getString(R.string.spec_aio, radiator)
                else {
                    items += res.getString(R.string.spec_air)
                    c.heightMm?.let { items += res.getString(R.string.spec_height, it) }
                }
                c.maxTdpW?.let { items += res.getString(R.string.spec_cooler_tdp, it) }
            }
            Category.CASE -> {
                s.size?.let(items::add)
                if (c.supportedFormFactors.isNotEmpty()) items += c.supportedFormFactors.joinToString("/")
                c.maxGpuLengthMm?.let { items += res.getString(R.string.spec_gpu_max, it) }
            }
            Category.MONITOR -> {
                s.sizeInch?.let { items += res.getString(R.string.spec_inch, trimDouble(it)) }
                s.resolution?.let { items += if (it == "2160p") "4K" else it }
                s.refreshHz?.let { items += res.getString(R.string.spec_hz, it) }
                s.panel?.let(items::add)
            }
        }
        return items.joinToString(" · ")
    }

    private fun capacity(res: Resources, gb: Int): String =
        if (gb >= 1000) res.getString(R.string.spec_capacity_tb, trimDouble(gb / 1000.0)) else res.getString(R.string.spec_capacity_gb, gb)

    private fun trimDouble(value: Double): String =
        if (value % 1.0 == 0.0) value.toInt().toString() else value.toString()

    /** Понятное описание проблемы совместимости. */
    fun issue(res: Resources, issue: Incompatibility): String = when (issue) {
        is Incompatibility.SocketMismatch -> res.getString(R.string.issue_socket, issue.cpuSocket, issue.boardSocket)
        is Incompatibility.CpuMemoryUnsupported -> res.getString(R.string.issue_cpu_memory, issue.boardType, issue.cpuTypes.joinToString("/"))
        is Incompatibility.RamTypeMismatch -> res.getString(R.string.issue_ram_type, issue.boardType, issue.ramType)
        is Incompatibility.FormFactorUnsupported -> res.getString(R.string.issue_form_factor, issue.boardFormFactor, issue.caseSupports.joinToString("/"))
        is Incompatibility.GpuTooLong -> res.getString(R.string.issue_gpu_length, issue.gpuLengthMm, issue.caseMaxMm)
        is Incompatibility.CoolerTooTall -> res.getString(R.string.issue_cooler_height, issue.coolerHeightMm, issue.caseMaxMm)
        is Incompatibility.RadiatorUnsupported ->
            if (issue.caseMaxMm <= 0) res.getString(R.string.issue_radiator_none)
            else res.getString(R.string.issue_radiator, issue.radiatorMm, issue.caseMaxMm)
        is Incompatibility.CoolerSocketUnsupported -> res.getString(R.string.issue_cooler_socket, issue.socket)
        is Incompatibility.PsuTooWeak -> res.getString(R.string.issue_psu, issue.psuWatts, issue.requiredWatts)
        is Incompatibility.NoIntegratedGraphics -> res.getString(R.string.issue_igpu, issue.cpuName)
        is Incompatibility.CoolerUnderpowered -> res.getString(R.string.issue_cooler_tdp, issue.coolerTdpW, issue.cpuTdpW)
    }

    /** Текст сборки для «Поделиться». */
    fun shareText(res: Resources, name: String?, request: BuildRequest, build: PcBuild, fps: List<FpsEstimate>): String {
        val prefs = request.preferences
        val sb = StringBuilder()
        sb.appendLine(res.getString(R.string.share_header, name ?: res.getString(R.string.default_build_name, formatRub(request.budget))))
        sb.appendLine(
            res.getString(
                R.string.share_budget,
                formatRub(request.budget),
                formatRub(build.totalPrice),
                formatRub(request.budget - build.totalPrice),
            )
        )
        sb.appendLine(res.getString(R.string.share_prefs, purposeName(res, prefs.purpose), prefs.resolution.label, prefs.refreshRate.hz))
        sb.appendLine()
        for (part in build.orderedParts()) {
            sb.appendLine("• ${categoryName(res, part.category)}: ${partName(res, part)} — ${formatRub(part.price)}")
        }
        if (fps.isNotEmpty()) {
            sb.appendLine()
            sb.appendLine(res.getString(R.string.share_fps, prefs.resolution.label))
            sb.appendLine(fps.joinToString(", ") { "${it.game} ~${it.fps}" })
        }
        sb.appendLine()
        sb.append(res.getString(R.string.share_footer))
        return sb.toString()
    }
}
