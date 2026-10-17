package dev.optimod.config;

import java.util.ArrayList;
import java.util.List;

/**
 * Схема файла {@code config/optimod.json}.
 * <p>
 * Поля публичные — так их сериализует Gson. Имена подобраны так, чтобы файл читался без
 * документации. Значения по умолчанию безопасные: они не меняют внешний вид игры
 * в обычных условиях, а лишь убирают лишнюю работу.
 */
public final class OptimodConfig {
	/** Версия схемы — пригодится для миграций в будущих версиях. */
	public int configVersion = 1;

	public EntityCulling entityCulling = new EntityCulling();
	public RenderDistance renderDistance = new RenderDistance();
	public Particles particles = new Particles();
	public Background background = new Background();
	public Memory memory = new Memory();
	public Animations animations = new Animations();
	public DebugOverlay debugOverlay = new DebugOverlay();
	public Compatibility compatibility = new Compatibility();

	/** Модуль 1: отсечение сущностей и блок-сущностей, скрытых за стенами. */
	public static final class EntityCulling {
		public boolean enabled = true;
		/** Отсекать обычные сущности (мобы, предметы, рамки и т.п.). */
		public boolean cullEntities = true;
		/** Отсекать блок-сущности (сундуки, таблички, баннеры...). */
		public boolean cullBlockEntities = true;
		/** Дальше этого расстояния (в блоках) проверка не делается — объект считается видимым. */
		public int maxCheckDistance = 128;
		/** Пауза между проходами фонового потока, мс. Меньше — быстрее реакция, больше нагрузка на CPU. */
		public int updateIntervalMs = 15;
	}

	/** Модуль 2: собственная дальность отрисовки, отдельно от дальности чанков. */
	public static final class RenderDistance {
		public boolean enabled = true;
		/** Максимальная дальность отрисовки сущностей, блоки. */
		public int entityDistance = 128;
		/** Максимальная дальность отрисовки блок-сущностей, блоки (в ванилле обычно 64). */
		public int blockEntityDistance = 64;
		/** Максимальная дальность появления частиц, блоки. */
		public int particleDistance = 128;
		/** Применять ли лимит дальности к игрокам. По умолчанию игроков не трогаем. */
		public boolean applyToPlayers = false;
	}

	/** Модуль 3: ограничение частиц. */
	public static final class Particles {
		public boolean enabled = true;
		/** Максимум одновременно живых частиц. Новые сверх лимита просто не создаются. */
		public int maxParticles = 8000;
		/**
		 * Не создавать частицы вне поля зрения камеры (дальше {@link #outOfViewMinDistance}).
		 * Выключено по умолчанию: долгоживущие частицы (дым костра) могут «пропасть» при повороте.
		 */
		public boolean skipOutOfViewSpawns = false;
		/** Ближе этого расстояния частицы вне поля зрения всё равно создаются. */
		public int outOfViewMinDistance = 16;
	}

	/** Модуль 4: экономия ресурсов, когда окно в фоне. */
	public static final class Background {
		public boolean enabled = true;
		/** Лимит FPS, когда окно открыто, но не в фокусе. */
		public int unfocusedFpsLimit = 10;
		/** Лимит FPS, когда окно свёрнуто или полностью перекрыто. */
		public int minimizedFpsLimit = 5;
		/** Приглушать звук, пока окно в фоне. */
		public boolean reduceVolumeInBackground = false;
		/** Громкость в фоне, % от обычной (0 — полная тишина). */
		public int backgroundVolumePercent = 20;
	}

	/** Модуль 5: память и запуск. */
	public static final class Memory {
		public boolean enabled = true;
		/** Переиспользовать одинаковые кубы моделей сущностей (применяется при загрузке ресурсов). */
		public boolean deduplicateModelCubes = true;
		/** Один раз собрать мусор после окончания загрузки игры — уменьшает занятую память. */
		public boolean gcAfterLoading = true;
		/** Собрать мусор после выхода из мира. */
		public boolean gcOnWorldLeave = false;
	}

	/** Модуль 6: обновлять только видимые анимированные текстуры. */
	public static final class Animations {
		public boolean enabled = true;
		/** Спрайты, которые анимируются всегда (используются вне чанков, например огонь на горящих мобах). */
		public List<String> alwaysAnimate = new ArrayList<>(List.of(
				"minecraft:block/fire_0",
				"minecraft:block/fire_1",
				"minecraft:block/soul_fire_0",
				"minecraft:block/soul_fire_1",
				"minecraft:block/nether_portal",
				"minecraft:block/water_still",
				"minecraft:block/water_flow",
				"minecraft:block/lava_still",
				"minecraft:block/lava_flow"
		));
	}

	/** Модуль 7: отладочный оверлей со статистикой мода. */
	public static final class DebugOverlay {
		public boolean enabled = true;
		/** Показывать только вместе с экраном отладки (F3). */
		public boolean onlyWithF3 = true;
		/** Угол экрана: TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT. */
		public OverlayPosition position = OverlayPosition.BOTTOM_RIGHT;
	}

	/** Совместимость с Sodium, Lithium, Iris и другими модами. */
	public static final class Compatibility {
		/** Автоматически выключать модули, которые пересекаются с установленными модами. */
		public boolean autoDisableOverlappingModules = true;
	}

	public enum OverlayPosition {
		TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT
	}

	/**
	 * Приводит значения к допустимым диапазонам и восстанавливает пропавшие секции
	 * (например, если файл правили вручную и удалили блок).
	 */
	public void sanitize() {
		if (entityCulling == null) entityCulling = new EntityCulling();
		if (renderDistance == null) renderDistance = new RenderDistance();
		if (particles == null) particles = new Particles();
		if (background == null) background = new Background();
		if (memory == null) memory = new Memory();
		if (animations == null) animations = new Animations();
		if (debugOverlay == null) debugOverlay = new DebugOverlay();
		if (compatibility == null) compatibility = new Compatibility();

		entityCulling.maxCheckDistance = clamp(entityCulling.maxCheckDistance, 16, 512);
		entityCulling.updateIntervalMs = clamp(entityCulling.updateIntervalMs, 1, 500);

		renderDistance.entityDistance = clamp(renderDistance.entityDistance, 8, 1024);
		renderDistance.blockEntityDistance = clamp(renderDistance.blockEntityDistance, 8, 1024);
		renderDistance.particleDistance = clamp(renderDistance.particleDistance, 4, 1024);

		particles.maxParticles = clamp(particles.maxParticles, 100, 16000);
		particles.outOfViewMinDistance = clamp(particles.outOfViewMinDistance, 0, 256);

		background.unfocusedFpsLimit = clamp(background.unfocusedFpsLimit, 1, 260);
		background.minimizedFpsLimit = clamp(background.minimizedFpsLimit, 1, 260);
		background.backgroundVolumePercent = clamp(background.backgroundVolumePercent, 0, 100);

		if (animations.alwaysAnimate == null) animations.alwaysAnimate = new ArrayList<>();
		if (debugOverlay.position == null) debugOverlay.position = OverlayPosition.BOTTOM_RIGHT;
	}

	private static int clamp(int value, int min, int max) {
		return Math.max(min, Math.min(max, value));
	}
}
