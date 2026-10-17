package com.neorig.pcbuilder.render.scene

import kotlin.math.sin

/** Режим RGB-подсветки. */
enum class RgbMode { STATIC, RAINBOW, BREATHING }

/** Состояние подсветки, которое выбирает пользователь. */
data class RgbState(
    val enabled: Boolean = true,
    val color: Rgba = Rgba.fromHex("#A855F7"),
    val mode: RgbMode = RgbMode.STATIC,
)

/** Один вызов отрисовки с уже посчитанной матрицей и цветами. */
class DrawCall(
    val mesh: MeshData,
    val model: FloatArray,
    val material: Material,
    val color: Rgba,
    val emissive: Rgba,
    val distance: Float,
)

/** Ореол подсветки в мировых координатах. */
class GlowCall(val position: Vec3, val size: Float, val color: Rgba, val intensity: Float)

/** Точечный источник от RGB-подсветки. */
class PointLight(val position: Vec3, val color: Rgba)

/** Всё, что нужно нарисовать в одном кадре. */
class FrameData(
    val opaque: List<DrawCall>,
    val transparent: List<DrawCall>,
    val glows: List<GlowCall>,
    val lights: List<PointLight>,
)

/**
 * Хранит детали сцены и анимирует их замену:
 * старая деталь «уезжает» и уменьшается, новая «прилетает» через стекло
 * с лёгким перелётом и бирюзовой вспышкой. Детали, которые не менялись,
 * плавно переезжают, если изменилась раскладка (например, новый корпус).
 */
class SceneAnimator {

    private class LivePart(
        var instance: PartInstance,
        val enterStart: Float,
        var exiting: Boolean = false,
        var exitStart: Float = 0f,
        var moveFrom: Vec3 = instance.anchor,
        var moveStart: Float = -100f,
    ) {
        fun anchorAt(now: Float): Vec3 {
            val t = (now - moveStart) / MOVE_DURATION
            return if (t >= 1f) instance.anchor else moveFrom.lerp(instance.anchor, Easing.inOutCubic(t))
        }
    }

    private val live = ArrayList<LivePart>()
    private var environment: PartModel? = null
    private var interiorLight: Vec3? = null

    val isEmpty: Boolean get() = live.isEmpty()

    /** Новая сцена. [animate] = false — показать сразу (например, при повороте экрана). */
    fun setScene(spec: SceneSpec, now: Float, animate: Boolean) {
        val firstScene = live.isEmpty()
        spec.parts.forEachIndexed { index, inst ->
            val existing = live.firstOrNull { it.instance.slot == inst.slot && !it.exiting }
            val enterAt = when {
                !animate -> now - ENTER_DURATION
                firstScene -> now + index * STAGGER
                else -> now + ENTER_DELAY
            }
            when {
                existing == null -> live += LivePart(inst, enterAt)
                existing.instance.partId == inst.partId -> {
                    val current = existing.anchorAt(now)
                    existing.instance = inst
                    if (animate && (current - inst.anchor).length() > 0.01f) {
                        existing.moveFrom = current
                        existing.moveStart = now
                    } else {
                        existing.moveFrom = inst.anchor
                        existing.moveStart = -100f
                    }
                }
                else -> {
                    existing.exiting = true
                    existing.exitStart = now
                    live += LivePart(inst, enterAt)
                }
            }
        }
        for (part in live) {
            if (!part.exiting && spec.parts.none { it.slot == part.instance.slot }) {
                part.exiting = true
                part.exitStart = now
            }
        }
        environment = spec.environment
        interiorLight = spec.interiorLight
    }

    /** Готовит кадр: матрицы с анимацией, цвета RGB, сортировку прозрачных объектов. */
    fun frame(now: Float, rgb: RgbState, eye: Vec3): FrameData {
        live.removeAll { it.exiting && now - it.exitStart > EXIT_DURATION }

        val opaque = ArrayList<DrawCall>(256)
        val transparent = ArrayList<DrawCall>(16)
        val glows = ArrayList<GlowCall>(32)
        val lightCandidates = ArrayList<Pair<Int, Vec3>>()

        environment?.let { env ->
            emit(env, Mat4.identity(), now, rgb, eye, Rgba.BLACK, opaque, transparent)
        }

        for (part in live) {
            if (now < part.enterStart) continue
            val anchor = part.anchorAt(now)
            val model = part.instance.model
            var scale = 1f
            var offset = Vec3.ZERO
            var highlight = Rgba.BLACK
            if (part.exiting) {
                val t = Easing.clamp01((now - part.exitStart) / EXIT_DURATION)
                scale = 1f - Easing.inCubic(t) * 0.999f
                offset = model.enterOffset * (Easing.inCubic(t) * 0.6f)
            } else {
                val t = Easing.clamp01((now - part.enterStart) / ENTER_DURATION)
                if (t < 1f) {
                    scale = 0.8f + 0.2f * Easing.outBack(t)
                    offset = model.enterOffset * (1f - Easing.outCubic(t))
                    highlight = HIGHLIGHT * ((1f - t) * 0.45f)
                }
            }
            val partMatrix = Mat4.chain(Mat4.translation(anchor + offset), Mat4.scale(scale))
            emit(model, partMatrix, now, rgb, eye, highlight, opaque, transparent)

            if (rgb.enabled && !part.exiting) {
                for (glow in model.glows) {
                    val pos = Mat4.transformPoint(partMatrix, glow.position)
                    glows += GlowCall(pos, glow.size * scale, rgbColorAt(rgb, now, pos), glow.intensity * rgbIntensity(rgb, now))
                }
                model.lights.firstOrNull()?.let {
                    lightCandidates += lightPriority(part.instance.slot) to Mat4.transformPoint(partMatrix, it)
                }
            }
        }

        transparent.sortByDescending { it.distance }
        val lights = ArrayList<PointLight>(MAX_LIGHTS)
        interiorLight?.let { lights += PointLight(it, INTERIOR_LIGHT) }
        lightCandidates.sortedBy { it.first }.take(MAX_LIGHTS - lights.size).forEach { (_, pos) ->
            lights += PointLight(pos, rgbColorAt(rgb, now, pos) * (1.4f * rgbIntensity(rgb, now)))
        }
        return FrameData(opaque, transparent, glows, lights)
    }

    private fun emit(
        model: PartModel,
        partMatrix: FloatArray,
        now: Float,
        rgb: RgbState,
        eye: Vec3,
        highlight: Rgba,
        opaque: MutableList<DrawCall>,
        transparent: MutableList<DrawCall>,
    ) {
        for (item in model.items) {
            var matrix = Mat4.multiply(partMatrix, item.transform)
            item.spin?.let { spin -> matrix = Mat4.multiply(matrix, Mat4.rotation((now * spin.degreesPerSecond) % 360f, spin.axis)) }
            val material = item.material
            var color = material.color
            var emissive = material.emissive
            if (material.role == MaterialRole.RGB) {
                if (rgb.enabled) {
                    val center = Vec3(matrix[12], matrix[13], matrix[14])
                    val c = rgbColorAt(rgb, now, center)
                    val k = rgbIntensity(rgb, now)
                    color = c * 0.3f
                    emissive = c * (2.4f * k)
                } else {
                    color = RGB_OFF
                    emissive = Rgba.BLACK
                }
            }
            if (highlight.r + highlight.g + highlight.b > 0f && material.role != MaterialRole.SHADOW) {
                emissive = Rgba(emissive.r + highlight.r, emissive.g + highlight.g, emissive.b + highlight.b)
            }
            val center = Vec3(matrix[12], matrix[13], matrix[14])
            val call = DrawCall(item.mesh, matrix, material, color, emissive, (center - eye).length())
            if (material.role == MaterialRole.GLASS || material.role == MaterialRole.SHADOW) transparent += call else opaque += call
        }
    }

    private fun rgbColorAt(rgb: RgbState, now: Float, position: Vec3): Rgba = when (rgb.mode) {
        RgbMode.RAINBOW -> Rgba.fromHsv(now * 70f + position.y * 5f + position.z * 2f, 0.85f, 1f)
        else -> rgb.color
    }

    private fun rgbIntensity(rgb: RgbState, now: Float): Float = when (rgb.mode) {
        RgbMode.BREATHING -> 0.2f + 0.8f * (0.5f + 0.5f * sin(now * 2.4f))
        else -> 1f
    }

    private fun lightPriority(slot: String): Int = LIGHT_PRIORITY.indexOf(slot).let { if (it < 0) 99 else it }

    companion object {
        const val ENTER_DURATION = 0.7f
        const val ENTER_DELAY = 0.18f
        const val EXIT_DURATION = 0.32f
        const val MOVE_DURATION = 0.55f
        const val STAGGER = 0.07f
        const val MAX_LIGHTS = 4
        private val HIGHLIGHT = Rgba(0.1f, 0.85f, 0.95f)
        private val RGB_OFF = Rgba.fromHex("#1C1D22")
        private val INTERIOR_LIGHT = Rgba(0.5f, 0.5f, 0.56f)
        private val LIGHT_PRIORITY = listOf("GPU", "COOLER", "RAM", "CASE", "MOTHERBOARD", "PSU", "SSD")
    }
}
