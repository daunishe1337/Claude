package dev.optimod.background;

import com.mojang.blaze3d.platform.Window;
import dev.optimod.Optimod;
import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.mixin.background.SoundManagerAccessor;
import net.minecraft.client.Minecraft;
import org.lwjgl.sdl.SDLVideo;

/**
 * Модуль 4: экономия ресурсов, когда окно игры не в фокусе или свёрнуто.
 * <p>
 * С версий 26.x окно игры работает через SDL (а не GLFW). Состояние окна берём флагами
 * {@code SDL_GetWindowFlags} — так же делают Sodium и Dynamic FPS для 26.3.
 * Ванильный ограничитель FPS ({@code FramerateLimitTracker}) мы не заменяем, а лишь
 * уменьшаем его результат: если ваниль уже выбрала меньший лимит, он сохраняется.
 */
public final class BackgroundManager {
	public enum WindowState {
		FOCUSED, UNFOCUSED, MINIMIZED
	}

	private static final long MINIMIZED_FLAGS = SDLVideo.SDL_WINDOW_MINIMIZED
			| SDLVideo.SDL_WINDOW_OCCLUDED
			| SDLVideo.SDL_WINDOW_HIDDEN;

	/** Последнее известное состояние окна (главный поток). */
	private static WindowState state = WindowState.FOCUSED;
	/** Множитель громкости для всех категорий звука, кроме общей. Читается при расчёте громкости. */
	private static volatile float volumeMultiplier = 1.0f;
	private static boolean pollErrorLogged;

	private BackgroundManager() {
	}

	/**
	 * Вызывается из {@code FramerateLimitTracker#getFramerateLimit} каждый кадр.
	 *
	 * @param vanillaLimit лимит, который выбрала игра
	 * @return итоговый лимит FPS
	 */
	public static int limitFramerate(int vanillaLimit) {
		if (!Modules.background) {
			return vanillaLimit;
		}

		state = pollWindowState();
		OptimodConfig.Background config = ConfigManager.get().background;
		int limit = switch (state) {
			case FOCUSED -> vanillaLimit;
			case UNFOCUSED -> config.unfocusedFpsLimit;
			case MINIMIZED -> config.minimizedFpsLimit;
		};
		return Math.min(vanillaLimit, limit);
	}

	/** Раз в тик: обновить состояние окна и, если нужно, громкость. */
	public static void tick(Minecraft client) {
		WindowState current = Modules.background ? pollWindowState() : WindowState.FOCUSED;
		if (current != state) {
			Optimod.LOGGER.debug("Окно игры: {} -> {}", state, current);
			state = current;
		}

		OptimodConfig.Background config = ConfigManager.get().background;
		float target = 1.0f;
		if (Modules.background && current != WindowState.FOCUSED && config.reduceVolumeInBackground) {
			target = config.backgroundVolumePercent / 100.0f;
		}

		if (target != volumeMultiplier) {
			volumeMultiplier = target;
			refreshSoundVolumes(client);
		}
	}

	public static float volumeMultiplier() {
		return volumeMultiplier;
	}

	public static WindowState state() {
		return state;
	}

	/** Читает флаги окна SDL. Дешёвый вызов (одно чтение структуры окна). */
	private static WindowState pollWindowState() {
		try {
			Window window = Minecraft.getInstance().getWindow();
			long flags = SDLVideo.SDL_GetWindowFlags(window.handle());
			if ((flags & MINIMIZED_FLAGS) != 0) {
				return WindowState.MINIMIZED;
			}
			if ((flags & SDLVideo.SDL_WINDOW_INPUT_FOCUS) == 0) {
				return WindowState.UNFOCUSED;
			}
			return WindowState.FOCUSED;
		} catch (Throwable t) {
			if (!pollErrorLogged) {
				pollErrorLogged = true;
				Optimod.LOGGER.warn("Не удалось прочитать состояние окна — модуль фона считает окно активным", t);
			}
			return WindowState.FOCUSED;
		}
	}

	/** Пересчитывает громкость уже играющих звуков (новые звуки сразу учитывают множитель). */
	private static void refreshSoundVolumes(Minecraft client) {
		try {
			Object engine = ((SoundManagerAccessor) client.getSoundManager()).optimod$getSoundEngine();
			((OptimodSoundEngine) engine).optimod$refreshVolumes();
		} catch (Throwable t) {
			Optimod.LOGGER.warn("Не удалось обновить громкость звуков", t);
		}
	}
}
