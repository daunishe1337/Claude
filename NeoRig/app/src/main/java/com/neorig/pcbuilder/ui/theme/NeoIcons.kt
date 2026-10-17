package com.neorig.pcbuilder.ui.theme

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.PathBuilder
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp
import com.neorig.pcbuilder.domain.model.Category

/**
 * Собственные контурные иконки (24×24) — без чужих логотипов и без тяжёлой
 * библиотеки material-icons-extended.
 */
object NeoIcons {

    val Cpu: ImageVector by lazy {
        icon("cpu") {
            rect(6f, 6f, 12f, 12f, 2f)
            rect(9.5f, 9.5f, 5f, 5f, 1f)
            for (p in listOf(9f, 12f, 15f)) {
                line(p, 3f, p, 6f); line(p, 18f, p, 21f)
                line(3f, p, 6f, p); line(18f, p, 21f, p)
            }
        }
    }

    val Gpu: ImageVector by lazy {
        icon("gpu") {
            rect(2.5f, 6f, 19f, 11f, 2f)
            circle(8.5f, 11.5f, 3f)
            circle(15.5f, 11.5f, 3f)
            line(6f, 17f, 6f, 19.5f); line(10f, 17f, 10f, 19.5f)
        }
    }

    val Motherboard: ImageVector by lazy {
        icon("motherboard") {
            rect(3f, 3f, 18f, 18f, 2.5f)
            rect(6.5f, 6.5f, 6f, 6f, 1f)
            line(15.5f, 6.5f, 15.5f, 12.5f); line(18f, 6.5f, 18f, 12.5f)
            line(6.5f, 16.5f, 15f, 16.5f)
        }
    }

    val Ram: ImageVector by lazy {
        icon("ram") {
            rect(2.5f, 7f, 19f, 9f, 1.5f)
            rect(5f, 9.5f, 3f, 4f, 0.5f); rect(10.5f, 9.5f, 3f, 4f, 0.5f); rect(16f, 9.5f, 3f, 4f, 0.5f)
            for (x in listOf(5f, 8f, 11f, 14f, 17f)) line(x, 16f, x, 18.5f)
        }
    }

    val Ssd: ImageVector by lazy {
        icon("ssd") {
            rect(2f, 8f, 20f, 8f, 1.5f)
            rect(5f, 10f, 7f, 4f, 0.5f)
            circle(18.5f, 12f, 1.2f)
        }
    }

    val Psu: ImageVector by lazy {
        icon("psu") {
            rect(3f, 5f, 18f, 14f, 2f)
            circle(10f, 12f, 4f)
            circle(10f, 12f, 1f)
            line(16.5f, 9f, 18.5f, 9f); line(16.5f, 12f, 18.5f, 12f); line(16.5f, 15f, 18.5f, 15f)
        }
    }

    val Cooler: ImageVector by lazy {
        icon("cooler") {
            rect(3f, 3f, 18f, 18f, 3f)
            circle(12f, 12f, 6.5f)
            circle(12f, 12f, 1.6f)
            line(12f, 10.4f, 12f, 6f); line(13.4f, 12.8f, 17.2f, 15f); line(10.6f, 12.8f, 6.8f, 15f)
        }
    }

    val Case: ImageVector by lazy {
        icon("case") {
            rect(6f, 2f, 12f, 20f, 2f)
            circle(12f, 7.5f, 2.5f)
            circle(12f, 14f, 2.5f)
            line(9f, 19f, 15f, 19f)
        }
    }

    val Monitor: ImageVector by lazy {
        icon("monitor") {
            rect(2f, 3.5f, 20f, 13f, 2f)
            line(12f, 16.5f, 12f, 20f)
            line(8f, 20.5f, 16f, 20.5f)
        }
    }

    /** Вкладка «Сборка»: ползунки настройки. */
    val Tune: ImageVector by lazy {
        icon("tune") {
            line(3f, 6f, 21f, 6f); circle(9f, 6f, 2.2f)
            line(3f, 12f, 21f, 12f); circle(15.5f, 12f, 2.2f)
            line(3f, 18f, 21f, 18f); circle(7f, 18f, 2.2f)
        }
    }

    /** Вкладка «3D»: изометрический куб. */
    val Cube: ImageVector by lazy {
        icon("cube") {
            moveTo(12f, 2.8f); lineTo(20.2f, 7.4f); lineTo(20.2f, 16.6f); lineTo(12f, 21.2f)
            lineTo(3.8f, 16.6f); lineTo(3.8f, 7.4f); close()
            line(3.8f, 7.4f, 12f, 12f); line(12f, 12f, 20.2f, 7.4f); line(12f, 12f, 12f, 21.2f)
        }
    }

    /** Вкладка «Мои сборки»: закладка. */
    val Bookmark: ImageVector by lazy {
        icon("bookmark") {
            moveTo(6.5f, 3f); lineTo(17.5f, 3f); lineTo(17.5f, 21f); lineTo(12f, 16.8f); lineTo(6.5f, 21f); close()
        }
    }

    /** Сравнение: две колонки. */
    val Compare: ImageVector by lazy {
        icon("compare") {
            rect(3f, 4f, 7.5f, 16f, 1.5f)
            rect(13.5f, 4f, 7.5f, 16f, 1.5f)
            line(5.5f, 9f, 8f, 9f); line(16f, 9f, 18.5f, 9f)
            line(5.5f, 13f, 8f, 13f); line(16f, 13f, 18.5f, 13f)
        }
    }

    /** Молния: потребление. */
    val Bolt: ImageVector by lazy {
        icon("bolt") {
            moveTo(13f, 2.5f); lineTo(5f, 13.5f); lineTo(11f, 13.5f); lineTo(10f, 21.5f); lineTo(19f, 10f); lineTo(13f, 10f); close()
        }
    }

    /** Волшебная палочка: автоисправление. */
    val Wand: ImageVector by lazy {
        icon("wand") {
            line(4f, 20f, 14.5f, 9.5f)
            line(16f, 3f, 16f, 6f); line(14.5f, 4.5f, 17.5f, 4.5f)
            line(20f, 8f, 20f, 10f); line(19f, 9f, 21f, 9f)
            line(10f, 4f, 10f, 5.6f); line(9.2f, 4.8f, 10.8f, 4.8f)
        }
    }

    fun forCategory(category: Category): ImageVector = when (category) {
        Category.CPU -> Cpu
        Category.MOTHERBOARD -> Motherboard
        Category.GPU -> Gpu
        Category.RAM -> Ram
        Category.SSD -> Ssd
        Category.PSU -> Psu
        Category.COOLER -> Cooler
        Category.CASE -> Case
        Category.MONITOR -> Monitor
    }

    // ---- Помощники построения путей ----------------------------------------------------------

    private fun icon(name: String, block: PathBuilder.() -> Unit): ImageVector =
        ImageVector.Builder(
            name = name,
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).apply {
            path(
                fill = null,
                stroke = SolidColor(Color.Black),
                strokeLineWidth = 1.7f,
                strokeLineCap = StrokeCap.Round,
                strokeLineJoin = StrokeJoin.Round,
                pathBuilder = block,
            )
        }.build()

    private fun PathBuilder.line(x1: Float, y1: Float, x2: Float, y2: Float) {
        moveTo(x1, y1)
        lineTo(x2, y2)
    }

    private fun PathBuilder.rect(x: Float, y: Float, w: Float, h: Float, r: Float) {
        moveTo(x + r, y)
        lineTo(x + w - r, y)
        arcToRelative(r, r, 0f, false, true, r, r)
        lineTo(x + w, y + h - r)
        arcToRelative(r, r, 0f, false, true, -r, r)
        lineTo(x + r, y + h)
        arcToRelative(r, r, 0f, false, true, -r, -r)
        lineTo(x, y + r)
        arcToRelative(r, r, 0f, false, true, r, -r)
        close()
    }

    private fun PathBuilder.circle(cx: Float, cy: Float, r: Float) {
        moveTo(cx - r, cy)
        arcToRelative(r, r, 0f, true, true, 2 * r, 0f)
        arcToRelative(r, r, 0f, true, true, -2 * r, 0f)
        close()
    }
}
