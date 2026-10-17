package dev.optimod.mixin.animation;

import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.QuadInstance;
import com.mojang.blaze3d.vertex.VertexConsumer;
import dev.optimod.animation.AnimationTracker;
import net.minecraft.client.resources.model.geometry.BakedQuad;
import org.spongepowered.asm.mixin.Mixin;

/**
 * Предметы (в руке, в рамке, на земле) и прочие модели рисуются квадами через
 * {@code VertexConsumer#putBakedQuad}. В ванильном {@code BufferBuilder} этот метод
 * не переопределён (используется default-метод интерфейса), поэтому мы добавляем
 * переопределение, которое вызывает исходную реализацию и отмечает спрайт.
 * Это не {@code @Overwrite}: поведение не меняется. Тот же приём использует Sodium в 26.3.
 * <p>
 * Лежит в НЕОБЯЗАТЕЛЬНОМ конфиге: если в будущей версии BufferBuilder сам переопределит
 * метод, Mixin откажется применять этот класс (requireAnnotations), игра продолжит работу.
 */
@Mixin(BufferBuilder.class)
public abstract class BufferBuilderMixin implements VertexConsumer {
	@Override
	public void putBakedQuad(PoseStack.Pose pose, BakedQuad quad, QuadInstance instance) {
		VertexConsumer.super.putBakedQuad(pose, quad, instance);
		AnimationTracker.markSprite(quad.materialInfo().sprite());
	}
}
