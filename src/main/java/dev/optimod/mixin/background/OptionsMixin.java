package dev.optimod.mixin.background;

import dev.optimod.background.BackgroundManager;
import net.minecraft.client.Options;
import net.minecraft.sounds.SoundSource;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Громкость каждой категории звука игра берёт из {@code Options#getSoundSourceVolume(SoundSource)}
 * (сверено с Dynamic FPS для 26.3). Пока окно в фоне, умножаем её на множитель модуля.
 * Сами настройки игрока не меняются и в файл не пишутся.
 * Общую громкость (MASTER) не трогаем: она применяется отдельно, к «слушателю».
 */
@Mixin(Options.class)
public abstract class OptionsMixin {
	@Inject(method = "getSoundSourceVolume", at = @At("RETURN"), cancellable = true)
	private void optimod$backgroundVolume(SoundSource source, CallbackInfoReturnable<Float> cir) {
		float multiplier = BackgroundManager.volumeMultiplier();
		if (multiplier != 1.0f && source != SoundSource.MASTER) {
			cir.setReturnValue(cir.getReturnValueF() * multiplier);
		}
	}
}
