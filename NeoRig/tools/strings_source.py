# Источник строк интерфейса: ключ -> (English, Русский).
# Сгенерировать values/strings.xml и values-ru/strings.xml:  python3 tools/strings_source.py
import os
from xml.sax.saxutils import escape

S = {
    "app_name": ("NeoRig", "NeoRig"),
    "nav_build": ("Build", "Сборка"),
    "nav_3d": ("3D", "3D"),
    "nav_saved": ("My builds", "Мои сборки"),
    "nav_settings": ("Settings", "Настройки"),

    "step_of": ("Step %1$d of %2$d", "Шаг %1$d из %2$d"),
    "budget_title": ("Your budget", "Ваш бюджет"),
    "budget_subtitle": ("The monitor is included. From 30 000 to 1 000 000 ₽", "Монитор входит в бюджет. От 30 000 до 1 000 000 ₽"),
    "budget_input_label": ("Budget", "Бюджет"),
    "budget_presets": ("Quick picks", "Быстрый выбор"),
    "budget_split_hint": ("About %1$s (%2$d%%) is reserved for the monitor, the rest goes to the system unit", "Около %1$s (%2$d%%) резервируется на монитор, остальное — на системный блок"),
    "budget_min_hint": ("Minimum for the current preferences: %1$s", "Минимум для текущих предпочтений: %1$s"),
    "budget_out_of_range": ("Enter an amount from %1$s to %2$s", "Введите сумму от %1$s до %2$s"),
    "action_next": ("Next", "Далее"),
    "action_back": ("Back", "Назад"),

    "prefs_title": ("Preferences", "Предпочтения"),
    "prefs_cpu": ("Processor", "Процессор"),
    "prefs_gpu": ("Graphics card", "Видеокарта"),
    "prefs_purpose": ("Purpose", "Назначение"),
    "prefs_monitor": ("Monitor", "Монитор"),
    "prefs_resolution": ("Resolution", "Разрешение"),
    "prefs_refresh": ("Refresh rate", "Частота"),
    "any": ("Any", "Неважно"),
    "purpose_gaming": ("Gaming", "Игры"),
    "purpose_gaming_desc": ("Most of the budget goes to the graphics card", "Больше всего бюджета — на видеокарту"),
    "purpose_work": ("Work & editing", "Работа и монтаж"),
    "purpose_work_desc": ("Multi-core CPU, more memory, fast SSD", "Многоядерный процессор, больше памяти, быстрый SSD"),
    "purpose_study": ("Study & office", "Учёба и офис"),
    "purpose_study_desc": ("Quiet and balanced, integrated graphics is fine", "Тихо и сбалансированно, подойдёт встроенная графика"),
    "hz": ("%1$d Hz", "%1$d Гц"),
    "action_assemble": ("Assemble", "Собрать"),
    "assembling": ("Assembling…", "Подбираем…"),
    "summary_budget": ("Budget", "Бюджет"),
    "summary_minimum": ("Minimum budget", "Минимальный бюджет"),
    "error_insufficient_title": ("Not enough budget", "Бюджета не хватает"),
    "error_insufficient_body": ("The cheapest compatible build for these preferences costs %1$s. Add %2$s to the budget or change the preferences.", "Самая дешёвая совместимая сборка для этих параметров стоит %1$s. Добавьте к бюджету %2$s или измените предпочтения."),
    "action_set_budget": ("Set %1$s and assemble", "Установить %1$s и собрать"),
    "error_no_parts": ("There are no compatible parts for these preferences", "Для этих предпочтений нет совместимых деталей"),

    "build_title": ("Your build", "Ваша сборка"),
    "build_empty_title": ("No build yet", "Сборки пока нет"),
    "build_empty_body": ("Set a budget and preferences, and we will pick compatible parts", "Укажите бюджет и предпочтения — мы подберём совместимые детали"),
    "action_start": ("Start", "Начать"),
    "total": ("Total", "Итого"),
    "remaining": ("Remaining", "Остаток"),
    "over_budget": ("Over budget", "Перерасход"),
    "spending_title": ("Budget split", "Распределение бюджета"),
    "compat_ok": ("All parts are compatible", "Все детали совместимы"),
    "compat_issues": ("Compatibility issues: %1$d", "Проблемы совместимости: %1$d"),
    "action_autofix": ("Fix automatically", "Исправить автоматически"),
    "power_line": ("Peak draw ≈ %1$d W · PSU must be ≥ %2$d W", "Потребление ≈ %1$d Вт · нужен БП ≥ %2$d Вт"),
    "preview_hint": ("Tap to open in 3D", "Нажмите, чтобы открыть в 3D"),
    "parts_title": ("Components", "Комплектующие"),
    "parts_hint": ("Tap a part to replace it", "Нажмите на деталь, чтобы заменить"),
    "fps_title": ("Estimated FPS", "Оценка FPS"),
    "fps_subtitle": ("High settings, %1$s. A rough estimate based on GPU and CPU tier", "Высокие настройки, %1$s. Условная оценка по тиру видеокарты и процессора"),
    "fps_limited_cpu": ("CPU-bound", "упор в CPU"),
    "fps_limited_cap": ("engine cap", "лимит движка"),
    "fps_above_refresh": ("≥ %1$d Hz", "≥ %1$d Гц"),
    "action_save": ("Save", "Сохранить"),
    "action_share": ("Share", "Поделиться"),
    "action_rebuild": ("Preferences", "Параметры"),
    "save_dialog_title": ("Save build", "Сохранить сборку"),
    "save_dialog_hint": ("Name", "Название"),
    "default_build_name": ("Build for %1$s", "Сборка на %1$s"),
    "action_cancel": ("Cancel", "Отмена"),
    "saved_toast": ("Build “%1$s” saved", "Сборка «%1$s» сохранена"),
    "share_chooser": ("Share build", "Поделиться сборкой"),
    "integrated_graphics": ("Integrated graphics", "Встроенная графика"),

    "picker_title": ("Choose: %1$s", "Выбор: %1$s"),
    "picker_only_compatible": ("Only compatible", "Только совместимые"),
    "picker_current": ("Current part", "Текущая деталь"),
    "picker_compatible": ("Compatible", "Совместимо"),
    "sort_price": ("Price", "Цена"),
    "sort_performance": ("Performance", "Мощность"),

    "cat_cpu": ("Processor", "Процессор"),
    "cat_motherboard": ("Motherboard", "Материнская плата"),
    "cat_gpu": ("Graphics card", "Видеокарта"),
    "cat_ram": ("Memory", "Оперативная память"),
    "cat_ssd": ("SSD", "SSD-накопитель"),
    "cat_psu": ("Power supply", "Блок питания"),
    "cat_cooler": ("CPU cooler", "Кулер"),
    "cat_case": ("Case", "Корпус"),
    "cat_monitor": ("Monitor", "Монитор"),

    "issue_socket": ("CPU socket %1$s doesn't match the motherboard (%2$s)", "Сокет процессора %1$s не совпадает с платой (%2$s)"),
    "issue_cpu_memory": ("The CPU doesn't support %1$s (only %2$s)", "Процессор не поддерживает %1$s (только %2$s)"),
    "issue_ram_type": ("The board takes %1$s, but the memory is %2$s", "Плата под %1$s, а память — %2$s"),
    "issue_form_factor": ("%1$s board doesn't fit the case (supports %2$s)", "Плата %1$s не встанет в корпус (поддерживает %2$s)"),
    "issue_gpu_length": ("The GPU is %1$d mm long, the case fits up to %2$d mm", "Видеокарта длиной %1$d мм, а в корпус влезает до %2$d мм"),
    "issue_cooler_height": ("The cooler is %1$d mm tall, the case allows %2$d mm", "Высота кулера %1$d мм, корпус допускает %2$d мм"),
    "issue_radiator": ("A %1$d mm radiator doesn't fit (max %2$d mm)", "Радиатор %1$d мм не помещается (максимум %2$d мм)"),
    "issue_radiator_none": ("This case has no room for a liquid cooling radiator", "В этот корпус не ставится радиатор СЖО"),
    "issue_cooler_socket": ("The cooler has no mount for %1$s", "У кулера нет крепления под %1$s"),
    "issue_psu": ("The %1$d W PSU is too weak: at least %2$d W is needed", "БП на %1$d Вт слабоват: нужно не меньше %2$d Вт"),
    "issue_igpu": ("%1$s has no integrated graphics", "У %1$s нет встроенной графики"),
    "issue_cooler_tdp": ("The cooler handles %1$d W, the CPU needs %2$d W — it may overheat", "Кулер рассчитан на %1$d Вт, а процессору нужно %2$d Вт — возможен перегрев"),

    "spec_cores": ("%1$d cores / %2$d threads", "ядер: %1$d, потоков: %2$d"),
    "spec_boost": ("up to %1$s GHz", "до %1$s ГГц"),
    "spec_igpu": ("integrated graphics", "встроенная графика"),
    "spec_igpu_desc": ("uses the processor's graphics", "графика в процессоре"),
    "spec_vram": ("%1$d GB VRAM", "%1$d ГБ видеопамяти"),
    "spec_length": ("%1$d mm", "%1$d мм"),
    "spec_tdp": ("%1$d W", "%1$d Вт"),
    "spec_capacity_gb": ("%1$d GB", "%1$d ГБ"),
    "spec_capacity_tb": ("%1$s TB", "%1$s ТБ"),
    "spec_speed": ("%1$d MHz", "%1$d МГц"),
    "spec_read": ("read %1$d MB/s", "чтение %1$d МБ/с"),
    "spec_wifi": ("Wi-Fi", "Wi-Fi"),
    "spec_modular": ("modular", "модульный"),
    "spec_air": ("air", "воздушный"),
    "spec_aio": ("liquid %1$d mm", "СЖО %1$d мм"),
    "spec_height": ("height %1$d mm", "высота %1$d мм"),
    "spec_cooler_tdp": ("up to %1$d W", "до %1$d Вт"),
    "spec_gpu_max": ("GPU up to %1$d mm", "видеокарта до %1$d мм"),
    "spec_inch": ("%1$s″", "%1$s″"),
    "spec_hz": ("%1$d Hz", "%1$d Гц"),

    "viewer_empty_title": ("Nothing to show yet", "Пока нечего показать"),
    "viewer_empty_body": ("Assemble a PC and it will appear here in 3D", "Соберите ПК — он появится здесь в 3D"),
    "viewer_rgb": ("RGB lighting", "RGB-подсветка"),
    "rgb_static": ("Static", "Статика"),
    "rgb_rainbow": ("Rainbow", "Радуга"),
    "rgb_breathing": ("Breathing", "Дыхание"),
    "viewer_reset": ("Reset camera", "Сбросить камеру"),
    "viewer_hint": ("Drag to rotate · pinch to zoom · double-tap to reset", "Палец — вращение · щипок — зум · двойной тап — сброс"),

    "saved_title": ("My builds", "Мои сборки"),
    "saved_empty_title": ("No saved builds", "Сохранённых сборок нет"),
    "saved_empty_body": ("Save a build on the Build tab to compare and share it later", "Сохраните сборку на вкладке «Сборка», чтобы потом сравнить её и поделиться"),
    "saved_compare_hint": ("Tick two builds to compare them", "Отметьте две сборки, чтобы сравнить их"),
    "action_compare": ("Compare (%1$d/2)", "Сравнить (%1$d/2)"),
    "action_open": ("Open", "Открыть"),
    "action_delete": ("Delete", "Удалить"),
    "delete_confirm_title": ("Delete the build?", "Удалить сборку?"),
    "delete_confirm_body": ("“%1$s” will be deleted permanently", "«%1$s» будет удалена без возможности восстановления"),

    "compare_title": ("Comparison", "Сравнение"),
    "compare_total": ("Total", "Итого"),
    "compare_fps": ("FPS, %1$s", "FPS, %1$s"),
    "compare_better": ("more powerful", "мощнее"),
    "compare_missing": ("Build not found", "Сборка не найдена"),

    "settings_title": ("Settings", "Настройки"),
    "settings_language": ("Language", "Язык"),
    "lang_system": ("System", "Системный"),
    "lang_ru": ("Русский", "Русский"),
    "lang_en": ("English", "English"),
    "settings_3d": ("3D view", "3D-вид"),
    "settings_antialiasing": ("Antialiasing (MSAA 4×)", "Сглаживание (MSAA 4×)"),
    "settings_antialiasing_desc": ("Smoother edges, slightly higher GPU load", "Гладкие края, чуть выше нагрузка на GPU"),
    "settings_autorotate": ("Auto-rotate the preview", "Автовращение превью"),
    "settings_autorotate_desc": ("The 3D preview on the Build tab slowly turns", "3D-превью на вкладке «Сборка» медленно вращается"),
    "settings_rgb_default": ("Lighting colour", "Цвет подсветки"),
    "settings_data": ("Data", "Данные"),
    "settings_parts_count": ("Parts in the database: %1$d", "Деталей в базе: %1$d"),
    "settings_clear": ("Delete all saved builds", "Удалить все сохранённые сборки"),
    "settings_clear_confirm": ("Delete all saved builds?", "Удалить все сохранённые сборки?"),
    "settings_about": ("About", "О приложении"),
    "settings_about_body": ("NeoRig %1$s — a PC configurator with a 3D preview. Prices in the database are approximate (in rubles) and may differ from shops. FPS figures are rough estimates.", "NeoRig %1$s — конфигуратор ПК с 3D-визуализацией. Цены в базе условные (в рублях) и могут отличаться от магазинов. FPS — примерная оценка."),

    "share_header": ("NeoRig build — %1$s", "Сборка NeoRig — %1$s"),
    "share_budget": ("Budget: %1$s · Total: %2$s · Remaining: %3$s", "Бюджет: %1$s · Итого: %2$s · Остаток: %3$s"),
    "share_prefs": ("Purpose: %1$s · Monitor: %2$s, %3$d Hz", "Назначение: %1$s · Монитор: %2$s, %3$d Гц"),
    "share_fps": ("Estimated FPS (%1$s):", "Оценка FPS (%1$s):"),
    "share_footer": ("Assembled in NeoRig", "Собрано в NeoRig"),
}


def esc(text):
    text = escape(text)
    text = text.replace("'", "\\'").replace('"', '\\"')
    return text


def write(path, index):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    lines = ['<?xml version="1.0" encoding="utf-8"?>', "<!-- Сгенерировано tools/strings_source.py — правьте источник -->", "<resources>"]
    for key, values in S.items():
        extra = ""
        lines.append(f'    <string name="{key}"{extra}>{esc(values[index])}</string>')
    lines.append("</resources>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "src", "main", "res")
    write(os.path.join(res, "values", "strings.xml"), 0)
    write(os.path.join(res, "values-ru", "strings.xml"), 1)
    print(f"{len(S)} строк записано")
