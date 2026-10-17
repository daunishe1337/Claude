package dev.optimod.animation;

/**
 * «Утиный» интерфейс, который миксин добавляет в {@code SpriteContents}:
 * отметка «спрайт использовался с прошлого тика» и служебные признаки.
 */
public interface OptimodSpriteActivity {
	/** Отметить спрайт как видимый (вызывается при отрисовке). */
	void optimod$markActive();

	/** Видим ли спрайт сейчас, без сброса отметки. */
	boolean optimod$isActive();

	/** Прочитать и сбросить отметку (раз в тик, перед обновлением анимации). */
	boolean optimod$consumeActive();

	/** Есть ли у спрайта анимация. */
	boolean optimod$isAnimated();

	void optimod$setAnimated(boolean animated);

	/** Анимировать всегда (список alwaysAnimate или жидкости). */
	boolean optimod$isPinned();

	void optimod$setPinned(boolean pinned);
}
