package dev.optimod.mixin.animation;

import dev.optimod.animation.OptimodSpriteActivity;
import net.minecraft.client.renderer.texture.SpriteContents;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;

/**
 * Поля «активности» спрайта. Только новые поля и методы — ничего не перехватывает.
 * Признак «анимирован» ставится при создании состояния анимации (см. TextureAtlasSpriteMixin),
 * поэтому не нужен доступ к закрытому классу {@code SpriteContents.AnimatedTexture}.
 */
@Mixin(SpriteContents.class)
public abstract class SpriteContentsMixin implements OptimodSpriteActivity {
	// Отметку ставят и потоки сборки чанков, и главный поток — volatile
	@Unique
	private volatile boolean optimod$active;
	@Unique
	private volatile boolean optimod$animated;
	@Unique
	private volatile boolean optimod$pinned;

	@Override
	public void optimod$markActive() {
		this.optimod$active = true;
	}

	@Override
	public boolean optimod$isActive() {
		return this.optimod$active;
	}

	@Override
	public boolean optimod$consumeActive() {
		boolean active = this.optimod$active;
		this.optimod$active = false;
		return active;
	}

	@Override
	public boolean optimod$isAnimated() {
		return this.optimod$animated;
	}

	@Override
	public void optimod$setAnimated(boolean animated) {
		this.optimod$animated = animated;
	}

	@Override
	public boolean optimod$isPinned() {
		return this.optimod$pinned;
	}

	@Override
	public void optimod$setPinned(boolean pinned) {
		this.optimod$pinned = pinned;
	}
}
