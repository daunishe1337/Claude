package dev.optimod.culling;

/**
 * «Утиный» интерфейс: миксин добавляет его в {@code Entity} и {@code BlockEntity},
 * чтобы хранить результат проверки видимости прямо в объекте (без HashMap и без аллокаций).
 * <p>
 * Префикс {@code optimod$} исключает совпадение имён с методами игры и других модов.
 */
public interface OptimodCullable {
	/** Скрыт ли объект (последний результат фонового потока). */
	boolean optimod$isCulled();

	void optimod$setCulled(boolean culled);

	/** Стоит ли объект в очереди фонового потока. */
	boolean optimod$isTracked();

	void optimod$setTracked(boolean tracked);

	/** Когда рендер в последний раз спрашивал про объект, мс. */
	long optimod$getLastRequest();

	void optimod$setLastRequest(long timeMillis);

	/** Сколько проходов подряд объект был невидим (только фоновый поток). */
	int optimod$getHiddenPasses();

	void optimod$setHiddenPasses(int passes);
}
