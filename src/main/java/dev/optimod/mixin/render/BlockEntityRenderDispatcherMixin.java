package dev.optimod.mixin.render;

import dev.optimod.render.RenderCullingHooks;
import net.minecraft.client.renderer.blockentity.BlockEntityRenderDispatcher;
import net.minecraft.client.renderer.blockentity.BlockEntityRenderer;
import net.minecraft.client.renderer.blockentity.state.BlockEntityRenderState;
import net.minecraft.client.renderer.feature.ModelFeatureRenderer;
import net.minecraft.world.level.block.entity.BlockEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Точка отсечения блок-сущностей: {@code BlockEntityRenderDispatcher#tryExtractRenderState}.
 * <p>
 * Этот метод вызывают и ваниль, и Sodium (у него свой обход секций), поэтому хук работает
 * в обоих случаях. Сигнатура 26.3 сверена с модом EntityCulling:
 * {@code (BlockEntity, float partialTicks, CrumblingOverlay, boolean isGloballyRendered)}.
 * Возврат null означает «ничего не рисовать» — вызывающий код это обрабатывает.
 */
@Mixin(BlockEntityRenderDispatcher.class)
public abstract class BlockEntityRenderDispatcherMixin {
	@Shadow
	public abstract <E extends BlockEntity> BlockEntityRenderer<E, ?> getRenderer(E blockEntity);

	@Inject(method = "tryExtractRenderState", at = @At("HEAD"), cancellable = true)
	private void optimod$cullBlockEntity(BlockEntity blockEntity, float partialTicks,
										 ModelFeatureRenderer.CrumblingOverlay crumblingOverlay, boolean isGloballyRendered,
										 CallbackInfoReturnable<BlockEntityRenderState> cir) {
		// «Глобальные» блок-сущности (маяк и т.п.) видны издалека — их не трогаем
		if (blockEntity == null || isGloballyRendered || !RenderCullingHooks.anyBlockEntityCulling()) {
			return;
		}

		BlockEntityRenderer<?, ?> renderer = this.getRenderer(blockEntity);
		// Нет рендерера — ваниль сама ничего не нарисует; рисуемые вне экрана (лучи) не трогаем
		if (renderer == null || renderer.shouldRenderOffScreen()) {
			return;
		}

		if (RenderCullingHooks.shouldSkipBlockEntity(blockEntity)) {
			cir.setReturnValue(null);
		}
	}
}
