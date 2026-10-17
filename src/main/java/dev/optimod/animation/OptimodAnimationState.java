package dev.optimod.animation;

import net.minecraft.client.renderer.texture.SpriteContents;

/** «Утиный» интерфейс для {@code SpriteContents.AnimationState}: связь анимации с её спрайтом. */
public interface OptimodAnimationState {
	void optimod$setContents(SpriteContents contents);
}
