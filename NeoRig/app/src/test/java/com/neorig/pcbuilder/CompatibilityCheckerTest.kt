package com.neorig.pcbuilder

import com.neorig.pcbuilder.domain.compat.CompatibilityChecker
import com.neorig.pcbuilder.domain.compat.Incompatibility
import com.neorig.pcbuilder.domain.compat.Severity
import com.neorig.pcbuilder.domain.model.PcBuild
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class CompatibilityCheckerTest {

    private val catalog = TestCatalog.catalog
    private val checker = CompatibilityChecker()

    private fun build(vararg ids: String) = PcBuild(ids.map { catalog.byId(it)!! }.associateBy { it.category })

    @Test
    fun socketMismatchDetected() {
        val issues = checker.check(build("cpu-r5-7500f", "mb-b760-d5"))
        assertTrue(issues.any { it is Incompatibility.SocketMismatch })
    }

    @Test
    fun ramTypeMismatchDetected() {
        val issues = checker.check(build("mb-b650-atx", "ram-d4-32-3200"))
        assertTrue(issues.single() is Incompatibility.RamTypeMismatch)
    }

    @Test
    fun formFactorChecked() {
        val issues = checker.check(build("mb-b650-atx", "case-nr200p"))
        assertTrue(issues.single() is Incompatibility.FormFactorUnsupported)
    }

    @Test
    fun gpuLengthAndCoolerHeightChecked() {
        val issues = checker.check(build("gpu-rtx5090", "case-cylon-mini", "cool-nhd15g2"))
        assertTrue(issues.any { it is Incompatibility.GpuTooLong })
        assertTrue(issues.any { it is Incompatibility.CoolerTooTall })
    }

    @Test
    fun radiatorChecked() {
        val issues = checker.check(build("aio-420", "case-lancool216"))
        assertTrue(issues.single() is Incompatibility.RadiatorUnsupported)
        assertTrue(checker.isClean(build("aio-420", "case-dbp901")))
    }

    @Test
    fun psuRuleIsDrawTimes1_3() {
        // 5090 (575) + 9800X3D (120) + 75 = 770 Вт; × 1.3 = 1001 Вт
        val b = build("cpu-r7-9800x3d", "gpu-rtx5090", "psu-1000-g")
        assertEquals(1001, checker.requiredPsuWatts(b))
        assertTrue(checker.check(b).single() is Incompatibility.PsuTooWeak)
        assertTrue(checker.isClean(build("cpu-r7-9800x3d", "gpu-rtx5090", "psu-1200-p")))
    }

    @Test
    fun coolerUnderpoweredIsOnlyWarning() {
        val issue = checker.check(build("cpu-i7-14700kf", "cool-se214")).single()
        assertTrue(issue is Incompatibility.CoolerUnderpowered)
        assertEquals(Severity.WARNING, issue.severity)
        assertTrue(!checker.hasErrors(build("cpu-i7-14700kf", "cool-se214")))
    }

    @Test
    fun integratedGraphicsNeedsIgpu() {
        assertTrue(checker.check(build("cpu-r5-5600", "gpu-integrated")).single() is Incompatibility.NoIntegratedGraphics)
        assertTrue(checker.isClean(build("cpu-r5-5600g", "gpu-integrated")))
    }
}
