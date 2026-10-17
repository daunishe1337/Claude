package dev.optimod.mixin.background;

import net.minecraft.client.sounds.SoundEngine;
import net.minecraft.client.sounds.SoundManager;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** Доступ к закрытому полю {@code SoundManager#soundEngine} (имя сверено с Dynamic FPS для 26.3). */
@Mixin(SoundManager.class)
public interface SoundManagerAccessor {
	@Accessor("soundEngine")
	SoundEngine optimod$getSoundEngine();
}
