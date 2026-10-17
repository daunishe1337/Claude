package dev.optimod.compat;

import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;

/**
 * Быстрые флаги «модуль активен прямо сейчас» = включён в конфиге И не выключен совместимостью.
 * <p>
 * Хуки миксинов вызываются тысячи раз за кадр, поэтому читают готовые volatile-поля,
 * а не пересчитывают условия. Поля обновляет {@link #refresh()} после загрузки
 * или изменения конфига.
 */
public final class Modules {
	public static volatile boolean entityCulling;
	public static volatile boolean renderDistance;
	public static volatile boolean particles;
	public static volatile boolean background;
	public static volatile boolean memory;
	public static volatile boolean animations;
	public static volatile boolean debugOverlay;

	private Modules() {
	}

	public static void refresh() {
		OptimodConfig config = ConfigManager.get();
		entityCulling = isActive(OptModule.ENTITY_CULLING, config);
		renderDistance = isActive(OptModule.RENDER_DISTANCE, config);
		particles = isActive(OptModule.PARTICLES, config);
		background = isActive(OptModule.BACKGROUND, config);
		memory = isActive(OptModule.MEMORY, config);
		animations = isActive(OptModule.ANIMATIONS, config);
		debugOverlay = isActive(OptModule.DEBUG_OVERLAY, config);
	}

	public static boolean isActive(OptModule module) {
		return isActive(module, ConfigManager.get());
	}

	private static boolean isActive(OptModule module, OptimodConfig config) {
		return module.isEnabledInConfig(config) && !CompatManager.isBlocked(module);
	}
}
