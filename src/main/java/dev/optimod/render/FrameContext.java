package dev.optimod.render;

import dev.optimod.culling.CullingManager;
import dev.optimod.stats.OptimodStats;
import net.minecraft.client.Camera;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.world.phys.Vec3;

/**
 * Данные текущего кадра: позиция камеры и пирамида видимости (frustum).
 * Заполняется в начале извлечения сущностей ({@code LevelExtractor#extractVisibleEntities}).
 * <p>
 * Используется только в главном потоке, поэтому поля обычные (не volatile).
 */
public final class FrameContext {
	public static Frustum frustum;
	public static double cameraX;
	public static double cameraY;
	public static double cameraZ;
	/** true после первого кадра в мире. */
	public static boolean valid;

	private FrameContext() {
	}

	public static void begin(Camera camera, Frustum cullingFrustum) {
		Vec3 position = camera.position();
		cameraX = position.x;
		cameraY = position.y;
		cameraZ = position.z;
		frustum = cullingFrustum;
		valid = true;

		OptimodStats.beginFrame();
		CullingManager.onFrameStart(cameraX, cameraY, cameraZ);
	}

	/** Сброс при выходе из мира. */
	public static void reset() {
		frustum = null;
		valid = false;
	}
}
