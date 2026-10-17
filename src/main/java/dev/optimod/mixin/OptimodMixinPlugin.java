package dev.optimod.mixin;

import dev.optimod.compat.CompatManager;
import dev.optimod.config.ConfigManager;
import dev.optimod.config.OptimodConfig;
import org.objectweb.asm.tree.ClassNode;
import org.spongepowered.asm.mixin.extensibility.IMixinConfigPlugin;
import org.spongepowered.asm.mixin.extensibility.IMixinInfo;

import java.util.List;
import java.util.Set;

/**
 * Решает, какие миксины вообще применять.
 * <p>
 * Большинство модулей включаются/выключаются на лету (хуки проверяют флаги {@code Modules}),
 * поэтому их миксины применяются всегда. Здесь отсекаются только случаи настоящего
 * технического конфликта, когда два мода не могут менять один и тот же метод.
 * <p>
 * Работает до запуска игры: нельзя трогать классы Minecraft.
 */
public final class OptimodMixinPlugin implements IMixinConfigPlugin {
	private static final String PACKAGE_PREFIX = "dev.optimod.mixin.";

	@Override
	public void onLoad(String mixinPackage) {
		ConfigManager.get();
		CompatManager.init();
	}

	@Override
	public boolean shouldApplyMixin(String targetClassName, String mixinClassName) {
		String name = mixinClassName.startsWith(PACKAGE_PREFIX)
				? mixinClassName.substring(PACKAGE_PREFIX.length())
				: mixinClassName;
		OptimodConfig config = ConfigManager.get();

		// Модуль анимаций: Sodium сам делает то же самое и добавляет BufferBuilder#putBakedQuad —
		// наш миксин добавил бы такой же метод, поэтому при Sodium не применяем ни одного.
		if (name.startsWith("animation.")) {
			return !CompatManager.isModLoaded("sodium");
		}

		// Дедупликация кубов моделей: ModernFix делает ровно то же самое (compact_entity_models).
		if (name.startsWith("memory.")) {
			return config.memory.enabled
					&& config.memory.deduplicateModelCubes
					&& !CompatManager.isModLoaded("modernfix");
		}

		return true;
	}

	@Override
	public String getRefMapperConfig() {
		// Игра без обфускации — refmap не нужен
		return null;
	}

	@Override
	public void acceptTargets(Set<String> myTargets, Set<String> otherTargets) {
	}

	@Override
	public List<String> getMixins() {
		return null;
	}

	@Override
	public void preApply(String targetClassName, ClassNode targetClass, String mixinClassName, IMixinInfo mixinInfo) {
	}

	@Override
	public void postApply(String targetClassName, ClassNode targetClass, String mixinClassName, IMixinInfo mixinInfo) {
	}
}
