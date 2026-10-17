package com.neorig.pcbuilder

import com.neorig.pcbuilder.domain.fps.FpsEstimator
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Resolution
import org.junit.Assert.assertTrue
import org.junit.Test

class FpsEstimatorTest {

    private val catalog = TestCatalog.catalog
    private val estimator = FpsEstimator()

    private fun build(cpu: String, gpu: String) =
        PcBuild(listOf(catalog.byId(cpu)!!, catalog.byId(gpu)!!).associateBy { it.category })

    @Test
    fun fasterGpuGivesMoreFps() {
        val slow = estimator.estimate(build("cpu-r7-9800x3d", "gpu-rtx5060"), Resolution.QHD)
        val fast = estimator.estimate(build("cpu-r7-9800x3d", "gpu-rtx5080"), Resolution.QHD)
        slow.zip(fast).forEach { (s, f) -> assertTrue("${s.game}: ${s.fps} vs ${f.fps}", f.fps >= s.fps) }
    }

    @Test
    fun higherResolutionLowersFps() {
        val b = build("cpu-r5-7500f", "gpu-rtx5070")
        val fhd = estimator.estimate(b, Resolution.FHD)
        val uhd = estimator.estimate(b, Resolution.UHD)
        fhd.zip(uhd).forEach { (a, c) -> assertTrue(a.fps >= c.fps) }
    }

    @Test
    fun valuesArePlausible() {
        val cyberpunk = estimator.estimate(build("cpu-r5-7500f", "gpu-rtx5070"), Resolution.QHD)
            .first { it.game.startsWith("Cyberpunk") }
        assertTrue("${cyberpunk.fps}", cyberpunk.fps in 50..110)
    }
}
