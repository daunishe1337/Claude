package com.neorig.pcbuilder.render.scene

import kotlin.math.abs
import kotlin.math.acos

/**
 * Удобный конструктор моделей из примитивов.
 * Все координаты — в локальной системе детали, сантиметры.
 */
class ModelBuilder {
    private val items = ArrayList<DrawItem>()
    private val glows = ArrayList<Glow>()
    private val lights = ArrayList<Vec3>()

    /** Бокс с центром [center] и размерами [size] (можно повернуть матрицей [rotation]). */
    fun box(center: Vec3, size: Vec3, material: Material, rotation: FloatArray? = null): ModelBuilder {
        val t = if (rotation == null) {
            Mat4.chain(Mat4.translation(center), Mat4.scale(size.x, size.y, size.z))
        } else {
            Mat4.chain(Mat4.translation(center), rotation, Mat4.scale(size.x, size.y, size.z))
        }
        items += DrawItem(Primitives.box, t, material)
        return this
    }

    /** Бокс, заданный двумя углами (min, max). */
    fun boxBetween(min: Vec3, max: Vec3, material: Material): ModelBuilder =
        box((min + max) * 0.5f, max - min, material)

    /** Цилиндр радиуса [radius] и длины [length] вдоль оси [axis]. */
    fun cylinder(center: Vec3, radius: Float, length: Float, axis: Vec3, material: Material): ModelBuilder {
        items += DrawItem(
            Primitives.cylinder,
            Mat4.chain(Mat4.translation(center), alignY(axis), Mat4.scale(radius * 2, length, radius * 2)),
            material,
        )
        return this
    }

    /** Произвольный меш с трансформацией. */
    fun mesh(mesh: MeshData, transform: FloatArray, material: Material, spin: Spin? = null): ModelBuilder {
        items += DrawItem(mesh, transform, material, spin)
        return this
    }

    /**
     * Вентилятор: квадратная рамка, крыльчатка (вращается) и обод.
     * @param facing направление потока воздуха
     * @param rgb    true — обод светится цветом RGB-подсветки
     */
    fun fan(
        center: Vec3,
        diameter: Float,
        facing: Vec3,
        frame: Material,
        blades: Material,
        rgb: Boolean,
        ringMaterial: Material = frame,
        speed: Float = 540f,
        thickness: Float = 2.5f,
    ): ModelBuilder {
        val align = alignY(facing)
        // Рамка: четыре планки по краям
        val half = diameter / 2f
        val bar = 0.9f
        for ((dx, dz, sx, sz) in listOf(
            Quad4(0f, half - bar / 2, diameter, bar),
            Quad4(0f, -half + bar / 2, diameter, bar),
            Quad4(half - bar / 2, 0f, bar, diameter - 2 * bar),
            Quad4(-half + bar / 2, 0f, bar, diameter - 2 * bar),
        )) {
            items += DrawItem(
                Primitives.box,
                Mat4.chain(Mat4.translation(center), align, Mat4.translation(dx, 0f, dz), Mat4.scale(sx, thickness, sz)),
                frame,
            )
        }
        // Обод (RGB-кольцо)
        items += DrawItem(
            Primitives.ring,
            Mat4.chain(Mat4.translation(center), align, Mat4.scale(diameter * 0.9f, minOf(diameter * 0.9f * 1.4f, thickness * 6f), diameter * 0.9f)),
            if (rgb) Material.rgb() else ringMaterial,
        )
        // Крыльчатка
        items += DrawItem(
            Primitives.fanBlades,
            Mat4.chain(Mat4.translation(center), align, Mat4.scale(diameter * 0.86f)),
            blades,
            Spin(Vec3.Y, speed),
        )
        if (rgb) {
            glows += Glow(center, diameter * 0.85f, 0.55f)
            lights += center
        }
        return this
    }

    fun glow(position: Vec3, size: Float, intensity: Float = 1f): ModelBuilder {
        glows += Glow(position, size, intensity)
        return this
    }

    fun light(position: Vec3): ModelBuilder {
        lights += position
        return this
    }

    /** Добавляет готовую модель со сдвигом. */
    fun include(model: PartModel, offset: Vec3): ModelBuilder {
        val shift = Mat4.translation(offset)
        model.items.forEach { items += DrawItem(it.mesh, Mat4.multiply(shift, it.transform), it.material, it.spin) }
        model.glows.forEach { glows += it.copy(position = it.position + offset) }
        model.lights.forEach { lights += it + offset }
        return this
    }

    fun build(enterOffset: Vec3 = Vec3(16f, 0f, 0f)) = PartModel(items.toList(), glows.toList(), lights.toList(), enterOffset)

    private data class Quad4(val a: Float, val b: Float, val c: Float, val d: Float)

    companion object {
        /** Поворот, переводящий ось Y в направление [axis]. */
        fun alignY(axis: Vec3): FloatArray {
            val a = axis.normalized()
            val dot = a.dot(Vec3.Y)
            if (dot > 0.9999f) return Mat4.identity()
            if (dot < -0.9999f) return Mat4.rotation(180f, Vec3.X)
            val angle = Math.toDegrees(acos(dot.coerceIn(-1f, 1f).toDouble())).toFloat()
            val rotAxis = Vec3.Y.cross(a)
            return if (abs(rotAxis.length()) < 1e-6f) Mat4.identity() else Mat4.rotation(angle, rotAxis)
        }
    }
}
