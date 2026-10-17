package dev.optimod.gui;

import com.terraformersmc.modmenu.api.ConfigScreenFactory;
import com.terraformersmc.modmenu.api.ModMenuApi;

/**
 * Кнопка «Настройки» в списке модов Mod Menu.
 * <p>
 * Класс загружается только самим Mod Menu (entrypoint "modmenu" в fabric.mod.json).
 * Без Mod Menu его никто не трогает, поэтому мод работает и без этой зависимости
 * (в build.gradle она подключена как compileOnly).
 */
public final class ModMenuIntegration implements ModMenuApi {
	@Override
	public ConfigScreenFactory<?> getModConfigScreenFactory() {
		return OptimodConfigScreen::new;
	}
}
