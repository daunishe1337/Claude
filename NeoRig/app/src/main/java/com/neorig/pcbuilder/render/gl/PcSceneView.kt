package com.neorig.pcbuilder.render.gl

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLSurfaceView
import android.util.Log
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.render.scene.OrbitCamera
import com.neorig.pcbuilder.render.scene.RgbState
import com.neorig.pcbuilder.render.scene.SceneComposer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.egl.EGLDisplay

/**
 * 3D-вид сборки на OpenGL ES 3.0.
 * Жесты: один палец — вращение (с инерцией), два пальца — зум, двойной тап — сброс камеры.
 */
@SuppressLint("ViewConstructor")
class PcSceneView(
    context: Context,
    private val composer: SceneComposer,
    antialiasing: Boolean,
) : GLSurfaceView(context) {

    private val camera = OrbitCamera()
    private val renderer = SceneRenderer(camera)
    private var worker: ExecutorService = Executors.newSingleThreadExecutor()
    private var currentBuild: PcBuild? = null
    private var hasScene = false

    /** Если false — жесты игнорируются (например, превью в списке). */
    var interactive: Boolean = true

    /** Вызывается при одиночном тапе (используется превью, чтобы открыть полный 3D-экран). */
    var onTap: (() -> Unit)? = null

    private val gestureDetector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
        override fun onDown(e: MotionEvent): Boolean {
            if (interactive) camera.beginDrag()
            return true
        }

        override fun onScroll(e1: MotionEvent?, e2: MotionEvent, distanceX: Float, distanceY: Float): Boolean {
            if (!interactive || e2.pointerCount > 1) return false
            camera.drag(distanceX * -1f, distanceY * -1f)
            return true
        }

        override fun onFling(e1: MotionEvent?, e2: MotionEvent, velocityX: Float, velocityY: Float): Boolean {
            if (!interactive || e2.pointerCount > 1) return false
            camera.endDrag(velocityX * 0.35f, velocityY * 0.35f)
            return true
        }

        override fun onDoubleTap(e: MotionEvent): Boolean {
            if (interactive) camera.reset()
            return true
        }

        override fun onSingleTapConfirmed(e: MotionEvent): Boolean {
            onTap?.invoke()
            return onTap != null
        }
    })

    private val scaleDetector = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
        override fun onScale(detector: ScaleGestureDetector): Boolean {
            if (interactive) camera.pinch(detector.scaleFactor)
            return true
        }
    })

    init {
        setEGLContextClientVersion(3)
        setEGLConfigChooser(MultisampleConfigChooser(antialiasing))
        preserveEGLContextOnPause = true
        setRenderer(renderer)
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    /** Показать сборку. Сцена строится в фоновом потоке (там же читаются .glb). */
    fun setBuild(build: PcBuild) {
        if (build == currentBuild) return
        currentBuild = build
        val animate = hasScene
        hasScene = true
        if (worker.isShutdown) worker = Executors.newSingleThreadExecutor()
        worker.execute {
            try {
                val scene = composer.compose(build)
                renderer.submitScene(scene, animate)
            } catch (e: Exception) {
                Log.e("NeoRigGL", "Не удалось построить сцену", e)
            }
        }
    }

    fun setRgb(state: RgbState) {
        renderer.rgbState = state
    }

    fun setAutoRotate(enabled: Boolean) {
        renderer.autoRotate = enabled
    }

    fun resetCamera() = camera.reset()

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (interactive && event.actionMasked == MotionEvent.ACTION_DOWN) {
            // Не даём родителю (прокрутке) перехватить жест вращения
            parent?.requestDisallowInterceptTouchEvent(true)
        }
        scaleDetector.onTouchEvent(event)
        gestureDetector.onTouchEvent(event)
        if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL) {
            camera.releaseWithoutFling()
        }
        return true
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (worker.isShutdown) worker = Executors.newSingleThreadExecutor()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        worker.shutdown()
    }

    /**
     * Выбор конфигурации EGL: сначала 4× MSAA (гладкие края), если не поддерживается —
     * без сглаживания. Требуется поддержка OpenGL ES 3.0.
     */
    private class MultisampleConfigChooser(private val antialiasing: Boolean) : EGLConfigChooser {
        override fun chooseConfig(egl: EGL10, display: EGLDisplay): EGLConfig {
            val attempts = buildList {
                if (antialiasing) add(attributes(depth = 24, samples = 4))
                add(attributes(depth = 24, samples = 0))
                add(attributes(depth = 16, samples = 0))
            }
            for (attribs in attempts) {
                val count = IntArray(1)
                if (!egl.eglChooseConfig(display, attribs, null, 0, count) || count[0] <= 0) continue
                val configs = arrayOfNulls<EGLConfig>(count[0])
                if (egl.eglChooseConfig(display, attribs, configs, configs.size, count)) {
                    configs.firstOrNull { it != null }?.let { return it }
                }
            }
            throw IllegalStateException("Нет подходящей конфигурации EGL (нужен OpenGL ES 3.0)")
        }

        private fun attributes(depth: Int, samples: Int): IntArray {
            val list = mutableListOf(
                EGL10.EGL_RED_SIZE, 8,
                EGL10.EGL_GREEN_SIZE, 8,
                EGL10.EGL_BLUE_SIZE, 8,
                EGL10.EGL_DEPTH_SIZE, depth,
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            )
            if (samples > 0) {
                list += listOf(EGL10.EGL_SAMPLE_BUFFERS, 1, EGL10.EGL_SAMPLES, samples)
            }
            list += EGL10.EGL_NONE
            return list.toIntArray()
        }

        private companion object {
            const val EGL_OPENGL_ES3_BIT = 0x40
        }
    }
}
