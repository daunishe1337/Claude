package dev.optimod.gui;

import com.mojang.blaze3d.platform.InputConstants;
import dev.optimod.OptimodClient;
import net.fabricmc.fabric.api.client.keymapping.v1.KeyMappingHelper;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import org.lwjgl.sdl.SDLScancode;

/**
 * Клавиша открытия настроек — чтобы экран был доступен и без Mod Menu.
 * В 26.x клавиши задаются скан-кодами SDL (сверено с тестами Fabric API для 26.3).
 * Переназначается в «Настройки → Управление → Optimod».
 */
public final class OptimodKeyMappings {
	private static KeyMapping openConfig;

	private OptimodKeyMappings() {
	}

	public static void register() {
		KeyMapping.Category category = KeyMapping.Category.register(OptimodClient.id("main"));
		openConfig = KeyMappingHelper.registerKeyMapping(new KeyMapping(
				"key.optimod.open_config",
				InputConstants.Type.KEYBOARD,
				SDLScancode.SDL_SCANCODE_O,
				category));
	}

	/** Раз в тик: обработать нажатия. */
	public static void tick(Minecraft client) {
		if (openConfig == null) {
			return;
		}
		while (openConfig.consumeClick()) {
			client.gui.setScreen(new OptimodConfigScreen(client.gui.screen()));
		}
	}
}
