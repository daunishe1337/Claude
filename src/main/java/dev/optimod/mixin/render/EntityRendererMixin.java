package dev.optimod.mixin.render;

import dev.optimod.render.RenderCullingHooks;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.client.renderer.entity.EntityRenderer;
import net.minecraft.client.renderer.entity.state.EntityRenderState;
import net.minecraft.world.entity.Entity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Точка отсечения сущностей: {@code EntityRenderer#shouldRender}.
 * <p>
 * В 26.3 сигнатура: {@code shouldRender(T entity, Frustum culler, double camX, double camY,
 * double camZ, float partialTicks)} — сверено с миксином Sodium для 26.3.
 * Если метод вернул false, сущность не попадает в извлечение кадра вовсе:
 * экономится и подготовка состояния, и отрисовка модели.
 */
@Mixin(EntityRenderer.class)
public abstract class EntityRendererMixin<T extends Entity, S extends EntityRenderState> {
	/** Ванильный признак: false у сущностей, которые нельзя отсекать (сверено с EntityCulling для 26.3). */
	@Shadow
	abstract boolean affectedByCulling(T entity);

	@Inject(method = "shouldRender", at = @At("HEAD"), cancellable = true)
	private void optimod$cullEntity(T entity, Frustum culler, double camX, double camY, double camZ, float partialTicks,
									CallbackInfoReturnable<Boolean> cir) {
		if (!RenderCullingHooks.anyEntityCulling()) {
			return;
		}
		if (RenderCullingHooks.shouldSkipEntity(entity, camX, camY, camZ, this.affectedByCulling(entity))) {
			cir.setReturnValue(false);
		}
	}
}
