package dev.optimod.background;

/** «Утиный» интерфейс, который миксин добавляет в {@code SoundEngine}. */
public interface OptimodSoundEngine {
	/** Пересчитать громкость всех играющих звуков по текущим настройкам. */
	void optimod$refreshVolumes();
}
