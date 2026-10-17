package dev.optimod.mixin.particle;

import dev.optimod.particle.ParticleTracker;
import net.minecraft.client.particle.Particle;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Учёт каждой созданной частицы — для лимита количества.
 * <p>
 * {@code "<init>"} без дескриптора совпадает со всеми конструкторами класса. Один конструктор
 * может вызывать другой ({@code this(...)}), поэтому флаг {@code optimod$counted} не даёт
 * посчитать частицу дважды.
 */
@Mixin(Particle.class)
public abstract class ParticleMixin {
	@Unique
	private boolean optimod$counted;

	@Inject(method = "<init>", at = @At("RETURN"))
	private void optimod$onCreated(CallbackInfo ci) {
		if (!this.optimod$counted) {
			this.optimod$counted = true;
			ParticleTracker.onParticleCreated((Particle) (Object) this);
		}
	}
}
