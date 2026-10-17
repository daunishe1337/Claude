package dev.optimod.particle;

import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.render.FrameContext;
import dev.optimod.stats.OptimodStats;

/**
 * Решает, можно ли создать новую частицу.
 * Вызывается из главного потока при каждом создании частицы — без аллокаций.
 */
public final class ParticleLimiter {
	private ParticleLimiter() {
	}

	/** Быстрый выход, если ни один связанный модуль не активен. */
	public static boolean isActive() {
		return Modules.renderDistance;
	}

	/**
	 * @return false — частицу не создавать
	 */
	public static boolean allowSpawn(double x, double y, double z) {
		if (!FrameContext.valid) {
			return true;
		}

		OptimodConfig config = ConfigManager.get();
		double dx = x - FrameContext.cameraX;
		double dy = y - FrameContext.cameraY;
		double dz = z - FrameContext.cameraZ;
		double distanceSq = dx * dx + dy * dy + dz * dz;

		// Модуль 2: дальность появления частиц
		if (Modules.renderDistance) {
			double max = config.renderDistance.particleDistance;
			if (distanceSq > max * max) {
				OptimodStats.curParticlesByDistance++;
				return false;
			}
		}
		return true;
	}
}
