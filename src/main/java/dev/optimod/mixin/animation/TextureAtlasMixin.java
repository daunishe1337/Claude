package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.renderer.texture.TextureAtlas;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Код, который берёт спрайт из атласа во время отрисовки (например, огонь на горящем мобе),
 * отмечает его как используемый. Хук сверен с Sodium для 26.3.
 */
@Mixin(TextureAtlas.class)
public abstract class TextureAtlasMixin {
	@Inject(method = "getSprite", at = @At("RETURN"))
	private void optimod$markSprite(CallbackInfoReturnable<TextureAtlasSprite> cir) {
		AnimationTracker.markSprite(cir.getReturnValue());
	}
}
