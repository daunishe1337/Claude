package com.neorig.pcbuilder.render.scene

import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tan

/** Трёхмерный вектор (единицы сцены — сантиметры). */
data class Vec3(val x: Float, val y: Float, val z: Float) {
    operator fun plus(o: Vec3) = Vec3(x + o.x, y + o.y, z + o.z)
    operator fun minus(o: Vec3) = Vec3(x - o.x, y - o.y, z - o.z)
    operator fun times(s: Float) = Vec3(x * s, y * s, z * s)
    fun dot(o: Vec3) = x * o.x + y * o.y + z * o.z
    fun cross(o: Vec3) = Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x)
    fun length() = sqrt(dot(this))
    fun normalized(): Vec3 {
        val l = length()
        return if (l < 1e-6f) this else this * (1f / l)
    }

    fun lerp(to: Vec3, t: Float) = Vec3(x + (to.x - x) * t, y + (to.y - y) * t, z + (to.z - z) * t)

    companion object {
        val ZERO = Vec3(0f, 0f, 0f)
        val X = Vec3(1f, 0f, 0f)
        val Y = Vec3(0f, 1f, 0f)
        val Z = Vec3(0f, 0f, 1f)
    }
}

/**
 * Операции с матрицами 4×4 в формате OpenGL (по столбцам, column-major).
 * Своя реализация, чтобы код сцены не зависел от Android и тестировался на JVM.
 */
object Mat4 {

    fun identity(): FloatArray = FloatArray(16).also { it[0] = 1f; it[5] = 1f; it[10] = 1f; it[15] = 1f }

    fun translation(x: Float, y: Float, z: Float): FloatArray =
        identity().also { it[12] = x; it[13] = y; it[14] = z }

    fun translation(v: Vec3): FloatArray = translation(v.x, v.y, v.z)

    fun scale(x: Float, y: Float, z: Float): FloatArray =
        FloatArray(16).also { it[0] = x; it[5] = y; it[10] = z; it[15] = 1f }

    fun scale(s: Float): FloatArray = scale(s, s, s)

    /** Поворот на [degrees] вокруг оси [axis] (ось нормализуется). */
    fun rotation(degrees: Float, axis: Vec3): FloatArray {
        val a = axis.normalized()
        val r = Math.toRadians(degrees.toDouble())
        val c = cos(r).toFloat()
        val s = sin(r).toFloat()
        val t = 1f - c
        val m = identity()
        m[0] = t * a.x * a.x + c
        m[1] = t * a.x * a.y + s * a.z
        m[2] = t * a.x * a.z - s * a.y
        m[4] = t * a.x * a.y - s * a.z
        m[5] = t * a.y * a.y + c
        m[6] = t * a.y * a.z + s * a.x
        m[8] = t * a.x * a.z + s * a.y
        m[9] = t * a.y * a.z - s * a.x
        m[10] = t * a.z * a.z + c
        return m
    }

    /** Матрица из кватерниона glTF (x, y, z, w). */
    fun fromQuaternion(x: Float, y: Float, z: Float, w: Float): FloatArray {
        val m = identity()
        m[0] = 1 - 2 * (y * y + z * z)
        m[1] = 2 * (x * y + z * w)
        m[2] = 2 * (x * z - y * w)
        m[4] = 2 * (x * y - z * w)
        m[5] = 1 - 2 * (x * x + z * z)
        m[6] = 2 * (y * z + x * w)
        m[8] = 2 * (x * z + y * w)
        m[9] = 2 * (y * z - x * w)
        m[10] = 1 - 2 * (x * x + y * y)
        return m
    }

    /** a × b. */
    fun multiply(a: FloatArray, b: FloatArray): FloatArray {
        val out = FloatArray(16)
        for (col in 0 until 4) {
            for (row in 0 until 4) {
                var sum = 0f
                for (k in 0 until 4) sum += a[k * 4 + row] * b[col * 4 + k]
                out[col * 4 + row] = sum
            }
        }
        return out
    }

    /** Произведение цепочки матриц слева направо. */
    fun chain(vararg matrices: FloatArray): FloatArray =
        matrices.reduce { acc, m -> multiply(acc, m) }

    /** Преобразование точки. */
    fun transformPoint(m: FloatArray, p: Vec3): Vec3 = Vec3(
        m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
        m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
        m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14],
    )

    /** Перспективная проекция. */
    fun perspective(fovYDegrees: Float, aspect: Float, near: Float, far: Float): FloatArray {
        val f = (1.0 / tan(Math.toRadians(fovYDegrees / 2.0))).toFloat()
        val m = FloatArray(16)
        m[0] = f / aspect
        m[5] = f
        m[10] = (far + near) / (near - far)
        m[11] = -1f
        m[14] = 2f * far * near / (near - far)
        return m
    }

    /** Матрица вида (камера в [eye] смотрит на [target]). */
    fun lookAt(eye: Vec3, target: Vec3, up: Vec3 = Vec3.Y): FloatArray {
        val f = (target - eye).normalized()
        val s = f.cross(up).normalized()
        val u = s.cross(f)
        val m = identity()
        m[0] = s.x; m[4] = s.y; m[8] = s.z
        m[1] = u.x; m[5] = u.y; m[9] = u.z
        m[2] = -f.x; m[6] = -f.y; m[10] = -f.z
        m[12] = -s.dot(eye)
        m[13] = -u.dot(eye)
        m[14] = f.dot(eye)
        return m
    }

    /**
     * Матрица нормалей 3×3 (обратная транспонированная к левому верхнему блоку 3×3).
     * Нужна, потому что у боксов неравномерный масштаб.
     */
    fun normalMatrix(m: FloatArray, out: FloatArray = FloatArray(9)): FloatArray {
        val a = m[0]; val b = m[4]; val c = m[8]
        val d = m[1]; val e = m[5]; val f = m[9]
        val g = m[2]; val h = m[6]; val i = m[10]
        // Матрица алгебраических дополнений C; (M^-1)^T = C / det
        val c00 = e * i - f * h
        val c01 = -(d * i - f * g)
        val c02 = d * h - e * g
        val c10 = -(b * i - c * h)
        val c11 = a * i - c * g
        val c12 = -(a * h - b * g)
        val c20 = b * f - c * e
        val c21 = -(a * f - c * d)
        val c22 = a * e - b * d
        val det = a * c00 + b * c01 + c * c02
        val inv = if (kotlin.math.abs(det) < 1e-12f) 0f else 1f / det
        // Записываем по столбцам (как ждёт glUniformMatrix3fv без транспонирования)
        out[0] = c00 * inv; out[1] = c10 * inv; out[2] = c20 * inv
        out[3] = c01 * inv; out[4] = c11 * inv; out[5] = c21 * inv
        out[6] = c02 * inv; out[7] = c12 * inv; out[8] = c22 * inv
        return out
    }
}

/** Функции плавности для анимаций. */
object Easing {
    fun clamp01(t: Float) = t.coerceIn(0f, 1f)
    fun outCubic(t: Float): Float { val p = 1f - clamp01(t); return 1f - p * p * p }
    fun inCubic(t: Float): Float { val p = clamp01(t); return p * p * p }
    fun inOutCubic(t: Float): Float {
        val p = clamp01(t)
        return if (p < 0.5f) 4f * p * p * p else 1f - (-2f * p + 2f).let { it * it * it } / 2f
    }

    /** Лёгкий «перелёт» в конце — деталь мягко встаёт на место. */
    fun outBack(t: Float): Float {
        val p = clamp01(t) - 1f
        val c1 = 1.4f
        return 1f + (c1 + 1f) * p * p * p + c1 * p * p
    }
}
