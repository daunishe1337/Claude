package com.neorig.pcbuilder.ui.viewer

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.neorig.pcbuilder.NeoRigApplication
import com.neorig.pcbuilder.domain.model.PcBuild
import com.neorig.pcbuilder.render.gl.PcSceneView
import com.neorig.pcbuilder.render.scene.RgbState

/**
 * 3D-вид сборки внутри Compose (обёртка над [PcSceneView]).
 *
 * @param interactive жесты вращения/зума (в превью — выключены)
 * @param resetSignal при изменении значения камера плавно возвращается в исходный ракурс
 * @param onTap      одиночный тап (превью открывает полноэкранный 3D)
 */
@Composable
fun Pc3DView(
    build: PcBuild,
    rgb: RgbState,
    modifier: Modifier = Modifier,
    interactive: Boolean = true,
    autoRotate: Boolean = false,
    antialiasing: Boolean = true,
    resetSignal: Int = 0,
    onTap: (() -> Unit)? = null,
) {
    val context = LocalContext.current
    val composer = remember(context) { (context.applicationContext as NeoRigApplication).container.sceneComposer }
    val lifecycleOwner = LocalLifecycleOwner.current
    var view by remember { mutableStateOf<PcSceneView?>(null) }

    // Конфигурация EGL выбирается один раз, поэтому смена сглаживания пересоздаёт View
    key(antialiasing) {
        AndroidView(
            modifier = modifier,
            factory = { ctx ->
                PcSceneView(ctx, composer, antialiasing).also { view = it }
            },
            update = { v ->
                v.interactive = interactive
                v.onTap = onTap
                v.setRgb(rgb)
                v.setAutoRotate(autoRotate)
                v.setBuild(build)
            },
        )
    }

    // GLSurfaceView нужно ставить на паузу вместе с экраном, иначе он тратит батарею в фоне
    val current = view
    DisposableEffect(lifecycleOwner, current) {
        if (current == null) return@DisposableEffect onDispose { }
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> current.onResume()
                Lifecycle.Event.ON_PAUSE -> current.onPause()
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    LaunchedEffect(resetSignal) {
        if (resetSignal != 0) view?.resetCamera()
    }
}
