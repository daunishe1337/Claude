package dev.optimod.animation;

import dev.optimod.compat.Modules;
import dev.optimod.config.ConfigManager;
import dev.optimod.render.FrameContext;
import it.unimi.dsi.fastutil.longs.LongIterator;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import net.minecraft.client.Minecraft;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.client.renderer.texture.SpriteContents;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.chunk.LevelChunk;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Модуль 6: загружать на видеокарту новые кадры анимированных текстур (вода в котле, магма,
 * ламинария, огонь, порталы…) только когда текстура видна.
 * <p>
 * Как определяется «видна»:
 * <ol>
 *     <li>Блоки мира. Когда игра собирает меш секции чанка, каждый квад проходит через
 *     {@code ModelBlockRenderer#putQuadWithTint} с позицией блока. Мы запоминаем, в каких
 *     секциях (16×16×16) встречается каждый анимированный спрайт. Раз в тик проверяем,
 *     попадает ли хотя бы одна такая секция в пирамиду видимости камеры.</li>
 *     <li>Всё остальное (интерфейс, частицы, предметы в руке, мгновенная отрисовка блоков)
 *     отмечает спрайт напрямую в момент отрисовки.</li>
 *     <li>Спрайты, которых мы ни разу не видели в мешах чанков, считаются «неизвестными»
 *     и анимируются всегда — так модуль не ломает то, чего не понимает.</li>
 * </ol>
 * Пропускается только загрузка кадра; счётчик кадров анимации продолжает идти, поэтому
 * когда текстура снова появится в кадре, она покажет правильную фазу.
 * <p>
 * Тот же подход (отметки «активности» спрайтов) используется в Sodium; при установленном
 * Sodium модуль не применяется вовсе.
 */
public final class AnimationTracker {
	/** Спрайт → секции мира, в мешах которых он встречается. Пишут потоки сборки чанков. */
	private static final ConcurrentHashMap<SpriteContents, SectionSet> WORLD_SECTIONS = new ConcurrentHashMap<>();

	/** Имена из конфига alwaysAnimate. */
	private static volatile Set<String> pinnedNames = Set.of();

	// Данные для проверки видимости в текущем тике (главный поток)
	private static Frustum tickFrustum;
	private static int cameraSectionX;
	private static int cameraSectionY;
	private static int cameraSectionZ;

	/** Тик, в котором регистрировались анимации (новая пачка = перезагрузка ресурсов). */
	private static int registerBatchTick = Integer.MIN_VALUE;
	private static int tickCounter;

	// Статистика за тик
	private static int curAnimations;
	private static int curSkipped;
	public static int lastAnimations;
	public static int lastSkipped;

	private AnimationTracker() {
	}

	// ------------------------------------------------------------------------------------
	// Регистрация (загрузка атласа текстур)
	// ------------------------------------------------------------------------------------

	/** Атлас создал состояние анимации для спрайта. */
	public static void registerAnimation(SpriteContents contents, SpriteContents.AnimationState state) {
		// Первая регистрация в новом тике = новая загрузка атласов: старые спрайты больше не нужны.
		// Секции заново заполнятся при пересборке чанков после перезагрузки ресурсов.
		if (registerBatchTick != tickCounter) {
			registerBatchTick = tickCounter;
			WORLD_SECTIONS.clear();
			pinnedNames = readPinnedNames();
		}

		OptimodSpriteActivity activity = (OptimodSpriteActivity) contents;
		activity.optimod$setAnimated(true);
		activity.optimod$setPinned(pinnedNames.contains(String.valueOf(contents.name())));
		((OptimodAnimationState) state).optimod$setContents(contents);
	}

	private static Set<String> readPinnedNames() {
		Set<String> names = new HashSet<>();
		for (String name : ConfigManager.get().animations.alwaysAnimate) {
			if (name != null && !name.isBlank()) {
				names.add(name.trim());
			}
		}
		return Set.copyOf(names);
	}

	// ------------------------------------------------------------------------------------
	// Отметки использования
	// ------------------------------------------------------------------------------------

	/** Спрайт нарисован прямо сейчас (интерфейс, частица, предмет и т.п.). */
	public static void markSprite(TextureAtlasSprite sprite) {
		if (sprite == null) {
			return;
		}
		OptimodSpriteActivity activity = (OptimodSpriteActivity) sprite.contents();
		if (activity.optimod$isAnimated()) {
			activity.optimod$markActive();
		}
	}

	/** Спрайт должен анимироваться всегда (жидкости: их меши строятся отдельным путём). */
	public static void pinSprite(TextureAtlasSprite sprite) {
		if (sprite != null) {
			((OptimodSpriteActivity) sprite.contents()).optimod$setPinned(true);
		}
	}

	/**
	 * Квад блока с позицией. Вызывается и из потоков сборки чанков, и из главного потока
	 * (поршни, падающие блоки и т.п.).
	 */
	public static void onBlockQuad(TextureAtlasSprite sprite, BlockPos pos) {
		if (sprite == null || pos == null) {
			return;
		}
		SpriteContents contents = sprite.contents();
		OptimodSpriteActivity activity = (OptimodSpriteActivity) contents;
		if (!activity.optimod$isAnimated()) {
			return;
		}

		long section = packSection(pos.getX() >> 4, pos.getY() >> 4, pos.getZ() >> 4);
		// Лямбда без захвата переменных — JVM не создаёт новый объект при каждом вызове
		WORLD_SECTIONS.computeIfAbsent(contents, key -> new SectionSet()).add(section);

		// В главном потоке это мгновенная отрисовка — значит, спрайт виден прямо сейчас
		if (Minecraft.getInstance().isSameThread()) {
			activity.optimod$markActive();
		}
	}

	// ------------------------------------------------------------------------------------
	// Раз в тик
	// ------------------------------------------------------------------------------------

	/**
	 * Начало клиентского тика — до того, как атласы обновят анимации.
	 * Отмечаем спрайты, у которых есть хотя бы одна секция в поле зрения.
	 */
	public static void beforeAnimationTick() {
		tickCounter++;
		lastAnimations = curAnimations;
		lastSkipped = curSkipped;
		curAnimations = 0;
		curSkipped = 0;

		if (!Modules.animations || WORLD_SECTIONS.isEmpty()) {
			return;
		}

		tickFrustum = FrameContext.valid ? FrameContext.frustum : null;
		cameraSectionX = floor(FrameContext.cameraX) >> 4;
		cameraSectionY = floor(FrameContext.cameraY) >> 4;
		cameraSectionZ = floor(FrameContext.cameraZ) >> 4;

		// Ссылка на статический метод — без аллокации лямбды
		WORLD_SECTIONS.forEach(AnimationTracker::updateWorldVisibility);
		tickFrustum = null;
	}

	private static void updateWorldVisibility(SpriteContents contents, SectionSet sections) {
		OptimodSpriteActivity activity = (OptimodSpriteActivity) contents;
		if (activity.optimod$isActive()) {
			return;
		}
		// Нет данных о камере (первый кадр, меню) — ничего не скрываем
		if (tickFrustum == null || sections.anyVisible(tickFrustum, cameraSectionX, cameraSectionY, cameraSectionZ)) {
			activity.optimod$markActive();
		}
	}

	/**
	 * Нужно ли загружать новый кадр этой анимации в текущем тике.
	 * Вызывается из {@code AnimationState#tick} для каждой анимации.
	 */
	public static boolean shouldAnimate(SpriteContents contents) {
		curAnimations++;
		if (contents == null || !Modules.animations) {
			return true;
		}
		OptimodSpriteActivity activity = (OptimodSpriteActivity) contents;
		boolean active = activity.optimod$consumeActive();
		if (active || activity.optimod$isPinned()) {
			return true;
		}
		// Неизвестный спрайт (не встречался в мешах чанков) — не берёмся судить
		return !WORLD_SECTIONS.containsKey(contents);
	}

	/** Загрузка кадра пропущена. */
	public static void onUploadSkipped() {
		curSkipped++;
	}

	// ------------------------------------------------------------------------------------
	// Жизненный цикл мира
	// ------------------------------------------------------------------------------------

	/** Чанк выгружен — его секции больше не нужны. */
	public static void onChunkUnload(ClientLevel level, LevelChunk chunk) {
		if (WORLD_SECTIONS.isEmpty()) {
			return;
		}
		int sectionX = chunk.getPos().x();
		int sectionZ = chunk.getPos().z();
		int minSectionY = level.getMinY() >> 4;
		int maxSectionY = level.getMaxY() >> 4;
		for (SectionSet sections : WORLD_SECTIONS.values()) {
			sections.removeColumn(sectionX, sectionZ, minSectionY, maxSectionY);
		}
	}

	/** Смена мира. */
	public static void clearWorld() {
		WORLD_SECTIONS.clear();
	}

	/** Сколько анимированных спрайтов привязано к секциям мира. */
	public static int trackedSpriteCount() {
		return WORLD_SECTIONS.size();
	}

	// ------------------------------------------------------------------------------------

	/** Упаковка координат секции: 22 бита X, 22 бита Z, 20 бит Y. */
	static long packSection(int x, int y, int z) {
		return ((long) (x & 0x3FFFFF) << 42) | ((long) (z & 0x3FFFFF) << 20) | (y & 0xFFFFFL);
	}

	static int unpackX(long key) {
		return (int) (key >> 42); // арифметический сдвиг восстанавливает знак
	}

	static int unpackZ(long key) {
		return (int) (key << 22 >> 42);
	}

	static int unpackY(long key) {
		return (int) (key << 44 >> 44);
	}

	private static int floor(double value) {
		int truncated = (int) value;
		return value < truncated ? truncated - 1 : truncated;
	}

	/** Набор секций одного спрайта. Синхронизирован: пишут потоки сборки, читает главный поток. */
	static final class SectionSet {
		private final LongOpenHashSet sections = new LongOpenHashSet();

		synchronized void add(long section) {
			sections.add(section);
		}

		synchronized void removeColumn(int sectionX, int sectionZ, int minSectionY, int maxSectionY) {
			for (int y = minSectionY; y <= maxSectionY; y++) {
				sections.remove(packSection(sectionX, y, sectionZ));
			}
		}

		synchronized boolean anyVisible(Frustum frustum, int camX, int camY, int camZ) {
			LongIterator iterator = sections.iterator();
			while (iterator.hasNext()) {
				long key = iterator.nextLong();
				if (isSectionVisible(frustum, unpackX(key), unpackY(key), unpackZ(key), camX, camY, camZ)) {
					return true;
				}
			}
			return false;
		}

		/**
		 * Секции рядом с камерой видны всегда; остальные проверяем по 27 точкам
		 * (углы, середины рёбер и граней, центр). Пирамида видимости широкая, поэтому
		 * секция, задетая ею, почти всегда содержит хотя бы одну из этих точек.
		 */
		private static boolean isSectionVisible(Frustum frustum, int x, int y, int z, int camX, int camY, int camZ) {
			if (Math.abs(x - camX) <= 2 && Math.abs(y - camY) <= 2 && Math.abs(z - camZ) <= 2) {
				return true;
			}
			double baseX = x << 4;
			double baseY = y << 4;
			double baseZ = z << 4;
			// Сначала центр — самый вероятный случай
			if (frustum.pointInFrustum(baseX + 8, baseY + 8, baseZ + 8)) {
				return true;
			}
			for (int dx = 0; dx <= 16; dx += 8) {
				for (int dy = 0; dy <= 16; dy += 8) {
					for (int dz = 0; dz <= 16; dz += 8) {
						if (frustum.pointInFrustum(baseX + dx, baseY + dy, baseZ + dz)) {
							return true;
						}
					}
				}
			}
			return false;
		}
	}
}
