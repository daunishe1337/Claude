package dev.optimod.config;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;
import dev.optimod.Optimod;
import net.fabricmc.loader.api.FabricLoader;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Загрузка и сохранение {@code config/optimod.json}.
 * <p>
 * Вызывается в том числе из mixin-плагина (очень рано), поэтому не использует классы игры —
 * только Gson и Fabric Loader.
 */
public final class ConfigManager {
	private static final Gson GSON = new GsonBuilder()
			.setPrettyPrinting()
			.disableHtmlEscaping()
			.create();

	private static volatile OptimodConfig config;

	private ConfigManager() {
	}

	/** Текущий конфиг. При первом обращении читается с диска. */
	public static OptimodConfig get() {
		OptimodConfig current = config;
		if (current == null) {
			synchronized (ConfigManager.class) {
				if (config == null) {
					load();
				}
				current = config;
			}
		}
		return current;
	}

	public static Path path() {
		return FabricLoader.getInstance().getConfigDir().resolve(Optimod.MOD_ID + ".json");
	}

	/**
	 * Читает файл. Если его нет или он повреждён — берёт значения по умолчанию.
	 * После чтения файл перезаписывается: так в нём появляются новые параметры из обновлений мода.
	 */
	public static synchronized void load() {
		Path file = path();
		OptimodConfig loaded = null;

		if (Files.isRegularFile(file)) {
			try (Reader reader = Files.newBufferedReader(file, StandardCharsets.UTF_8)) {
				loaded = GSON.fromJson(reader, OptimodConfig.class);
			} catch (IOException | JsonParseException e) {
				Optimod.LOGGER.error("Не удалось прочитать {} — используются значения по умолчанию. Старый файл сохранён как .broken", file, e);
				backupBrokenFile(file);
			}
		}

		if (loaded == null) {
			loaded = new OptimodConfig();
		}

		loaded.sanitize();
		config = loaded;
		save();
	}

	/** Атомарно записывает конфиг: сначала во временный файл, затем переименование. */
	public static synchronized void save() {
		OptimodConfig current = config;
		if (current == null) {
			return;
		}

		current.sanitize();
		Path file = path();
		Path tmp = file.resolveSibling(file.getFileName() + ".tmp");

		try {
			Files.createDirectories(file.getParent());
			try (Writer writer = Files.newBufferedWriter(tmp, StandardCharsets.UTF_8)) {
				GSON.toJson(current, writer);
			}
			try {
				Files.move(tmp, file, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
			} catch (AtomicMoveNotSupportedException e) {
				Files.move(tmp, file, StandardCopyOption.REPLACE_EXISTING);
			}
		} catch (IOException e) {
			Optimod.LOGGER.error("Не удалось сохранить конфиг {}", file, e);
		}
	}

	private static void backupBrokenFile(Path file) {
		try {
			Files.move(file, file.resolveSibling(file.getFileName() + ".broken"), StandardCopyOption.REPLACE_EXISTING);
		} catch (IOException e) {
			Optimod.LOGGER.warn("Не удалось переименовать повреждённый конфиг {}", file, e);
		}
	}
}
