package com.neorig.pcbuilder.render.scene

import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.sin

/**
 * Орбитальная камера: вращение пальцем (с инерцией), зум щипком,
 * плавный сброс по двойному тапу и плавный переезд при смене сцены.
 * Методы синхронизированы: жесты приходят из UI-потока, кадры рисуются в GL-потоке.
 */
class OrbitCamera {

    var yaw = DEFAULT_YAW
        private set
    var pitch = DEFAULT_PITCH
        private set
    var zoom = 1f
        private set

    private var yawVelocity = 0f
    private var pitchVelocity = 0f
    private var dragging = false

    private var target = Vec3(0f, 25f, 0f)
    private var desiredTarget = target
    private var radius = 50f
    private var distance = 110f
    private var aspect = 1f

    // Анимация сброса
    private var resetProgress = 1f
    private var resetFromYaw = 0f
    private var resetFromPitch = 0f
    private var resetFromZoom = 1f

    /** Новая сцена: камера плавно переезжает к её центру. */
    @Synchronized
    fun frame(focus: Vec3, radius: Float, immediate: Boolean) {
        desiredTarget = focus
        this.radius = radius
        if (immediate) {
            target = desiredTarget
            distance = desiredDistance()
        }
    }

    /** Соотношение сторон экрана: в узком (портретном) окне камера отъезжает дальше. */
    @Synchronized
    fun setAspect(value: Float) {
        aspect = value.coerceIn(0.2f, 5f)
    }

    /** Расстояние, при котором сфера радиуса [radius] целиком помещается в кадр. */
    private fun desiredDistance(): Float {
        val halfY = Math.toRadians(FOV_Y / 2.0)
        val halfX = kotlin.math.atan(kotlin.math.tan(halfY) * aspect)
        val half = minOf(halfY, halfX)
        return radius / sin(half).toFloat()
    }

    @Synchronized
    fun beginDrag() {
        dragging = true
        yawVelocity = 0f
        pitchVelocity = 0f
        resetProgress = 1f
    }

    @Synchronized
    fun drag(dxPixels: Float, dyPixels: Float) {
        yaw -= dxPixels * DEG_PER_PIXEL
        pitch = (pitch + dyPixels * DEG_PER_PIXEL * 0.8f).coerceIn(MIN_PITCH, MAX_PITCH)
    }

    /** Отпустили палец: продолжаем вращение по инерции. */
    @Synchronized
    fun endDrag(velocityXPx: Float, velocityYPx: Float) {
        dragging = false
        yawVelocity = -velocityXPx * DEG_PER_PIXEL
        pitchVelocity = velocityYPx * DEG_PER_PIXEL * 0.8f
    }

    @Synchronized
    fun releaseWithoutFling() {
        dragging = false
    }

    /** Щипок: factor > 1 — приблизить. */
    @Synchronized
    fun pinch(factor: Float) {
        if (factor <= 0f) return
        zoom = (zoom / factor).coerceIn(MIN_ZOOM, MAX_ZOOM)
        resetProgress = 1f
    }

    /** Двойной тап: плавно вернуть ракурс по умолчанию. */
    @Synchronized
    fun reset() {
        resetFromYaw = normalizeYaw(yaw)
        resetFromPitch = pitch
        resetFromZoom = zoom
        yawVelocity = 0f
        pitchVelocity = 0f
        resetProgress = 0f
    }

    /** Шаг анимации; [autoRotate] — медленное вращение «витрины». */
    @Synchronized
    fun update(dt: Float, autoRotate: Boolean) {
        if (resetProgress < 1f) {
            resetProgress = (resetProgress + dt / RESET_DURATION).coerceAtMost(1f)
            val t = Easing.inOutCubic(resetProgress)
            yaw = resetFromYaw + (DEFAULT_YAW - resetFromYaw) * t
            pitch = resetFromPitch + (DEFAULT_PITCH - resetFromPitch) * t
            zoom = resetFromZoom + (1f - resetFromZoom) * t
        } else if (!dragging) {
            yaw += yawVelocity * dt
            pitch = (pitch + pitchVelocity * dt).coerceIn(MIN_PITCH, MAX_PITCH)
            val damping = exp(-4.5f * dt)
            yawVelocity *= damping
            pitchVelocity *= damping
            if (abs(yawVelocity) < 0.5f) yawVelocity = 0f
            if (abs(pitchVelocity) < 0.5f) pitchVelocity = 0f
            if (autoRotate && yawVelocity == 0f) yaw += AUTO_ROTATE_SPEED * dt
        }
        // Плавное приближение к цели сцены
        val k = 1f - exp(-6f * dt)
        target = target.lerp(desiredTarget, k)
        distance += (desiredDistance() - distance) * k
    }

    @Synchronized
    fun eye(): Vec3 {
        val yawR = Math.toRadians(yaw.toDouble())
        val pitchR = Math.toRadians(pitch.toDouble())
        val d = distance * zoom
        return Vec3(
            target.x + (d * cos(pitchR) * sin(yawR)).toFloat(),
            target.y + (d * sin(pitchR)).toFloat(),
            target.z + (d * cos(pitchR) * cos(yawR)).toFloat(),
        )
    }

    @Synchronized
    fun viewMatrix(): FloatArray = Mat4.lookAt(eye(), target)

    @Synchronized
    fun currentDistance(): Float = distance * zoom

    private fun normalizeYaw(value: Float): Float {
        // Приводим к ближайшему эквиваленту DEFAULT_YAW, чтобы сброс шёл коротким путём
        var y = value
        while (y - DEFAULT_YAW > 180f) y -= 360f
        while (y - DEFAULT_YAW < -180f) y += 360f
        return y
    }

    companion object {
        const val FOV_Y = 38f
        const val DEFAULT_YAW = 58f
        const val DEFAULT_PITCH = 15f
        private const val MIN_PITCH = -8f
        private const val MAX_PITCH = 80f
        private const val MIN_ZOOM = 0.35f
        private const val MAX_ZOOM = 2.2f
        private const val DEG_PER_PIXEL = 0.32f
        private const val RESET_DURATION = 0.6f
        private const val AUTO_ROTATE_SPEED = 9f
    }
}
