package com.neorig.pcbuilder.render.scene

/**
 * Геометрия в памяти (до загрузки в GPU).
 * @property positions xyz-координаты вершин
 * @property normals   нормали (xyz)
 * @property colors    цвета вершин RGBA или null (тогда цвет — белый)
 * @property indices   треугольники
 */
class MeshData(
    val positions: FloatArray,
    val normals: FloatArray,
    val colors: FloatArray?,
    val indices: IntArray,
) {
    val vertexCount: Int get() = positions.size / 3

    /** Габариты: пара (min, max). */
    fun bounds(): Pair<Vec3, Vec3> {
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE; var minZ = Float.MAX_VALUE
        var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE; var maxZ = -Float.MAX_VALUE
        for (i in 0 until vertexCount) {
            val x = positions[i * 3]; val y = positions[i * 3 + 1]; val z = positions[i * 3 + 2]
            if (x < minX) minX = x; if (y < minY) minY = y; if (z < minZ) minZ = z
            if (x > maxX) maxX = x; if (y > maxY) maxY = y; if (z > maxZ) maxZ = z
        }
        return Vec3(minX, minY, minZ) to Vec3(maxX, maxY, maxZ)
    }
}

/**
 * Сборщик мешей: позволяет склеить много примитивов (например, 60 рёбер радиатора)
 * в один меш, чтобы рисовать их одним вызовом.
 */
class MeshBuilder {
    private val positions = ArrayList<Float>()
    private val normals = ArrayList<Float>()
    private val colors = ArrayList<Float>()
    private val indices = ArrayList<Int>()
    private var hasColors = false

    /** Добавляет [mesh], преобразованный матрицей [transform], с цветом вершин [color]. */
    fun add(mesh: MeshData, transform: FloatArray = Mat4.identity(), color: Rgba? = null): MeshBuilder {
        val base = positions.size / 3
        val normalMat = Mat4.normalMatrix(transform)
        for (i in 0 until mesh.vertexCount) {
            val p = Mat4.transformPoint(transform, Vec3(mesh.positions[i * 3], mesh.positions[i * 3 + 1], mesh.positions[i * 3 + 2]))
            positions.add(p.x); positions.add(p.y); positions.add(p.z)
            val nx = mesh.normals[i * 3]; val ny = mesh.normals[i * 3 + 1]; val nz = mesh.normals[i * 3 + 2]
            val n = Vec3(
                normalMat[0] * nx + normalMat[3] * ny + normalMat[6] * nz,
                normalMat[1] * nx + normalMat[4] * ny + normalMat[7] * nz,
                normalMat[2] * nx + normalMat[5] * ny + normalMat[8] * nz,
            ).normalized()
            normals.add(n.x); normals.add(n.y); normals.add(n.z)
            val c = when {
                color != null -> floatArrayOf(color.r, color.g, color.b, color.a)
                mesh.colors != null -> floatArrayOf(
                    mesh.colors[i * 4], mesh.colors[i * 4 + 1], mesh.colors[i * 4 + 2], mesh.colors[i * 4 + 3],
                )
                else -> floatArrayOf(1f, 1f, 1f, 1f)
            }
            c.forEach { colors.add(it) }
        }
        if (color != null || mesh.colors != null) hasColors = true
        mesh.indices.forEach { indices.add(base + it) }
        return this
    }

    val isEmpty: Boolean get() = indices.isEmpty()

    fun build(): MeshData = MeshData(
        positions.toFloatArray(),
        normals.toFloatArray(),
        if (hasColors) colors.toFloatArray() else null,
        indices.toIntArray(),
    )
}
