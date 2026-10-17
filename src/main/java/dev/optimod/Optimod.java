package dev.optimod;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Общие константы мода.
 * <p>
 * Класс загружается ещё из mixin-плагина, до запуска игры. Поэтому здесь НЕЛЬЗЯ ссылаться
 * на классы Minecraft: если загрузить их раньше времени, к ним уже не применятся миксины
 * (наши и чужие).
 */
public final class Optimod {
	/** Идентификатор мода (совпадает с fabric.mod.json). */
	public static final String MOD_ID = "optimod";

	/** Логгер мода: все сообщения в latest.log помечены как [Optimod]. */
	public static final Logger LOGGER = LoggerFactory.getLogger("Optimod");

	private Optimod() {
	}
}
