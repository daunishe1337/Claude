package com.neorig.pcbuilder

import com.neorig.pcbuilder.domain.engine.AssembleResult
import com.neorig.pcbuilder.domain.engine.BuildEngine
import com.neorig.pcbuilder.domain.model.BuildPreferences
import com.neorig.pcbuilder.domain.model.BuildRequest
import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.domain.model.Purpose
import com.neorig.pcbuilder.render.scene.GlbFormatException
import com.neorig.pcbuilder.render.scene.GlbLoader
import com.neorig.pcbuilder.render.scene.Mat4
import com.neorig.pcbuilder.render.scene.OrbitCamera
import com.neorig.pcbuilder.render.scene.RgbMode
import com.neorig.pcbuilder.render.scene.RgbState
import com.neorig.pcbuilder.render.scene.SceneAnimator
import com.neorig.pcbuilder.render.scene.SceneComposer
import com.neorig.pcbuilder.render.scene.Vec3
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import kotlin.math.abs

class SceneTest {

    private val catalog = TestCatalog.catalog
    private val modelsDir = File(TestCatalog.assetsDir, "models")

    private fun build(budget: Int, purpose: Purpose = Purpose.GAMING): PcBuild =
        (BuildEngine(catalog).assemble(BuildRequest(budget, BuildPreferences(purpose = purpose))) as AssembleResult.Success).build

    @Test
    fun normalMatrixIsInverseTranspose() {
        val m = Mat4.multiply(Mat4.rotation(30f, Vec3(0.3f, 1f, 0.2f)), Mat4.scale(2f, 0.5f, 3f))
        val expected = Mat4.multiply(Mat4.rotation(30f, Vec3(0.3f, 1f, 0.2f)), Mat4.scale(0.5f, 2f, 1f / 3f))
        val n = Mat4.normalMatrix(m)
        for (col in 0 until 3) for (row in 0 until 3) {
            assertEquals("[$row,$col]", expected[col * 4 + row], n[col * 3 + row], 1e-4f)
        }
    }

    @Test
    fun composesEveryCategoryWithFallbackGeometry() {
        // Источник моделей всегда пустой — должна сработать упрощённая геометрия
        val composer = SceneComposer(modelSource = { null })
        for (budget in listOf(60_000, 150_000, 400_000, 1_000_000)) {
            val b = build(budget)
            val scene = composer.compose(b)
            val slots = scene.parts.map { it.slot }.toSet()
            for (category in Category.entries) {
                if (category == Category.GPU && b[Category.GPU]!!.isIntegratedGraphics) continue
                assertTrue("$category отсутствует", category.name in slots)
            }
            assertEquals(0, composer.lastGlbCount)
            assertTrue(scene.parts.all { it.model.items.isNotEmpty() })
        }
    }

    @Test
    fun everyPartOfCatalogHasModel() {
        val composer = SceneComposer(modelSource = { path -> File(TestCatalog.assetsDir, path).takeIf { it.exists() }?.readBytes() })
        val base = build(150_000)
        for (category in Category.entries) {
            for (part in catalog.of(category)) {
                val scene = composer.compose(base.with(part))
                if (!part.isIntegratedGraphics) {
                    assertTrue("${part.id}", scene.parts.any { it.partId == part.id && it.model.items.isNotEmpty() })
                }
            }
        }
    }

    @Test
    fun glbModelsAreLoadedFromAssets() {
        val files = modelsDir.listFiles { f -> f.extension == "glb" }.orEmpty()
        assertTrue("нет .glb в ${modelsDir.absolutePath}", files.size >= 5)
        for (file in files) {
            val model = GlbLoader.parse(file.readBytes())
            assertTrue(file.name, model.primitives.isNotEmpty())
            val (min, max) = model.bounds()
            assertTrue(file.name, max.x > min.x && max.y > min.y && max.z > min.z)
        }
        val composer = SceneComposer(modelSource = { path -> File(TestCatalog.assetsDir, path).takeIf { it.exists() }?.readBytes() })
        composer.compose(build(200_000))
        assertTrue("ожидали детали из .glb", composer.lastGlbCount >= 3)
    }

    @Test
    fun brokenGlbFallsBack() {
        try {
            GlbLoader.parse(ByteArray(64) { it.toByte() })
            fail("ожидали исключение")
        } catch (e: GlbFormatException) {
            // ок
        }
        val composer = SceneComposer(modelSource = { ByteArray(100) })
        val scene = composer.compose(build(200_000))
        assertTrue(scene.parts.all { it.model.items.isNotEmpty() })
    }

    @Test
    fun animatorReplacesPartsSmoothly() {
        val composer = SceneComposer(modelSource = { null })
        val animator = SceneAnimator()
        val first = build(150_000)
        animator.setScene(composer.compose(first), now = 0f, animate = true)
        val eye = Vec3(80f, 40f, 80f)
        val rgb = RgbState(mode = RgbMode.RAINBOW)
        val settled = animator.frame(5f, rgb, eye)
        assertTrue(settled.opaque.isNotEmpty())
        assertTrue(settled.transparent.isNotEmpty())
        assertTrue(settled.glows.isNotEmpty())
        assertTrue(settled.lights.size in 1..4)

        // Меняем видеокарту: в середине анимации рисуются и старая, и новая
        val newGpu = catalog.of(Category.GPU).last()
        animator.setScene(composer.compose(first.with(newGpu)), now = 5f, animate = true)
        val during = animator.frame(5.25f, rgb, eye)
        val after = animator.frame(7f, rgb, eye)
        assertTrue(during.opaque.size > after.opaque.size)

        // Выключенная подсветка — без ореолов, остаётся только нейтральная лампа внутри корпуса
        val off = animator.frame(8f, RgbState(enabled = false), eye)
        assertTrue(off.glows.isEmpty())
        assertEquals(1, off.lights.size)
        // Прозрачные объекты отсортированы от дальних к ближним
        val distances = after.transparent.map { it.distance }
        assertEquals(distances.sortedDescending(), distances)
    }

    @Test
    fun cameraResetsToDefault() {
        val camera = OrbitCamera()
        camera.frame(Vec3(0f, 20f, 0f), 50f, immediate = true)
        camera.beginDrag()
        camera.drag(300f, -120f)
        camera.endDrag(0f, 0f)
        camera.pinch(1.8f)
        camera.reset()
        repeat(60) { camera.update(1f / 60f, autoRotate = false) }
        assertEquals(OrbitCamera.DEFAULT_YAW, camera.yaw, 0.01f)
        assertEquals(OrbitCamera.DEFAULT_PITCH, camera.pitch, 0.01f)
        assertTrue(abs(camera.zoom - 1f) < 1e-3f)
    }
}
