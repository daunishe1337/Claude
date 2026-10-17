package dev.optimod.gui;

import com.mojang.serialization.Codec;
import dev.optimod.compat.CompatManager;
import dev.optimod.compat.Modules;
import dev.optimod.compat.OptModule;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import net.minecraft.client.Minecraft;
import net.minecraft.client.OptionInstance;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.screens.options.OptionsSubScreen;
import net.minecraft.network.chat.CommonComponents;
import net.minecraft.network.chat.Component;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.function.Consumer;
import java.util.function.IntConsumer;

/**
 * Экран настроек мода.
 * <p>
 * Построен на ванильных {@code OptionsSubScreen} и {@code OptionInstance} — так же устроен экран
 * настроек Mod Menu для 26.3, поэтому не нужны сторонние библиотеки (Cloth Config, YACL).
 * Открывается из Mod Menu (если установлен) или клавишей (по умолчанию O).
 * <p>
 * Изменения применяются сразу; в файл конфига пишутся при закрытии экрана.
 */
public class OptimodConfigScreen extends OptionsSubScreen {
	private static final String PREFIX = "optimod.option.";

	public OptimodConfigScreen(Screen parent) {
		super(parent, Minecraft.getInstance().options, Component.translatable("optimod.config.title"));
	}

	@Override
	protected void addOptions() {
		if (this.list == null) {
			return;
		}
		OptimodConfig c = ConfigManager.get();

		// 1. Отсечение скрытых
		this.list.addSmall(
				moduleToggle(OptModule.ENTITY_CULLING, "entity_culling.enabled", c.entityCulling.enabled, v -> c.entityCulling.enabled = v),
				toggle("entity_culling.cull_entities", c.entityCulling.cullEntities, v -> c.entityCulling.cullEntities = v),
				toggle("entity_culling.cull_block_entities", c.entityCulling.cullBlockEntities, v -> c.entityCulling.cullBlockEntities = v),
				slider("entity_culling.max_check_distance", 16, 512, 16, c.entityCulling.maxCheckDistance, "optimod.unit.blocks", v -> c.entityCulling.maxCheckDistance = v),
				slider("entity_culling.update_interval", 1, 200, 1, c.entityCulling.updateIntervalMs, "optimod.unit.ms", v -> c.entityCulling.updateIntervalMs = v)
		);

		// 2. Дальность отрисовки
		this.list.addSmall(
				moduleToggle(OptModule.RENDER_DISTANCE, "render_distance.enabled", c.renderDistance.enabled, v -> c.renderDistance.enabled = v),
				toggle("render_distance.apply_to_players", c.renderDistance.applyToPlayers, v -> c.renderDistance.applyToPlayers = v),
				slider("render_distance.entity", 16, 512, 8, c.renderDistance.entityDistance, "optimod.unit.blocks", v -> c.renderDistance.entityDistance = v),
				slider("render_distance.block_entity", 8, 512, 8, c.renderDistance.blockEntityDistance, "optimod.unit.blocks", v -> c.renderDistance.blockEntityDistance = v),
				slider("render_distance.particle", 4, 512, 4, c.renderDistance.particleDistance, "optimod.unit.blocks", v -> c.renderDistance.particleDistance = v)
		);

		// 3. Частицы
		this.list.addSmall(
				moduleToggle(OptModule.PARTICLES, "particles.enabled", c.particles.enabled, v -> c.particles.enabled = v),
				toggle("particles.skip_out_of_view", c.particles.skipOutOfViewSpawns, v -> c.particles.skipOutOfViewSpawns = v),
				slider("particles.max", 100, 16000, 100, c.particles.maxParticles, "optimod.unit.count", v -> c.particles.maxParticles = v),
				slider("particles.out_of_view_distance", 0, 64, 1, c.particles.outOfViewMinDistance, "optimod.unit.blocks", v -> c.particles.outOfViewMinDistance = v)
		);

		// 4. Фон
		this.list.addSmall(
				moduleToggle(OptModule.BACKGROUND, "background.enabled", c.background.enabled, v -> c.background.enabled = v),
				toggle("background.reduce_volume", c.background.reduceVolumeInBackground, v -> c.background.reduceVolumeInBackground = v),
				slider("background.unfocused_fps", 1, 60, 1, c.background.unfocusedFpsLimit, "optimod.unit.fps", v -> c.background.unfocusedFpsLimit = v),
				slider("background.minimized_fps", 1, 60, 1, c.background.minimizedFpsLimit, "optimod.unit.fps", v -> c.background.minimizedFpsLimit = v),
				slider("background.volume", 0, 100, 5, c.background.backgroundVolumePercent, "optimod.unit.percent", v -> c.background.backgroundVolumePercent = v)
		);

		// 5. Память
		this.list.addSmall(
				moduleToggle(OptModule.MEMORY, "memory.enabled", c.memory.enabled, v -> c.memory.enabled = v),
				toggle("memory.deduplicate_cubes", c.memory.deduplicateModelCubes, v -> c.memory.deduplicateModelCubes = v),
				toggle("memory.gc_after_loading", c.memory.gcAfterLoading, v -> c.memory.gcAfterLoading = v),
				toggle("memory.gc_on_world_leave", c.memory.gcOnWorldLeave, v -> c.memory.gcOnWorldLeave = v)
		);

		// 6. Анимации и 7. Оверлей
		this.list.addSmall(
				moduleToggle(OptModule.ANIMATIONS, "animations.enabled", c.animations.enabled, v -> c.animations.enabled = v),
				moduleToggle(OptModule.DEBUG_OVERLAY, "debug_overlay.enabled", c.debugOverlay.enabled, v -> c.debugOverlay.enabled = v),
				toggle("debug_overlay.only_with_f3", c.debugOverlay.onlyWithF3, v -> c.debugOverlay.onlyWithF3 = v),
				overlayPosition(c)
		);

		// Совместимость
		this.list.addSmall(
				toggle("compatibility.auto_disable", c.compatibility.autoDisableOverlappingModules, v -> c.compatibility.autoDisableOverlappingModules = v)
		);
	}

	@Override
	public void removed() {
		ConfigManager.save();
		Modules.refresh();
	}

	// ----------------------------------------------------------------------------------
	// Конструкторы опций
	// ----------------------------------------------------------------------------------

	/** Переключатель вкл/выкл. */
	private static OptionInstance<Boolean> toggle(String key, boolean value, Consumer<Boolean> setter) {
		return new OptionInstance<>(
				PREFIX + key,
				OptionInstance.cachedConstantTooltip(Component.translatable(PREFIX + key + ".tooltip")),
				(caption, enabled) -> CommonComponents.optionNameValue(caption, onOff(enabled)),
				OptionInstance.BOOLEAN_VALUES,
				value,
				enabled -> {
					setter.accept(enabled);
					Modules.refresh();
				});
	}

	/** Главный переключатель модуля: показывает, если модуль выключен совместимостью. */
	private static OptionInstance<Boolean> moduleToggle(OptModule module, String key, boolean value, Consumer<Boolean> setter) {
		String blockedBy = CompatManager.blockedBy(module);
		return new OptionInstance<>(
				PREFIX + key,
				OptionInstance.cachedConstantTooltip(blockedBy != null
						? Component.translatable("optimod.option.blocked.tooltip", blockedBy)
						: Component.translatable(PREFIX + key + ".tooltip")),
				(caption, enabled) -> CommonComponents.optionNameValue(caption, blockedBy != null
						? Component.translatable("optimod.option.blocked", blockedBy)
						: onOff(enabled)),
				OptionInstance.BOOLEAN_VALUES,
				value,
				enabled -> {
					setter.accept(enabled);
					Modules.refresh();
				});
	}

	/**
	 * Ползунок целого числа с шагом. Внутри OptionInstance хранится номер шага,
	 * наружу — реальное значение (номер × шаг).
	 */
	private static OptionInstance<Integer> slider(String key, int min, int max, int step, int value,
												  String unitKey, IntConsumer setter) {
		int minIndex = min / step;
		int maxIndex = max / step;
		int index = Math.max(minIndex, Math.min(maxIndex, Math.round((float) value / step)));
		return new OptionInstance<>(
				PREFIX + key,
				OptionInstance.cachedConstantTooltip(Component.translatable(PREFIX + key + ".tooltip")),
				(caption, stepIndex) -> Component.translatable("options.generic_value", caption,
						Component.translatable(unitKey, stepIndex * step)),
				new OptionInstance.IntRange(minIndex, maxIndex),
				index,
				stepIndex -> {
					setter.accept(stepIndex * step);
					Modules.refresh();
				});
	}

	/** Выбор угла экрана для оверлея (кнопка-переключатель по кругу). */
	private static OptionInstance<OptimodConfig.OverlayPosition> overlayPosition(OptimodConfig c) {
		List<OptimodConfig.OverlayPosition> values = Arrays.asList(OptimodConfig.OverlayPosition.values());
		return new OptionInstance<>(
				PREFIX + "debug_overlay.position",
				OptionInstance.cachedConstantTooltip(Component.translatable(PREFIX + "debug_overlay.position.tooltip")),
				(caption, position) -> CommonComponents.optionNameValue(caption,
						Component.translatable("optimod.position." + position.name().toLowerCase(Locale.ROOT))),
				new OptionInstance.Enum<>(values, Codec.STRING.xmap(OptimodConfig.OverlayPosition::valueOf, OptimodConfig.OverlayPosition::name)),
				c.debugOverlay.position,
				position -> c.debugOverlay.position = position);
	}

	private static Component onOff(boolean enabled) {
		return Component.translatable(enabled ? "options.on" : "options.off");
	}
}
