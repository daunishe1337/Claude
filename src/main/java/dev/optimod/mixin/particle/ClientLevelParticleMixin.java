package dev.optimod.mixin.particle;

import dev.optimod.particle.ParticleLimiter;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.BlockState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Частицы разрушения блоков создаются в обход {@code ParticleEngine#createParticle}
 * (сразу пачкой до 64 штук), поэтому ограничиваем их отдельно.
 * Сигнатуры 26.3 сверены с модом Sodium Extra. Звук разрушения проигрывается отдельно
 * и не затрагивается.
 */
@Mixin(ClientLevel.class)
public abstract class ClientLevelParticleMixin {
	/** Облако частиц при разрушении блока. */
	@Inject(method = "addDestroyBlockEffect", at = @At("HEAD"), cancellable = true)
	private void optimod$limitDestroyEffect(BlockPos pos, BlockState state, CallbackInfo ci) {
		if (ParticleLimiter.isActive() && !ParticleLimiter.allowSpawn(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5)) {
			ci.cancel();
		}
	}

	/** Крошки, пока блок ломают. */
	@Inject(method = "addBreakingParticles", at = @At("HEAD"), cancellable = true)
	private void optimod$limitBreakingParticles(BlockPos pos, Direction direction, BlockState state, CallbackInfo ci) {
		if (ParticleLimiter.isActive() && !ParticleLimiter.allowSpawn(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5)) {
			ci.cancel();
		}
	}
}
