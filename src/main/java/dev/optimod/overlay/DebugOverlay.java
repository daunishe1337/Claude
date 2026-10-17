package dev.optimod.overlay;

import dev.optimod.animation.AnimationTracker;
import dev.optimod.background.BackgroundManager;
import dev.optimod.compat.CompatManager;
import dev.optimod.compat.Modules;
import dev.optimod.compat.OptModule;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.memory.CubeCache;
import dev.optimod.memory.MemoryManager;
import dev.optimod.particle.ParticleTracker;
import dev.optimod.stats.OptimodStats;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElement;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.network.chat.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Модуль 7: отладочный оверлей — показывает, сколько работы мод сэкономил.
 * <p>
 * Регистрируется через Fabric HUD API ({@code HudElementRegistry}); в 26.3 элемент HUD
 * реализует {@code extractRenderState(GuiGraphicsExtractor, DeltaTracker)}.
 * По умолчанию виден только вместе с экраном отладки F3.
 */
public final class DebugOverlay implements HudElement {
	private static final int TEXT_COLOR = 0xFFFFFFFF;   // цвет обязательно с альфой (ARGB)
	private static final int TITLE_COLOR = 0xFF55FFFF;
	private static final int BACKGROUND_COLOR = 0x90000000;
	private static final int PADDING = 2;
	private static final int MARGIN = 4;

	/** Строки переиспользуются между кадрами, чтобы не создавать новый список. */
	private final List<String> lines = new ArrayList<>(12);

	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, DeltaTracker deltaTracker) {
		if (!Modules.debugOverlay) {
			return;
		}
		Minecraft minecraft = Minecraft.getInstance();
		OptimodConfig config = ConfigManager.get();
		if (config.debugOverlay.onlyWithF3 && !minecraft.debugEntries.isOverlayVisible()) {
			return;
		}

		collectLines(config);

		Font font = minecraft.font;
		int width = 0;
		for (String line : lines) {
			width = Math.max(width, font.width(line));
		}
		int height = lines.size() * font.lineHeight;

		int x;
		int y;
		switch (config.debugOverlay.position) {
			case TOP_LEFT -> {
				x = MARGIN;
				y = MARGIN;
			}
			case TOP_RIGHT -> {
				x = graphics.guiWidth() - width - MARGIN;
				y = MARGIN;
			}
			case BOTTOM_LEFT -> {
				x = MARGIN;
				y = graphics.guiHeight() - height - MARGIN;
			}
			default -> {
				x = graphics.guiWidth() - width - MARGIN;
				y = graphics.guiHeight() - height - MARGIN;
			}
		}

		graphics.fill(x - PADDING, y - PADDING, x + width + PADDING, y + height + PADDING, BACKGROUND_COLOR);
		for (int i = 0; i < lines.size(); i++) {
			graphics.text(font, lines.get(i), x, y + i * font.lineHeight, i == 0 ? TITLE_COLOR : TEXT_COLOR);
		}
	}

	private void collectLines(OptimodConfig config) {
		lines.clear();
		lines.add("Optimod");

		// Модуль 1 — отсечение невидимых
		if (Modules.entityCulling) {
			lines.add(tr("optimod.overlay.culling",
					OptimodStats.lastEntitiesOccluded, OptimodStats.lastBlockEntitiesOccluded));
			lines.add(tr("optimod.overlay.culling_thread",
					String.format(Locale.ROOT, "%.2f", OptimodStats.cullPassMicros / 1000.0),
					OptimodStats.cullTracked, OptimodStats.cullHidden));
		} else {
			lines.add(disabledLine(OptModule.ENTITY_CULLING));
		}

		// Модуль 2 — дальность отрисовки
		if (Modules.renderDistance) {
			lines.add(tr("optimod.overlay.distance",
					OptimodStats.lastEntitiesByDistance, OptimodStats.lastBlockEntitiesByDistance,
					OptimodStats.lastParticlesByDistance));
		} else {
			lines.add(disabledLine(OptModule.RENDER_DISTANCE));
		}

		// Модуль 3 — частицы
		if (Modules.particles) {
			lines.add(tr("optimod.overlay.particles",
					ParticleTracker.liveCount(), config.particles.maxParticles,
					OptimodStats.lastParticlesByLimit, OptimodStats.lastParticlesOutOfView));
		} else {
			lines.add(disabledLine(OptModule.PARTICLES));
		}

		// Модуль 4 — фон
		if (Modules.background) {
			lines.add(tr("optimod.overlay.background",
					tr("optimod.overlay.window." + BackgroundManager.state().name().toLowerCase(Locale.ROOT)),
					config.background.unfocusedFpsLimit, config.background.minimizedFpsLimit));
		} else {
			lines.add(disabledLine(OptModule.BACKGROUND));
		}

		// Модуль 5 — память
		if (Modules.memory) {
			lines.add(tr("optimod.overlay.memory",
					MemoryManager.usedHeapMb(), MemoryManager.maxHeapMb(), CubeCache.reusedCount()));
		} else {
			lines.add(disabledLine(OptModule.MEMORY));
		}

		// Модуль 6 — анимации
		if (Modules.animations) {
			lines.add(tr("optimod.overlay.animations",
					AnimationTracker.lastSkipped, AnimationTracker.lastAnimations,
					AnimationTracker.trackedSpriteCount()));
		} else {
			lines.add(disabledLine(OptModule.ANIMATIONS));
		}
	}

	/** «Модуль: выключен» или «Модуль: отключён из-за Sodium». */
	private static String disabledLine(OptModule module) {
		String name = tr("optimod.module." + module.key);
		String blockedBy = CompatManager.blockedBy(module);
		return blockedBy != null
				? tr("optimod.overlay.blocked", name, blockedBy)
				: tr("optimod.overlay.disabled", name);
	}

	private static String tr(String key, Object... args) {
		return Component.translatable(key, args).getString();
	}
}
