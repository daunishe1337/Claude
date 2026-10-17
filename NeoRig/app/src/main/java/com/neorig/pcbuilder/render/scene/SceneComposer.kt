package com.neorig.pcbuilder.render.scene

import com.neorig.pcbuilder.domain.model.Category
import com.neorig.pcbuilder.domain.model.Part
import com.neorig.pcbuilder.domain.model.PcBuild
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * Превращает сборку в 3D-сцену: раскладывает детали внутри корпуса,
 * ставит рядом монитор и стол.
 *
 * Для деталей с полем `visual.model` пытается загрузить .glb из assets/models.
 * Если файла нет или он повреждён — используется процедурная модель из [PartModels],
 * поэтому приложение никогда не падает из-за отсутствующей модели.
 *
 * @param modelSource возвращает байты файла «models/имя.glb» или null, если файла нет
 * @param log         куда писать предупреждения (в приложении — Logcat)
 */
class SceneComposer(
    private val modelSource: (path: String) -> ByteArray?,
    private val log: (String) -> Unit = {},
) {
    private val glbCache = HashMap<String, GlbModel?>()
    private val modelCache = object : LinkedHashMap<String, PartModel>(32, 0.75f, true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<String, PartModel>?) = size > 48
    }

    /** Сколько деталей нарисовано из .glb (для отладки и тестов). */
    var lastGlbCount: Int = 0
        private set

    @Synchronized
    fun compose(build: PcBuild): SceneSpec {
        lastGlbCount = 0
        val case = build[Category.CASE]
        val dims = PartModels.caseDims(case?.visual?.style ?: "mid")
        val itx = case?.visual?.style == "itx"
        val board = build[Category.MOTHERBOARD]
        val layout = BoardLayout.of(board?.compat?.formFactor)
        val psu = build[Category.PSU]
        val psuLength = psu?.let { PartModels.psuLength(it) } ?: 14f

        // Опорные точки внутри корпуса
        val boardX = -dims.width / 2 + 1.6f
        val boardTop = dims.height - 3.5f
        val boardRear = -dims.depth / 2 + 2.5f
        val socket = Vec3(boardX, boardTop - layout.socketV, boardRear + layout.socketU)

        val parts = ArrayList<PartInstance>()
        fun add(part: Part?, anchor: Vec3, key: String, factory: (Part) -> PartModel) {
            if (part == null) return
            val model = modelCache.getOrPut("${part.id}|$key") { factory(part) }
            parts += PartInstance(part.category.name, part.id, anchor, model)
        }

        add(case, Vec3.ZERO, "$psuLength") { PartModels.case(it, dims, psuLength) }
        add(board, Vec3(boardX, boardTop, boardRear), "") { PartModels.motherboard(it, layout) }
        add(build[Category.CPU], socket, "") { PartModels.cpu(it) }
        add(build[Category.RAM], Vec3(boardX, boardTop - layout.ramV, boardRear + layout.ramU), board?.compat?.formFactor ?: "") {
            glbRam(it, layout) ?: PartModels.ram(it, layout)
        }
        add(build[Category.SSD], Vec3(boardX, boardTop - layout.m2V, boardRear + layout.m2U), "") { PartModels.ssd(it) }
        build[Category.GPU]?.takeUnless { it.isIntegratedGraphics }?.let { gpu ->
            add(gpu, Vec3(boardX + 0.6f, boardTop - layout.pcieV + 0.6f, boardRear - 1f), "") {
                glbGpu(it) ?: PartModels.gpu(it)
            }
        }

        // Кулер: башня на сокете или СЖО с радиатором под крышей
        build[Category.COOLER]?.let { cooler ->
            if (cooler.compat.radiatorMm != null) {
                val horizontalTop = !itx
                val radiatorWorld = if (horizontalTop) {
                    Vec3(dims.width / 2 - 1f - 6.4f, dims.height - 1.2f - 1.35f, -0.6f)
                } else {
                    Vec3(0f, 1.4f + 1.35f, 0f)
                }
                val key = "${case?.id}|${board?.compat?.formFactor}"
                add(cooler, socket, key) { part ->
                    val offset = radiatorWorld - socket
                    val pump = glbPart(part, Category.COOLER)
                    if (pump != null) {
                        ModelBuilder()
                            .include(pump, Vec3.ZERO)
                            .include(PartModels.aio(part, offset, horizontalTop, withFans = horizontalTop, includePump = false), Vec3.ZERO)
                            .build()
                    } else {
                        PartModels.aio(part, offset, horizontalTop, withFans = horizontalTop)
                    }
                }
            } else {
                add(cooler, socket, "") { glbPart(it, Category.COOLER) ?: PartModels.towerCooler(it) }
            }
        }

        // Блок питания: внизу сзади, в ITX-корпусе — сверху спереди
        psu?.let {
            val anchor = if (itx) {
                Vec3(-dims.width / 2 + 8.6f, dims.height - 1f - 8.6f, dims.depth / 2 - 1.5f - psuLength / 2)
            } else {
                Vec3(-dims.width / 2 + 8.6f, 1.45f, -dims.depth / 2 + 1f + psuLength / 2)
            }
            add(it, anchor, "") { p -> PartModels.psu(p) }
        }

        // Монитор: слева от корпуса с точки зрения камеры по умолчанию, экраном к зрителю.
        // Камера смотрит на стеклянную сторону (+X), поэтому «лево» в кадре — это −X и +Z.
        val viewYaw = Math.toRadians(OrbitCamera.DEFAULT_YAW.toDouble())
        val toCamera = Vec3(sin(viewYaw).toFloat(), 0f, cos(viewYaw).toFloat())
        val left = Vec3(-cos(viewYaw).toFloat(), 0f, sin(viewYaw).toFloat())
        val caseExtent = abs(left.x) * dims.width / 2 + abs(left.z) * dims.depth / 2
        val monitor = build[Category.MONITOR]
        var monitorAnchor: Vec3? = null
        var monitorWidth = 0f
        var monitorTop = 0f
        val monitorYaw = OrbitCamera.DEFAULT_YAW - MONITOR_TURN
        if (monitor != null) {
            val (w, h) = PartModels.monitorSize(monitor)
            monitorWidth = w
            monitorTop = 11f + h
            monitorAnchor = left * (caseExtent + w / 2 + 7f) - toCamera * 12f
            add(monitor, monitorAnchor, "$monitorYaw") { glbMonitor(it, monitorYaw) ?: PartModels.monitor(it, monitorYaw) }
        }

        // Стол развёрнут к камере и накрывает корпус и монитор
        val span = caseExtent + (if (monitorAnchor != null) monitorWidth + 7f else 0f)
        val deskLength = span + caseExtent + 34f
        val deskCenter = left * ((span - caseExtent) / 2) - toCamera * 6f
        val environment = PartModels.environment(deskCenter, OrbitCamera.DEFAULT_YAW, deskLength, 78f, dims, monitorAnchor, monitorYaw, monitorWidth)

        // Камера: описываем вокруг корпуса и монитора сферу — всё помещается в кадр
        val keyPoints = ArrayList<Vec3>()
        for (x in listOf(-dims.width / 2, dims.width / 2)) for (y in listOf(0f, dims.height)) for (z in listOf(-dims.depth / 2, dims.depth / 2)) {
            keyPoints += Vec3(x, y, z)
        }
        if (monitorAnchor != null) {
            val rot = Mat4.chain(Mat4.translation(monitorAnchor), Mat4.rotation(monitorYaw, Vec3.Y))
            for (x in listOf(-monitorWidth / 2, monitorWidth / 2)) for (y in listOf(0f, monitorTop)) for (z in listOf(-12f, -3f)) {
                keyPoints += Mat4.transformPoint(rot, Vec3(x, y, z))
            }
        }
        val boxMin = Vec3(keyPoints.minOf { it.x }, keyPoints.minOf { it.y }, keyPoints.minOf { it.z })
        val boxMax = Vec3(keyPoints.maxOf { it.x }, keyPoints.maxOf { it.y }, keyPoints.maxOf { it.z })
        val focus = (boxMin + boxMax) * 0.5f
        val radius = keyPoints.maxOf { (it - focus).length() }
        // Нейтральная лампа внутри корпуса: детали видны и при выключенной подсветке
        val interior = Vec3(dims.width * 0.25f, dims.height * 0.6f, 0f)
        return SceneSpec(parts, environment, focus, radius, interior)
    }

    // ---- .glb-модели ----------------------------------------------------------------------

    private fun loadGlb(name: String): GlbModel? = glbCache.getOrPut(name) {
        try {
            val bytes = modelSource("models/$name")
            if (bytes == null) {
                log("Модель $name не найдена — рисую упрощённую геометрию")
                null
            } else {
                GlbLoader.parse(bytes)
            }
        } catch (e: Exception) {
            log("Модель $name не загрузилась (${e.message}) — рисую упрощённую геометрию")
            null
        }
    }

    /** Модель детали из .glb, вписанная в габариты категории. */
    private fun glbPart(part: Part, category: Category): PartModel? {
        val name = part.visual.model ?: return null
        val glb = loadGlb(name) ?: return null
        // Габариты совпадают с процедурными моделями, поэтому .glb и «боксы» взаимозаменяемы
        val (min, max) = when (category) {
            Category.COOLER -> when {
                part.compat.radiatorMm != null -> Vec3(0.6f, -3.3f, -3.3f) to Vec3(4.5f, 4.1f, 3.3f)
                part.visual.style == "dual-tower" -> {
                    val h = (part.compat.heightMm ?: 160) / 10f
                    Vec3(0.6f, -6.1f, -5.7f) to Vec3(h, 6.1f, 8.25f)
                }
                else -> {
                    val h = (part.compat.heightMm ?: 155) / 10f
                    Vec3(0.6f, -6.1f, -2.6f) to Vec3(h, 6.1f, 5.15f)
                }
            }
            else -> return null
        }
        lastGlbCount++
        return GlbAdapter.toPartModel(glb, part, min, max, uniform = false)
    }

    private fun glbGpu(part: Part): PartModel? {
        val name = part.visual.model ?: return null
        val glb = loadGlb(name) ?: return null
        val size = PartModels.gpuSize(part)
        lastGlbCount++
        return GlbAdapter.toPartModel(glb, part, Vec3(0f, -size.y, 0f), Vec3(size.x, 0f, size.z), uniform = false)
    }

    private fun glbRam(part: Part, layout: BoardLayout): PartModel? {
        val name = part.visual.model ?: return null
        val glb = loadGlb(name) ?: return null
        val stick = GlbAdapter.toPartModel(glb, part, Vec3(0f, -6.65f, -0.35f), Vec3(4.4f, 6.65f, 0.35f), uniform = false)
        val b = ModelBuilder()
        for (slot in PartModels.ramSlotsUsed(part, layout)) b.include(stick, Vec3(0f, 0f, slot * 0.95f))
        lastGlbCount++
        return b.build()
    }

    private fun glbMonitor(part: Part, yaw: Float): PartModel? {
        val name = part.visual.model ?: return null
        val glb = loadGlb(name) ?: return null
        val (w, h) = PartModels.monitorSize(part)
        val model = GlbAdapter.toPartModel(glb, part, Vec3(-w / 2 - 0.6f, 0f, -14f), Vec3(w / 2 + 0.6f, 11f + h + 0.6f, -3.6f), uniform = false)
        lastGlbCount++
        return GlbAdapter.rotated(model, yaw, Vec3(0f, 18f, 0f))
    }

    companion object {
        /** На сколько градусов монитор развёрнут от направления на камеру. */
        const val MONITOR_TURN = 14f

        /** Минимальное значение — чтобы не было деления на ноль у вырожденных моделей. */
        internal fun safe(v: Float) = if (v < 1e-4f) 1e-4f else v

        internal fun clampScale(s: Float) = min(max(s, 1e-4f), 1e4f)
    }
}

/** Перевод glTF-модели в модель детали: подгонка размеров и роли материалов по именам. */
object GlbAdapter {

    /**
     * Вписывает модель в бокс [min]..[max].
     * Материалы с именами body / accent / rgb / glass / screen получают цвета детали.
     * Узлы с «spin» в имени вращаются вокруг своей оси Y (вентиляторы).
     */
    fun toPartModel(glb: GlbModel, part: Part, min: Vec3, max: Vec3, uniform: Boolean): PartModel {
        val (bMin, bMax) = glb.bounds()
        val size = bMax - bMin
        val target = max - min
        var sx = SceneComposer.clampScale(target.x / SceneComposer.safe(size.x))
        var sy = SceneComposer.clampScale(target.y / SceneComposer.safe(size.y))
        var sz = SceneComposer.clampScale(target.z / SceneComposer.safe(size.z))
        if (uniform) {
            val s = minOf(sx, sy, sz)
            sx = s; sy = s; sz = s
        }
        // Центр модели → центр целевого бокса
        val modelCenter = (bMin + bMax) * 0.5f
        val targetCenter = (min + max) * 0.5f
        val fit = Mat4.chain(
            Mat4.translation(targetCenter),
            Mat4.scale(sx, sy, sz),
            Mat4.translation(modelCenter * -1f),
        )
        val b = ModelBuilder()
        val rgbCenters = ArrayList<Vec3>()
        for (prim in glb.primitives) {
            val material = materialFor(prim.material, part)
            val transform = Mat4.multiply(fit, prim.nodeMatrix)
            val spin = if (prim.nodeName.contains("spin", ignoreCase = true)) Spin(Vec3.Y, 480f) else null
            b.mesh(prim.mesh, transform, material, spin)
            if (material.role == MaterialRole.RGB) {
                val (pMin, pMax) = prim.mesh.bounds()
                rgbCenters += Mat4.transformPoint(transform, (pMin + pMax) * 0.5f)
            }
        }
        if (part.visual.rgb) {
            rgbCenters.take(6).forEach { b.glow(it, 6f, 0.45f) }
            rgbCenters.firstOrNull()?.let { b.light(it) }
        }
        return b.build()
    }

    /** Поворачивает готовую модель вокруг вертикальной оси (для монитора). */
    fun rotated(model: PartModel, yaw: Float, enterOffset: Vec3): PartModel {
        val rot = Mat4.rotation(yaw, Vec3.Y)
        return PartModel(
            items = model.items.map { DrawItem(it.mesh, Mat4.multiply(rot, it.transform), it.material, it.spin) },
            glows = model.glows.map { it.copy(position = Mat4.transformPoint(rot, it.position)) },
            lights = model.lights.map { Mat4.transformPoint(rot, it) },
            enterOffset = enterOffset,
        )
    }

    private fun materialFor(m: GlbMaterial, part: Part): Material {
        val name = m.name.lowercase()
        return when {
            "rgb" in name -> if (part.visual.rgb) Material.rgb() else Material.solid(Rgba.fromHex(part.visual.accent), 0.4f, 0.4f)
            "glass" in name || m.blend -> Material.glass(m.baseColor.withAlpha(min(m.baseColor.a, 0.35f)))
            "screen" in name -> Material.unlit(m.baseColor)
            "body" in name -> Material(Rgba.fromHex(part.visual.color), m.metallic, m.roughness)
            "accent" in name -> Material(Rgba.fromHex(part.visual.accent), m.metallic, m.roughness)
            else -> Material(m.baseColor, m.metallic, m.roughness, m.emissive)
        }
    }
}
