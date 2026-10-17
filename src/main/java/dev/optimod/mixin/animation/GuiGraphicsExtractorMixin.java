package dev.optimod.mixin.animation;

import com.mojang.renderpearl.api.pipeline.RenderPipeline;
import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Спрайты, нарисованные в интерфейсе (иконки, оверлей портала). В 26.3 класс отрисовки GUI
 * называется {@code GuiGraphicsExtractor}, а рендер — {@code com.mojang.renderpearl}.
 * Дескрипторы обоих перегруженных {@code blitSprite} взяты из Sodium для 26.3.
 */
@Mixin(GuiGraphicsExtractor.class)
public abstract class GuiGraphicsExtractorMixin {
	@Inject(method = "blitSprite(Lcom/mojang/renderpearl/api/pipeline/RenderPipeline;Lnet/minecraft/client/renderer/texture/TextureAtlasSprite;IIIII)V",
			at = @At("HEAD"))
	private void optimod$markSprite(RenderPipeline pipeline, TextureAtlasSprite sprite,
									int x, int y, int width, int height, int color, CallbackInfo ci) {
		AnimationTracker.markSprite(sprite);
	}

	@Inject(method = "blitSprite(Lcom/mojang/renderpearl/api/pipeline/RenderPipeline;Lnet/minecraft/client/renderer/texture/TextureAtlasSprite;IIIIIIIII)V",
			at = @At("HEAD"))
	private void optimod$markSpriteRegion(RenderPipeline pipeline, TextureAtlasSprite sprite,
										  int spriteWidth, int spriteHeight, int textureX, int textureY,
										  int x, int y, int width, int height, int color, CallbackInfo ci) {
		AnimationTracker.markSprite(sprite);
	}
}
