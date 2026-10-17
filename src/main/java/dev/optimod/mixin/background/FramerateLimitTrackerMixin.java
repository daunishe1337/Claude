package dev.optimod.mixin.background;

import com.mojang.blaze3d.platform.FramerateLimitTracker;
import dev.optimod.background.BackgroundManager;
import dev.optimod.compat.Modules;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * В 26.3 игра спрашивает лимит FPS у {@code FramerateLimitTracker#getFramerateLimit()}
 * (так же его перехватывает Dynamic FPS для 26.3). Внедряемся в RETURN — то есть уже после
 * ванильной логики (меню, бездействие, свёрнутое окно) — и только уменьшаем значение.
 */
@Mixin(FramerateLimitTracker.class)
public abstract class FramerateLimitTrackerMixin {
	@Inject(method = "getFramerateLimit", at = @At("RETURN"), cancellable = true)
	private void optimod$limitInBackground(CallbackInfoReturnable<Integer> cir) {
		if (!Modules.background) {
			return;
		}
		int vanilla = cir.getReturnValueI();
		int limited = BackgroundManager.limitFramerate(vanilla);
		if (limited != vanilla) {
			cir.setReturnValue(limited);
		}
	}
}
