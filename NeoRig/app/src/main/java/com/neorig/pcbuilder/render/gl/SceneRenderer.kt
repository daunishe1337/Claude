package com.neorig.pcbuilder.render.gl

import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.os.SystemClock
import android.util.Log
import com.neorig.pcbuilder.render.scene.DrawCall
import com.neorig.pcbuilder.render.scene.FrameData
import com.neorig.pcbuilder.render.scene.Mat4
import com.neorig.pcbuilder.render.scene.MaterialRole
import com.neorig.pcbuilder.render.scene.MeshData
import com.neorig.pcbuilder.render.scene.OrbitCamera
import com.neorig.pcbuilder.render.scene.Primitives
import com.neorig.pcbuilder.render.scene.RgbState
import com.neorig.pcbuilder.render.scene.SceneAnimator
import com.neorig.pcbuilder.render.scene.SceneSpec
import com.neorig.pcbuilder.render.scene.Shaders
import com.neorig.pcbuilder.render.scene.Vec3
import java.util.IdentityHashMap
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * Рендерер OpenGL ES 3.0.
 * Порядок отрисовки кадра: фон → непрозрачные детали → тени и стекло (с сортировкой)
 * → аддитивные ореолы RGB-подсветки.
 */
class SceneRenderer(private val camera: OrbitCamera) : GLSurfaceView.Renderer {

    private lateinit var litProgram: GlProgram
    private lateinit var glowProgram: GlProgram
    private lateinit var backgroundProgram: GlProgram
    private val meshes = IdentityHashMap<MeshData, GpuMesh>()
    private val animator = SceneAnimator()

    private var width = 1
    private var height = 1
    private var startTime = SystemClock.elapsedRealtime()
    private var lastFrameTime = 0f
    private var ready = false

    @Volatile private var pendingScene: SceneSpec? = null
    @Volatile private var pendingAnimate = true
    @Volatile var rgbState: RgbState = RgbState()
    @Volatile var autoRotate: Boolean = false

    private val normalMatrix = FloatArray(9)
    private val pointPositions = FloatArray(12)
    private val pointColors = FloatArray(12)

    /** Передать новую сцену (из любого потока). Применится в начале следующего кадра. */
    fun submitScene(scene: SceneSpec, animate: Boolean) {
        pendingAnimate = animate
        pendingScene = scene
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        // Контекст новый: старые GPU-ресурсы недействительны
        meshes.clear()
        try {
            litProgram = GlProgram(Shaders.LIT_VERTEX, Shaders.LIT_FRAGMENT)
            glowProgram = GlProgram(Shaders.GLOW_VERTEX, Shaders.GLOW_FRAGMENT)
            backgroundProgram = GlProgram(Shaders.BACKGROUND_VERTEX, Shaders.BACKGROUND_FRAGMENT)
            ready = true
        } catch (e: IllegalStateException) {
            Log.e(TAG, "3D недоступно", e)
            ready = false
        }
        GLES30.glEnable(GLES30.GL_DEPTH_TEST)
        GLES30.glDepthFunc(GLES30.GL_LEQUAL)
        GLES30.glDisable(GLES30.GL_CULL_FACE)
        GLES30.glClearColor(0.03f, 0.035f, 0.06f, 1f)
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        this.width = width.coerceAtLeast(1)
        this.height = height.coerceAtLeast(1)
        GLES30.glViewport(0, 0, this.width, this.height)
    }

    override fun onDrawFrame(gl: GL10?) {
        val now = (SystemClock.elapsedRealtime() - startTime) / 1000f
        val dt = (now - lastFrameTime).coerceIn(0f, 0.1f)
        lastFrameTime = now

        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT or GLES30.GL_DEPTH_BUFFER_BIT)
        if (!ready) return

        val aspect = width.toFloat() / height
        camera.setAspect(aspect)
        pendingScene?.let { scene ->
            pendingScene = null
            val first = animator.isEmpty
            animator.setScene(scene, now, pendingAnimate)
            camera.frame(scene.focus, scene.radius, immediate = first)
        }
        camera.update(dt, autoRotate)

        val eye = camera.eye()
        val distance = camera.currentDistance()
        val projection = Mat4.perspective(OrbitCamera.FOV_Y, aspect, (distance * 0.05f).coerceAtLeast(2f), distance * 6f + 400f)
        val view = camera.viewMatrix()
        val viewProj = Mat4.multiply(projection, view)
        val frame = animator.frame(now, rgbState, eye)

        drawBackground(aspect)
        drawLit(frame, viewProj, eye)
        drawGlows(frame, viewProj, view)
        releaseUnused(frame)
    }

    // ---- Проходы ------------------------------------------------------------------------------

    private fun drawBackground(aspect: Float) {
        GLES30.glDisable(GLES30.GL_DEPTH_TEST)
        GLES30.glDepthMask(false)
        backgroundProgram.use()
        GLES30.glUniform3f(backgroundProgram.uniform("uTop"), 0.075f, 0.06f, 0.13f)
        GLES30.glUniform3f(backgroundProgram.uniform("uBottom"), 0.015f, 0.018f, 0.03f)
        GLES30.glUniform3f(backgroundProgram.uniform("uGlowA"), 0.16f, 0.07f, 0.30f)
        GLES30.glUniform3f(backgroundProgram.uniform("uGlowB"), 0.02f, 0.13f, 0.16f)
        GLES30.glUniform1f(backgroundProgram.uniform("uAspect"), aspect)
        gpu(FULLSCREEN).draw()
        GLES30.glDepthMask(true)
        GLES30.glEnable(GLES30.GL_DEPTH_TEST)
    }

    private fun drawLit(frame: FrameData, viewProj: FloatArray, eye: Vec3) {
        val p = litProgram
        p.use()
        GLES30.glUniformMatrix4fv(p.uniform("uViewProj"), 1, false, viewProj, 0)
        GLES30.glUniform3f(p.uniform("uCameraPos"), eye.x, eye.y, eye.z)
        GLES30.glUniform3f(p.uniform("uKeyDir"), 0.45f, 0.85f, 0.65f)
        GLES30.glUniform3f(p.uniform("uKeyColor"), 1.55f, 1.5f, 1.42f)
        GLES30.glUniform3f(p.uniform("uFillDir"), -0.8f, 0.35f, 0.25f)
        GLES30.glUniform3f(p.uniform("uFillColor"), 0.22f, 0.26f, 0.42f)
        GLES30.glUniform3f(p.uniform("uRimColor"), 0.30f, 0.16f, 0.55f)
        GLES30.glUniform3f(p.uniform("uSkyColor"), 0.13f, 0.13f, 0.19f)
        GLES30.glUniform3f(p.uniform("uGroundColor"), 0.035f, 0.035f, 0.05f)

        val lights = frame.lights
        for (i in 0 until 4) {
            val l = lights.getOrNull(i)
            pointPositions[i * 3] = l?.position?.x ?: 0f
            pointPositions[i * 3 + 1] = l?.position?.y ?: 0f
            pointPositions[i * 3 + 2] = l?.position?.z ?: 0f
            pointColors[i * 3] = l?.color?.r ?: 0f
            pointColors[i * 3 + 1] = l?.color?.g ?: 0f
            pointColors[i * 3 + 2] = l?.color?.b ?: 0f
        }
        GLES30.glUniform1i(p.uniform("uPointCount"), lights.size)
        GLES30.glUniform3fv(p.uniform("uPointPos"), 4, pointPositions, 0)
        GLES30.glUniform3fv(p.uniform("uPointColor"), 4, pointColors, 0)

        // Непрозрачные объекты
        GLES30.glDisable(GLES30.GL_BLEND)
        for (call in frame.opaque) drawCall(call)

        // Тени и стекло: смешивание, без записи глубины, от дальних к ближним
        GLES30.glEnable(GLES30.GL_BLEND)
        GLES30.glBlendFunc(GLES30.GL_SRC_ALPHA, GLES30.GL_ONE_MINUS_SRC_ALPHA)
        GLES30.glDepthMask(false)
        for (call in frame.transparent) drawCall(call)
        GLES30.glDepthMask(true)
    }

    private fun drawCall(call: DrawCall) {
        val p = litProgram
        GLES30.glUniformMatrix4fv(p.uniform("uModel"), 1, false, call.model, 0)
        Mat4.normalMatrix(call.model, normalMatrix)
        GLES30.glUniformMatrix3fv(p.uniform("uNormalMatrix"), 1, false, normalMatrix, 0)
        val m = call.material
        GLES30.glUniform4f(p.uniform("uBaseColor"), call.color.r, call.color.g, call.color.b, call.color.a)
        GLES30.glUniform3f(p.uniform("uEmissive"), call.emissive.r, call.emissive.g, call.emissive.b)
        GLES30.glUniform1f(p.uniform("uMetallic"), m.metallic)
        GLES30.glUniform1f(p.uniform("uRoughness"), m.roughness)
        val unlit = m.role == MaterialRole.UNLIT || m.role == MaterialRole.SHADOW
        GLES30.glUniform1f(p.uniform("uUnlit"), if (unlit) 1f else 0f)
        GLES30.glUniform1f(p.uniform("uGlass"), if (m.role == MaterialRole.GLASS) 1f else 0f)
        gpu(call.mesh).draw()
    }

    private fun drawGlows(frame: FrameData, viewProj: FloatArray, view: FloatArray) {
        if (frame.glows.isEmpty()) return
        val p = glowProgram
        p.use()
        GLES30.glUniformMatrix4fv(p.uniform("uViewProj"), 1, false, viewProj, 0)
        // Оси камеры в мировых координатах — из строк матрицы вида
        val right = Vec3(view[0], view[4], view[8])
        val up = Vec3(view[1], view[5], view[9])
        GLES30.glEnable(GLES30.GL_BLEND)
        GLES30.glBlendFunc(GLES30.GL_ONE, GLES30.GL_ONE)
        GLES30.glDepthMask(false)
        val quad = gpu(Primitives.quad)
        for (glow in frame.glows) {
            GLES30.glUniform3f(p.uniform("uCenter"), glow.position.x, glow.position.y, glow.position.z)
            GLES30.glUniform3f(p.uniform("uRight"), right.x * glow.size, right.y * glow.size, right.z * glow.size)
            GLES30.glUniform3f(p.uniform("uUp"), up.x * glow.size, up.y * glow.size, up.z * glow.size)
            GLES30.glUniform3f(p.uniform("uColor"), glow.color.r, glow.color.g, glow.color.b)
            GLES30.glUniform1f(p.uniform("uIntensity"), glow.intensity)
            quad.draw()
        }
        GLES30.glDepthMask(true)
        GLES30.glDisable(GLES30.GL_BLEND)
    }

    // ---- Ресурсы ------------------------------------------------------------------------------

    private fun gpu(mesh: MeshData): GpuMesh = meshes.getOrPut(mesh) { GpuMesh(mesh) }

    private var frameCounter = 0

    /** Раз в ~2 секунды освобождаем буферы мешей, которые больше не рисуются. */
    private fun releaseUnused(frame: FrameData) {
        if (++frameCounter % 120 != 0) return
        val used = java.util.Collections.newSetFromMap(IdentityHashMap<MeshData, Boolean>())
        frame.opaque.forEach { used += it.mesh }
        frame.transparent.forEach { used += it.mesh }
        used += Primitives.quad
        used += FULLSCREEN
        val iterator = meshes.entries.iterator()
        while (iterator.hasNext()) {
            val entry = iterator.next()
            if (entry.key !in used && !isPrimitive(entry.key)) {
                entry.value.release()
                iterator.remove()
            }
        }
    }

    private fun isPrimitive(mesh: MeshData) =
        mesh === Primitives.box || mesh === Primitives.cylinder || mesh === Primitives.ring || mesh === Primitives.fanBlades

    private companion object {
        const val TAG = "NeoRigGL"

        /** Полноэкранный квадрат для фона (в координатах NDC). */
        val FULLSCREEN = MeshData(
            positions = floatArrayOf(-1f, -1f, 0f, 1f, -1f, 0f, 1f, 1f, 0f, -1f, 1f, 0f),
            normals = FloatArray(12),
            colors = null,
            indices = intArrayOf(0, 1, 2, 0, 2, 3),
        )
    }
}
