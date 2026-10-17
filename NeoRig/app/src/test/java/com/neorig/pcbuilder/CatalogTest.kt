package com.neorig.pcbuilder

import com.neorig.pcbuilder.domain.model.Category
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CatalogTest {

    private val catalog = TestCatalog.catalog

    @Test
    fun everyCategoryHas10to20Parts() {
        for (category in Category.entries) {
            val size = catalog.of(category).size
            assertTrue("$category: $size", size in 10..20)
        }
    }

    @Test
    fun idsAreUniqueAndCategoriesAssigned() {
        val all = Category.entries.flatMap { catalog.of(it) }
        assertEquals(all.size, all.map { it.id }.toSet().size)
        for (category in Category.entries) {
            assertTrue(catalog.of(category).all { it.category == category })
        }
    }

    @Test
    fun compatibilityFieldsArePresent() {
        catalog.of(Category.CPU).forEach {
            assertNotNull(it.id, it.compat.socket)
            assertTrue(it.id, it.compat.memoryTypes.isNotEmpty())
            assertNotNull(it.id, it.compat.tdpW)
            assertNotNull(it.id, it.specs.integratedGraphics)
        }
        catalog.of(Category.MOTHERBOARD).forEach {
            assertNotNull(it.id, it.compat.socket)
            assertNotNull(it.id, it.compat.memoryType)
            assertTrue(it.id, it.compat.formFactor in setOf("ATX", "mATX", "ITX"))
        }
        catalog.of(Category.GPU).forEach {
            assertNotNull(it.id, it.compat.tdpW)
            assertNotNull(it.id, it.compat.lengthMm)
        }
        catalog.of(Category.RAM).forEach { assertNotNull(it.id, it.compat.memoryType) }
        catalog.of(Category.PSU).forEach { assertNotNull(it.id, it.compat.powerW) }
        catalog.of(Category.COOLER).forEach {
            assertTrue(it.id, it.compat.sockets.isNotEmpty())
            assertNotNull(it.id, it.compat.maxTdpW)
            assertTrue(it.id, (it.compat.heightMm != null) xor (it.compat.radiatorMm != null))
        }
        catalog.of(Category.CASE).forEach {
            assertTrue(it.id, it.compat.supportedFormFactors.isNotEmpty())
            assertNotNull(it.id, it.compat.maxGpuLengthMm)
            assertNotNull(it.id, it.compat.maxCoolerHeightMm)
            assertNotNull(it.id, it.compat.maxRadiatorMm)
        }
        catalog.of(Category.MONITOR).forEach {
            assertTrue(it.id, it.specs.resolution in setOf("1080p", "1440p", "2160p"))
            assertNotNull(it.id, it.specs.refreshHz)
            assertNotNull(it.id, it.specs.sizeInch)
        }
    }

    @Test
    fun colorsAreValidHex() {
        val hex = Regex("^#[0-9A-Fa-f]{6}$")
        Category.entries.flatMap { catalog.of(it) }.forEach {
            assertTrue(it.id, hex.matches(it.visual.color))
            assertTrue(it.id, hex.matches(it.visual.accent))
        }
    }
}
