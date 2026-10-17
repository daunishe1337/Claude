package dev.optimod.particle;

import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.render.FrameContext;
import dev.optimod.stats.OptimodStats;
import net.minecraft.client.renderer.culling.Frustum;

/**
 * Решает, можно ли создать новую частицу.
 * Вызывается из главного потока при каждом создании частицы — без аллокаций.
 * <p>
 * Порядок проверок: дальность (модуль 2) → лимит количества (модуль 3) →
 * вне поля зрения (модуль 3, по желанию).
 * <p>
 * Про «невидимые» частицы: уже созданные частицы вне пирамиды видимости ваниль 26.3
 * не рисует сама (проверка {@code Frustum#pointInFrustum} при извлечении группы частиц).
 * Мы дополнительно можем не создавать их вовсе — тогда экономится и тик (движение,
 * столкновения с блоками), а не только отрисовка.
 */
public final class ParticleLimiter {
	private ParticleLimiter() {
	}

	/** Быстрый выход, если ни один связанный модуль не активен. */
	public static boolean isActive() {
		return Modules.renderDistance || Modules.particles;
	}

	/**
	 * @return false — частицу не создавать
	 */
	public static boolean allowSpawn(double x, double y, double z) {
		OptimodConfig config = ConfigManager.get();

		// Модуль 3: лимит одновременно живых частиц (не зависит от камеры)
		if (Modules.particles && ParticleTracker.liveCount() >= config.particles.maxParticles) {
			OptimodStats.curParticlesByLimit++;
			return false;
		}

		if (!FrameContext.valid) {
			return true;
		}

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

		// Модуль 3: не создавать частицы позади камеры / вне обзора (по желанию)
		if (Modules.particles && config.particles.skipOutOfViewSpawns) {
			double minDistance = config.particles.outOfViewMinDistance;
			Frustum frustum = FrameContext.frustum;
			if (frustum != null && distanceSq > minDistance * minDistance && !frustum.pointInFrustum(x, y, z)) {
				OptimodStats.curParticlesOutOfView++;
				return false;
			}
		}
		return true;
	}
}
