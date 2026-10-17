package dev.optimod;

import dev.optimod.background.BackgroundManager;
import dev.optimod.compat.CompatManager;
import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.culling.CullingManager;
import dev.optimod.particle.ParticleTracker;
import dev.optimod.render.FrameContext;
import dev.optimod.stats.OptimodStats;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientLifecycleEvents;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.resources.Identifier;

/**
 * Клиентская точка входа (entrypoint "client" в fabric.mod.json).
 * <p>
 * Мод чисто клиентский: не регистрирует блоков, предметов и сетевых пакетов,
 * поэтому подключается к любому серверу, в том числе без Optimod.
 */
public final class OptimodClient implements ClientModInitializer {
	/** Мир на прошлом тике — чтобы заметить вход/выход/смену измерения. */
	private static ClientLevel lastLevel;
	/** Счётчик тиков для статистики «за секунду». */
	private static int tickCounter;

	@Override
	public void onInitializeClient() {
		long start = System.nanoTime();

		// Конфиг и совместимость уже могли быть загружены mixin-плагином — вызовы идемпотентны
		ConfigManager.get();
		CompatManager.init();
		CompatManager.logReport();
		Modules.refresh();

		ClientTickEvents.START_CLIENT_TICK.register(OptimodClient::onStartTick);
		ClientTickEvents.END_CLIENT_TICK.register(OptimodClient::onEndTick);
		ClientLifecycleEvents.CLIENT_STOPPING.register(client -> CullingManager.shutdown());

		Optimod.LOGGER.info("Optimod инициализирован за {} мс. Конфиг: {}",
				(System.nanoTime() - start) / 1_000_000, ConfigManager.path());
	}

	/** Идентификатор в пространстве имён мода, например {@code optimod:debug_overlay}. */
	public static Identifier id(String path) {
		return Identifier.fromNamespaceAndPath(Optimod.MOD_ID, path);
	}

	/** Начало клиентского тика (до тика текстур, сущностей и т.д.). */
	private static void onStartTick(Minecraft client) {
	}

	/** Конец клиентского тика. */
	private static void onEndTick(Minecraft client) {
		if (++tickCounter >= 20) {
			tickCounter = 0;
			OptimodStats.endSecond();
		}
		ParticleTracker.tick();
		BackgroundManager.tick(client);

		ClientLevel level = client.level;
		if (level != lastLevel) {
			onLevelChanged(lastLevel, level);
			lastLevel = level;
		}
	}

	/** Вход в мир, выход из мира или смена измерения. */
	private static void onLevelChanged(ClientLevel oldLevel, ClientLevel newLevel) {
		Optimod.LOGGER.debug("Смена мира: {} -> {}", oldLevel, newLevel);
		FrameContext.reset();
		CullingManager.onLevelChanged(newLevel);
		ParticleTracker.clear();
	}
}
