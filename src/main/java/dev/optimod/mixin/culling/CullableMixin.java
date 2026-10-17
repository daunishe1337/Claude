package dev.optimod.mixin.culling;

import dev.optimod.culling.OptimodCullable;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.entity.BlockEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;

/**
 * Добавляет в {@link Entity} и {@link BlockEntity} поля для результата проверки видимости.
 * Миксин ничего не перехватывает — только новые поля и методы, поэтому безопасен
 * при любых изменениях этих классов.
 */
@Mixin({Entity.class, BlockEntity.class})
public abstract class CullableMixin implements OptimodCullable {
	// volatile: пишет фоновый поток, читает главный
	@Unique
	private volatile boolean optimod$culled;
	@Unique
	private volatile boolean optimod$tracked;
	@Unique
	private volatile long optimod$lastRequest;
	// Только фоновый поток
	@Unique
	private int optimod$hiddenPasses;

	@Override
	public boolean optimod$isCulled() {
		return this.optimod$culled;
	}

	@Override
	public void optimod$setCulled(boolean culled) {
		this.optimod$culled = culled;
	}

	@Override
	public boolean optimod$isTracked() {
		return this.optimod$tracked;
	}

	@Override
	public void optimod$setTracked(boolean tracked) {
		this.optimod$tracked = tracked;
	}

	@Override
	public long optimod$getLastRequest() {
		return this.optimod$lastRequest;
	}

	@Override
	public void optimod$setLastRequest(long timeMillis) {
		this.optimod$lastRequest = timeMillis;
	}

	@Override
	public int optimod$getHiddenPasses() {
		return this.optimod$hiddenPasses;
	}

	@Override
	public void optimod$setHiddenPasses(int passes) {
		this.optimod$hiddenPasses = passes;
	}
}
