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
		curEntitiesOccluded = 0;
		curBlockEntitiesOccluded = 0;
	}
}
