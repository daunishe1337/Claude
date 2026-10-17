package com.neorig.pcbuilder

import com.neorig.pcbuilder.data.repo.BuildCodec
import com.neorig.pcbuilder.domain.engine.AssembleResult
import com.neorig.pcbuilder.domain.engine.BuildEngine
import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.CpuBrand
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.domain.model.RefreshRate
import com.neorig.pcbuilder.domain.model.Resolution
import com.neorig.pcbuilder.ui.common.formatRub
import com.neorig.pcbuilder.ui.common.formatRubDelta
import com.neorig.pcbuilder.ui.common.parseAmount
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CodecAndFormatTest {

    @Test
    fun buildRoundTripsThroughJson() {
        val catalog = TestCatalog.catalog
        val request = BuildRequest(180_000, BuildPreferences(CpuBrand.AMD, purpose = Purpose.WORK, resolution = Resolution.UHD, refreshRate = RefreshRate.HZ_60))
        val build = (BuildEngine(catalog).assemble(request) as AssembleResult.Success).build
        val decoded = BuildCodec.decodeParts(BuildCodec.encodeParts(build), catalog)
        assertEquals(build, decoded)
        assertEquals(request, BuildCodec.decodeRequest(BuildCodec.encodeRequest(request)))
    }

    @Test
    fun brokenJsonIsIgnored() {
        assertTrue(BuildCodec.decodeParts("{oops", TestCatalog.catalog).parts.isEmpty())
        assertNull(BuildCodec.decodeRequest("not json"))
        // Неизвестные детали пропускаются, остальные сохраняются
        val partial = BuildCodec.decodeParts("""{"CPU":"cpu-r5-5600","GPU":"gpu-deleted"}""", TestCatalog.catalog)
        assertEquals(setOf("cpu-r5-5600"), partial.parts.values.map { it.id }.toSet())
    }

    @Test
    fun rublesAreFormattedWithSpaces() {
        assertEquals("150 000 ₽", formatRub(150_000))
        assertEquals("1 000 000 ₽", formatRub(1_000_000))
        assertEquals("990 ₽", formatRub(990))
        assertEquals("−2 500 ₽", formatRub(-2_500))
        assertEquals("+3 500 ₽", formatRubDelta(3_500))
        assertEquals(150_000, parseAmount("150 000 ₽"))
        assertNull(parseAmount("abc"))
    }
}
