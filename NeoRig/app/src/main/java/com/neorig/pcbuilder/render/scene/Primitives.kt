package com.neorig.pcbuilder.render.scene

import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * Базовые примитивы. Все «круглые» примитивы построены вокруг оси Y,
 * размеры единичные — нужный размер задаётся матрицей трансформации.
 * Примитивы кэшируются (lazy), чтобы GPU-буферы переиспользовались.
 */
object Primitives {

    /** Куб 1×1×1 с центром в начале координат. */
    val box: MeshData by lazy { createBox() }

    /** Цилиндр радиуса 0.5 и высоты 1 вдоль оси Y (с крышками). */
    val cylinder: MeshData by lazy { createCylinder(32) }

    /** Тор вокруг оси Y: большой радиус 0.5, малый — 0.06 (обод вентилятора). */
    val ring: MeshData by lazy { createTorus(40, 10, 0.5f, 0.06f) }

    /** Лопасти вентилятора диаметром 1 вокруг оси Y. */
    val fanBlades: MeshData by lazy { createFanBlades(9) }

    /** Квадрат 1×1 в плоскости XY, нормаль +Z. */
    val quad: MeshData by lazy {
        MeshData(
            positions = floatArrayOf(-0.5f, -0.5f, 0f, 0.5f, -0.5f, 0f, 0.5f, 0.5f, 0f, -0.5f, 0.5f, 0f),
            normals = floatArrayOf(0f, 0f, 1f, 0f, 0f, 1f, 0f, 0f, 1f, 0f, 0f, 1f),
            colors = null,
            indices = intArrayOf(0, 1, 2, 0, 2, 3),
        )
    }

    /** Квадрат в плоскости XY с цветами по углам (градиент): нижний-левый, нижний-правый, верхний-правый, верхний-левый. */
    fun gradientQuad(bl: Rgba, br: Rgba, tr: Rgba, tl: Rgba): MeshData = MeshData(
        positions = quad.positions,
        normals = quad.normals,
        colors = floatArrayOf(
            bl.r, bl.g, bl.b, bl.a, br.r, br.g, br.b, br.a,
            tr.r, tr.g, tr.b, tr.a, tl.r, tl.g, tl.b, tl.a,
        ),
        indices = quad.indices,
    )

    /**
     * «Мягкий» прямоугольник для тени: внутренняя часть непрозрачная,
     * к краям альфа плавно уходит в 0. Лежит в плоскости XZ, нормаль +Y.
     * @param inner доля размера, которая остаётся полностью тёмной (0..1)
     */
    fun softRect(inner: Float = 0.55f): MeshData {
        val h = 0.5f
        val i = 0.5f * inner
        val positions = floatArrayOf(
            -i, 0f, -i, i, 0f, -i, i, 0f, i, -i, 0f, i, // внутренние
            -h, 0f, -h, h, 0f, -h, h, 0f, h, -h, 0f, h, // внешние
        )
        val normals = FloatArray(24) { if (it % 3 == 1) 1f else 0f }
        val colors = FloatArray(32) { idx ->
            val vertex = idx / 4
            val channel = idx % 4
            if (channel == 3) (if (vertex < 4) 1f else 0f) else 1f
        }
        val indices = intArrayOf(
            0, 1, 2, 0, 2, 3,
            4, 5, 1, 4, 1, 0,
            5, 6, 2, 5, 2, 1,
            6, 7, 3, 6, 3, 2,
            7, 4, 0, 7, 0, 3,
        )
        return MeshData(positions, normals, colors, indices)
    }

    private fun createBox(): MeshData {
        val p = ArrayList<Float>()
        val n = ArrayList<Float>()
        val idx = ArrayList<Int>()
        // Каждая грань: нормаль + два касательных направления
        val faces = listOf(
            Triple(Vec3(1f, 0f, 0f), Vec3(0f, 0f, -1f), Vec3(0f, 1f, 0f)),
            Triple(Vec3(-1f, 0f, 0f), Vec3(0f, 0f, 1f), Vec3(0f, 1f, 0f)),
            Triple(Vec3(0f, 1f, 0f), Vec3(1f, 0f, 0f), Vec3(0f, 0f, -1f)),
            Triple(Vec3(0f, -1f, 0f), Vec3(1f, 0f, 0f), Vec3(0f, 0f, 1f)),
            Triple(Vec3(0f, 0f, 1f), Vec3(1f, 0f, 0f), Vec3(0f, 1f, 0f)),
            Triple(Vec3(0f, 0f, -1f), Vec3(-1f, 0f, 0f), Vec3(0f, 1f, 0f)),
        )
        for ((normal, u, v) in faces) {
            val base = p.size / 3
            val center = normal * 0.5f
            for ((su, sv) in listOf(-1f to -1f, 1f to -1f, 1f to 1f, -1f to 1f)) {
                val pos = center + u * (0.5f * su) + v * (0.5f * sv)
                p += listOf(pos.x, pos.y, pos.z)
                n += listOf(normal.x, normal.y, normal.z)
            }
            idx += listOf(base, base + 1, base + 2, base, base + 2, base + 3)
        }
        return MeshData(p.toFloatArray(), n.toFloatArray(), null, idx.toIntArray())
    }

    private fun createCylinder(segments: Int): MeshData {
        val p = ArrayList<Float>()
        val n = ArrayList<Float>()
        val idx = ArrayList<Int>()
        // Боковая поверхность
        for (i in 0..segments) {
            val a = 2.0 * PI * i / segments
            val x = cos(a).toFloat(); val z = sin(a).toFloat()
            p += listOf(x * 0.5f, -0.5f, z * 0.5f, x * 0.5f, 0.5f, z * 0.5f)
            n += listOf(x, 0f, z, x, 0f, z)
        }
        for (i in 0 until segments) {
            val a = i * 2; val b = a + 1; val c = a + 2; val d = a + 3
            idx += listOf(a, b, c, c, b, d)
        }
        // Крышки
        for (cap in listOf(-0.5f, 0.5f)) {
            val centerIndex = p.size / 3
            p += listOf(0f, cap, 0f)
            n += listOf(0f, if (cap > 0) 1f else -1f, 0f)
            for (i in 0..segments) {
                val a = 2.0 * PI * i / segments
                p += listOf(cos(a).toFloat() * 0.5f, cap, sin(a).toFloat() * 0.5f)
                n += listOf(0f, if (cap > 0) 1f else -1f, 0f)
            }
            for (i in 0 until segments) {
                val a = centerIndex + 1 + i
                if (cap > 0) idx += listOf(centerIndex, a + 1, a) else idx += listOf(centerIndex, a, a + 1)
            }
        }
        return MeshData(p.toFloatArray(), n.toFloatArray(), null, idx.toIntArray())
    }

    private fun createTorus(major: Int, minor: Int, radius: Float, tube: Float): MeshData {
        val p = ArrayList<Float>()
        val n = ArrayList<Float>()
        val idx = ArrayList<Int>()
        for (i in 0..major) {
            val u = 2.0 * PI * i / major
            val cu = cos(u).toFloat(); val su = sin(u).toFloat()
            for (j in 0..minor) {
                val v = 2.0 * PI * j / minor
                val cv = cos(v).toFloat(); val sv = sin(v).toFloat()
                val r = radius + tube * cv
                p += listOf(r * cu, tube * sv, r * su)
                n += listOf(cv * cu, sv, cv * su)
            }
        }
        val stride = minor + 1
        for (i in 0 until major) for (j in 0 until minor) {
            val a = i * stride + j
            val b = (i + 1) * stride + j
            // Обход против часовой стрелки снаружи (как требует glTF и наш шейдер)
            idx += listOf(a, a + 1, b, a + 1, b + 1, b)
        }
        return MeshData(p.toFloatArray(), n.toFloatArray(), null, idx.toIntArray())
    }

    private fun createFanBlades(count: Int): MeshData {
        val builder = MeshBuilder()
        for (i in 0 until count) {
            val angle = 360f * i / count
            // Лопасть — тонкая пластина, развёрнутая под углом атаки
            val blade = Mat4.chain(
                Mat4.rotation(angle, Vec3.Y),
                Mat4.translation(0.29f, 0f, 0f),
                Mat4.rotation(28f, Vec3.X),
                Mat4.rotation(-12f, Vec3.Y),
                Mat4.scale(0.27f, 0.02f, 0.15f),
            )
            builder.add(box, blade)
        }
        // Ступица
        builder.add(cylinder, Mat4.scale(0.34f, 0.12f, 0.34f))
        return builder.build()
    }
}
