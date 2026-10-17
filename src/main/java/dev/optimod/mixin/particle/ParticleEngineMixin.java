package dev.optimod.mixin.particle;

import dev.optimod.particle.ParticleLimiter;
import net.minecraft.client.particle.Particle;
import net.minecraft.client.particle.ParticleEngine;
import net.minecraft.core.particles.ParticleOptions;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Главная точка создания частиц: {@code ParticleEngine#createParticle(ParticleOptions, x, y, z, xd, yd, zd)}.
 * Через неё проходят частицы мира, сущностей, фейерверков и т.д.
 * Сигнатура 26.3 сверена с модом Sodium Extra. Ванильный метод может вернуть null —
 * вызывающий код к этому готов, поэтому «отказ» = вернуть null до создания объекта.
 */
@Mixin(ParticleEngine.class)
public abstract class ParticleEngineMixin {
	@Inject(method = "createParticle", at = @At("HEAD"), cancellable = true)
	private void optimod$limitParticle(ParticleOptions options, double x, double y, double z,
									   double xSpeed, double ySpeed, double zSpeed,
									   CallbackInfoReturnable<Particle> cir) {
		if (ParticleLimiter.isActive() && !ParticleLimiter.allowSpawn(x, y, z)) {
			cir.setReturnValue(null);
		}
	}
}
