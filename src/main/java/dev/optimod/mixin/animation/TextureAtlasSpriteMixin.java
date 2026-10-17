package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Атлас создаёт состояние анимации через {@code TextureAtlasSprite#createAnimationState}
 * (в 26.3 вызывается из {@code TextureAtlas#upload}, сверено с Sodium Extra).
 * Обработчик не принимает аргументы метода — они не нужны, и так хук не зависит от их типов.
 */
@Mixin(TextureAtlasSprite.class)
public abstract class TextureAtlasSpriteMixin {
	@Inject(method = "createAnimationState", at = @At("RETURN"))
	private void optimod$linkAnimationState(CallbackInfoReturnable<SpriteContents.AnimationState> cir) {
		SpriteContents.AnimationState state = cir.getReturnValue();
		if (state != null) {
			AnimationTracker.registerAnimation(((TextureAtlasSprite) (Object) this).contents(), state);
		}
	}
}
