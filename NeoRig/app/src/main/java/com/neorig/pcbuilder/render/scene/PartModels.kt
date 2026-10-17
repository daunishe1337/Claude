package com.neorig.pcbuilder.render.scene

import com.neorig.pcbuilder.domain.model.Part
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min

/** Габариты корпуса, см: ширина (X), высота (Y), глубина (Z). */
data class CaseDims(val width: Float, val height: Float, val depth: Float)

/**
 * Геометрия материнской платы: размеры и положение элементов
 * в координатах платы (u — от заднего края к переду, v — от верхнего края вниз).
 */
data class BoardLayout(
    val height: Float,
    val width: Float,
    val socketU: Float,
    val socketV: Float,
    val ramU: Float,
    val ramV: Float,
    val ramSlots: Int,
    val pcieV: Float,
    val m2U: Float,
    val m2V: Float,
) {
    companion object {
        fun of(formFactor: String?): BoardLayout = when (formFactor) {
            "ITX" -> BoardLayout(17f, 17f, 7.5f, 6.5f, 12.6f, 7.4f, 2, 14.9f, 7.5f, 11.4f)
            "mATX" -> BoardLayout(24.4f, 24.4f, 11.5f, 8f, 16.8f, 8.6f, 4, 14f, 11.5f, 11.6f)
            else -> BoardLayout(30.5f, 24.4f, 11.5f, 8f, 16.8f, 8.6f, 4, 14f, 11.5f, 11.6f)
        }
    }
}

/**
 * Процедурные модели деталей — «упрощённая геометрия» из боксов, цилиндров и колец.
 * Используются, если .glb-модели нет или она не загрузилась, а также для деталей,
 * у которых модель по определению простая (корпус, плата, БП).
 */
object PartModels {

    // ---- Общие материалы ------------------------------------------------------------------

    val aluminium = Material.solid(Rgba.fromHex("#B9BFCA"), metallic = 0.85f, roughness = 0.32f)
    val nickel = Material.solid(Rgba.fromHex("#D3D7DE"), metallic = 0.9f, roughness = 0.25f)
    val darkPlastic = Material.solid(Rgba.fromHex("#1B1D22"), roughness = 0.75f)
    val blackMatte = Material.solid(Rgba.fromHex("#0E0F12"), roughness = 0.85f)
    val slotPlastic = Material.solid(Rgba.fromHex("#2A2D35"), roughness = 0.6f)
    val goldPins = Material.solid(Rgba.fromHex("#C9A54A"), metallic = 0.9f, roughness = 0.35f)

    private fun body(part: Part, metallic: Float = 0.25f, roughness: Float = 0.5f) =
        Material.solid(Rgba.fromHex(part.visual.color), metallic, roughness)

    private fun accent(part: Part, metallic: Float = 0.5f, roughness: Float = 0.4f) =
        Material.solid(Rgba.fromHex(part.visual.accent), metallic, roughness)

    private fun isLight(part: Part) = Rgba.fromHex(part.visual.color).luminance > 0.45f

    private fun bladeMaterial(part: Part) =
        if (isLight(part)) Material.solid(Rgba.fromHex("#E9EBEF"), roughness = 0.5f)
        else Material.solid(Rgba.fromHex("#1E2026"), roughness = 0.55f)

    // ---- Корпус ---------------------------------------------------------------------------

    fun caseDims(style: String): CaseDims = when (style) {
        "mini" -> CaseDims(20f, 40f, 38f)
        "itx" -> CaseDims(18.5f, 29.5f, 37.5f)
        "panoramic" -> CaseDims(28.5f, 46f, 45f)
        "full" -> CaseDims(24f, 55f, 52f)
        else -> CaseDims(22f, 47f, 45f)
    }

    /** Корпус: панели, стекло, ножки, вентиляторы, кожух БП. Начало координат — центр дна. */
    fun case(part: Part, dims: CaseDims, psuLength: Float): PartModel {
        val b = ModelBuilder()
        val w = dims.width; val h = dims.height; val d = dims.depth
        val shell = body(part, metallic = 0.35f, roughness = 0.45f)
        val inner = Material.solid(Rgba.fromHex(part.visual.color) * 0.55f, metallic = 0.2f, roughness = 0.7f)
        val panoramic = part.visual.style == "panoramic"
        val wood = part.visual.style == "wood"
        val rgb = part.visual.rgb
        val t = 0.6f

        // Ножки
        for (x in listOf(-w / 2 + 2f, w / 2 - 2f)) for (z in listOf(-d / 2 + 3f, d / 2 - 3f)) {
            b.box(Vec3(x, 0.4f, z), Vec3(3f, 0.8f, 4f), blackMatte)
        }
        // Дно, крыша, задняя стенка, стенка за платой
        b.boxBetween(Vec3(-w / 2, 0.8f, -d / 2), Vec3(w / 2, 0.8f + t, d / 2), shell)
        b.boxBetween(Vec3(-w / 2, h - t, -d / 2), Vec3(w / 2, h, d / 2), shell)
        b.boxBetween(Vec3(-w / 2 + 2f, h - t - 0.05f, -d / 2 + 3f), Vec3(w / 2 - 2f, h - t + 0.02f, d / 2 - 3f), inner) // сетка сверху
        b.boxBetween(Vec3(-w / 2, 0.8f, -d / 2), Vec3(w / 2, h, -d / 2 + t), shell)
        b.boxBetween(Vec3(-w / 2, 0.8f, -d / 2), Vec3(-w / 2 + t, h, d / 2), inner)
        // Слоты расширения на задней стенке
        for (i in 0 until 7) {
            b.box(Vec3(-w / 2 + 6f, h - 22f - i * 2f, -d / 2 + t + 0.05f), Vec3(9f, 0.9f, 0.12f), inner)
        }

        // Передняя панель
        val frontZ = d / 2
        when {
            panoramic -> {
                b.boxBetween(Vec3(-w / 2, 0.8f, frontZ - 0.6f), Vec3(-w / 2 + 1.2f, h, frontZ), shell)
                b.mesh(Primitives.box, Mat4.chain(Mat4.translation(0.6f, h / 2 + 0.4f, frontZ - 0.25f), Mat4.scale(w - 1.2f, h - 0.8f, 0.4f)), Material.glass(glassTint(part)))
            }
            wood -> {
                b.boxBetween(Vec3(-w / 2, 0.8f, frontZ - 1.4f), Vec3(w / 2, h, frontZ - 0.8f), blackMatte)
                val woodMat = Material.solid(Rgba.fromHex(part.visual.accent), roughness = 0.8f)
                val slats = 9
                for (i in 0 until slats) {
                    val x = -w / 2 + 1.2f + i * (w - 2.4f) / (slats - 1)
                    b.box(Vec3(x, h / 2 + 0.4f, frontZ - 0.4f), Vec3(1.3f, h - 2f, 0.8f), woodMat)
                }
            }
            else -> {
                b.boxBetween(Vec3(-w / 2, 0.8f, frontZ - 1.2f), Vec3(w / 2, h, frontZ), shell)
                b.boxBetween(Vec3(-w / 2 + 1.5f, 2.5f, frontZ + 0.01f), Vec3(w / 2 - 1.5f, h - 1.5f, frontZ + 0.05f), inner) // сетка
            }
        }
        if (rgb && !wood) {
            // Вертикальная световая полоса на передней кромке
            b.boxBetween(Vec3(w / 2 - 0.9f, 3f, frontZ - 0.2f), Vec3(w / 2 - 0.4f, h - 3f, frontZ + 0.08f), Material.rgb())
            for (i in 0 until 4) b.glow(Vec3(w / 2 - 0.6f, 5f + i * (h - 10f) / 3f, frontZ + 0.2f), 6f, 0.45f)
        }

        // Боковое стекло (сторона +X) с рамкой
        val glassFrom = if (panoramic) frontZ - 0.6f else frontZ - 1.2f
        b.mesh(
            Primitives.box,
            Mat4.chain(Mat4.translation(w / 2 - 0.2f, h / 2 + 0.4f, (-d / 2 + glassFrom) / 2), Mat4.scale(0.35f, h - 0.8f, glassFrom + d / 2)),
            Material.glass(glassTint(part)),
        )
        b.boxBetween(Vec3(w / 2 - 0.6f, h - 1f, -d / 2), Vec3(w / 2, h, glassFrom), shell)
        b.boxBetween(Vec3(w / 2 - 0.6f, 0.8f, -d / 2), Vec3(w / 2, 1.8f, glassFrom), shell)
        if (!panoramic) b.boxBetween(Vec3(w / 2 - 0.6f, 0.8f, -d / 2), Vec3(w / 2, h, -d / 2 + 1f), shell)

        // Кожух блока питания (закрывает переднюю часть низа)
        if (part.visual.style != "itx") {
            val shroudFrom = -d / 2 + 1f + psuLength + 2.5f
            if (shroudFrom < d / 2 - 4f) {
                b.boxBetween(Vec3(-w / 2 + t, 1.4f, shroudFrom), Vec3(w / 2 - 0.6f, 11f, d / 2 - 1.4f), inner)
            }
        }

        // Вентиляторы: задний и передние
        val frame = if (isLight(part)) Material.solid(Rgba.fromHex("#E3E6EB"), roughness = 0.5f) else darkPlastic
        val blades = bladeMaterial(part)
        val fanSize = if (part.visual.style == "full") 14f else 12f
        val boardTop = h - 3.5f
        if (part.visual.style != "itx") {
            b.fan(Vec3(-w / 2 + 8.5f, boardTop - 6.5f, -d / 2 + t + 1.3f), 12f, Vec3(0f, 0f, -1f), frame, blades, rgb)
        }
        val frontFans = when (part.visual.style) {
            "itx" -> 0
            "mini" -> 2
            else -> 3
        }
        if (frontFans > 0) {
            val top = h - 3f
            val bottom = if (part.visual.style == "mini") 12f else 12.5f
            val step = (top - bottom) / frontFans
            val fanZ = (if (panoramic) frontZ - 0.6f else frontZ - 1.2f) - 1.35f
            val size = min(fanSize, step - 0.4f)
            for (i in 0 until frontFans) {
                b.fan(Vec3(0f, bottom + step * (i + 0.5f), fanZ), size, Vec3(0f, 0f, -1f), frame, blades, rgb)
            }
        }
        return b.build(enterOffset = Vec3(0f, 22f, 0f))
    }

    private fun glassTint(part: Part): Rgba =
        if (isLight(part)) Rgba(0.85f, 0.9f, 0.95f, 0.05f) else Rgba(0.3f, 0.34f, 0.45f, 0.08f)

    // ---- Материнская плата ----------------------------------------------------------------

    /** Плата: начало координат — верхний задний угол лицевой поверхности; X — от платы наружу. */
    fun motherboard(part: Part, layout: BoardLayout): PartModel {
        val b = ModelBuilder()
        val pcb = Material.solid(Rgba.fromHex(part.visual.color), metallic = 0.1f, roughness = 0.55f)
        val heatsink = accent(part, metallic = 0.75f, roughness = 0.35f)
        val h = layout.height; val w = layout.width
        // Текстолит
        b.boxBetween(Vec3(-0.2f, -h, 0f), Vec3(0f, 0f, w), pcb)
        // Кожух задней панели и радиаторы VRM (Г-образно вокруг сокета)
        b.boxBetween(Vec3(0f, -14f.coerceAtMost(h - 1f), 0.4f), Vec3(2.6f, -0.8f, 3.4f), darkPlastic)
        b.boxBetween(Vec3(0f, -2.8f, 3.6f), Vec3(2.2f, -0.8f, layout.socketU + 3.5f), heatsink)
        b.boxBetween(Vec3(0f, -layout.socketV - 3.5f, 3.6f), Vec3(2.2f, -2.8f, 6f), heatsink)
        // Сокет
        b.box(Vec3(0.15f, -layout.socketV, layout.socketU), Vec3(0.3f, 6f, 6f), slotPlastic)
        // Слоты памяти
        for (i in 0 until layout.ramSlots) {
            b.box(Vec3(0.35f, -layout.ramV, layout.ramU + i * 0.95f), Vec3(0.7f, 13.6f, 0.5f), slotPlastic)
        }
        // Слот PCIe x16 с металлическим усилением
        b.box(Vec3(0.4f, -layout.pcieV, 2.2f + 4.25f), Vec3(0.8f, 0.7f, 8.5f), nickel)
        // Радиатор чипсета
        if (h > 20f) {
            b.box(Vec3(0.6f, -h + 5f, w - 6f), Vec3(1.2f, 5f, 5.5f), heatsink)
        }
        // Слот M.2 (сам накопитель — отдельная деталь)
        b.box(Vec3(0.1f, -layout.m2V, layout.m2U), Vec3(0.2f, 2.4f, 8.4f), slotPlastic)
        // Разъём питания 24-pin
        b.box(Vec3(0.8f, -9f, w - 0.8f), Vec3(1.6f, 5.5f, 1.2f), darkPlastic)
        if (part.visual.rgb) {
            b.box(Vec3(2.65f, -7.5f, 1.9f), Vec3(0.1f, 9f, 0.5f), Material.rgb())
            b.glow(Vec3(2.9f, -5f, 1.9f), 5f, 0.4f)
            b.glow(Vec3(2.9f, -10f, 1.9f), 5f, 0.4f)
        }
        return b.build()
    }

    // ---- Процессор -------------------------------------------------------------------------

    /** Процессор на сокете: начало координат — центр сокета на поверхности платы. */
    fun cpu(part: Part): PartModel {
        val b = ModelBuilder()
        b.box(Vec3(0.35f, 0f, 0f), Vec3(0.12f, 4.6f, 4.6f), Material.solid(Rgba.fromHex("#2E7D4F"), roughness = 0.6f))
        b.box(Vec3(0.55f, 0f, 0f), Vec3(0.3f, 4.0f, 4.0f), Material.solid(Rgba.fromHex(part.visual.color), 0.9f, 0.3f))
        b.box(Vec3(0.71f, -1.6f, 1.6f), Vec3(0.02f, 0.6f, 0.6f), accent(part))
        return b.build()
    }

    // ---- Видеокарта ------------------------------------------------------------------------

    /** Размеры видеокарты, см: высота (X), толщина (Y), длина (Z). */
    fun gpuSize(part: Part): Vec3 {
        val length = (part.compat.lengthMm ?: 250) / 10f
        return when (part.visual.style) {
            "single-fan" -> Vec3(11f, 3.8f, length)
            "triple-fan" -> Vec3(13.5f, 6.2f, length)
            else -> Vec3(12.2f, 4.6f, length)
        }
    }

    /** Видеокарта: X — от платы наружу, Y — вниз от слота (толщина), Z — от задней планки к переду. */
    fun gpu(part: Part): PartModel {
        val b = ModelBuilder()
        val size = gpuSize(part)
        val hgt = size.x; val thick = size.y; val len = size.z
        val shroud = body(part, metallic = 0.45f, roughness = 0.38f)
        val acc = accent(part, metallic = 0.4f, roughness = 0.35f)
        // Планка крепления
        b.boxBetween(Vec3(-0.5f, -thick - 0.2f, -0.3f), Vec3(hgt * 0.92f, 0.6f, 0f), nickel)
        // Бэкплейт и плата
        b.boxBetween(Vec3(0f, -0.15f, 0f), Vec3(hgt, 0.25f, len - 0.3f), Material.solid(Rgba.fromHex(part.visual.color) * 0.8f, 0.6f, 0.35f))
        b.boxBetween(Vec3(0f, -0.4f, 0f), Vec3(hgt - 0.3f, -0.15f, len - 0.5f), blackMatte)
        // Радиатор (видно по краям) и кожух
        b.boxBetween(Vec3(0.4f, -thick + 0.6f, 0.4f), Vec3(hgt - 0.4f, -0.4f, len - 0.6f), aluminium)
        b.boxBetween(Vec3(0f, -thick, 0f), Vec3(hgt, -thick + 1.1f, len), shroud)
        b.boxBetween(Vec3(hgt - 0.5f, -thick, 0f), Vec3(hgt, -0.4f, len), shroud)
        // Акцентные вставки на кожухе
        b.boxBetween(Vec3(hgt * 0.15f, -thick - 0.05f, len * 0.02f), Vec3(hgt * 0.85f, -thick + 0.05f, len * 0.06f), acc)
        b.boxBetween(Vec3(hgt * 0.15f, -thick - 0.05f, len * 0.94f), Vec3(hgt * 0.85f, -thick + 0.05f, len * 0.98f), acc)
        // Вентиляторы снизу (поток воздуха вниз → вверх, лицом к -Y)
        val fans = when (part.visual.style) { "single-fan" -> 1; "triple-fan" -> 3; else -> 2 }
        val fanD = min(hgt - 1.6f, len / fans - 1.2f)
        for (i in 0 until fans) {
            val z = len * (i + 0.5f) / fans
            b.fan(
                Vec3(hgt / 2, -thick - 0.35f, z), fanD, Vec3(0f, -1f, 0f),
                frame = shroud, blades = bladeMaterial(part), rgb = false, ringMaterial = acc, speed = 420f,
                thickness = 0.6f,
            )
        }
        // Светящаяся полоса на торце (её видно через стекло)
        if (part.visual.rgb) {
            b.boxBetween(Vec3(hgt + 0.01f, -thick * 0.55f, len * 0.2f), Vec3(hgt + 0.1f, -thick * 0.35f, len * 0.8f), Material.rgb())
            for (i in 0 until 3) b.glow(Vec3(hgt + 0.5f, -thick * 0.45f, len * (0.3f + 0.2f * i)), 7f, 0.5f)
            b.light(Vec3(hgt + 2f, -thick / 2, len / 2))
        } else {
            b.boxBetween(Vec3(hgt + 0.01f, -thick * 0.55f, len * 0.2f), Vec3(hgt + 0.08f, -thick * 0.4f, len * 0.8f), acc)
        }
        // Разъём питания
        b.box(Vec3(hgt - 0.2f, 0.6f, len * 0.7f), Vec3(1.2f, 1f, 2.4f), blackMatte)
        return b.build()
    }

    // ---- Оперативная память ----------------------------------------------------------------

    /** Индексы занятых слотов: 2 модуля — во 2-м и 4-м слоте (A2/B2), 4 — во всех. */
    fun ramSlotsUsed(part: Part, layout: BoardLayout): List<Int> {
        val modules = (part.specs.modules ?: 2).coerceAtMost(layout.ramSlots)
        return when {
            layout.ramSlots <= 2 || modules >= layout.ramSlots -> (0 until modules).toList()
            modules == 1 -> listOf(1)
            else -> listOf(1, 3)
        }
    }

    /** Планки ОЗУ: начало координат — первый слот (центр по длине) на поверхности платы. */
    fun ram(part: Part, layout: BoardLayout): PartModel {
        val b = ModelBuilder()
        val spreader = body(part, metallic = 0.55f, roughness = 0.35f)
        val top = accent(part, metallic = 0.4f, roughness = 0.4f)
        for (slot in ramSlotsUsed(part, layout)) {
            val z = slot * 0.95f
            b.mesh(ramStick(part, spreader, top), Mat4.translation(0f, 0f, z), spreader)
            if (part.visual.rgb) {
                b.box(Vec3(4.15f, 0f, z), Vec3(0.5f, 12.6f, 0.62f), Material.rgb())
                b.glow(Vec3(4.6f, -4f, z), 4.5f, 0.45f)
                b.glow(Vec3(4.6f, 4f, z), 4.5f, 0.45f)
            } else {
                b.box(Vec3(4.0f, 0f, z), Vec3(0.3f, 12.8f, 0.62f), top)
            }
        }
        if (part.visual.rgb) b.light(Vec3(6f, 0f, 1.5f))
        return b.build()
    }

    private fun ramStick(part: Part, spreader: Material, top: Material): MeshData {
        val mb = MeshBuilder()
        mb.add(Primitives.box, Mat4.chain(Mat4.translation(1.8f, 0f, 0f), Mat4.scale(3.6f, 13.3f, 0.7f)))
        mb.add(Primitives.box, Mat4.chain(Mat4.translation(0.35f, 0f, 0f), Mat4.scale(0.7f, 13.0f, 0.14f)))
        return mb.build()
    }

    // ---- Накопитель M.2 --------------------------------------------------------------------

    fun ssd(part: Part): PartModel {
        val b = ModelBuilder()
        val heatsink = part.visual.style == "m2-heatsink"
        b.box(Vec3(0.25f, 0f, 0f), Vec3(0.12f, 2.2f, 8f), Material.solid(Rgba.fromHex(part.visual.color), 0.1f, 0.5f))
        if (heatsink) {
            b.box(Vec3(0.75f, 0f, 0f), Vec3(0.9f, 2.3f, 7.8f), accent(part, 0.8f, 0.3f))
            for (i in 0 until 6) b.box(Vec3(1.25f, 0f, -3.2f + i * 1.28f), Vec3(0.2f, 2.3f, 0.5f), accent(part, 0.8f, 0.3f))
            if (part.visual.rgb) {
                b.box(Vec3(1.3f, 0f, 3.6f), Vec3(0.1f, 2.0f, 0.3f), Material.rgb())
                b.glow(Vec3(1.6f, 0f, 3.6f), 3f, 0.4f)
            }
        } else {
            b.box(Vec3(0.36f, 0f, -0.6f), Vec3(0.1f, 1.6f, 2.4f), blackMatte)
            b.box(Vec3(0.36f, 0f, 2.3f), Vec3(0.1f, 1.6f, 1.6f), blackMatte)
            b.box(Vec3(0.33f, 0f, -3.2f), Vec3(0.05f, 1.7f, 1.0f), accent(part))
        }
        return b.build()
    }

    // ---- Блок питания ----------------------------------------------------------------------

    fun psuLength(part: Part): Float {
        val watts = part.compat.powerW ?: 650
        return when {
            watts >= 1200 -> 18f
            watts >= 1000 -> 16f
            else -> 14f
        }
    }

    /** БП: начало координат — центр нижней грани. */
    fun psu(part: Part): PartModel {
        val b = ModelBuilder()
        val len = psuLength(part)
        val shell = body(part, metallic = 0.5f, roughness = 0.4f)
        b.box(Vec3(0f, 4.3f, 0f), Vec3(15f, 8.6f, len), shell)
        // Решётка вентилятора снизу
        b.cylinder(Vec3(0f, -0.02f, 0f), 6.2f, 0.06f, Vec3.Y, blackMatte)
        // Наклейка-полоса сбоку (+X)
        b.box(Vec3(7.52f, 4.3f, 0f), Vec3(0.05f, 3f, len * 0.7f), accent(part))
        // Модульные разъёмы спереди
        b.box(Vec3(0f, 4.3f, len / 2 + 0.05f), Vec3(12f, 6f, 0.1f), blackMatte)
        if (part.visual.rgb) {
            b.box(Vec3(7.55f, 7.6f, 0f), Vec3(0.06f, 0.5f, len * 0.8f), Material.rgb())
            b.glow(Vec3(8f, 7.6f, 0f), 6f, 0.4f)
        }
        return b.build()
    }

    // ---- Кулеры ----------------------------------------------------------------------------

    /** Башенный (или низкопрофильный) кулер: начало координат — центр сокета на плате. */
    fun towerCooler(part: Part): PartModel {
        val b = ModelBuilder()
        val height = (part.compat.heightMm ?: 155) / 10f
        val fins = Material.solid(Rgba.fromHex(part.visual.color), metallic = 0.8f, roughness = 0.3f)
        val acc = accent(part, metallic = 0.5f, roughness = 0.35f)
        val pipes = if (isLight(part)) nickel else Material.solid(Rgba.fromHex("#C47A45"), 0.9f, 0.3f)
        val frame = if (isLight(part)) Material.solid(Rgba.fromHex("#E8EAEE"), roughness = 0.5f) else darkPlastic
        // Основание на крышке процессора
        b.box(Vec3(1.1f, 0f, 0f), Vec3(1.0f, 4.2f, 4.2f), nickel)

        if (part.visual.style == "low-profile") {
            // Низкопрофильный: плоский радиатор и вентилятор сверху
            b.box(Vec3(2.2f, 0f, 0f), Vec3(1.2f, 9.5f, 9.5f), fins)
            b.fan(Vec3(3.6f, 0f, 0f), 9.2f, Vec3.X, frame, bladeMaterial(part), part.visual.rgb, acc)
            return b.build()
        }

        val dual = part.visual.style == "dual-tower"
        val stackDepth = if (dual) 4.2f else 5.2f
        val stacks = if (dual) listOf(-3.6f, 3.6f) else listOf(0f)
        // Тепловые трубки
        for (i in 0 until 4) {
            val y = -2.7f + i * 1.8f
            for (z in stacks) b.cylinder(Vec3(height / 2 + 0.6f, y, z), 0.32f, height - 1.6f, Vec3.X, pipes)
        }
        // Рёбра радиатора — склеиваем в один меш
        val finMesh = MeshBuilder()
        val finCount = ((height - 3.5f) / 0.32f).toInt()
        for (z in stacks) {
            for (i in 0 until finCount) {
                val x = 3f + i * 0.32f
                finMesh.add(Primitives.box, Mat4.chain(Mat4.translation(x, 0f, z), Mat4.scale(0.06f, 12f, stackDepth)))
            }
        }
        b.mesh(finMesh.build(), Mat4.identity(), fins)
        // Верхняя крышка (видна через стекло)
        for (z in stacks) b.box(Vec3(height - 0.25f, 0f, z), Vec3(0.5f, 12.2f, stackDepth + 0.2f), acc)
        // Вентиляторы: между башнями и/или спереди, поток к задней стенке (-Z)
        val fanX = 3f + (height - 3.5f) / 2f
        val fanZs = if (dual) listOf(0f, 3.6f + stackDepth / 2 + 1.3f) else listOf(stackDepth / 2 + 1.3f)
        for (z in fanZs) {
            b.fan(Vec3(fanX, 0f, z), 12f, Vec3(0f, 0f, -1f), frame, bladeMaterial(part), part.visual.rgb, acc)
        }
        if (part.visual.rgb) {
            for (z in stacks) b.glow(Vec3(height + 0.4f, 0f, z), 7f, 0.35f)
        }
        return b.build()
    }

    /**
     * СЖО: помпа на процессоре + радиатор с вентиляторами.
     * @param radiatorOffset центр радиатора относительно сокета
     * @param horizontal     true — радиатор лежит под крышей корпуса
     */
    fun aio(part: Part, radiatorOffset: Vec3, horizontal: Boolean, withFans: Boolean, includePump: Boolean = true): PartModel {
        val b = ModelBuilder()
        val acc = accent(part)
        val frame = if (isLight(part)) Material.solid(Rgba.fromHex("#E8EAEE"), roughness = 0.5f) else darkPlastic
        if (includePump) b.include(aioPump(part), Vec3.ZERO)
        addRadiator(b, part, radiatorOffset, horizontal, withFans, frame, acc)
        return b.build()
    }

    /** Помпа СЖО (крышка на процессоре). */
    fun aioPump(part: Part): PartModel {
        val b = ModelBuilder()
        val shell = body(part, metallic = 0.4f, roughness = 0.4f)
        val acc = accent(part)
        b.cylinder(Vec3(2.3f, 0f, 0f), 3.3f, 3.6f, Vec3.X, shell)
        if (part.visual.style == "aio-lcd") {
            // Круглый экран на помпе
            b.cylinder(Vec3(4.12f, 0f, 0f), 2.9f, 0.05f, Vec3.X, Material.unlit(Rgba.fromSrgb(0.35f, 0.15f, 0.65f)))
            b.cylinder(Vec3(4.16f, 0f, 0f), 1.4f, 0.04f, Vec3.X, Material.unlit(Rgba.fromSrgb(0.1f, 0.85f, 0.9f)))
        } else {
            b.cylinder(Vec3(4.12f, 0f, 0f), 2.6f, 0.05f, Vec3.X, blackMatte)
        }
        b.mesh(
            Primitives.ring,
            Mat4.chain(Mat4.translation(4.1f, 0f, 0f), ModelBuilder.alignY(Vec3.X), Mat4.scale(6.4f, 4f, 6.4f)),
            if (part.visual.rgb) Material.rgb() else acc,
        )
        if (part.visual.rgb) {
            b.glow(Vec3(4.8f, 0f, 0f), 9f, 0.6f)
            b.light(Vec3(6f, 0f, 0f))
        }
        return b.build()
    }

    private fun addRadiator(
        b: ModelBuilder,
        part: Part,
        radiatorOffset: Vec3,
        horizontal: Boolean,
        withFans: Boolean,
        frame: Material,
        acc: Material,
    ) {
        val length = (part.compat.radiatorMm ?: 240) / 10f + 3f
        val radSize = if (horizontal) Vec3(12.4f, 2.7f, length) else Vec3(12.4f, length, 2.7f)
        b.box(radiatorOffset, radSize, Material.solid(Rgba.fromHex("#1A1B1F"), 0.5f, 0.5f))
        // Шланги — два сегмента от помпы к радиатору
        val tube = Material.solid(Rgba.fromHex("#141518"), roughness = 0.8f)
        for (dz in listOf(-1.2f, 1.2f)) {
            val start = Vec3(3.4f, 2.6f, dz)
            val end = radiatorOffset + Vec3(-4f, if (horizontal) -1.4f else 0f, dz - length / 2 + 2f)
            val mid = Vec3(start.x, end.y, start.z)
            tubeSegment(b, start, mid, tube)
            tubeSegment(b, mid, end, tube)
        }
        if (withFans) {
            val fanCount = ((part.compat.radiatorMm ?: 240) / 120).coerceAtLeast(1)
            val fanSize = (part.compat.radiatorMm ?: 240) / 10f / fanCount
            for (i in 0 until fanCount) {
                val along = -length / 2 + 1.5f + fanSize * (i + 0.5f)
                val center = if (horizontal) radiatorOffset + Vec3(0f, -2.7f, along) else radiatorOffset + Vec3(0f, along, -2.7f)
                val facing = if (horizontal) Vec3.Y else Vec3.Z
                b.fan(center, fanSize - 0.2f, facing, frame, bladeMaterial(part), part.visual.rgb, acc)
            }
        }
    }

    private fun tubeSegment(b: ModelBuilder, from: Vec3, to: Vec3, material: Material) {
        val dir = to - from
        val len = dir.length()
        if (len < 0.01f) return
        b.cylinder((from + to) * 0.5f, 0.55f, len, dir, material)
    }

    // ---- Монитор ---------------------------------------------------------------------------

    /** Размер экрана 16:9 по диагонали, см. */
    fun monitorSize(part: Part): Pair<Float, Float> {
        val diagonal = (part.specs.sizeInch ?: 27.0).toFloat() * 2.54f
        return diagonal * 0.8716f to diagonal * 0.4903f
    }

    /** Монитор: начало координат — центр подставки на столе; [yaw] — поворот к зрителю. */
    fun monitor(part: Part, yaw: Float): PartModel {
        val b = ModelBuilder()
        val (w, h) = monitorSize(part)
        val shell = body(part, metallic = 0.3f, roughness = 0.45f)
        val rot = Mat4.rotation(yaw, Vec3.Y)
        val bottom = 11f
        val curved = part.visual.style == "curved"

        fun add(center: Vec3, size: Vec3, material: Material) {
            b.mesh(Primitives.box, Mat4.chain(rot, Mat4.translation(center), Mat4.scale(size.x, size.y, size.z)), material)
        }
        // Подставка: основание и стойка
        add(Vec3(0f, 0.4f, -3f), Vec3(min(26f, w * 0.45f), 0.8f, 17f), shell)
        add(Vec3(0f, bottom / 2 + 3f, -6.5f), Vec3(4.5f, bottom + 4f, 1.6f), shell)
        // Панель и экран
        val segments = if (curved) 10 else 1
        val radius = w * 1.4f
        val screenTop = Rgba.fromSrgb(0.36f, 0.16f, 0.72f)
        val screenMid = Rgba.fromSrgb(0.10f, 0.08f, 0.22f)
        val screenBottom = Rgba.fromSrgb(0.05f, 0.62f, 0.68f)
        for (i in 0 until segments) {
            val segW = w / segments
            val x = -w / 2 + segW * (i + 0.5f)
            val angle = if (curved) Math.toDegrees((x / radius).toDouble()).toFloat() else 0f
            val zCurve = if (curved) (1f - cos(x / radius)) * radius else 0f
            val seg = Mat4.chain(rot, Mat4.translation(x, bottom + h / 2, -4.6f - zCurve), Mat4.rotation(-angle, Vec3.Y))
            b.mesh(Primitives.box, Mat4.chain(seg, Mat4.translation(0f, 0f, -0.9f), Mat4.scale(segW + 0.05f, h + 1.2f, 1.6f)), shell)
            // Градиент экрана: сверху фиолетовый, снизу бирюзовый
            val t0 = i.toFloat() / segments
            val t1 = (i + 1).toFloat() / segments
            val quad = Primitives.gradientQuad(
                screenBottom.mix(screenMid, t0 * 0.6f), screenBottom.mix(screenMid, t1 * 0.6f),
                screenTop.mix(screenMid, 1f - t1), screenTop.mix(screenMid, 1f - t0),
            )
            b.mesh(quad, Mat4.chain(seg, Mat4.translation(0f, 0f, 0.02f), Mat4.scale(segW, h, 1f)), Material.unlit(Rgba.WHITE))
        }
        // «Интерфейс» на экране: панель задач и окно
        val ui = Material.unlit(Rgba.fromSrgb(0.03f, 0.03f, 0.06f, 1f))
        b.mesh(Primitives.quad, Mat4.chain(rot, Mat4.translation(0f, bottom + h * 0.04f, -4.55f), Mat4.scale(w * 0.98f, h * 0.05f, 1f)), ui)
        b.mesh(Primitives.quad, Mat4.chain(rot, Mat4.translation(-w * 0.12f, bottom + h * 0.55f, -4.56f), Mat4.scale(w * 0.42f, h * 0.48f, 1f)), Material.unlit(Rgba.fromSrgb(0.06f, 0.06f, 0.12f)))
        b.mesh(Primitives.quad, Mat4.chain(rot, Mat4.translation(-w * 0.12f, bottom + h * 0.77f, -4.555f), Mat4.scale(w * 0.42f, h * 0.04f, 1f)), Material.unlit(Rgba.fromSrgb(0.55f, 0.36f, 0.96f)))
        // Мягкий отсвет экрана
        b.glow(Mat4.transformPoint(rot, Vec3(0f, bottom + h / 2, -2f)), w * 0.9f, 0.18f)
        return b.build(enterOffset = Vec3(0f, 18f, 0f))
    }

    // ---- Окружение -------------------------------------------------------------------------

    /**
     * Стол, тени и неоновая кромка — всё, что не относится к деталям.
     * Стол развёрнут на [yaw] градусов, чтобы его передний край был обращён к камере.
     */
    fun environment(
        center: Vec3,
        yaw: Float,
        length: Float,
        depth: Float,
        caseDims: CaseDims,
        monitorAnchor: Vec3?,
        monitorYaw: Float,
        monitorWidth: Float,
    ): PartModel {
        val b = ModelBuilder()
        val desk = Material.solid(Rgba.fromHex("#1A1C24"), metallic = 0.15f, roughness = 0.35f)
        val deskFrame = Mat4.chain(Mat4.translation(center), Mat4.rotation(yaw, Vec3.Y))
        b.mesh(Primitives.box, Mat4.chain(deskFrame, Mat4.translation(0f, -1.5f, 0f), Mat4.scale(length, 3f, depth)), desk)
        // Неоновая кромка по переднему краю стола
        b.mesh(
            Primitives.box,
            Mat4.chain(deskFrame, Mat4.translation(0f, -1.4f, depth / 2 + 0.08f), Mat4.scale(length, 0.4f, 0.16f)),
            Material.unlit(Rgba.fromSrgb(0.55f, 0.3f, 1f)),
        )
        // Тени под корпусом и монитором
        b.mesh(Primitives.softRect(0.6f), Mat4.chain(Mat4.translation(0f, 0.04f, 0f), Mat4.scale(caseDims.width + 10f, 1f, caseDims.depth + 10f)), Material.shadow(0.6f))
        if (monitorAnchor != null) {
            b.mesh(
                Primitives.softRect(0.5f),
                Mat4.chain(Mat4.translation(monitorAnchor + Vec3(0f, 0.03f, 0f)), Mat4.rotation(monitorYaw, Vec3.Y), Mat4.translation(0f, 0f, -4f), Mat4.scale(max(26f, monitorWidth * 0.6f), 1f, 22f)),
                Material.shadow(0.5f),
            )
        }
        return b.build(enterOffset = Vec3.ZERO)
    }
}
