package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.particle.SingleQuadParticle;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Частицы с текстурой блока (например, крошки разбитой магмы) отмечают свой спрайт.
 * Метод и поле сверены с Sodium для 26.3.
 */
@Mixin(SingleQuadParticle.class)
public abstract class SingleQuadParticleMixin {
	@Shadow
	protected TextureAtlasSprite sprite;

	@Inject(method = "extractRotatedQuad(Lnet/minecraft/client/renderer/state/level/QuadParticleRenderState;Lorg/joml/Quaternionf;FFFF)V",
			at = @At("HEAD"))
	private void optimod$markSprite(CallbackInfo ci) {
		AnimationTracker.markSprite(this.sprite);
	}
}
