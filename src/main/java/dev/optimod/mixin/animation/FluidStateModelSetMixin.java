package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.renderer.block.FluidModel;
import net.minecraft.client.renderer.block.FluidStateModelSet;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Жидкости рисуются отдельным путём, где нет позиции блока на уровне квада.
 * Поэтому их спрайты (вода, лава и жидкости модов) просто закрепляем как «анимировать всегда».
 * Метод и геттеры модели сверены с Sodium для 26.3.
 */
@Mixin(FluidStateModelSet.class)
public abstract class FluidStateModelSetMixin {
	@Inject(method = "get", at = @At("RETURN"))
	private void optimod$pinFluidSprites(CallbackInfoReturnable<FluidModel> cir) {
		FluidModel model = cir.getReturnValue();
		if (model == null) {
			return;
		}
		AnimationTracker.pinSprite(model.stillMaterial().sprite());
		AnimationTracker.pinSprite(model.flowingMaterial().sprite());
		var overlay = model.overlayMaterial();
		if (overlay != null) {
			AnimationTracker.pinSprite(overlay.sprite());
		}
	}
}
