package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.renderer.block.BlockAndTintGetter;
import net.minecraft.client.renderer.block.BlockQuadOutput;
import net.minecraft.client.renderer.block.ModelBlockRenderer;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Каждый квад модели блока с позицией блока. Через этот метод в 26.3 проходит вывод квадов
 * блоков (абстракция {@code BlockQuadOutput}) — по нему мы узнаём, в каких секциях мира
 * встречается анимированный спрайт. Сигнатура сверена с Sodium для 26.3.
 * <p>
 * Вызывается в потоках сборки чанков — внутри только дешёвые операции.
 */
@Mixin(ModelBlockRenderer.class)
public abstract class ModelBlockRendererMixin {
	@Inject(method = "putQuadWithTint", at = @At("HEAD"))
	private void optimod$trackQuad(BlockQuadOutput output, float x, float y, float z,
								   BlockAndTintGetter level, BlockState state, BlockPos pos, BakedQuad quad,
								   CallbackInfo ci) {
		AnimationTracker.onBlockQuad(quad.materialInfo().sprite(), pos);
	}
}
