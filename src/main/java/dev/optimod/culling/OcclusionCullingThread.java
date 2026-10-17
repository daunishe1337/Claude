package dev.optimod.culling;

import dev.optimod.Optimod;
import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.stats.OptimodStats;
import it.unimi.dsi.fastutil.longs.Long2ByteOpenHashMap;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.AABB;

/**
 * Фоновый поток проверки видимости.
 * <p>
 * Для каждой цели из камеры пускаются лучи в центр и углы её габаритной рамки.
 * Луч идёт по сетке блоков (алгоритм 3D-DDA, Amanatides–Woo); если на пути встречается
 * непрозрачный полный блок ({@code BlockState#isSolidRender}) — луч перекрыт.
 * Цель видима, если хотя бы один луч дошёл до рамки.
 * <p>
 * Чтение мира из чужого потока: клиентские чанки хранятся в атомарных массивах и читаются
 * без блокировок (так же делает мод EntityCulling). Редкие гонки при выгрузке чанка
 * перехватываются — в этом случае цель считается видимой (безопасный вариант).
 * <p>
 * Внутри прохода нет аллокаций: координаты в примитивах, одна изменяемая позиция блока
 * и кэш непрозрачности, который очищается, но не пересоздаётся.
 */
final class OcclusionCullingThread extends Thread {
	/** Объект, о котором рендер не спрашивал столько мс, удаляется из проверки. */
	private static final long EVICT_AFTER_MS = 1000L;
	/** Данные камеры старше этого считаются устаревшими (игра на паузе в меню, нет мира). */
	private static final long CAMERA_STALE_MS = 1000L;
	/** Сколько проходов подряд цель должна быть невидимой, чтобы её скрыть (защита от мерцания). */
	private static final int HIDDEN_PASSES_TO_CULL = 2;
	/** Ближе этого (в квадрате блоков) всё считается видимым. */
	private static final double ALWAYS_VISIBLE_DISTANCE_SQ = 3.0 * 3.0;
	/** Слишком большие объекты не проверяем (как в Sodium) — слишком много лучей и мало пользы. */
	private static final double MAX_BOX_VOLUME = 16.0 * 16.0 * 16.0 * 8.0;
	/** Насколько точки-цели сдвигаются от углов внутрь рамки (доля от размера). */
	private static final double CORNER_INSET = 0.1;
	/** Предел размера кэша непрозрачности за один проход. */
	private static final int MAX_CACHE_SIZE = 262_144;

	private static final byte TRANSPARENT = 1;
	private static final byte OPAQUE = 2;

	private volatile boolean running = true;

	private final BlockPos.MutableBlockPos mutablePos = new BlockPos.MutableBlockPos();
	private final Long2ByteOpenHashMap opacityCache = new Long2ByteOpenHashMap(16_384);

	/** Мир текущего прохода и его границы по высоте. */
	private ClientLevel level;
	private int minBuildY;
	private int maxBuildY;
	/** Были ли скрытые объекты на прошлом проходе (чтобы сбросить их при выключении модуля). */
	private boolean hadResults;
	private boolean errorLogged;

	OcclusionCullingThread() {
		super("Optimod Occlusion Culling");
		setDaemon(true);
		setPriority(Thread.NORM_PRIORITY - 2);
	}

	void shutdown() {
		running = false;
		interrupt();
	}

	@Override
	public void run() {
		while (running) {
			long start = System.nanoTime();
			long sleepMs;
			try {
				sleepMs = runPass();
			} catch (Throwable t) {
				// Поток не должен умирать из-за редкой ошибки чтения мира
				if (!errorLogged) {
					errorLogged = true;
					Optimod.LOGGER.warn("Ошибка в потоке отсечения — проход пропущен (сообщение выводится один раз)", t);
				}
				sleepMs = 50L;
			}
			OptimodStats.cullPassMicros = (int) ((System.nanoTime() - start) / 1000L);

			try {
				Thread.sleep(Math.max(1L, sleepMs));
			} catch (InterruptedException e) {
				if (!running) {
					return;
				}
			}
		}
	}

	/** Один проход по всем целям. Возвращает паузу до следующего прохода, мс. */
	private long runPass() {
		OptimodConfig.EntityCulling config = ConfigManager.get().entityCulling;
		ClientLevel currentLevel = CullingManager.level;
		long now = System.currentTimeMillis();

		if (!Modules.entityCulling || currentLevel == null || now - CullingManager.cameraUpdatedAt > CAMERA_STALE_MS) {
			if (hadResults) {
				CullingManager.clearTracked();
				hadResults = false;
			}
			OptimodStats.cullTracked = 0;
			OptimodStats.cullHidden = 0;
			return 100L;
		}

		level = currentLevel;
		minBuildY = currentLevel.getMinY();
		maxBuildY = currentLevel.getMaxY();
		opacityCache.clear();

		double camX = CullingManager.cameraX;
		double camY = CullingManager.cameraY;
		double camZ = CullingManager.cameraZ;
		double maxDistance = config.maxCheckDistance;
		double maxDistanceSq = maxDistance * maxDistance;

		// Камера внутри непрозрачного блока (зритель в стене) — ничего не скрываем
		boolean cameraInsideOpaque = isOpaque(floor(camX), floor(camY), floor(camZ));

		int tracked = 0;
		int hidden = 0;

		for (OptimodCullable cullable : CullingManager.TRACKED) {
			if (!running) {
				break;
			}

			if (now - cullable.optimod$getLastRequest() > EVICT_AFTER_MS) {
				// Порядок важен: сначала убрать из набора, потом снять флаг (см. CullingManager#isCulled)
				CullingManager.TRACKED.remove(cullable);
				cullable.optimod$setTracked(false);
				cullable.optimod$setCulled(false);
				cullable.optimod$setHiddenPasses(0);
				continue;
			}

			tracked++;
			boolean visible;
			try {
				visible = cameraInsideOpaque || isTargetVisible(cullable, camX, camY, camZ, maxDistanceSq);
			} catch (RuntimeException e) {
				visible = true;
			}

			if (visible) {
				cullable.optimod$setHiddenPasses(0);
				cullable.optimod$setCulled(false);
			} else {
				int passes = cullable.optimod$getHiddenPasses() + 1;
				cullable.optimod$setHiddenPasses(passes);
				if (passes >= HIDDEN_PASSES_TO_CULL) {
					cullable.optimod$setCulled(true);
					hidden++;
				}
			}
		}

		level = null;
		hadResults = true;
		OptimodStats.cullTracked = tracked;
		OptimodStats.cullHidden = hidden;
		return config.updateIntervalMs;
	}

	private boolean isTargetVisible(OptimodCullable cullable, double camX, double camY, double camZ, double maxDistanceSq) {
		double minX, minY, minZ, maxX, maxY, maxZ;

		if (cullable instanceof Entity entity) {
			// AABB неизменяемый, ссылка на него читается атомарно — безопасно из другого потока
			AABB box = entity.getBoundingBox();
			minX = box.minX - 0.5;
			minY = box.minY - 0.5;
			minZ = box.minZ - 0.5;
			maxX = box.maxX + 0.5;
			maxY = box.maxY + 0.5;
			maxZ = box.maxZ + 0.5;
		} else if (cullable instanceof BlockEntity blockEntity) {
			BlockPos pos = blockEntity.getBlockPos();
			// С запасом: баннеры и таблички выходят за пределы своего блока
			minX = pos.getX() - 0.5;
			minY = pos.getY() - 1.0;
			minZ = pos.getZ() - 0.5;
			maxX = pos.getX() + 1.5;
			maxY = pos.getY() + 2.0;
			maxZ = pos.getZ() + 1.5;
		} else {
			return true;
		}

		if ((maxX - minX) * (maxY - minY) * (maxZ - minZ) > MAX_BOX_VOLUME) {
			return true;
		}

		// Камера внутри рамки
		if (camX >= minX && camX <= maxX && camY >= minY && camY <= maxY && camZ >= minZ && camZ <= maxZ) {
			return true;
		}

		// Расстояние до ближайшей точки рамки
		double nearestX = Math.max(minX, Math.min(camX, maxX)) - camX;
		double nearestY = Math.max(minY, Math.min(camY, maxY)) - camY;
		double nearestZ = Math.max(minZ, Math.min(camZ, maxZ)) - camZ;
		double distanceSq = nearestX * nearestX + nearestY * nearestY + nearestZ * nearestZ;
		if (distanceSq < ALWAYS_VISIBLE_DISTANCE_SQ || distanceSq > maxDistanceSq) {
			// Очень близко — точно видно; слишком далеко — не наша забота (это модуль дальности)
			return true;
		}

		double centerX = (minX + maxX) * 0.5;
		double centerY = (minY + maxY) * 0.5;
		double centerZ = (minZ + maxZ) * 0.5;

		// Сначала центр — чаще всего его достаточно
		if (isRayClear(camX, camY, camZ, centerX, centerY, centerZ, minX, minY, minZ, maxX, maxY, maxZ)) {
			return true;
		}

		double insetX = (maxX - minX) * CORNER_INSET;
		double insetY = (maxY - minY) * CORNER_INSET;
		double insetZ = (maxZ - minZ) * CORNER_INSET;

		for (int corner = 0; corner < 8; corner++) {
			double pointX = (corner & 1) == 0 ? minX + insetX : maxX - insetX;
			double pointY = (corner & 2) == 0 ? minY + insetY : maxY - insetY;
			double pointZ = (corner & 4) == 0 ? minZ + insetZ : maxZ - insetZ;
			if (isRayClear(camX, camY, camZ, pointX, pointY, pointZ, minX, minY, minZ, maxX, maxY, maxZ)) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Проходит лучом от камеры к точке по сетке блоков (3D-DDA).
	 *
	 * @return true, если луч достиг рамки цели раньше, чем упёрся в непрозрачный блок
	 */
	private boolean isRayClear(double fromX, double fromY, double fromZ, double toX, double toY, double toZ,
							   double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
		double dx = toX - fromX;
		double dy = toY - fromY;
		double dz = toZ - fromZ;

		int x = floor(fromX);
		int y = floor(fromY);
		int z = floor(fromZ);
		int endX = floor(toX);
		int endY = floor(toY);
		int endZ = floor(toZ);

		int stepX = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
		int stepY = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
		int stepZ = dz > 0 ? 1 : (dz < 0 ? -1 : 0);

		// Параметр t ∈ [0, 1] вдоль отрезка: сколько t нужно, чтобы пересечь один блок по оси
		double tDeltaX = stepX != 0 ? Math.abs(1.0 / dx) : Double.MAX_VALUE;
		double tDeltaY = stepY != 0 ? Math.abs(1.0 / dy) : Double.MAX_VALUE;
		double tDeltaZ = stepZ != 0 ? Math.abs(1.0 / dz) : Double.MAX_VALUE;

		// Значение t на ближайшей границе блока по каждой оси
		double tMaxX = stepX > 0 ? (x + 1 - fromX) * tDeltaX : (stepX < 0 ? (fromX - x) * tDeltaX : Double.MAX_VALUE);
		double tMaxY = stepY > 0 ? (y + 1 - fromY) * tDeltaY : (stepY < 0 ? (fromY - y) * tDeltaY : Double.MAX_VALUE);
		double tMaxZ = stepZ > 0 ? (z + 1 - fromZ) * tDeltaZ : (stepZ < 0 ? (fromZ - z) * tDeltaZ : Double.MAX_VALUE);

		// Число шагов до блока-цели — ограничитель на случай ошибок округления
		int steps = Math.abs(endX - x) + Math.abs(endY - y) + Math.abs(endZ - z);

		for (int i = 0; i < steps; i++) {
			if (tMaxX < tMaxY) {
				if (tMaxX < tMaxZ) {
					x += stepX;
					tMaxX += tDeltaX;
				} else {
					z += stepZ;
					tMaxZ += tDeltaZ;
				}
			} else if (tMaxY < tMaxZ) {
				y += stepY;
				tMaxY += tDeltaY;
			} else {
				z += stepZ;
				tMaxZ += tDeltaZ;
			}

			// Вошли в рамку цели — луч дошёл
			if (x + 1 > minX && x < maxX && y + 1 > minY && y < maxY && z + 1 > minZ && z < maxZ) {
				return true;
			}
			if (isOpaque(x, y, z)) {
				return false;
			}
		}
		return true;
	}

	/** Непрозрачен ли блок. Результат кэшируется на время прохода. */
	private boolean isOpaque(int x, int y, int z) {
		if (y < minBuildY || y >= maxBuildY) {
			return false;
		}

		long key = packPos(x, y, z);
		byte cached = opacityCache.get(key);
		if (cached != 0) {
			return cached == OPAQUE;
		}

		if (opacityCache.size() >= MAX_CACHE_SIZE) {
			opacityCache.clear();
		}

		BlockState state = level.getBlockState(mutablePos.set(x, y, z));
		boolean opaque = state != null && state.isSolidRender();
		opacityCache.put(key, opaque ? OPAQUE : TRANSPARENT);
		return opaque;
	}

	/** Упаковка координат блока в long: 26 бит X, 26 бит Z, 12 бит Y (как в ванилле). */
	private static long packPos(int x, int y, int z) {
		return ((long) (x & 0x3FFFFFF) << 38) | ((long) (z & 0x3FFFFFF) << 12) | (y & 0xFFFL);
	}

	private static int floor(double value) {
		int truncated = (int) value;
		return value < truncated ? truncated - 1 : truncated;
	}
}
