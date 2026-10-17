package dev.optimod.compat;

import dev.optimod.Optimod;
import dev.optimod.config.ConfigManager;
import net.fabricmc.loader.api.FabricLoader;

import java.util.EnumMap;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Определяет установленные моды и решает, какие наши модули с ними пересекаются.
 * <p>
 * Как и {@link ConfigManager}, вызывается из mixin-плагина, поэтому использует только Fabric Loader.
 */
public final class CompatManager {
	/**
	 * Известный мод и модули, функции которых он уже выполняет.
	 *
	 * @param hardBlocks модули, которые выключаются ВСЕГДА (технический конфликт миксинов),
	 *                   независимо от настройки autoDisableOverlappingModules
	 */
	private record KnownMod(String modId, String name, Set<OptModule> overlaps, Set<OptModule> hardBlocks, String reason) {
	}

	private static final List<KnownMod> KNOWN_MODS = List.of(
			new KnownMod("sodium", "Sodium",
					EnumSet.of(OptModule.ENTITY_CULLING, OptModule.ANIMATIONS),
					EnumSet.of(OptModule.ANIMATIONS),
					"Sodium сам отсекает сущности в невидимых секциях и анимирует только видимые текстуры"),
			new KnownMod("iris", "Iris",
					EnumSet.of(OptModule.ENTITY_CULLING),
					EnumSet.noneOf(OptModule.class),
					"проход теней Iris рисует сущности с точки зрения солнца — отсечение по камере ломало бы тени"),
			new KnownMod("lithium", "Lithium",
					EnumSet.noneOf(OptModule.class),
					EnumSet.noneOf(OptModule.class),
					"Lithium оптимизирует игровую логику (тики, физику), а Optimod — только клиентский рендер; пересечений нет"),
			new KnownMod("dynamic_fps", "Dynamic FPS",
					EnumSet.of(OptModule.BACKGROUND),
					EnumSet.noneOf(OptModule.class),
					"Dynamic FPS уже ограничивает FPS и звук в фоне"),
			new KnownMod("entityculling", "EntityCulling",
					EnumSet.of(OptModule.ENTITY_CULLING),
					EnumSet.noneOf(OptModule.class),
					"EntityCulling выполняет то же отсечение по видимости")
	);

	/** Почему модуль выключен совместимостью: модуль → название мода. */
	private static final Map<OptModule, String> BLOCKED_BY = new EnumMap<>(OptModule.class);

	private static boolean initialized;
	private static boolean reported;

	private CompatManager() {
	}

	/** Определяет пересечения. Безопасно вызывать несколько раз. */
	public static synchronized void init() {
		if (initialized) {
			return;
		}
		initialized = true;

		boolean autoDisable = ConfigManager.get().compatibility.autoDisableOverlappingModules;
		for (KnownMod mod : KNOWN_MODS) {
			if (!isModLoaded(mod.modId())) {
				continue;
			}
			for (OptModule module : mod.overlaps()) {
				if (autoDisable || mod.hardBlocks().contains(module)) {
					BLOCKED_BY.putIfAbsent(module, mod.name());
				}
			}
		}
	}

	/** Пишет в лог итог проверки совместимости. Вызывается один раз из точки входа клиента. */
	public static synchronized void logReport() {
		init();
		if (reported) {
			return;
		}
		reported = true;

		boolean autoDisable = ConfigManager.get().compatibility.autoDisableOverlappingModules;
		boolean anyFound = false;

		for (KnownMod mod : KNOWN_MODS) {
			if (!isModLoaded(mod.modId())) {
				continue;
			}
			anyFound = true;

			if (mod.overlaps().isEmpty()) {
				Optimod.LOGGER.info("Обнаружен {}: {}. Ничего не отключено.", mod.name(), mod.reason());
				continue;
			}

			for (OptModule module : mod.overlaps()) {
				if (mod.name().equals(BLOCKED_BY.get(module))) {
					Optimod.LOGGER.warn("Обнаружен {}: модуль «{}» отключён — {}.", mod.name(), module.logName, mod.reason());
				} else if (!autoDisable) {
					Optimod.LOGGER.warn("Обнаружен {}: модуль «{}» пересекается с ним ({}), но оставлен включённым, "
							+ "т.к. compatibility.autoDisableOverlappingModules = false.", mod.name(), module.logName, mod.reason());
				}
			}
		}

		if (!anyFound) {
			Optimod.LOGGER.info("Модов с пересекающимися функциями не найдено — все модули работают по настройкам.");
		}
	}

	/** Выключен ли модуль из-за другого мода. */
	public static boolean isBlocked(OptModule module) {
		init();
		return BLOCKED_BY.containsKey(module);
	}

	/** Название мода, из-за которого выключен модуль, или null. */
	public static String blockedBy(OptModule module) {
		init();
		return BLOCKED_BY.get(module);
	}

	public static boolean isModLoaded(String modId) {
		return FabricLoader.getInstance().isModLoaded(modId);
	}
}
