package dev.optimod.culling;

import dev.optimod.Optimod;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientLevel;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Связующее звено между рендер-потоком и фоновым потоком отсечения.
 * <p>
 * Схема работы:
 * <ol>
 *     <li>Рендер (главный поток) для каждой сущности/блок-сущности вызывает {@link #isCulled}:
 *     объект получает отметку «нужен» и попадает в общий набор {@link #TRACKED};</li>
 *     <li>фоновый поток {@link OcclusionCullingThread} по кругу проверяет лучами, видна ли цель
 *     из камеры, и записывает результат в сам объект;</li>
 *     <li>рендер читает готовый результат — проверка не тормозит кадр.</li>
 * </ol>
 * Объекты, о которых рендер давно не спрашивал (ушли из зоны видимости, выгружены),
 * фоновый поток сам удаляет из набора.
 */
public final class CullingManager {
	/** Объекты, видимость которых проверяет фоновый поток. Потокобезопасный набор. */
	static final Set<OptimodCullable> TRACKED = ConcurrentHashMap.newKeySet();

	// Данные для фонового потока (пишет главный поток)
	static volatile ClientLevel level;
	static volatile double cameraX;
	static volatile double cameraY;
	static volatile double cameraZ;
	/** Когда камера обновлялась в последний раз — устаревшим данным поток не доверяет. */
	static volatile long cameraUpdatedAt;

	/** Время начала текущего кадра, мс (главный поток). */
	private static long frameTimeMillis;

	private static OcclusionCullingThread thread;

	private CullingManager() {
	}

	/** Начало кадра: запоминаем камеру и мир для фонового потока. */
	public static void onFrameStart(double x, double y, double z) {
		cameraX = x;
		cameraY = y;
		cameraZ = z;
		frameTimeMillis = System.currentTimeMillis();
		cameraUpdatedAt = frameTimeMillis;
		level = Minecraft.getInstance().level;
	}

	/**
	 * Скрыт ли объект стенами. Заодно ставит его в очередь проверки.
	 * Вызывается из главного потока для каждого объекта каждый кадр — без аллокаций.
	 */
	public static boolean isCulled(OptimodCullable cullable) {
		long now = frameTimeMillis != 0L ? frameTimeMillis : System.currentTimeMillis();
		cullable.optimod$setLastRequest(now);

		if (!cullable.optimod$isTracked()) {
			// Новый объект: пока не проверен — считаем видимым
			cullable.optimod$setTracked(true);
			TRACKED.add(cullable);
			ensureThreadStarted();
			return false;
		}
		return cullable.optimod$isCulled();
	}

	/** Смена мира: старые объекты больше не нужны. */
	public static void onLevelChanged(ClientLevel newLevel) {
		level = newLevel;
		cameraUpdatedAt = 0L;
		clearTracked();
	}

	/** Сбрасывает все результаты — например, когда модуль выключили. */
	static void clearTracked() {
		for (OptimodCullable cullable : TRACKED) {
			// Порядок важен: сначала убрать из набора, потом снять флаг (см. isCulled)
			TRACKED.remove(cullable);
			cullable.optimod$setTracked(false);
			cullable.optimod$setCulled(false);
			cullable.optimod$setHiddenPasses(0);
		}
	}

	private static synchronized void ensureThreadStarted() {
		if (thread == null || !thread.isAlive()) {
			thread = new OcclusionCullingThread();
			thread.start();
			Optimod.LOGGER.info("Запущен фоновый поток отсечения невидимых сущностей");
		}
	}

	/** Остановка при выходе из игры. */
	public static synchronized void shutdown() {
		if (thread != null) {
			thread.shutdown();
			thread = null;
		}
		clearTracked();
	}
}
