package dev.optimod.particle;

import dev.optimod.compat.Modules;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import net.minecraft.client.Minecraft;
import net.minecraft.client.particle.Particle;

import java.util.ArrayList;

/**
 * Считает живые частицы.
 * <p>
 * В 26.3 частицы хранятся в группах ({@code ParticleGroup}) внутри {@code ParticleEngine},
 * и готового публичного счётчика нет. Поэтому мы сами запоминаем каждую созданную частицу
 * (хук конструктора {@code Particle}) и раз в тик выбрасываем умершие ({@code isAlive() == false}).
 * <p>
 * Защита от «вечных» записей: если игра выбросила частицы без пометки смерти (смена мира,
 * перезагрузка ресурсов), список очищается при смене мира, а любая запись старше
 * {@link #MAX_AGE_TICKS} удаляется — на счётчик это не влияет сильнее, чем на минуту.
 * <p>
 * Только главный поток. Без аллокаций в установившемся режиме (списки переиспользуются).
 */
public final class ParticleTracker {
	/** Через сколько тиков запись удаляется в любом случае (60 секунд). */
	private static final int MAX_AGE_TICKS = 20 * 60;

	private static final ArrayList<Particle> LIVE = new ArrayList<>(2048);
	/** Тик рождения для каждой записи из LIVE (параллельный список без упаковки в Integer). */
	private static final IntArrayList BIRTH_TICKS = new IntArrayList(2048);

	private static int currentTick;

	private ParticleTracker() {
	}

	/** Вызывается из конструктора частицы. */
	public static void onParticleCreated(Particle particle) {
		if (!Modules.particles) {
			return;
		}
		Minecraft minecraft = Minecraft.getInstance();
		// Частицы создаются в главном потоке; чужие потоки (если какой-то мод так делает) не учитываем
		if (minecraft == null || !minecraft.isSameThread()) {
			return;
		}
		LIVE.add(particle);
		BIRTH_TICKS.add(currentTick);
	}

	/** Сколько частиц живо (с точностью до одного тика). */
	public static int liveCount() {
		return LIVE.size();
	}

	/** Раз в тик: выбросить умершие и слишком старые записи. */
	public static void tick() {
		currentTick++;
		if (!Modules.particles) {
			clear();
			return;
		}

		int size = LIVE.size();
		int write = 0;
		for (int read = 0; read < size; read++) {
			Particle particle = LIVE.get(read);
			int birth = BIRTH_TICKS.getInt(read);
			if (particle.isAlive() && currentTick - birth <= MAX_AGE_TICKS) {
				LIVE.set(write, particle);
				BIRTH_TICKS.set(write, birth);
				write++;
			}
		}
		// Обрезаем хвост, не создавая новых списков
		for (int i = size - 1; i >= write; i--) {
			LIVE.remove(i);
		}
		BIRTH_TICKS.size(write);
	}

	/** Смена мира: все старые частицы уничтожены игрой. */
	public static void clear() {
		LIVE.clear();
		BIRTH_TICKS.clear();
	}
}
