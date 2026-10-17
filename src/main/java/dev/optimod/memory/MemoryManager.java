package dev.optimod.memory;

import dev.optimod.Optimod;
import dev.optimod.compat.CompatManager;
import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import net.minecraft.client.Minecraft;

import java.lang.management.ManagementFactory;

/**
 * Модуль 5: память и запуск.
 * <ul>
 *     <li>замер времени запуска игры (до исчезновения экрана загрузки) — пишется в лог;</li>
 *     <li>одна сборка мусора сразу после загрузки: при загрузке ресурсов создаются сотни МБ
 *     временных объектов; после полной сборки JVM (G1) может вернуть часть памяти системе;</li>
 *     <li>по желанию — сборка мусора после выхода из мира;</li>
 *     <li>сводка дедупликации кубов моделей ({@link CubeCache}).</li>
 * </ul>
 */
public final class MemoryManager {
	private static final long MB = 1024L * 1024L;

	private static boolean startupHandled;

	private MemoryManager() {
	}

	/** Раз в тик. Ловим момент, когда исчез экран загрузки. */
	public static void tick(Minecraft client) {
		if (startupHandled || client.gui.overlay() != null) {
			return;
		}
		startupHandled = true;

		long uptimeMs = ManagementFactory.getRuntimeMXBean().getUptime();
		Optimod.LOGGER.info("Игра загрузилась за {} с (от запуска JVM до исчезновения экрана загрузки)",
				String.format("%.1f", uptimeMs / 1000.0));

		logCubeCacheSummary();

		OptimodConfig.Memory config = ConfigManager.get().memory;
		if (Modules.memory && config.gcAfterLoading) {
			collectGarbage("после загрузки");
		}
	}

	/** Выход из мира (в меню). */
	public static void onWorldLeft() {
		if (Modules.memory && ConfigManager.get().memory.gcOnWorldLeave) {
			collectGarbage("после выхода из мира");
		}
	}

	/** Занято памяти в куче, МБ. */
	public static long usedHeapMb() {
		Runtime runtime = Runtime.getRuntime();
		return (runtime.totalMemory() - runtime.freeMemory()) / MB;
	}

	/** Максимум кучи (-Xmx), МБ. */
	public static long maxHeapMb() {
		return Runtime.getRuntime().maxMemory() / MB;
	}

	private static void collectGarbage(String reason) {
		long before = usedHeapMb();
		long start = System.nanoTime();
		System.gc();
		long tookMs = (System.nanoTime() - start) / 1_000_000L;
		Optimod.LOGGER.info("Сборка мусора {}: {} МБ -> {} МБ за {} мс", reason, before, usedHeapMb(), tookMs);
	}

	private static void logCubeCacheSummary() {
		OptimodConfig.Memory config = ConfigManager.get().memory;
		if (!config.enabled || !config.deduplicateModelCubes) {
			return;
		}
		if (CompatManager.isModLoaded("modernfix")) {
			Optimod.LOGGER.info("Дедупликация кубов моделей отключена: её уже выполняет ModernFix");
		} else if (CubeCache.hasSeenAnyCube()) {
			Optimod.LOGGER.info("Дедупликация кубов моделей: уникальных {}, переиспользовано {}",
					CubeCache.uniqueCount(), CubeCache.reusedCount());
		} else {
			Optimod.LOGGER.warn("Дедупликация кубов моделей не сработала: вероятно, в этой версии игры "
					+ "изменился конструктор ModelPart.Cube (см. README, раздел про genSources)");
		}
	}
}
