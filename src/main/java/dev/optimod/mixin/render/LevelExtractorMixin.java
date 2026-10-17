package dev.optimod.mixin.render;

import dev.optimod.render.FrameContext;
import net.minecraft.client.Camera;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.client.renderer.extract.LevelExtractor;
import net.minecraft.client.renderer.state.level.LevelRenderState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Начало кадра. В 26.3 подготовка кадра вынесена из LevelRenderer в {@code LevelExtractor};
 * метод {@code extractVisibleEntities(Camera, Frustum, DeltaTracker, LevelRenderState)}
 * вызывается каждый кадр в мире (сигнатура сверена с EntityCulling для 26.3).
 * Отсюда берём камеру и пирамиду видимости.
 */
@Mixin(LevelExtractor.class)
public abstract class LevelExtractorMixin {
	@Inject(method = "extractVisibleEntities", at = @At("HEAD"))
	private void optimod$beginFrame(Camera camera, Frustum frustum, DeltaTracker deltaTracker,
									LevelRenderState levelRenderState, CallbackInfo ci) {
		FrameContext.begin(camera, frustum);
	}
}
