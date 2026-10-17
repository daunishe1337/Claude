package com.neorig.pcbuilder.render.scene

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.floatOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Ошибка формата .glb — сцена перехватывает её и рисует упрощённую геометрию. */
class GlbFormatException(message: String) : Exception(message)

/** Материал из glTF (PBR metallic-roughness, без текстур). */
data class GlbMaterial(
    val name: String,
    val baseColor: Rgba,
    val metallic: Float,
    val roughness: Float,
    val emissive: Rgba,
    val blend: Boolean,
)

/** Примитив модели: геометрия + мировая матрица узла + материал. */
class GlbPrimitive(
    val mesh: MeshData,
    val nodeMatrix: FloatArray,
    val material: GlbMaterial,
    val nodeName: String,
)

/** Загруженная модель: плоский список примитивов. */
class GlbModel(val primitives: List<GlbPrimitive>) {

    /** Габариты модели с учётом матриц узлов. */
    fun bounds(): Pair<Vec3, Vec3> {
        var min = Vec3(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE)
        var max = Vec3(-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE)
        for (prim in primitives) {
            val m = prim.nodeMatrix
            val pos = prim.mesh.positions
            for (i in 0 until prim.mesh.vertexCount) {
                val p = Mat4.transformPoint(m, Vec3(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]))
                min = Vec3(minOf(min.x, p.x), minOf(min.y, p.y), minOf(min.z, p.z))
                max = Vec3(maxOf(max.x, p.x), maxOf(max.y, p.y), maxOf(max.z, p.z))
            }
        }
        return min to max
    }
}

/**
 * Минимальный загрузчик бинарного glTF 2.0 (.glb).
 *
 * Поддерживается: меши из треугольников, POSITION / NORMAL / COLOR_0, индексы (u8/u16/u32),
 * иерархия узлов (matrix или TRS), материалы baseColorFactor / metallic / roughness /
 * emissiveFactor / alphaMode. Текстуры игнорируются (цвет берётся из фактора или из детали).
 * Не поддерживается: Draco/meshopt-сжатие, sparse-аксессоры, анимации, скиннинг.
 */
object GlbLoader {

    private const val MAGIC = 0x46546C67 // "glTF"
    private const val CHUNK_JSON = 0x4E4F534A
    private const val CHUNK_BIN = 0x004E4942
    private val SUPPORTED_EXTENSIONS = setOf("KHR_materials_emissive_strength", "KHR_materials_unlit")

    private val json = Json { ignoreUnknownKeys = true }

    fun parse(bytes: ByteArray): GlbModel {
        if (bytes.size < 20) throw GlbFormatException("Файл слишком короткий")
        val header = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        if (header.getInt(0) != MAGIC) throw GlbFormatException("Это не .glb")
        if (header.getInt(4) != 2) throw GlbFormatException("Нужен glTF 2.0")
        val total = minOf(header.getInt(8), bytes.size)

        var jsonText: String? = null
        var bin: ByteBuffer? = null
        var offset = 12
        while (offset + 8 <= total) {
            val length = header.getInt(offset)
            val type = header.getInt(offset + 4)
            val start = offset + 8
            if (length < 0 || start + length > total) throw GlbFormatException("Повреждённый чанк")
            when (type) {
                CHUNK_JSON -> jsonText = String(bytes, start, length, Charsets.UTF_8)
                CHUNK_BIN -> bin = ByteBuffer.wrap(bytes, start, length).slice().order(ByteOrder.LITTLE_ENDIAN)
            }
            offset = start + length
        }
        val root = json.parseToJsonElement(jsonText ?: throw GlbFormatException("Нет JSON-чанка")).jsonObject

        val required = root["extensionsRequired"]?.jsonArray?.map { it.jsonPrimitive.content }.orEmpty()
        val unsupported = required.filterNot { it in SUPPORTED_EXTENSIONS }
        if (unsupported.isNotEmpty()) throw GlbFormatException("Не поддерживается: $unsupported")

        return Parser(root, bin).parse()
    }

    private class Parser(private val root: JsonObject, private val bin: ByteBuffer?) {

        private val accessors = root.array("accessors")
        private val bufferViews = root.array("bufferViews")
        private val meshes = root.array("meshes")
        private val nodes = root.array("nodes")
        private val materials = root.array("materials").map { parseMaterial(it.jsonObject) }
        private val result = ArrayList<GlbPrimitive>()

        fun parse(): GlbModel {
            val scenes = root.array("scenes")
            val sceneIndex = root["scene"]?.jsonPrimitive?.intOrNull ?: 0
            val roots: List<Int> = if (scenes.isNotEmpty()) {
                scenes.getOrNull(sceneIndex)?.jsonObject?.array("nodes")?.map { it.jsonPrimitive.intOrNull ?: 0 }.orEmpty()
            } else {
                // Сцен нет — берём узлы, которые не являются чьими-то детьми
                val children = nodes.flatMap { node -> node.jsonObject.array("children").map { it.jsonPrimitive.intOrNull ?: -1 } }.toSet()
                nodes.indices.filter { it !in children }
            }
            roots.forEach { visit(it, Mat4.identity(), depth = 0) }
            if (result.isEmpty()) throw GlbFormatException("В модели нет треугольников")
            return GlbModel(result)
        }

        private fun visit(index: Int, parent: FloatArray, depth: Int) {
            if (depth > 64) throw GlbFormatException("Слишком глубокая иерархия")
            val node = nodes.getOrNull(index)?.jsonObject ?: return
            val world = Mat4.multiply(parent, localMatrix(node))
            val name = node["name"]?.jsonPrimitive?.content ?: "node$index"
            node["mesh"]?.jsonPrimitive?.intOrNull?.let { meshIndex ->
                val mesh = meshes.getOrNull(meshIndex)?.jsonObject ?: return@let
                for (prim in mesh.array("primitives")) readPrimitive(prim.jsonObject, world, name)
            }
            node.array("children").forEach { visit(it.jsonPrimitive.intOrNull ?: -1, world, depth + 1) }
        }

        private fun localMatrix(node: JsonObject): FloatArray {
            node["matrix"]?.jsonArray?.let { arr ->
                if (arr.size == 16) return FloatArray(16) { arr[it].jsonPrimitive.floatOrNull ?: 0f }
            }
            val t = node.floats("translation") ?: floatArrayOf(0f, 0f, 0f)
            val r = node.floats("rotation") ?: floatArrayOf(0f, 0f, 0f, 1f)
            val s = node.floats("scale") ?: floatArrayOf(1f, 1f, 1f)
            return Mat4.chain(
                Mat4.translation(t[0], t[1], t[2]),
                Mat4.fromQuaternion(r[0], r[1], r[2], r[3]),
                Mat4.scale(s[0], s[1], s[2]),
            )
        }

        private fun readPrimitive(prim: JsonObject, world: FloatArray, nodeName: String) {
            val mode = prim["mode"]?.jsonPrimitive?.intOrNull ?: 4
            if (mode != 4) return // только треугольники
            val attributes = prim["attributes"]?.jsonObject ?: return
            val posIndex = attributes["POSITION"]?.jsonPrimitive?.intOrNull ?: return
            val positions = readFloats(posIndex, expectedComponents = 3)
            val vertexCount = positions.size / 3
            val indices = prim["indices"]?.jsonPrimitive?.intOrNull?.let { readIndices(it) }
                ?: IntArray(vertexCount) { it }
            if (indices.any { it < 0 || it >= vertexCount }) throw GlbFormatException("Индекс вне диапазона")
            val normals = attributes["NORMAL"]?.jsonPrimitive?.intOrNull
                ?.let { readFloats(it, expectedComponents = 3) }
                ?.takeIf { it.size == positions.size }
                ?: computeNormals(positions, indices)
            val colors = attributes["COLOR_0"]?.jsonPrimitive?.intOrNull?.let { readColors(it, vertexCount) }
            val material = prim["material"]?.jsonPrimitive?.intOrNull?.let { materials.getOrNull(it) } ?: DEFAULT_MATERIAL
            result += GlbPrimitive(MeshData(positions, normals, colors, indices), world, material, nodeName)
        }

        // ---- Чтение аксессоров -------------------------------------------------------------

        private class View(val data: ByteBuffer, val offset: Int, val stride: Int, val count: Int, val components: Int, val componentType: Int, val normalized: Boolean)

        private fun view(accessorIndex: Int): View {
            val accessor = accessors.getOrNull(accessorIndex)?.jsonObject ?: throw GlbFormatException("Нет аксессора $accessorIndex")
            if (accessor.containsKey("sparse")) throw GlbFormatException("Sparse-аксессоры не поддерживаются")
            val buffer = bin ?: throw GlbFormatException("Нет бинарного чанка")
            val viewIndex = accessor["bufferView"]?.jsonPrimitive?.intOrNull ?: throw GlbFormatException("Аксессор без bufferView")
            val bufferView = bufferViews.getOrNull(viewIndex)?.jsonObject ?: throw GlbFormatException("Нет bufferView")
            if ((bufferView["buffer"]?.jsonPrimitive?.intOrNull ?: 0) != 0) throw GlbFormatException("Внешние буферы не поддерживаются")
            val componentType = accessor["componentType"]?.jsonPrimitive?.intOrNull ?: 5126
            val components = when (accessor["type"]?.jsonPrimitive?.content) {
                "SCALAR" -> 1; "VEC2" -> 2; "VEC3" -> 3; "VEC4" -> 4; "MAT4" -> 16
                else -> throw GlbFormatException("Неизвестный тип аксессора")
            }
            val componentSize = when (componentType) {
                5120, 5121 -> 1; 5122, 5123 -> 2; 5125, 5126 -> 4
                else -> throw GlbFormatException("Неизвестный componentType $componentType")
            }
            val offset = (bufferView["byteOffset"]?.jsonPrimitive?.intOrNull ?: 0) +
                (accessor["byteOffset"]?.jsonPrimitive?.intOrNull ?: 0)
            val stride = bufferView["byteStride"]?.jsonPrimitive?.intOrNull ?: (componentSize * components)
            val count = accessor["count"]?.jsonPrimitive?.intOrNull ?: 0
            val lastByte = offset + stride * (count - 1).coerceAtLeast(0) + componentSize * components
            if (count > 0 && lastByte > buffer.limit()) throw GlbFormatException("Аксессор выходит за буфер")
            val normalized = accessor["normalized"]?.jsonPrimitive?.booleanOrNull ?: false
            return View(buffer, offset, stride, count, components, componentType, normalized)
        }

        private fun component(v: View, element: Int, c: Int): Float {
            val pos = v.offset + element * v.stride
            return when (v.componentType) {
                5126 -> v.data.getFloat(pos + c * 4)
                5121 -> (v.data.get(pos + c).toInt() and 0xFF).let { if (v.normalized) it / 255f else it.toFloat() }
                5120 -> v.data.get(pos + c).toInt().let { if (v.normalized) maxOf(it / 127f, -1f) else it.toFloat() }
                5123 -> (v.data.getShort(pos + c * 2).toInt() and 0xFFFF).let { if (v.normalized) it / 65535f else it.toFloat() }
                5122 -> v.data.getShort(pos + c * 2).toInt().let { if (v.normalized) maxOf(it / 32767f, -1f) else it.toFloat() }
                5125 -> v.data.getInt(pos + c * 4).toFloat()
                else -> 0f
            }
        }

        private fun readFloats(accessorIndex: Int, expectedComponents: Int): FloatArray {
            val v = view(accessorIndex)
            if (v.components != expectedComponents) throw GlbFormatException("Ожидали $expectedComponents компоненты")
            val out = FloatArray(v.count * v.components)
            for (i in 0 until v.count) for (c in 0 until v.components) out[i * v.components + c] = component(v, i, c)
            return out
        }

        private fun readIndices(accessorIndex: Int): IntArray {
            val v = view(accessorIndex)
            return IntArray(v.count) { i ->
                val pos = v.offset + i * v.stride
                when (v.componentType) {
                    5121 -> v.data.get(pos).toInt() and 0xFF
                    5123 -> v.data.getShort(pos).toInt() and 0xFFFF
                    5125 -> v.data.getInt(pos)
                    else -> throw GlbFormatException("Неверный тип индексов")
                }
            }
        }

        private fun readColors(accessorIndex: Int, vertexCount: Int): FloatArray? {
            val v = view(accessorIndex)
            if (v.count != vertexCount || (v.components != 3 && v.components != 4)) return null
            val out = FloatArray(vertexCount * 4)
            for (i in 0 until vertexCount) {
                for (c in 0 until 3) out[i * 4 + c] = component(v, i, c)
                out[i * 4 + 3] = if (v.components == 4) component(v, i, 3) else 1f
            }
            return out
        }

        // ---- Материалы ---------------------------------------------------------------------

        private fun parseMaterial(obj: JsonObject): GlbMaterial {
            val pbr = obj["pbrMetallicRoughness"]?.jsonObject
            val base = pbr?.floats("baseColorFactor") ?: floatArrayOf(1f, 1f, 1f, 1f)
            val emissive = obj.floats("emissiveFactor") ?: floatArrayOf(0f, 0f, 0f)
            val strength = obj["extensions"]?.jsonObject?.get("KHR_materials_emissive_strength")?.jsonObject
                ?.get("emissiveStrength")?.jsonPrimitive?.floatOrNull ?: 1f
            return GlbMaterial(
                name = obj["name"]?.jsonPrimitive?.content ?: "",
                // glTF хранит цвета уже в линейном пространстве
                baseColor = Rgba(base[0], base[1], base[2], base.getOrElse(3) { 1f }),
                metallic = pbr?.get("metallicFactor")?.jsonPrimitive?.floatOrNull ?: 1f,
                roughness = pbr?.get("roughnessFactor")?.jsonPrimitive?.floatOrNull ?: 1f,
                emissive = Rgba(emissive[0] * strength, emissive[1] * strength, emissive[2] * strength),
                blend = obj["alphaMode"]?.jsonPrimitive?.content == "BLEND",
            )
        }

        private fun computeNormals(positions: FloatArray, indices: IntArray): FloatArray {
            val normals = FloatArray(positions.size)
            for (t in 0 until indices.size / 3) {
                val ia = indices[t * 3]; val ib = indices[t * 3 + 1]; val ic = indices[t * 3 + 2]
                val a = Vec3(positions[ia * 3], positions[ia * 3 + 1], positions[ia * 3 + 2])
                val b = Vec3(positions[ib * 3], positions[ib * 3 + 1], positions[ib * 3 + 2])
                val c = Vec3(positions[ic * 3], positions[ic * 3 + 1], positions[ic * 3 + 2])
                val n = (b - a).cross(c - a)
                for (i in intArrayOf(ia, ib, ic)) {
                    normals[i * 3] += n.x; normals[i * 3 + 1] += n.y; normals[i * 3 + 2] += n.z
                }
            }
            for (i in 0 until normals.size / 3) {
                val n = Vec3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]).normalized()
                normals[i * 3] = n.x; normals[i * 3 + 1] = n.y; normals[i * 3 + 2] = n.z
            }
            return normals
        }
    }

    private val DEFAULT_MATERIAL = GlbMaterial("default", Rgba(0.8f, 0.8f, 0.8f), 0f, 0.6f, Rgba.BLACK, false)

    private fun JsonObject.array(key: String): List<JsonElement> = (this[key] as? JsonArray).orEmpty()

    private fun JsonObject.floats(key: String): FloatArray? =
        (this[key] as? JsonArray)?.let { arr -> FloatArray(arr.size) { arr[it].jsonPrimitive.floatOrNull ?: 0f } }
}
