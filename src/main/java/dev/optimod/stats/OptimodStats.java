package dev.optimod.stats;

/**
 * Счётчики эффекта мода для отладочного оверлея.
 * <p>
 * Поля без volatile пишутся и читаются только в главном (рендер-) потоке.
 * Счётчики «за кадр» накапливаются в {@code cur*}, а в начале следующего кадра
 * переносятся в {@code last*} — оверлей показывает именно их.
 */
public final class OptimodStats {
	// --- Модуль 1: отсечение невидимых (за кадр) ---
	public static int curEntitiesOccluded;
	public static int curBlockEntitiesOccluded;
	public static int lastEntitiesOccluded;
	public static int lastBlockEntitiesOccluded;

	// --- Модуль 2: дальность отрисовки (за кадр) ---
	public static int curEntitiesByDistance;
	public static int curBlockEntitiesByDistance;
	public static int lastEntitiesByDistance;
	public static int lastBlockEntitiesByDistance;

	// --- Частицы: пропущено за последнюю секунду (модули 2 и 3) ---
	public static int curParticlesByDistance;
	public static int lastParticlesByDistance;

	// --- Фоновый поток отсечения (пишет фоновый поток) ---
	/** Длительность последнего прохода, микросекунды. */
	public static volatile int cullPassMicros;
	/** Сколько объектов отслеживается. */
	public static volatile int cullTracked;
	/** Сколько из них сейчас скрыто. */
	public static volatile int cullHidden;

	private OptimodStats() {
	}

	/** Вызывается в начале извлечения кадра (LevelExtractor). */
	public static void beginFrame() {
		lastEntitiesOccluded = curEntitiesOccluded;
		lastBlockEntitiesOccluded = curBlockEntitiesOccluded;
		lastEntitiesByDistance = curEntitiesByDistance;
		lastBlockEntitiesByDistance = curBlockEntitiesByDistance;
		curEntitiesOccluded = 0;
		curBlockEntitiesOccluded = 0;
		curEntitiesByDistance = 0;
		curBlockEntitiesByDistance = 0;
	}

	/** Вызывается раз в секунду (каждые 20 тиков) — для счётчиков частиц. */
	public static void endSecond() {
		lastParticlesByDistance = curParticlesByDistance;
		curParticlesByDistance = 0;
	}
}
