package dev.optimod.mixin.memory;

import com.llamalad7.mixinextras.injector.wrapoperation.Operation;
import com.llamalad7.mixinextras.injector.wrapoperation.WrapOperation;
import dev.optimod.memory.CubeCache;
import net.minecraft.client.model.geom.ModelPart;
import net.minecraft.client.model.geom.builders.CubeDefinition;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;

import java.util.Set;

/**
 * Дедупликация кубов моделей сущностей при запекании.
 * <p>
 * Здесь нельзя обойтись {@code @Inject}: нужно подменить сам результат {@code new ModelPart.Cube(...)}.
 * Используется {@code @WrapOperation} из MixinExtras (входит в Fabric Loader) — в отличие от
 * {@code @Redirect} он корректно уживается с такими же обёртками других модов.
 * <p>
 * ВНИМАНИЕ: дескриптор конструктора сверен с ModernFix для 26.1.2; для 26.3 он не проверен.
 * Поэтому миксин лежит в НЕОБЯЗАТЕЛЬНОМ конфиге ({@code required: false, defaultRequire: 0}):
 * если сигнатура изменилась, хук просто не применится, игра не упадёт, а в логе будет
 * предупреждение от MemoryManager. Как найти правильный дескриптор — см. README.
 */
@Mixin(CubeDefinition.class)
public abstract class CubeDefinitionMixin {
	@WrapOperation(
			method = "bake",
			at = @At(value = "NEW", target = "(IIFFFFFFFFFZFFLjava/util/Set;)Lnet/minecraft/client/model/geom/ModelPart$Cube;")
	)
	private ModelPart.Cube optimod$deduplicateCube(int texU, int texV,
												   float originX, float originY, float originZ,
												   float dimensionX, float dimensionY, float dimensionZ,
												   float growX, float growY, float growZ,
												   boolean mirror, float texScaleU, float texScaleV,
												   Set<?> visibleFaces,
												   Operation<ModelPart.Cube> original) {
		if (!CubeCache.isEnabled()) {
			return original.call(texU, texV, originX, originY, originZ, dimensionX, dimensionY, dimensionZ,
					growX, growY, growZ, mirror, texScaleU, texScaleV, visibleFaces);
		}

		CubeCache.CubeKey key = new CubeCache.CubeKey(texU, texV, originX, originY, originZ,
				dimensionX, dimensionY, dimensionZ, growX, growY, growZ, mirror, texScaleU, texScaleV, visibleFaces);
		ModelPart.Cube cached = CubeCache.get(key);
		if (cached != null) {
			return cached;
		}

		ModelPart.Cube created = original.call(texU, texV, originX, originY, originZ, dimensionX, dimensionY, dimensionZ,
				growX, growY, growZ, mirror, texScaleU, texScaleV, visibleFaces);
		return CubeCache.store(key, created);
	}
}
