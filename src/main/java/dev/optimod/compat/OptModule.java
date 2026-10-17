package dev.optimod.compat;

import dev.optimod.config.OptimodConfig;

/**
 * Модули мода. Каждый можно выключить в конфиге, а также его может автоматически
 * выключить {@link CompatManager}, если установлен мод с пересекающейся функцией.
 */
public enum OptModule {
	ENTITY_CULLING("entity_culling", "отсечение скрытых сущностей"),
	RENDER_DISTANCE("render_distance", "дальность отрисовки"),
	PARTICLES("particles", "ограничение частиц"),
	BACKGROUND("background", "снижение FPS в фоне"),
	MEMORY("memory", "оптимизация памяти"),
	ANIMATIONS("animations", "анимации только видимых текстур"),
	DEBUG_OVERLAY("debug_overlay", "отладочный оверлей");

	/** Ключ для переводов: {@code optimod.module.<key>}. */
	public final String key;
	/** Название для логов (лог пишем по-русски, независимо от языка игры). */
	public final String logName;

	OptModule(String key, String logName) {
		this.key = key;
		this.logName = logName;
	}

	/** Включён ли модуль в конфиге (без учёта совместимости). */
	public boolean isEnabledInConfig(OptimodConfig config) {
		return switch (this) {
			case ENTITY_CULLING -> config.entityCulling.enabled;
			case RENDER_DISTANCE -> config.renderDistance.enabled;
			case PARTICLES -> config.particles.enabled;
			case BACKGROUND -> config.background.enabled;
			case MEMORY -> config.memory.enabled;
			case ANIMATIONS -> config.animations.enabled;
			case DEBUG_OVERLAY -> config.debugOverlay.enabled;
		};
	}
}
