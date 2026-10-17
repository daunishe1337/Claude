package dev.optimod.memory;

import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import net.minecraft.client.model.geom.ModelPart;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Кэш одинаковых кубов моделей сущностей.
 * <p>
 * При запекании моделей ({@code CubeDefinition#bake}) игра создаёт отдельный объект
 * {@code ModelPart.Cube} (с массивами вершин и полигонов) для каждой копии одинакового куба —
 * у разных мобов, слоёв брони, вариантов моделей их тысячи. Куб после создания не меняется,
 * поэтому одинаковые можно разделять. Тот же приём использует ModernFix (compact_entity_models).
 * <p>
 * Запекание может идти в нескольких потоках — отсюда ConcurrentHashMap и атомарные счётчики.
 */
public final class CubeCache {
	/** Защита от бесконечного роста при постоянной смене ресурс-паков. */
	private static final int MAX_ENTRIES = 100_000;

	private static final ConcurrentHashMap<CubeKey, ModelPart.Cube> CACHE = new ConcurrentHashMap<>();
	private static final AtomicInteger REUSED = new AtomicInteger();
	private static final AtomicInteger CREATED = new AtomicInteger();

	/** Параметры конструктора куба. Record сравнивает float через Float.compare — корректно для ключа. */
	public record CubeKey(int texU, int texV,
						  float originX, float originY, float originZ,
						  float dimensionX, float dimensionY, float dimensionZ,
						  float growX, float growY, float growZ,
						  boolean mirror, float texScaleU, float texScaleV,
						  Set<?> visibleFaces) {
	}

	private CubeCache() {
	}

	public static boolean isEnabled() {
		OptimodConfig.Memory config = ConfigManager.get().memory;
		return config.enabled && config.deduplicateModelCubes;
	}

	public static ModelPart.Cube get(CubeKey key) {
		ModelPart.Cube cube = CACHE.get(key);
		if (cube != null) {
			REUSED.incrementAndGet();
		}
		return cube;
	}

	/** Кладёт новый куб; если другой поток успел раньше — возвращает его куб. */
	public static ModelPart.Cube store(CubeKey key, ModelPart.Cube cube) {
		CREATED.incrementAndGet();
		if (CACHE.size() >= MAX_ENTRIES) {
			return cube;
		}
		ModelPart.Cube previous = CACHE.putIfAbsent(key, cube);
		return previous != null ? previous : cube;
	}

	/** Сколько раз вместо нового объекта отдан уже существующий. */
	public static int reusedCount() {
		return REUSED.get();
	}

	/** Сколько уникальных кубов создано. */
	public static int uniqueCount() {
		return CACHE.size();
	}

	/** Сработал ли хук хотя бы раз (если нет — сигнатура в этой версии игры не совпала). */
	public static boolean hasSeenAnyCube() {
		return CREATED.get() > 0 || REUSED.get() > 0;
	}
}
