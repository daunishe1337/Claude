package com.neorig.pcbuilder.render.scene

/** Роль материала: от неё зависит, как рендерер его рисует. */
enum class MaterialRole {
    /** Обычная непрозрачная поверхность. */
    SOLID,

    /** RGB-подсветка: цвет берётся из настроек, при выключенной подсветке — тёмный. */
    RGB,

    /** Стекло: прозрачный проход с сортировкой и френелем. */
    GLASS,

    /** Самосветящаяся поверхность без освещения (экран монитора, фон стола). */
    UNLIT,

    /** Мягкая тень на столе (прозрачный проход, без освещения). */
    SHADOW,
}

/**
 * Материал поверхности.
 * @property color     базовый цвет (линейный); альфа учитывается у стекла и теней
 * @property metallic  0 — пластик, 1 — металл (сильнее и цветнее блик)
 * @property roughness 0 — зеркальный блик, 1 — матовый
 * @property emissive  собственное свечение
 */
data class Material(
    val color: Rgba,
    val metallic: Float = 0f,
    val roughness: Float = 0.6f,
    val emissive: Rgba = Rgba.BLACK,
    val role: MaterialRole = MaterialRole.SOLID,
) {
    companion object {
        fun solid(color: Rgba, metallic: Float = 0f, roughness: Float = 0.6f) =
            Material(color, metallic, roughness)

        fun rgb() = Material(Rgba(0.05f, 0.05f, 0.06f), roughness = 0.3f, role = MaterialRole.RGB)

        fun glass(tint: Rgba) = Material(tint, metallic = 0f, roughness = 0.05f, role = MaterialRole.GLASS)

        fun unlit(color: Rgba) = Material(color, role = MaterialRole.UNLIT)

        fun shadow(alpha: Float) = Material(Rgba(0f, 0f, 0f, alpha), role = MaterialRole.SHADOW)
    }
}

/** Вращение элемента (вентиляторы): угол = время × скорость, ось — локальная. */
data class Spin(val axis: Vec3, val degreesPerSecond: Float)

/**
 * Один рисуемый элемент детали.
 * Итоговая матрица: якорь детали × [transform] × (вращение [spin], если есть).
 */
class DrawItem(
    val mesh: MeshData,
    val transform: FloatArray,
    val material: Material,
    val spin: Spin? = null,
)

/** Источник RGB-свечения: ореол (billboard) в локальных координатах детали. */
data class Glow(val position: Vec3, val size: Float, val intensity: Float = 1f)

/** Готовая 3D-модель детали в локальных координатах. */
class PartModel(
    val items: List<DrawItem>,
    val glows: List<Glow> = emptyList(),
    /** Точки, откуда RGB-подсветка освещает соседние детали. */
    val lights: List<Vec3> = emptyList(),
    /** Направление, откуда деталь «прилетает» при замене. */
    val enterOffset: Vec3 = Vec3(14f, 0f, 0f),
)

/** Деталь, размещённая в сцене. */
class PartInstance(
    /** Ключ слота (категория): по нему рендерер понимает, что деталь заменили. */
    val slot: String,
    /** id детали (если id тот же — анимация замены не нужна). */
    val partId: String,
    /** Положение детали в сцене. */
    val anchor: Vec3,
    val model: PartModel,
)

/** Вся сцена: детали + окружение + параметры камеры. */
class SceneSpec(
    val parts: List<PartInstance>,
    val environment: PartModel,
    /** Точка, вокруг которой вращается камера. */
    val focus: Vec3,
    /** Радиус сцены — по нему подбирается расстояние камеры. */
    val radius: Float,
    /** Нейтральная лампа внутри корпуса (чтобы детали было видно без RGB). */
    val interiorLight: Vec3? = null,
)
