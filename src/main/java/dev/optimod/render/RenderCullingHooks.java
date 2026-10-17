package dev.optimod.render;

import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import dev.optimod.culling.CullingManager;
import dev.optimod.culling.OptimodCullable;
import dev.optimod.stats.OptimodStats;
import net.minecraft.client.Minecraft;
import net.minecraft.world.entity.Display;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.block.entity.BlockEntity;

/**
 * Решения «рисовать или нет» для сущностей и блок-сущностей.
 * Вызываются из миксинов в главном потоке для каждого объекта каждый кадр,
 * поэтому сначала идут самые дешёвые проверки.
 */
public final class RenderCullingHooks {
	private RenderCullingHooks() {
	}

	/** Нужна ли вообще работа по сущностям (быстрый выход, если модули выключены). */
	public static boolean anyEntityCulling() {
		return Modules.entityCulling;
	}

	/** Нужна ли вообще работа по блок-сущностям. */
	public static boolean anyBlockEntityCulling() {
		return Modules.entityCulling;
	}

	/**
	 * @param affectedByCulling результат ванильного {@code EntityRenderer#affectedByCulling}:
	 *                          false у сущностей, которые всегда рисуются (например, дракон Края)
	 * @return true — сущность НЕ рисовать
	 */
	public static boolean shouldSkipEntity(Entity entity, double camX, double camY, double camZ, boolean affectedByCulling) {
		if (!affectedByCulling || isAlwaysVisible(entity)) {
			return false;
		}

		OptimodConfig config = ConfigManager.get();

		// Модуль 1: скрыта ли сущность стенами (результат фонового потока)
		if (Modules.entityCulling && config.entityCulling.cullEntities && isOcclusionCandidate(entity)) {
			if (CullingManager.isCulled((OptimodCullable) entity)) {
				OptimodStats.curEntitiesOccluded++;
				return true;
			}
		}
		return false;
	}

	/**
	 * Блок-сущности, рисуемые «глобально» или вне экрана (лучи маяков и т.п.),
	 * отфильтровываются в миксине ещё до этого вызова.
	 *
	 * @return true — блок-сущность НЕ рисовать
	 */
	public static boolean shouldSkipBlockEntity(BlockEntity blockEntity) {
		OptimodConfig config = ConfigManager.get();

		// Модуль 1: скрыта ли стенами
		if (Modules.entityCulling && config.entityCulling.cullBlockEntities) {
			if (CullingManager.isCulled((OptimodCullable) blockEntity)) {
				OptimodStats.curBlockEntitiesOccluded++;
				return true;
			}
		}
		return false;
	}

	/** Сущности, которые не трогает ни один модуль. */
	private static boolean isAlwaysVisible(Entity entity) {
		Minecraft minecraft = Minecraft.getInstance();
		if (entity == minecraft.getCameraEntity() || entity == minecraft.player) {
			return true;
		}
		// Транспорт, на котором едет игрок (в т.ч. через цепочку пассажиров)
		if (minecraft.player != null && entity.hasIndirectPassenger(minecraft.player)) {
			return true;
		}
		// Светящиеся сущности видны сквозь стены по механике игры
		return minecraft.shouldEntityAppearGlowing(entity);
	}

	/**
	 * Можно ли скрывать сущность из-за стен.
	 * Игроков и сущностей с видимым именем не трогаем: их ник рисуется сквозь стены.
	 * Display-сущности бывают огромными при нулевом хитбоксе — их тоже пропускаем.
	 */
	private static boolean isOcclusionCandidate(Entity entity) {
		return !(entity instanceof Player)
				&& !(entity instanceof Display)
				&& !entity.shouldShowName();
	}
}
