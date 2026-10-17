package dev.optimod.mixin.background;

import dev.optimod.background.OptimodSoundEngine;
import net.minecraft.client.resources.sounds.SoundInstance;
import net.minecraft.client.sounds.ChannelAccess;
import net.minecraft.client.sounds.SoundEngine;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Пересчёт громкости уже играющих звуков. Поля и метод {@code calculateVolume} сверены
 * с Dynamic FPS для 26.3. Ничего не перехватывает — только добавляет метод.
 */
@Mixin(SoundEngine.class)
public abstract class SoundEngineMixin implements OptimodSoundEngine {
	@Shadow
	private boolean loaded;

	@Shadow
	@Final
	private Map<SoundInstance, ChannelAccess.ChannelHandle> instanceToChannel;

	@Shadow
	private float calculateVolume(SoundInstance instance) {
		throw new AssertionError("Тело заменяется миксином");
	}

	@Override
	public void optimod$refreshVolumes() {
		if (!this.loaded) {
			return;
		}

		// Копия: звук может закончиться прямо во время обхода
		List<SoundInstance> sounds;
		try {
			sounds = new ArrayList<>(this.instanceToChannel.keySet());
		} catch (RuntimeException e) {
			return;
		}

		for (SoundInstance sound : sounds) {
			ChannelAccess.ChannelHandle handle = this.instanceToChannel.get(sound);
			if (handle == null) {
				continue;
			}
			float volume = this.calculateVolume(sound);
			// Канал живёт в звуковом потоке — меняем громкость через его очередь
			handle.execute(channel -> channel.setVolume(volume));
		}
	}
}
