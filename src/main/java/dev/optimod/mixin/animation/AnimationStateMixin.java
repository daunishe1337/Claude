package dev.optimod.mixin.animation;

import dev.optimod.animation.AnimationTracker;
import dev.optimod.animation.OptimodAnimationState;
import net.minecraft.client.renderer.texture.SpriteContents;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Состояние одной анимации. Каждый тик игра вызывает {@code tick()} (сдвигает счётчики кадров),
 * а затем {@code needsToDraw()} — нужно ли перерисовать кадр в атласе на видеокарте.
 * Мы не трогаем {@code tick()} (фаза анимации остаётся правильной), а для невидимых
 * спрайтов отвечаем «рисовать не нужно» — экономится загрузка кадра.
 * Имена методов и поле {@code frame} сверены с Sodium для 26.3.
 */
@Mixin(SpriteContents.AnimationState.class)
public abstract class AnimationStateMixin implements OptimodAnimationState {
	/** Номер текущего кадра анимации. */
	@Shadow
	private int frame;

	@Unique
	private SpriteContents optimod$contents;
	@Unique
	private boolean optimod$skipThisTick;
	/** Анимация хотя бы раз прошла все кадры (до этого не вмешиваемся — как в Sodium). */
	@Unique
	private boolean optimod$cycleCompleted;
	@Unique
	private int optimod$lastFrame;

	@Override
	public void optimod$setContents(SpriteContents contents) {
		this.optimod$contents = contents;
	}

	@Inject(method = "tick", at = @At("HEAD"))
	private void optimod$beforeTick(CallbackInfo ci) {
		this.optimod$skipThisTick = !AnimationTracker.shouldAnimate(this.optimod$contents);
	}

	@Inject(method = "needsToDraw", at = @At("HEAD"), cancellable = true)
	private void optimod$skipInvisible(CallbackInfoReturnable<Boolean> cir) {
		if (!this.optimod$cycleCompleted) {
			// Номер кадра уменьшился — значит, анимация прошла полный круг
			if (this.frame < this.optimod$lastFrame) {
				this.optimod$cycleCompleted = true;
			} else {
				this.optimod$lastFrame = this.frame;
				return;
			}
		}

		if (this.optimod$skipThisTick) {
			AnimationTracker.onUploadSkipped();
			cir.setReturnValue(false);
		}
	}
}
