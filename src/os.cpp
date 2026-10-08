// Экран компьютера: заставка POST, BIOS (Raven / Polaris / NovaBIOS), установщик и рабочий стол NovaOS
#include "game.hpp"
#include "platform.hpp"
#include "render.hpp"
#include <algorithm>

using namespace ui;

namespace {

// ---------------------------------------------------------------------------
// Общие строки об оборудовании

std::string cpuLine(const Build& b, const OcSettings& oc) {
    if (!b.cpu) return "не обнаружен";
    bool canOC = b.mb && b.mb->def->oc;
    float ghz = b.cpu->def->ghz + (canOC ? oc.cpuOC * 0.1f : 0);
    return fmt("%s @ %.1f ГГц", b.cpu->def->name.c_str(), ghz);
}

std::string ramLine(const Build& b, const OcSettings& oc) {
    if (b.ram.empty()) return "не обнаружена";
    return fmt("%d ГБ %s @ %d МГц (%d шт.)", b.ramGB, ramName(b.ram[0]->def->ram), ramMhz(b, oc.xmp), (int)b.ram.size());
}

std::string videoLine(const Build& b, bool& bad) {
    bad = false;
    if (b.gpuWorks()) return fmt("%s (%d ГБ)", b.gpu->def->name.c_str(), b.gpu->def->vram);
    if (b.gpu) {
        bad = true;
        return b.gpu->def->name + " — NOT SUPPORTED, используется встроенная графика";
    }
    return "встроенная графика процессора";
}

std::string driveLine(const Entity* d) {
    std::string cap = d->def->capGB >= 1000 ? fmt("%d ТБ", d->def->capGB / 1000) : fmt("%d ГБ", d->def->capGB);
    return fmt("%s %s%s", storName(d->def->stor), cap.c_str(), d->osInstalled ? "  [NovaOS]" : "");
}

float mbTemp(const Computer& c) { return 22 + (c.cpuTemp - 24) * 0.35f; }

int fanRpm(const Computer& c, int i) {
    int base = c.oc.fanProfile == 0 ? 750 : (c.oc.fanProfile == 2 ? 1700 : 1150);
    float heat = clampf((c.cpuTemp - 35) / 50, 0, 1);
    return (int)(base * (0.8f + 0.6f * heat)) + i * 23;
}

// ---------------------------------------------------------------------------
// Оформление BIOS у разных производителей (вымышленные бренды)

struct BiosSkin {
    const char* brand;
    const char* title;
    Color bg, bg2, panel, accent, text, dim, sel;
    bool leftMenu;
};

BiosSkin skinFor(const Build& b) {
    int style = b.mb ? b.mb->def->biosStyle : 0;
    if (style == 1)
        return BiosSkin{"RAVEN",           "RAVEN UEFI • Gaming BIOS", rgba(10, 8, 10), rgba(34, 12, 14),
                        rgba(24, 18, 20),  rgba(225, 38, 48),          rgba(238, 236, 236), rgba(150, 130, 130),
                        rgba(80, 16, 20),  true};
    if (style == 2)
        return BiosSkin{"POLARIS",         "POLARIS UEFI Setup Utility", rgba(8, 16, 32), rgba(14, 30, 58),
                        rgba(16, 28, 50),  rgba(40, 190, 240),           rgba(232, 240, 250), rgba(130, 155, 185),
                        rgba(20, 70, 110), false};
    return BiosSkin{"NovaBIOS",         "NovaBIOS Setup Utility v2.1", rgba(22, 44, 140), rgba(22, 44, 140),
                    rgba(200, 200, 204), rgba(255, 220, 80),           rgba(255, 255, 255), rgba(190, 200, 230),
                    rgba(200, 200, 204), false};
}

const char* kBiosTabs[] = {"Главная", "Разгон", "Мониторинг", "Загрузка", "Выход"};

// Строка «параметр — значение»
void kv(float x, float y, float w, const std::string& k, const std::string& v, Color kc, Color vc, float fs = 17) {
    gfx::text(k, x, y, fs, kc);
    gfx::text(v, x + w * 0.38f, y, fs, vc, true);
}

}  // namespace

// ---------------------------------------------------------------------------

void Game::drawOS(Computer& c, float W, float H) {
    Entity* cs = world.get(c.caseId);
    Build b = gatherBuild(world, cs);
    const Color white = rgba(255, 255, 255);

    switch (c.st) {
        // ----------------------------------------------------------- POST
        case PCState::Post: {
            BiosSkin sk = skinFor(b);
            int style = b.mb ? b.mb->def->biosStyle : 0;
            gfx::rect(Rect{0, 0, W, H}, style == 0 ? rgba(0, 0, 0) : sk.bg);
            float y = 30;
            if (style == 0) {
                gfx::text("NovaBIOS v2.1  •  Energy Star Ally", 30, y, 20, white, true);
                y += 34;
                gfx::text(b.mb ? b.mb->def->name : "", 30, y, 16, rgba(180, 180, 180));
                y += 40;
            } else {
                // Заставка производителя
                gfx::rectGrad(Rect{0, 0, W, H}, sk.bg2, sk.bg);
                gfx::textCentered(sk.brand, Rect{0, 150, W, 120}, 110, sk.accent, true);
                gfx::rect(Rect{W * 0.3f, 285, W * 0.4f, 3}, sk.accent);
                gfx::textCentered(b.mb ? b.mb->def->name : "", Rect{0, 300, W, 40}, 22, sk.text);
                y = 430;
            }
            Color lc = style == 0 ? rgba(200, 200, 200) : sk.dim;
            Color vc = style == 0 ? white : sk.text;
            float fs = style == 0 ? 17 : 15;
            float lh = style == 0 ? 28 : 22;
            kv(30, y, 900, "Процессор", cpuLine(b, c.oc), lc, vc, fs);
            y += lh;
            kv(30, y, 900, "Память", ramLine(b, c.oc), lc, vc, fs);
            y += lh;
            bool badGpu;
            std::string vl = videoLine(b, badGpu);
            kv(30, y, 900, "Видео", vl, lc, badGpu ? rgba(255, 80, 80) : vc, fs);
            y += lh;
            if (style == 0) {
                for (Entity* d : b.drives) {
                    kv(30, y, 900, "Накопитель", driveLine(d), lc, vc, fs);
                    y += lh;
                }
                if (b.usb) {
                    kv(30, y, 900, "USB", "Флешка NovaOS (загрузочная)", lc, vc, fs);
                    y += lh;
                }
            }
            if (!b.cooler) {
                gfx::text("ВНИМАНИЕ: кулер процессора не обнаружен!", 30, y, fs, rgba(255, 90, 90), true);
                y += lh;
            } else if (b.cpu && !b.cpu->pasted) {
                gfx::text("ВНИМАНИЕ: высокая температура процессора — проверьте термопасту", 30, y, fs, rgba(255, 190, 60), true);
                y += lh;
            }
            gfx::text(fmt("CPU %.0f°C", c.cpuTemp), W - 150, 30, 18, style == 0 ? white : sk.text, true);
            Rect bb{30, H - 70, 360, 44};
            if (c.rebootToBios) {
                gfx::text("Вход в BIOS...", 30, H - 60, 20, style == 0 ? rgba(255, 220, 80) : sk.accent, true);
            } else {
                if (button(bb, "DEL — войти в BIOS", 19, style == 0 ? rgba(60, 60, 70) : sk.accent)) enterBios(c);
                float p = clampf(c.t / 3.5f, 0, 1);
                bar(Rect{W - 330, H - 52, 300, 10}, p, style == 0 ? rgba(200, 200, 200) : sk.accent);
            }
            break;
        }

        // ----------------------------------------------------------- BIOS
        case PCState::Bios: {
            BiosSkin sk = skinFor(b);
            int style = b.mb ? b.mb->def->biosStyle : 0;
            OcSettings& e = c.biosEdit;
            bool canOC = b.mb && b.mb->def->oc;
            bool amd = b.mb && b.mb->def->socket.rfind("AM", 0) == 0;
            gfx::rect(Rect{0, 0, W, H}, sk.bg);
            if (style != 0) gfx::rectGrad(Rect{0, 0, W, 90}, sk.bg2, sk.bg);

            // Шапка
            float hh = style == 0 ? 44 : 84;
            if (style == 0) {
                gfx::rect(Rect{0, 0, W, hh}, rgba(200, 200, 204));
                gfx::textCentered(sk.title, Rect{0, 0, W, hh}, 20, rgba(22, 44, 140), true);
            } else {
                gfx::text(sk.brand, 24, 12, 38, sk.accent, true);
                gfx::text(sk.title, 26, 56, 14, sk.dim);
                float x = 330;
                auto stat = [&](const std::string& k, const std::string& v) {
                    gfx::text(k, x, 16, 13, sk.dim);
                    gfx::text(v, x, 36, 20, sk.text, true);
                    x += 150;
                };
                stat("CPU темп.", fmt("%.0f°C", c.cpuTemp));
                stat("Плата темп.", fmt("%.0f°C", mbTemp(c)));
                stat("CPU частота", b.cpu ? fmt("%.1f ГГц", b.cpu->def->ghz + (canOC ? c.oc.cpuOC * 0.1f : 0)) : "—");
                stat("Память", b.ram.empty() ? "—" : fmt("%d МГц", ramMhz(b, c.oc.xmp)));
                gfx::rect(Rect{0, hh - 2, W, 2}, sk.accent);
            }

            // Вкладки
            Rect content;
            if (sk.leftMenu) {
                float mw = 200;
                gfx::rect(Rect{0, hh, mw, H - hh - 36}, sk.panel);
                for (int i = 0; i < 5; i++) {
                    Rect t{0, hh + 16 + i * 56.0f, mw, 50};
                    bool sel = c.biosTab == i;
                    if (sel) {
                        gfx::rect(t, sk.sel);
                        gfx::rect(Rect{t.x, t.y, 5, t.h}, sk.accent);
                    } else if (hover(t)) {
                        gfx::rect(t, withAlpha(sk.sel, 120));
                    }
                    gfx::text(kBiosTabs[i], t.x + 24, t.y + 13, 20, sel ? sk.text : sk.dim, true);
                    if (click(t)) c.biosTab = i;
                }
                content = Rect{mw + 24, hh + 20, W - mw - 48, H - hh - 76};
            } else {
                float tx = 16;
                for (int i = 0; i < 5; i++) {
                    float tw = gfx::textWidth(kBiosTabs[i], 18, true) + 36;
                    Rect t{tx, hh + 8, tw, 40};
                    bool sel = c.biosTab == i;
                    if (style == 0) {
                        if (sel) gfx::rect(t, rgba(200, 200, 204));
                        gfx::textCentered(kBiosTabs[i], t, 18, sel ? rgba(22, 44, 140) : rgba(255, 255, 255), true);
                    } else {
                        gfx::roundRect(t, 6, sel ? sk.accent : (hover(t) ? sk.sel : sk.panel));
                        gfx::textCentered(kBiosTabs[i], t, 18, sel ? rgba(5, 20, 35) : sk.text, true);
                    }
                    if (click(t)) c.biosTab = i;
                    tx += tw + 6;
                }
                content = Rect{24, hh + 64, W - 48, H - hh - 120};
                if (style == 2) {
                    // Справочная колонка справа, как у многих UEFI
                    Rect help{W - 250, content.y, 226, content.h};
                    gfx::roundRect(help, 8, sk.panel);
                    gfx::text("Описание", help.x + 14, help.y + 12, 16, sk.accent, true);
                    const char* desc[] = {
                        "Сводка по системе: процессор, память, видеокарта и накопители.",
                        "Разгон процессора и профиль памяти. Повышайте частоту постепенно и проверяйте стабильность в BenchMax.",
                        "Температуры, напряжения и обороты вентиляторов. Профиль вентиляторов влияет на охлаждение.",
                        "Порядок загрузки. Поставьте USB первым, чтобы переустановить систему с флешки.",
                        "Сохраните изменения, чтобы они применились после перезагрузки."};
                    gfx::textWrapped(desc[c.biosTab], help.x + 14, help.y + 40, help.w - 28, 14, sk.dim);
                    content.w -= 240;
                }
            }
            Color tc = sk.text, dc = sk.dim;
            if (style == 0) {
                gfx::rect(content, rgba(200, 200, 204));
                content = Rect{content.x + 16, content.y + 14, content.w - 32, content.h - 28};
                tc = rgba(20, 30, 90);
                dc = rgba(70, 70, 90);
            }

            float x = content.x, y = content.y, w = content.w;
            switch (c.biosTab) {
                case 0: {
                    kv(x, y, w, "Материнская плата", b.mb ? b.mb->def->name : "", dc, tc);
                    y += 32;
                    kv(x, y, w, "Версия BIOS", fmt("%s 7.%d", sk.brand, 10 + (b.mb ? b.mb->id % 30 : 0)), dc, tc);
                    y += 32;
                    kv(x, y, w, "Процессор", b.cpu ? b.cpu->def->name : "—", dc, tc);
                    y += 32;
                    if (b.cpu) {
                        kv(x, y, w, "Ядра / потоки", fmt("%d / %d", b.cpu->def->cores, b.cpu->def->threads), dc, tc);
                        y += 32;
                        kv(x, y, w, "Сокет", b.cpu->def->socket, dc, tc);
                        y += 32;
                    }
                    kv(x, y, w, "Память", ramLine(b, c.oc), dc, tc);
                    y += 32;
                    bool badGpu;
                    std::string vl = videoLine(b, badGpu);
                    kv(x, y, w, "Видеоадаптер", vl, dc, badGpu ? rgba(230, 60, 60) : tc, vl.size() > 40 ? 14 : 17);
                    y += 32;
                    for (Entity* d : b.drives) {
                        kv(x, y, w, "Накопитель", driveLine(d), dc, tc);
                        y += 32;
                    }
                    kv(x, y, w, "Блок питания", b.psu ? fmt("%d Вт", b.psu->def->watts) : "—", dc, tc);
                    break;
                }
                case 1: {
                    if (!canOC) {
                        gfx::textWrapped(fmt("Чипсет %s не поддерживает разгон процессора. Нужна плата с чипсетом "
                                             "B550/B650/X670E/Z790. Профиль памяти доступен.",
                                             b.mb ? b.mb->def->chipset.c_str() : "?"),
                                         x, y, w, 16, style == 0 ? rgba(150, 20, 20) : sk.accent);
                        y += 56;
                    }
                    Color rowC = canOC ? tc : withAlpha(dc, 140);
                    // Частота
                    float ghz = b.cpu ? b.cpu->def->ghz + e.cpuOC * 0.1f : 0;
                    gfx::text("Частота процессора", x, y, 17, rowC);
                    gfx::textRight(fmt("+%d МГц  →  %.1f ГГц", e.cpuOC * 100, ghz), x + w, y, 17, rowC, true);
                    float v = (float)e.cpuOC;
                    if (canOC && slider(Rect{x, y + 24, w, 24}, v, 0, 10, 1, &e.cpuOC)) {
                        e.cpuOC = (int)v;
                        c.biosDirty = true;
                    } else if (!canOC) {
                        bar(Rect{x, y + 32, w, 8}, 0, dc);
                    }
                    y += 64;
                    // Напряжение
                    Color vcol = e.vcore >= 1.45f ? rgba(255, 70, 70) : (e.vcore >= 1.38f ? rgba(255, 180, 50) : rowC);
                    gfx::text("Напряжение CPU (Vcore)", x, y, 17, rowC);
                    gfx::textRight(fmt("%.2f В%s", e.vcore, e.vcore >= 1.45f ? "  ОПАСНО!" : ""), x + w, y, 17,
                                   canOC ? vcol : rowC, true);
                    if (canOC && slider(Rect{x, y + 24, w, 24}, e.vcore, 1.10f, 1.50f, 0.01f, &e.vcore))
                        c.biosDirty = true;
                    else if (!canOC)
                        bar(Rect{x, y + 32, w, 8}, 0, dc);
                    y += 64;
                    // Качество кристалла
                    if (b.cpu) {
                        int stars = 1 + (int)(b.cpu->quality * 4.99f);
                        gfx::text("Качество кристалла", x, y, 17, rowC);
                        Color on = style == 0 ? rgba(150, 110, 0) : sk.accent;
                        for (int i = 0; i < 5; i++)
                            gfx::text(i < stars ? "★" : "☆", x + w - 5 * 22 + i * 22, y - 2, 20, i < stars ? on : dc, true);
                        y += 34;
                    }
                    // Профиль памяти
                    std::string prof = amd ? "EXPO" : "XMP";
                    std::string xl = fmt("Профиль памяти %s (%d МГц)", prof.c_str(),
                                         b.ram.empty() ? 0 : ramMhz(b, true));
                    bool xmp = e.xmp;
                    gfx::text(xl, x, y + 4, 17, tc);
                    if (button(Rect{x + w - 120, y, 120, 30}, xmp ? "Вкл" : "Выкл", 16,
                               xmp ? (style == 0 ? rgba(30, 130, 50) : sk.accent) : rgba(90, 90, 100))) {
                        e.xmp = !e.xmp;
                        c.biosDirty = true;
                    }
                    y += 48;
                    if (canOC && b.cpu) {
                        if (button(Rect{x, y, 220, 38}, "Авто-разгон", 16, style == 0 ? rgba(40, 70, 170) : sk.accent)) {
                            // Безопасный разгон под 1.30 В с учётом качества кристалла
                            int best = 0;
                            for (int k = 10; k >= 0; k--)
                                if (requiredVcore(b, k) <= 1.30f) {
                                    best = k;
                                    break;
                                }
                            e.cpuOC = best;
                            e.vcore = 1.30f;
                            c.biosDirty = true;
                        }
                        if (button(Rect{x + 236, y, 220, 38}, "Сброс разгона", 16, rgba(90, 90, 100))) {
                            e.cpuOC = 0;
                            e.vcore = 1.20f;
                            c.biosDirty = true;
                        }
                    }
                    y += 50;
                    gfx::textWrapped("Слишком высокая частота при низком напряжении вызовет сбои под нагрузкой. "
                                     "Напряжение от 1.45 В при перегреве может сжечь процессор!",
                                     x, y, w, 14, dc);
                    break;
                }
                case 2: {
                    kv(x, y, w, "Температура CPU", fmt("%.1f°C", c.cpuTemp), dc,
                       c.cpuTemp > 85 ? rgba(240, 60, 60) : tc);
                    y += 30;
                    kv(x, y, w, "Температура платы", fmt("%.1f°C", mbTemp(c)), dc, tc);
                    y += 30;
                    float jit = std::sin(time * 3.1f) * 0.004f;
                    kv(x, y, w, "Vcore", fmt("%.3f В", (canOC ? c.oc.vcore : 1.2f) + jit), dc, tc);
                    y += 30;
                    kv(x, y, w, "+12V / +5V / +3.3V", fmt("%.2f / %.2f / %.2f В", 12.05f + jit * 3, 5.02f + jit, 3.31f),
                       dc, tc);
                    y += 30;
                    if (b.cooler) {
                        kv(x, y, w, "CPU_FAN", fmt("%d об/мин", fanRpm(c, 0) + 200), dc, tc);
                        y += 30;
                    }
                    for (size_t i = 0; i < b.fans.size(); i++) {
                        kv(x, y, w, fmt("SYS_FAN%d", (int)i + 1), fmt("%d об/мин", fanRpm(c, (int)i + 1)), dc, tc);
                        y += 30;
                    }
                    y += 12;
                    gfx::text("Профиль вентиляторов", x, y, 18, tc, true);
                    y += 32;
                    for (int i = 0; i < 3; i++) {
                        bool sel = e.fanProfile == i;
                        Color bc = sel ? (style == 0 ? rgba(40, 70, 170) : sk.accent) : rgba(90, 90, 100);
                        if (button(Rect{x + i * 170.0f, y, 160, 38}, fanProfileName(i), 16, bc)) {
                            e.fanProfile = i;
                            c.biosDirty = true;
                        }
                    }
                    y += 50;
                    gfx::textWrapped("Тихий — меньше шума, но горячее. Турбо — лучшее охлаждение.", x, y, w, 14, dc);
                    break;
                }
                case 3: {
                    gfx::text("Приоритет загрузки", x, y, 19, tc, true);
                    y += 36;
                    std::vector<std::string> devs;
                    std::string usb = b.usb ? "USB: Флешка NovaOS" : "";
                    std::vector<std::string> drives;
                    for (Entity* d : b.drives) drives.push_back(driveLine(d));
                    if (e.bootUsbFirst && b.usb) devs.push_back(usb);
                    for (auto& d : drives) devs.push_back(d);
                    if (!e.bootUsbFirst && b.usb) devs.push_back(usb);
                    if (devs.empty()) gfx::text("Загрузочные устройства не найдены", x, y, 17, rgba(230, 60, 60));
                    for (size_t i = 0; i < devs.size(); i++) {
                        Rect r{x, y, w, 36};
                        gfx::roundRect(r, 6, style == 0 ? rgba(180, 180, 186) : sk.panel);
                        gfx::text(fmt("#%d   %s", (int)i + 1, devs[i].c_str()), r.x + 12, r.y + 8, 17, tc);
                        y += 42;
                    }
                    y += 10;
                    if (button(Rect{x, y, 340, 40}, e.bootUsbFirst ? "Первым: USB → сменить" : "Первым: накопитель → сменить",
                               16, style == 0 ? rgba(40, 70, 170) : sk.accent)) {
                        e.bootUsbFirst = !e.bootUsbFirst;
                        c.biosDirty = true;
                    }
                    y += 56;
                    gfx::textWrapped("Чтобы переустановить NovaOS, вставьте флешку и поставьте USB первым.", x, y, w, 14, dc);
                    break;
                }
                case 4: {
                    auto apply = [&]() {
                        c.oc.cpuOC = canOC ? e.cpuOC : 0;
                        c.oc.vcore = canOC ? e.vcore : 1.20f;
                        c.oc.xmp = e.xmp;
                        c.oc.fanProfile = e.fanProfile;
                        c.oc.bootUsbFirst = e.bootUsbFirst;
                    };
                    Color bc = style == 0 ? rgba(40, 70, 170) : sk.accent;
                    if (button(Rect{x, y, 420, 46}, "Сохранить и перезагрузить (F10)", 18, bc)) {
                        apply();
                        toast("Настройки BIOS сохранены", theme::good);
                        rebootPC(c, false);
                    }
                    y += 58;
                    if (button(Rect{x, y, 420, 46}, "Выйти без сохранения", 18, rgba(90, 90, 100))) rebootPC(c, false);
                    y += 58;
                    if (button(Rect{x, y, 420, 46}, "Загрузить настройки по умолчанию", 18, rgba(90, 90, 100))) {
                        OcSettings def;
                        e.cpuOC = def.cpuOC;
                        e.vcore = def.vcore;
                        e.xmp = def.xmp;
                        e.fanProfile = def.fanProfile;
                        e.bootUsbFirst = def.bootUsbFirst;
                        c.biosDirty = true;
                    }
                    y += 70;
                    if (c.biosDirty) gfx::text("Есть несохранённые изменения", x, y, 16, style == 0 ? rgba(150, 20, 20) : sk.accent, true);
                    break;
                }
            }
            if (ctx().active && plat::keyPressed(plat::K_F10)) {
                c.oc.cpuOC = canOC ? e.cpuOC : 0;
                c.oc.vcore = canOC ? e.vcore : 1.20f;
                c.oc.xmp = e.xmp;
                c.oc.fanProfile = e.fanProfile;
                c.oc.bootUsbFirst = e.bootUsbFirst;
                toast("Настройки BIOS сохранены", theme::good);
                rebootPC(c, false);
            }
            // Нижняя строка подсказок
            Rect foot{0, H - 36, W, 36};
            gfx::rect(foot, style == 0 ? rgba(200, 200, 204) : sk.panel);
            gfx::text("F10: сохранить и выйти   •   Мышь: выбор   •   Esc: отойти от компьютера", 20, H - 28, 15,
                      style == 0 ? rgba(22, 44, 140) : sk.dim);
            break;
        }

        // ----------------------------------------------------------- Нет загрузки
        case PCState::NoBoot: {
            gfx::rect(Rect{0, 0, W, H}, rgba(0, 0, 0));
            gfx::text("Ошибка загрузки", 40, 40, 26, white, true);
            gfx::textWrapped(c.msg, 40, 90, W - 80, 22, rgba(220, 220, 220));
            gfx::text("_", 40, 160 + (std::fmod(time, 1.0f) < 0.5f ? 0 : 1000), 24, white);
            if (button(Rect{40, H - 80, 300, 44}, "Перезагрузить", 18, rgba(60, 60, 70))) rebootPC(c, false);
            if (button(Rect{356, H - 80, 300, 44}, "Войти в BIOS", 18, rgba(60, 60, 70))) rebootPC(c, true);
            break;
        }

        // ----------------------------------------------------------- Установщик
        case PCState::Installer:
        case PCState::Installing: {
            gfx::rectGrad(Rect{0, 0, W, H}, rgba(20, 60, 140), rgba(60, 20, 110));
            Rect win{W / 2 - 340, 90, 680, 460};
            gfx::roundRect(win, 14, rgba(245, 247, 252));
            gfx::text("Установка NovaOS", win.x + 30, win.y + 24, 30, rgba(30, 40, 70), true);
            if (c.st == PCState::Installer) {
                gfx::text("Выберите накопитель для установки:", win.x + 30, win.y + 80, 18, rgba(80, 90, 110));
                float y = win.y + 120;
                if (b.drives.empty())
                    gfx::text("Накопители не найдены. Выключите ПК и установите SSD/HDD.", win.x + 30, y, 17,
                              rgba(200, 40, 40));
                for (Entity* d : b.drives) {
                    Rect r{win.x + 30, y, win.w - 60, 56};
                    gfx::roundRect(r, 10, rgba(225, 230, 240));
                    gfx::text(d->def->name, r.x + 16, r.y + 8, 18, rgba(30, 40, 70), true);
                    gfx::text(d->osInstalled ? "NovaOS уже установлена — будет переустановлена"
                                             : (d->def->stor == StorType::HDD ? "Жёсткий диск: установка будет долгой"
                                                                               : "Быстрая установка"),
                              r.x + 16, r.y + 32, 14, rgba(90, 100, 120));
                    if (button(Rect{r.x + r.w - 150, r.y + 10, 136, 36}, "Установить", 16)) {
                        c.installDrive = d->id;
                        c.installProgress = 0;
                        c.st = PCState::Installing;
                        c.t = 0;
                    }
                    y += 66;
                }
                if (b.bootDrive() && button(Rect{win.x + 30, win.y + win.h - 66, 300, 44}, "Загрузить установленную ОС",
                                            16, rgba(120, 130, 150))) {
                    c.oc.bootUsbFirst = false;
                    c.st = PCState::Booting;
                    c.t = 0;
                }
            } else {
                const char* stage = c.installProgress < 0.35f ? "Копирование файлов..."
                                    : c.installProgress < 0.75f ? "Установка компонентов и драйверов..."
                                                                : "Почти готово...";
                gfx::text(stage, win.x + 30, win.y + 120, 20, rgba(60, 70, 90));
                bar(Rect{win.x + 30, win.y + 170, win.w - 60, 22}, c.installProgress, theme::accent, rgba(210, 215, 230));
                gfx::text(fmt("%d%%", (int)(c.installProgress * 100)), win.x + 30, win.y + 205, 26, rgba(30, 40, 70), true);
                gfx::textWrapped("Не выключайте компьютер и не вынимайте флешку.", win.x + 30, win.y + 260, win.w - 60, 16,
                                 rgba(110, 120, 140));
            }
            break;
        }

        // ----------------------------------------------------------- Загрузка ОС
        case PCState::Booting: {
            gfx::rect(Rect{0, 0, W, H}, rgba(6, 10, 22));
            gfx::textCentered("NovaOS", Rect{0, H * 0.32f, W, 90}, 72, rgba(120, 190, 255), true);
            for (int i = 0; i < 8; i++) {
                float a = time * 4.0f - i * 0.35f;
                gfx::circle(Vec2{W / 2 + std::cos(a) * 26, H * 0.62f + std::sin(a) * 26}, 5.0f - i * 0.45f,
                            rgba(200, 220, 255, 255 - i * 25), 10);
            }
            break;
        }

        // ----------------------------------------------------------- Сбой
        case PCState::Crash: {
            gfx::rect(Rect{0, 0, W, H}, rgba(120, 20, 28));
            gfx::text("NovaOS остановлена", 70, 110, 44, white, true);
            gfx::textWrapped("Произошла критическая ошибка, и компьютер был остановлен, чтобы защитить ваши данные.", 70,
                             180, W - 140, 22, rgba(255, 220, 220));
            gfx::text("Код ошибки: " + c.crashReason, 70, 280, 20, white, true);
            gfx::textWrapped("Совет: уменьшите разгон или поднимите напряжение (BIOS — разгон процессора, "
                             "OC Tuner — разгон видеокарты).",
                             70, 320, W - 140, 18, rgba(255, 200, 200));
            if (button(Rect{70, H - 120, 280, 48}, "Перезагрузить", 18, rgba(200, 60, 70))) rebootPC(c, false);
            if (button(Rect{366, H - 120, 280, 48}, "Войти в BIOS", 18, rgba(200, 60, 70))) rebootPC(c, true);
            break;
        }

        // ----------------------------------------------------------- Рабочий стол
        case PCState::Desktop: {
            gfx::rectGrad(Rect{0, 0, W, H}, rgba(24, 40, 96), rgba(86, 34, 110));
            gfx::circle(Vec2{W * 0.78f, H * 0.3f}, 220, rgba(255, 255, 255, 14), 48);
            gfx::circle(Vec2{W * 0.86f, H * 0.62f}, 140, rgba(255, 255, 255, 10), 40);
            gfx::text("NovaOS", W - 260, H - 150, 54, rgba(255, 255, 255, 40), true);

            struct AppInfo {
                const char* name;
                const char* icon;
                Color color;
            };
            const AppInfo apps[APP_COUNT] = {
                {"BenchMax", "▶", rgba(240, 120, 40)},  {"Сенсоры", "°C", rgba(60, 180, 110)},
                {"OC Tuner", "⚡", rgba(220, 60, 70)},   {"NovaMiner", "$", rgba(230, 180, 40)},
                {"RGB Studio", "●", rgba(170, 80, 220)}, {"Магазин", "★", rgba(50, 130, 240)},
                {"Змейка", "S", rgba(110, 200, 60)},     {"О системе", "i", rgba(120, 130, 150)},
            };
            for (int i = 0; i < APP_COUNT; i++) {
                Rect r{20 + (i / 4) * 96.0f, 20 + (i % 4) * 104.0f, 88, 96};
                if (hover(r)) gfx::roundRect(r, 10, rgba(255, 255, 255, 30));
                Rect ic{r.x + 16, r.y + 8, 56, 56};
                gfx::roundRect(ic, 14, apps[i].color);
                gfx::textCentered(apps[i].icon, ic, 28, white, true);
                gfx::textCentered(apps[i].name, Rect{r.x - 6, r.y + 66, r.w + 12, 24}, 14, white);
                if (click(r)) {
                    c.app = i;
                    c.startMenu = false;
                    if (i == APP_SNAKE) c.snake.reset();
                    if (i == APP_OC) c.ocEdit = c.oc;
                }
            }

            // Окно приложения
            if (c.app >= 0) {
                Rect win{220, 24, W - 244, H - 88};
                gfx::roundRect(Rect{win.x + 4, win.y + 6, win.w, win.h}, 12, rgba(0, 0, 0, 70));
                gfx::roundRect(win, 12, rgba(30, 34, 44, 250));
                gfx::roundRect(Rect{win.x, win.y, win.w, 40}, 12, apps[c.app].color);
                gfx::rect(Rect{win.x, win.y + 28, win.w, 12}, apps[c.app].color);
                gfx::text(apps[c.app].name, win.x + 16, win.y + 8, 19, white, true);
                Rect close{win.x + win.w - 46, win.y + 4, 40, 32};
                if (button(close, "×", 24, rgba(0, 0, 0, 60))) {
                    if (c.app == APP_BENCH) c.benchRunning = false;
                    c.app = APP_NONE;
                }
                Rect cr{win.x + 20, win.y + 56, win.w - 40, win.h - 72};
                if (c.app >= 0) {
                    Perf perf = estimatePerf(b, c.oc);
                    switch (c.app) {
                        case APP_BENCH: {
                            if (c.benchRunning) {
                                Rect vp{cr.x, cr.y, cr.w, cr.h - 70};
                                gfx::rect(vp, rgba(5, 8, 16));
                                for (int i = 0; i < 26; i++) {
                                    float a = time * (0.6f + i * 0.07f) + i;
                                    float px = vp.x + vp.w * 0.5f + std::cos(a) * (60 + i * 11);
                                    float py = vp.y + vp.h * 0.5f + std::sin(a * 1.3f) * (30 + i * 6);
                                    Color cc = hsv(i * 14.0f + time * 40, 0.7f, 1.0f);
                                    float s = 10 + (i % 5) * 6;
                                    gfx::triangle(Vec2{px, py - s}, Vec2{px - s, py + s}, Vec2{px + s, py + s}, cc);
                                }
                                gfx::text(fmt("%.0f FPS", c.benchFps), vp.x + 16, vp.y + 12, 30, white, true);
                                gfx::text(fmt("CPU %.0f°C  GPU %.0f°C", c.cpuTemp, c.gpuTemp), vp.x + 16, vp.y + 50, 16,
                                          rgba(200, 210, 230));
                                bar(Rect{cr.x, cr.y + cr.h - 50, cr.w, 16}, c.benchT / 12.0f, rgba(240, 120, 40));
                                gfx::text("Тест выполняется...", cr.x, cr.y + cr.h - 28, 16, theme::dim);
                            } else {
                                gfx::text("BenchMax — тест производительности", cr.x, cr.y, 22, white, true);
                                gfx::textWrapped("Нагружает процессор и видеокарту на 12 секунд и выдаёт очки. "
                                                 "Отличный способ проверить разгон на стабильность.",
                                                 cr.x, cr.y + 34, cr.w * 0.55f, 15, theme::dim);
                                if (button(Rect{cr.x, cr.y + 100, 220, 50}, "Запустить тест", 19, rgba(240, 120, 40),
                                           white, !c.mining)) {
                                    c.benchRunning = true;
                                    c.benchT = 0;
                                    c.benchFps = 0;
                                }
                                if (c.mining) gfx::text("Сначала остановите майнинг", cr.x, cr.y + 158, 14, theme::warn);
                                gfx::text(fmt("Последний результат: %d", c.lastScore), cr.x, cr.y + 190, 19, white, true);
                                gfx::text(fmt("Ваш рекорд: %d", bestScore), cr.x, cr.y + 218, 17, theme::dim);
                                // Таблица лидеров
                                struct Row {
                                    const char* who;
                                    int score;
                                };
                                std::vector<Row> rows = {{"ProGamer — RTX 4090 + R7 9800X3D", 28400},
                                                         {"Макс — RTX 4080 + R7 7800X3D", 21500},
                                                         {"Лиза — RTX 4070 + R5 7600", 13300},
                                                         {"Дима — RTX 4060 + i5-12400F", 8900},
                                                         {"Аня — RTX 4050 + R5 5600", 6700},
                                                         {"Офисный ПК — встройка", 1400}};
                                if (bestScore > 0) rows.push_back({"ВЫ", bestScore});
                                std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b2) { return a.score > b2.score; });
                                float lx = cr.x + cr.w * 0.58f, ly = cr.y;
                                gfx::text("Таблица лидеров", lx, ly, 18, white, true);
                                ly += 30;
                                for (size_t i = 0; i < rows.size(); i++) {
                                    bool me = std::string(rows[i].who) == "ВЫ";
                                    Rect rr{lx, ly, cr.x + cr.w - lx, 30};
                                    gfx::roundRect(rr, 6, me ? rgba(240, 120, 40, 120) : rgba(255, 255, 255, 12));
                                    gfx::text(fmt("%d. %s", (int)i + 1, rows[i].who), rr.x + 8, rr.y + 6, 13, white);
                                    gfx::textRight(fmt("%d", rows[i].score), rr.x + rr.w - 8, rr.y + 5, 15, white, true);
                                    ly += 34;
                                }
                                gfx::text(fmt("Оценка этой системы: ~%d", perf.score), cr.x, cr.y + cr.h - 30, 16,
                                          theme::dim);
                            }
                            break;
                        }
                        case APP_SENSORS: {
                            auto gauge = [&](float gx, const char* name, float temp, float load) {
                                Rect g{gx, cr.y, 200, 120};
                                gfx::roundRect(g, 10, rgba(255, 255, 255, 14));
                                Color tc2 = temp > 90 ? theme::bad : (temp > 75 ? theme::warn : theme::good);
                                gfx::text(name, g.x + 14, g.y + 10, 16, theme::dim);
                                gfx::text(fmt("%.0f°C", temp), g.x + 14, g.y + 34, 40, tc2, true);
                                bar(Rect{g.x + 14, g.y + 92, g.w - 28, 10}, load, theme::accent);
                            };
                            gauge(cr.x, "Процессор", c.cpuTemp, c.cpuLoad);
                            gauge(cr.x + 216, b.gpuWorks() ? "Видеокарта" : "Видео (встройка)", c.gpuTemp, c.gpuLoad);
                            Rect pw{cr.x + 432, cr.y, cr.w - 432, 120};
                            gfx::roundRect(pw, 10, rgba(255, 255, 255, 14));
                            gfx::text("Потребление", pw.x + 14, pw.y + 10, 16, theme::dim);
                            int watts = b.psu ? b.psu->def->watts : 1;
                            gfx::text(fmt("%.0f / %d Вт", c.power, watts), pw.x + 14, pw.y + 34, 30,
                                      c.power > watts * 0.9f ? theme::bad : white, true);
                            bar(Rect{pw.x + 14, pw.y + 92, pw.w - 28, 10}, c.power / watts,
                                c.power > watts * 0.9f ? theme::bad : theme::good);
                            // График температур
                            Rect gr{cr.x, cr.y + 136, cr.w, 200};
                            gfx::roundRect(gr, 10, rgba(0, 0, 0, 80));
                            for (int i = 1; i < 4; i++)
                                gfx::rect(Rect{gr.x, gr.y + gr.h * i / 4, gr.w, 1}, rgba(255, 255, 255, 20));
                            auto plot = [&](const std::deque<float>& h, Color col) {
                                for (size_t i = 1; i < h.size(); i++) {
                                    float x0 = gr.x + gr.w * (i - 1) / 119.0f, x1 = gr.x + gr.w * i / 119.0f;
                                    float y0 = gr.y + gr.h * (1 - clampf((h[i - 1] - 20) / 90, 0, 1));
                                    float y1 = gr.y + gr.h * (1 - clampf((h[i] - 20) / 90, 0, 1));
                                    gfx::line(Vec2{x0, y0}, Vec2{x1, y1}, 2.5f, col);
                                }
                            };
                            plot(c.cpuHist, rgba(255, 120, 80));
                            plot(c.gpuHist, rgba(80, 200, 255));
                            gfx::text("CPU", gr.x + 10, gr.y + 8, 14, rgba(255, 120, 80), true);
                            gfx::text("GPU", gr.x + 54, gr.y + 8, 14, rgba(80, 200, 255), true);
                            float y = gr.y + gr.h + 16;
                            gfx::text(fmt("Вентиляторы корпуса: %d  •  профиль: %s  •  термопаста: %s", (int)b.fans.size(),
                                          fanProfileName(c.oc.fanProfile), b.cpu && b.cpu->pasted ? "есть" : "НЕТ"),
                                      cr.x, y, 15, white);
                            gfx::text(fmt("Кулер: %s", b.cooler ? b.cooler->def->name.c_str() : "НЕТ"), cr.x, y + 24,
                                      15, b.cooler ? white : theme::bad);
                            break;
                        }
                        case APP_OC: {
                            OcSettings& e = c.ocEdit;
                            gfx::text("Разгон видеокарты", cr.x, cr.y, 22, white, true);
                            float y = cr.y + 40;
                            if (!b.gpuWorks()) {
                                gfx::textWrapped(b.gpu ? "Видеокарта NOT SUPPORTED — драйвер не загружен, разгон недоступен."
                                                       : "Дискретная видеокарта не установлена.",
                                                 cr.x, y, cr.w, 17, theme::bad);
                                y += 50;
                            } else {
                                float v = (float)e.gpuCore;
                                gfx::text("Частота ядра", cr.x, y, 17, white);
                                gfx::textRight(fmt("+%d МГц", e.gpuCore), cr.x + cr.w, y, 17, white, true);
                                if (slider(Rect{cr.x, y + 24, cr.w, 24}, v, 0, 300, 10, &e.gpuCore)) e.gpuCore = (int)v;
                                y += 64;
                                v = (float)e.gpuMem;
                                gfx::text("Частота памяти", cr.x, y, 17, white);
                                gfx::textRight(fmt("+%d МГц", e.gpuMem), cr.x + cr.w, y, 17, white, true);
                                if (slider(Rect{cr.x, y + 24, cr.w, 24}, v, 0, 1200, 50, &e.gpuMem)) e.gpuMem = (int)v;
                                y += 70;
                                OcSettings preview = c.oc;
                                preview.gpuCore = e.gpuCore;
                                preview.gpuMem = e.gpuMem;
                                Perf pp = estimatePerf(b, preview);
                                float load = estimatePower(b, preview, 1, 1);
                                gfx::text(fmt("Оценка BenchMax: %d  (сейчас %d)", pp.score, perf.score), cr.x, y, 17,
                                          white);
                                gfx::text(fmt("Потребление под нагрузкой: %.0f Вт из %d Вт", load,
                                              b.psu ? b.psu->def->watts : 0),
                                          cr.x, y + 26, 17, b.psu && load > b.psu->def->watts ? theme::bad : white);
                                y += 66;
                                if (button(Rect{cr.x, y, 180, 44}, "Применить", 18, rgba(220, 60, 70))) {
                                    c.oc.gpuCore = e.gpuCore;
                                    c.oc.gpuMem = e.gpuMem;
                                    toast("Разгон видеокарты применён. Проверьте стабильность в BenchMax.");
                                }
                                if (button(Rect{cr.x + 196, y, 180, 44}, "Сброс", 18, rgba(90, 90, 100))) {
                                    e.gpuCore = e.gpuMem = 0;
                                    c.oc.gpuCore = c.oc.gpuMem = 0;
                                }
                                y += 64;
                            }
                            gfx::rect(Rect{cr.x, y, cr.w, 1}, rgba(255, 255, 255, 40));
                            y += 14;
                            gfx::text(fmt("Процессор: +%d МГц, %.2f В  •  память: %s", c.oc.cpuOC * 100, c.oc.vcore,
                                          c.oc.xmp ? "XMP/EXPO" : "стандарт"),
                                      cr.x, y, 16, theme::dim);
                            gfx::text("Разгон процессора и профиль памяти настраиваются в BIOS.", cr.x, y + 24, 16,
                                      theme::dim);
                            if (button(Rect{cr.x, y + 56, 280, 42}, "Перезагрузить в BIOS", 17, rgba(90, 90, 100)))
                                rebootPC(c, true);
                            break;
                        }
                        case APP_MINER: {
                            gfx::text("NovaMiner", cr.x, cr.y, 22, white, true);
                            if (!b.gpuWorks()) {
                                gfx::textWrapped("Для майнинга нужна поддерживаемая видеокарта RTX 40-й серии. "
                                                 "Старые карты (NOT SUPPORTED) и встроенная графика не подходят.",
                                                 cr.x, cr.y + 40, cr.w, 17, theme::bad);
                                break;
                            }
                            float rate = perf.gpuEff * 0.004f * 3600;
                            gfx::text(fmt("Хешрейт: %.1f MH/s", perf.gpuEff), cr.x, cr.y + 44, 20, white);
                            gfx::text(fmt("Доход: ~%s в час", ::money(rate).c_str()), cr.x, cr.y + 74, 20,
                                      rgba(120, 230, 140));
                            gfx::text(fmt("Заработано за сессию: %s", ::money(c.minedSession).c_str()), cr.x,
                                      cr.y + 104, 18, white);
                            gfx::text(fmt("Видеокарта: %.0f°C", c.gpuTemp), cr.x, cr.y + 134, 18,
                                      c.gpuTemp > 85 ? theme::bad : white);
                            if (button(Rect{cr.x, cr.y + 180, 240, 52}, c.mining ? "Остановить" : "Начать майнинг", 19,
                                       c.mining ? rgba(200, 70, 70) : rgba(230, 180, 40), white, !c.benchRunning))
                                c.mining = !c.mining;
                            if (c.mining) {
                                for (int i = 0; i < 12; i++) {
                                    float ph = std::fmod(time * 0.8f + i * 0.37f, 1.0f);
                                    Rect blk{cr.x + 300 + (i % 6) * 70.0f, cr.y + 180 + (i / 6) * 70.0f, 56, 56};
                                    gfx::roundRect(blk, 8, rgba(230, 180, 40, (int)(60 + 180 * ph)));
                                }
                            }
                            gfx::textWrapped("Майнинг сильно греет видеокарту и потребляет энергию. Следите за "
                                             "температурой и мощностью блока питания.",
                                             cr.x, cr.y + cr.h - 50, cr.w, 14, theme::dim);
                            break;
                        }
                        case APP_RGB: {
                            gfx::text("RGB Studio", cr.x, cr.y, 22, white, true);
                            const Color pal[] = {rgba(255, 40, 40),  rgba(255, 140, 0),  rgba(255, 230, 0),
                                                 rgba(40, 230, 60),  rgba(0, 220, 220),  rgba(0, 200, 255),
                                                 rgba(40, 80, 255),  rgba(170, 60, 255), rgba(255, 60, 200),
                                                 rgba(255, 255, 255)};
                            for (int i = 0; i < 10; i++) {
                                Rect sw{cr.x + i * 62.0f, cr.y + 44, 54, 54};
                                bool sel = c.oc.rgb.r == pal[i].r && c.oc.rgb.g == pal[i].g && c.oc.rgb.b == pal[i].b;
                                if (sel) gfx::roundRect(Rect{sw.x - 3, sw.y - 3, sw.w + 6, sw.h + 6}, 10, white);
                                gfx::roundRect(sw, 8, pal[i]);
                                if (click(sw)) {
                                    c.oc.rgb = pal[i];
                                    if (c.oc.rgbMode == 1 || c.oc.rgbMode == 3) c.oc.rgbMode = 0;
                                }
                            }
                            const char* modes[] = {"Статичный", "Радуга", "Дыхание", "Выключить"};
                            for (int i = 0; i < 4; i++)
                                if (button(Rect{cr.x + i * 160.0f, cr.y + 120, 150, 44}, modes[i], 16,
                                           c.oc.rgbMode == i ? rgba(170, 80, 220) : rgba(80, 80, 92)))
                                    c.oc.rgbMode = i;
                            Rect pv{cr.x, cr.y + 190, cr.w, 80};
                            gfx::roundRect(pv, 10, rgba(10, 10, 14));
                            for (int i = 0; i < 24; i++) {
                                Color cc = c.oc.rgbMode == 1 ? hsv(time * 70 + i * 15, 0.85f, 1)
                                           : c.oc.rgbMode == 3 ? rgba(40, 40, 44)
                                                               : c.oc.rgb;
                                if (c.oc.rgbMode == 2) {
                                    float k = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * 2.5f));
                                    cc = rgba((int)(cc.r * k), (int)(cc.g * k), (int)(cc.b * k));
                                }
                                gfx::circle(Vec2{pv.x + 24 + i * (pv.w - 48) / 23, pv.y + 40}, 11, cc, 14);
                            }
                            gfx::textWrapped("Светятся: RGB-вентиляторы, корпус Glass Tower, память DDR5, "
                                             "СЖО и платы Raven/Polaris старших серий.",
                                             cr.x, cr.y + 290, cr.w, 15, theme::dim);
                            break;
                        }
                        case APP_SHOP:
                            drawShop(cr, 18, c.shopCat, c.shopScroll);
                            break;
                        case APP_SNAKE: {
                            Snake& s = c.snake;
                            float cell = std::min(cr.w / 28, (cr.h - 50) / 16);
                            Rect board{cr.x + (cr.w - cell * 28) / 2, cr.y + 40, cell * 28, cell * 16};
                            gfx::text(fmt("Счёт: %d   (+$1 за яблоко)", s.score), cr.x, cr.y + 4, 18, white, true);
                            gfx::textRight("Стрелки/WASD", cr.x + cr.w, cr.y + 6, 15, theme::dim);
                            gfx::rect(board, rgba(16, 30, 18));
                            gfx::roundRect(Rect{board.x + s.fx * cell + 2, board.y + s.fy * cell + 2, cell - 4, cell - 4},
                                           cell * 0.4f, rgba(240, 60, 60));
                            for (size_t i = 0; i < s.body.size(); i++)
                                gfx::roundRect(Rect{board.x + s.body[i].first * cell + 1, board.y + s.body[i].second * cell + 1,
                                                    cell - 2, cell - 2},
                                               4, i == 0 ? rgba(170, 240, 90) : rgba(110, 200, 60));
                            if (s.dead) {
                                gfx::rect(board, rgba(0, 0, 0, 150));
                                gfx::textCentered("Игра окончена", Rect{board.x, board.y + board.h / 2 - 60, board.w, 50},
                                                  32, white, true);
                                if (button(Rect{board.x + board.w / 2 - 110, board.y + board.h / 2, 220, 46}, "Ещё раз", 18,
                                           rgba(110, 200, 60)))
                                    s.reset();
                            }
                            break;
                        }
                        case APP_INFO: {
                            gfx::text("NovaOS 2.0 Home", cr.x, cr.y, 24, white, true);
                            float y = cr.y + 44;
                            auto line = [&](const std::string& k, const std::string& v, Color vc = rgba(255, 255, 255)) {
                                gfx::text(k, cr.x, y, 16, theme::dim);
                                gfx::text(v, cr.x + 190, y, 16, vc, true);
                                y += 28;
                            };
                            line("Материнская плата", b.mb ? b.mb->def->name : "—");
                            line("Процессор", cpuLine(b, c.oc));
                            line("Память", ramLine(b, c.oc));
                            bool badGpu;
                            std::string vl = videoLine(b, badGpu);
                            line("Видео", vl, badGpu ? theme::bad : rgba(255, 255, 255));
                            for (Entity* d : b.drives) line("Накопитель", driveLine(d));
                            line("Блок питания", b.psu ? b.psu->def->name : "—");
                            line("Время работы", fmt("%d мин %02d с", (int)c.t / 60, (int)c.t % 60));
                            if (button(Rect{cr.x, cr.y + cr.h - 50, 260, 44}, "Перезагрузить в BIOS", 17, rgba(90, 90, 100)))
                                rebootPC(c, true);
                            break;
                        }
                    }
                }
            }

            // Панель задач
            Rect bar2{0, H - 44, W, 44};
            gfx::rect(bar2, rgba(12, 14, 22, 235));
            Rect start{8, H - 38, 44, 32};
            if (button(start, "N", 20, c.startMenu ? theme::accent : rgba(60, 70, 100))) c.startMenu = !c.startMenu;
            if (c.app >= 0) {
                const char* names[] = {"BenchMax", "Сенсоры", "OC Tuner", "NovaMiner", "RGB Studio", "Магазин", "Змейка", "О системе"};
                gfx::roundRect(Rect{62, H - 38, 150, 32}, 6, rgba(255, 255, 255, 30));
                gfx::textCentered(names[c.app], Rect{62, H - 38, 150, 32}, 15, white);
            }
            if (c.mining) gfx::text("⚡ Майнинг", 230, H - 32, 15, rgba(230, 180, 40), true);
            gfx::textRight(fmt("CPU %.0f°C   %s   %02d:%02d", c.cpuTemp, ::money(money).c_str(),
                               ((int)time / 60 + 12) % 24, (int)time % 60),
                           W - 14, H - 32, 16, white);
            if (c.startMenu) {
                Rect m{8, H - 44 - 170, 260, 164};
                gfx::roundRect(m, 10, rgba(22, 26, 38, 245));
                gfx::text("NovaOS", m.x + 14, m.y + 10, 18, white, true);
                if (button(Rect{m.x + 10, m.y + 40, m.w - 20, 34}, "Перезагрузить", 16, rgba(60, 70, 100))) {
                    c.startMenu = false;
                    rebootPC(c, false);
                } else if (button(Rect{m.x + 10, m.y + 80, m.w - 20, 34}, "Перезагрузить в BIOS", 16, rgba(60, 70, 100))) {
                    c.startMenu = false;
                    rebootPC(c, true);
                } else if (button(Rect{m.x + 10, m.y + 120, m.w - 20, 34}, "Выключить", 16, rgba(200, 60, 70))) {
                    c.startMenu = false;
                    shutdownPC(c);
                }
            }
            break;
        }
        default:
            gfx::rect(Rect{0, 0, W, H}, rgba(0, 0, 0));
            break;
    }
}
