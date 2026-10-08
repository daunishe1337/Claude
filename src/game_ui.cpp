// Интерфейсы: HUD, телефон (магазин, заказы, продажа, помощь), меню, пауза, настройки
#include "game.hpp"
#include "platform.hpp"
#include "render.hpp"
#include <algorithm>

using namespace ui;

namespace {
const Cat kShopCats[] = {Cat::Case, Cat::Motherboard, Cat::CPU, Cat::Cooler, Cat::RAM, Cat::GPU,
                         Cat::PSU,  Cat::Storage,     Cat::Fan, Cat::Paste,  Cat::USB};
constexpr int kShopCatCount = sizeof(kShopCats) / sizeof(kShopCats[0]);

const char* shortCat(Cat c) {
    switch (c) {
        case Cat::Case: return "Корпуса";
        case Cat::Motherboard: return "Платы";
        case Cat::CPU: return "Процессоры";
        case Cat::Cooler: return "Кулеры";
        case Cat::RAM: return "ОЗУ";
        case Cat::GPU: return "Видеокарты";
        case Cat::PSU: return "БП";
        case Cat::Storage: return "Накопители";
        case Cat::Fan: return "Вентиляторы";
        case Cat::Paste: return "Термопаста";
        case Cat::USB: return "Флешки";
        default: return "?";
    }
}

// Список с прокруткой по элементам: возвращает индекс первого видимого
int listScroll(Rect area, int& scroll, int total, int visible) {
    int maxScroll = std::max(0, total - visible);
    if (hover(area) && ctx().wheel != 0) scroll -= (int)ctx().wheel;
    scroll = std::max(0, std::min(scroll, maxScroll));
    return scroll;
}

void pager(Rect r, int& scroll, int total, int visible, float fs) {
    if (total <= visible) return;
    int maxScroll = total - visible;
    Rect up{r.x, r.y, r.h * 1.6f, r.h}, dn{r.x + r.w - r.h * 1.6f, r.y, r.h * 1.6f, r.h};
    if (button(up, "▲", fs, theme::panel2, theme::text, scroll > 0)) scroll = std::max(0, scroll - visible);
    if (button(dn, "▼", fs, theme::panel2, theme::text, scroll < maxScroll)) scroll = std::min(maxScroll, scroll + visible);
    gfx::textCentered(fmt("%d–%d из %d", scroll + 1, std::min(total, scroll + visible), total), r, fs * 0.85f,
                      theme::dim);
}
}  // namespace

// ---------------------------------------------------------------------------
// Магазин (общий для телефона и NovaOS)

void Game::drawShop(Rect a, float fs, int& cat, int& scroll) {
    cat = std::max(0, std::min(cat, kShopCatCount - 1));
    // Категории «чипами» с переносом строк
    float x = a.x, y = a.y, chipH = fs * 1.7f;
    for (int i = 0; i < kShopCatCount; i++) {
        std::string name = shortCat(kShopCats[i]);
        float w = gfx::textWidth(name, fs * 0.82f, true) + fs * 1.2f;
        if (x + w > a.x + a.w) {
            x = a.x;
            y += chipH + 6;
        }
        Rect r{x, y, w, chipH};
        bool sel = i == cat;
        if (button(r, name, fs * 0.82f, sel ? theme::accent : theme::panel2, theme::text)) {
            cat = i;
            scroll = 0;
        }
        x += w + 6;
    }
    y += chipH + 10;

    std::vector<const PartDef*> items;
    for (auto& d : catalog())
        if (d.cat == kShopCats[cat]) items.push_back(&d);
    float cardH = fs * 4.6f, pagerH = fs * 1.8f;
    Rect list{a.x, y, a.w, a.y + a.h - y - pagerH - 8};
    int visible = std::max(1, (int)(list.h / (cardH + 8)));
    int first = listScroll(list, scroll, (int)items.size(), visible);
    for (int i = first; i < (int)items.size() && i < first + visible; i++) {
        const PartDef* d = items[i];
        Rect c{list.x, list.y + (i - first) * (cardH + 8), list.w, cardH};
        gfx::roundRect(c, 10, theme::panel2);
        float bw = fs * 5.2f;
        float tx = c.x + 12, tw = c.w - bw - 32;
        gfx::text(d->name, tx, c.y + 8, fs, theme::text, true);
        float sy = c.y + 10 + fs * 1.35f;
        if (d->cat == Cat::GPU && !d->supported) {
            Rect badge{tx, sy, gfx::textWidth("NOT SUPPORTED", fs * 0.75f, true) + 14, fs * 1.2f};
            gfx::roundRect(badge, 5, rgba(200, 40, 40));
            gfx::textCentered("NOT SUPPORTED", badge, fs * 0.75f, rgba(255, 255, 255), true);
            sy += fs * 1.45f;
            gfx::textWrapped("Устаревшая карта: игра её не поддерживает", tx, sy, tw, fs * 0.72f, rgba(255, 140, 140));
        } else {
            gfx::textWrapped(partSpecs(*d), tx, sy, tw, fs * 0.75f, theme::dim);
        }
        gfx::textRight(::money(d->price), c.x + c.w - 14, c.y + 8, fs * 1.05f, rgba(120, 230, 140), true);
        Rect b{c.x + c.w - bw - 12, c.y + c.h - fs * 1.9f - 8, bw, fs * 1.9f};
        if (button(b, "Купить", fs * 0.9f, money >= d->price ? theme::accent : rgba(90, 90, 100))) buy(d);
    }
    pager(Rect{a.x, a.y + a.h - pagerH, a.w, pagerH}, scroll, (int)items.size(), visible, fs * 0.9f);
}

// ---------------------------------------------------------------------------
// Телефон

void Game::drawPhone(Rect r) {
    gfx::roundRect(Rect{r.x - 6, r.y - 6, r.w + 12, r.h + 12}, 34, rgba(10, 10, 12));
    gfx::roundRect(r, 30, rgba(16, 18, 24));
    Rect s{r.x + 10, r.y + 34, r.w - 20, r.h - 44};
    gfx::roundRect(Rect{r.x + r.w / 2 - 50, r.y + 10, 100, 16}, 8, rgba(5, 5, 6));
    gfx::text("PC Shop", r.x + 26, r.y + 8, 15, theme::dim, true);
    gfx::textRight(fmt("%02d:%02d", ((int)time / 60 + 12) % 24, (int)time % 60), r.x + r.w - 26, r.y + 8, 15,
                   theme::dim, true);

    // Шапка с деньгами
    gfx::roundRect(Rect{s.x, s.y, s.w, 62}, 14, rgba(36, 70, 140));
    gfx::text("Баланс", s.x + 14, s.y + 6, 14, rgba(200, 215, 255));
    gfx::text(::money(money), s.x + 14, s.y + 24, 28, theme::text, true);
    gfx::textRight(fmt("Заказов: %d", ordersDone), s.x + s.w - 14, s.y + 10, 14, rgba(200, 215, 255));
    gfx::textRight(fmt("Рекорд: %d", bestScore), s.x + s.w - 14, s.y + 32, 14, rgba(200, 215, 255));

    const char* tabs[] = {"Магазин", "Заказы", "Продажа", "Помощь"};
    float tabH = 44;
    Rect content{s.x, s.y + 74, s.w, s.h - 74 - tabH - 10};
    for (int i = 0; i < 4; i++) {
        Rect t{s.x + i * s.w / 4 + 2, s.y + s.h - tabH, s.w / 4 - 4, tabH};
        if (button(t, tabs[i], 14, i == phoneTab ? theme::accent : theme::panel2)) {
            phoneTab = i;
            phoneScroll = 0;
        }
    }

    const float fs = 16;
    if (phoneTab == 0) {
        drawShop(content, fs, phoneCat, phoneScroll);
        if (!deliveries.empty())
            gfx::text(fmt("В пути: %d", (int)deliveries.size()), content.x + 4, content.y + content.h - 18, 13,
                      theme::warn);
    } else if (phoneTab == 1) {
        float y = content.y;
        y += gfx::textWrapped("Клиенты ждут сборки. Поставьте готовый ПК на деревянный поддон у двери и нажмите «Сдать».",
                              content.x, y, content.w, 13, theme::dim) + 6;
        for (int i = 0; i < (int)orders.size(); i++) {
            const OrderTemplate& t = orderTemplates()[orders[i].tmpl];
            float descH = gfx::textWrapped(t.desc, 0, 0, content.w - 24, 13, theme::dim, false);
            Rect c{content.x, y, content.w, 92 + descH};
            if (c.y + c.h > content.y + content.h) break;
            gfx::roundRect(c, 12, theme::panel2);
            gfx::text(t.title, c.x + 12, c.y + 8, 17, theme::text, true);
            gfx::text(t.client, c.x + 12, c.y + 30, 13, theme::accent);
            gfx::textWrapped(t.desc, c.x + 12, c.y + 48, content.w - 24, 13, theme::dim);
            gfx::text("Награда: " + ::money(orders[i].reward), c.x + 12, c.y + c.h - 30, 16, rgba(120, 230, 140), true);
            Rect b{c.x + c.w - 112, c.y + c.h - 36, 100, 28};
            if (button(b, "Сдать", 14, theme::good)) completeOrder(i);
            y += c.h + 8;
        }
    } else if (phoneTab == 2) {
        auto items = shipItems();
        gfx::textWrapped("Положите ненужное на деревянный поддон у двери. Новые детали в коробке — 85% цены, "
                         "б/у — 60%, сгоревшие — почти даром.",
                         content.x, content.y, content.w, 13, theme::dim);
        float y = content.y + 56, rowH = 40;
        Rect list{content.x, y, content.w, content.h - 56 - 110};
        int visible = std::max(1, (int)(list.h / rowH));
        int first = listScroll(list, phoneScroll, (int)items.size(), visible);
        double total = 0;
        for (Entity* e : items) total += sellValue(e);
        if (items.empty()) gfx::textCentered("Поддон пуст", list, 16, theme::dim);
        for (int i = first; i < (int)items.size() && i < first + visible; i++) {
            Entity* e = items[i];
            Rect row{list.x, list.y + (i - first) * rowH, list.w, rowH - 4};
            gfx::roundRect(row, 8, theme::panel2);
            std::string name = e->def->name;
            std::vector<Entity*> inside;
            world.descendants(e->id, inside);
            if (!inside.empty()) name += fmt(" (+%d)", (int)inside.size());
            if (e->inBox) name += " [коробка]";
            if (e->broken) name += " [сгорел]";
            gfx::text(name, row.x + 10, row.y + 9, 14, theme::text);
            gfx::textRight(::money(sellValue(e)), row.x + row.w - 10, row.y + 9, 15, rgba(120, 230, 140), true);
        }
        pager(Rect{content.x, list.y + list.h + 4, content.w, 28}, phoneScroll, (int)items.size(), visible, 14);
        gfx::text("Итого:", content.x, content.y + content.h - 80, 16, theme::text);
        gfx::textRight(::money(total), content.x + content.w, content.y + content.h - 80, 20, rgba(120, 230, 140), true);
        if (button(Rect{content.x, content.y + content.h - 46, content.w, 42}, "Продать всё", 17, theme::good,
                   theme::text, !items.empty()))
            sellAll();
    } else {
        const char* help =
            "УПРАВЛЕНИЕ\n"
            "WASD — ходить, Shift — бег, C/Ctrl — присесть, Пробел — прыжок\n"
            "ЛКМ — взять предмет / поставить / установить в слот\n"
            "ПКМ — повернуть предмет в руках, G — бросить\n"
            "E — распаковать коробку, кнопка питания ПК, сесть за монитор\n"
            "Tab — телефон, Esc — пауза, F11 — полный экран\n\n"
            "КАК СОБРАТЬ ПК\n"
            "1. Купите и распакуйте детали (они приедут к двери).\n"
            "2. Поставьте корпус на стол и вставьте в него плату.\n"
            "3. Процессор (сокет должен совпадать!), термопаста, кулер.\n"
            "4. ОЗУ того же типа, что у платы (DDR3/DDR4/DDR5).\n"
            "5. Видеокарта RTX 40-й серии (старые — NOT SUPPORTED).\n"
            "6. Блок питания и накопитель. NVMe — в слот M.2 на плате.\n"
            "7. Вставьте флешку NovaOS в USB на корпусе и включите ПК.\n"
            "8. Подойдите к монитору и нажмите E.\n\n"
            "Майнинг, разгон и заказы приносят деньги!";
        gfx::textWrapped(help, content.x, content.y, content.w, 13.5f, theme::text);
    }
}

// ---------------------------------------------------------------------------
// HUD

void Game::drawHUD(int w, int h) {
    float cx = w * 0.5f, cy = h * 0.5f;
    if (mode == Mode::Play) {
        bool hot = target.entity || target.monitor || target.slot >= 0 || target.pasteCpu;
        gfx::circle(Vec2{cx, cy}, hot ? 4.5f : 3.0f, hot ? rgba(255, 255, 255, 230) : rgba(255, 255, 255, 150), 12);
    }
    // Деньги
    gfx::roundRect(Rect{16, 16, 230, 64}, 12, rgba(16, 20, 28, 200));
    gfx::text(::money(money), 30, 22, 30, rgba(130, 235, 150), true);
    gfx::text(fmt("Заказов: %d  •  Рекорд: %d", ordersDone, bestScore), 30, 56, 14, theme::dim);

    if (mode != Mode::Play) return;

    // Подсказки по цели
    std::vector<std::string> hints;
    Entity* held = world.get(heldId);
    Entity* te = world.get(target.entity);
    if (held) {
        if (target.pasteCpu) hints.push_back(target.slotError.empty() ? "ЛКМ — нанести термопасту" : target.slotError);
        else if (target.slot >= 0) {
            Entity* p = world.get(target.slotParent);
            auto slots = p ? World::slotsFor(p->def) : std::vector<SlotDef>{};
            std::string sn = (target.slot < (int)slots.size()) ? slots[target.slot].name : "";
            hints.push_back(target.slotError.empty() ? "ЛКМ — установить: " + sn : target.slotError);
        } else if (target.entity || target.hitStatic)
            hints.push_back("ЛКМ — поставить");
        hints.push_back("ПКМ — повернуть   G — бросить");
    } else if (te) {
        Entity* root = world.rootOf(te);
        if (te->parent) hints.push_back("ЛКМ — вынуть");
        else hints.push_back("ЛКМ — взять");
        if (te->inBox && !te->parent) hints.push_back("E — распаковать");
        else if (root && root->def->cat == Cat::Case && !root->inBox)
            hints.push_back(caseIsOn(root) ? "E — выключить ПК" : "E — включить ПК");
    } else if (target.monitor) {
        hints.push_back("E — сесть за компьютер");
    }
    float hy = cy + 40;
    for (auto& s : hints) {
        float tw = gfx::textWidth(s, 17) + 24;
        gfx::roundRect(Rect{cx - tw / 2, hy, tw, 30}, 8, rgba(16, 20, 28, 190));
        gfx::textCentered(s, Rect{cx - tw / 2, hy, tw, 30}, 17, target.slotError.empty() || &s != &hints[0]
                                                                     ? theme::text
                                                                     : theme::warn);
        hy += 36;
    }

    // Информация о предмете под прицелом
    Entity* info = te ? te : nullptr;
    if (info) {
        std::string title = info->def->name + (info->inBox ? " (в коробке)" : "");
        std::vector<std::pair<std::string, Color>> lines;
        lines.push_back({partSpecs(*info->def), theme::dim});
        if (info->def->cat == Cat::GPU && !info->def->supported) lines.push_back({"NOT SUPPORTED", theme::bad});
        if (info->broken) lines.push_back({"Сгорел — не работает", theme::bad});
        if (info->def->cat == Cat::Storage && !info->inBox)
            lines.push_back({info->osInstalled ? "NovaOS установлена" : "Пустой (нет ОС)",
                             info->osInstalled ? theme::good : theme::dim});
        if (info->def->cat == Cat::CPU && !info->inBox)
            lines.push_back({info->pasted ? "Термопаста нанесена" : "Без термопасты",
                             info->pasted ? theme::good : theme::warn});
        if (info->def->cat == Cat::Paste) lines.push_back({fmt("Осталось нанесений: %d", info->uses), theme::dim});
        if (info->def->cat == Cat::Case && !info->inBox) {
            Build b = gatherBuild(world, info);
            auto probs = buildProblems(b);
            if (probs.empty()) lines.push_back({"Сборка готова к работе", theme::good});
            else lines.push_back({"Не хватает: " + probs[0] + (probs.size() > 1 ? fmt(" (+%d)", (int)probs.size() - 1) : ""), theme::warn});
            auto it = comps.find(info->id);
            if (it != comps.end() && it->second.on())
                lines.push_back({fmt("CPU %.0f°C  •  GPU %.0f°C  •  %.0f Вт", it->second.cpuTemp, it->second.gpuTemp,
                                     it->second.power),
                                 theme::text});
        }
        float pw = gfx::textWidth(title, 18, true);
        for (auto& l : lines) pw = std::max(pw, gfx::textWidth(l.first, 14));
        pw += 28;
        Rect p{16, h - 52 - (34 + 21.0f * lines.size()), pw, 34 + 21.0f * lines.size()};
        gfx::roundRect(p, 10, rgba(16, 20, 28, 200));
        gfx::text(title, p.x + 14, p.y + 7, 18, theme::text, true);
        for (size_t i = 0; i < lines.size(); i++)
            gfx::text(lines[i].first, p.x + 14, p.y + 32 + 21.0f * i, 14, lines[i].second);
    }

    // Что в руках
    if (held) {
        std::string s = "В руках: " + held->def->name;
        float tw = gfx::textWidth(s, 16) + 28;
        gfx::roundRect(Rect{w - tw - 16, h - 56.0f, tw, 40}, 10, rgba(16, 20, 28, 200));
        gfx::text(s, w - tw - 2, h - 46.0f, 16, theme::text);
    }

    // Пошаговая подсказка для первой сборки
    bool anyOs = false;
    for (auto& up : world.ents) anyOs |= up->osInstalled;
    if (!anyOs) {
        Entity* best = nullptr;
        int bestN = -1;
        for (auto& up : world.ents) {
            if (up->def->cat != Cat::Case || up->inBox) continue;
            std::vector<Entity*> d;
            world.descendants(up->id, d);
            if ((int)d.size() > bestN) bestN = (int)d.size(), best = up.get();
        }
        Build b = gatherBuild(world, best);
        std::vector<std::pair<std::string, bool>> steps = {
            {"Корпус распакован", best != nullptr},
            {"Материнская плата в корпусе", b.mb != nullptr},
            {"Процессор в сокете", b.cpu != nullptr},
            {"Термопаста на процессоре", b.cpu && b.cpu->pasted},
            {"Кулер установлен", b.cooler != nullptr},
            {"Оперативная память", !b.ram.empty()},
            {"Видео (RTX 40 или встройка)", b.cpu && b.hasVideo()},
            {"Блок питания", b.psu != nullptr},
            {"Накопитель", !b.drives.empty()},
            {"Флешка NovaOS в USB", b.usb != nullptr},
            {"Включить ПК и установить ОС", false},
        };
        Rect p{w - 300.0f, 16, 284, 40 + 22.0f * steps.size()};
        gfx::roundRect(p, 12, rgba(16, 20, 28, 190));
        gfx::text("Первая сборка", p.x + 14, p.y + 9, 17, theme::text, true);
        for (size_t i = 0; i < steps.size(); i++) {
            float y = p.y + 36 + 22.0f * i;
            gfx::text(steps[i].second ? "✓" : "•", p.x + 14, y, 15, steps[i].second ? theme::good : theme::dim, true);
            gfx::text(steps[i].first, p.x + 34, y, 14, steps[i].second ? theme::dim : theme::text);
        }
    }
    gfx::text("Tab — телефон", 18, h - 30.0f, 14, rgba(255, 255, 255, 120));
}

// ---------------------------------------------------------------------------
// Меню, пауза, настройки

void Game::drawMenu(int w, int h) {
    gfx::rectGrad(Rect{0, 0, (float)w, (float)h}, rgba(8, 10, 18, 120), rgba(8, 10, 18, 230));
    float cx = w * 0.5f;
    float ty = h * 0.16f;
    gfx::textCentered("PC SIMULATOR 2", Rect{0, ty, (float)w, 90}, 78, rgba(255, 255, 255), true);
    gfx::textCentered("Собери компьютер мечты", Rect{0, ty + 84, (float)w, 30}, 22, rgba(140, 190, 255));
    float bw = 320, bh = 54, y = h * 0.45f;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Продолжить", 22, theme::accent, theme::text, hasSave)) {
        if (!load()) toast("Не удалось загрузить сохранение", theme::bad);
    }
    y += bh + 14;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Новая игра", 22, hasSave ? theme::panel2 : theme::accent)) newGame();
    y += bh + 14;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Настройки", 22, theme::panel2)) {
        settingsBack = Mode::Menu;
        mode = Mode::Settings;
    }
    y += bh + 14;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Выход", 22, theme::panel2)) quit = true;
    gfx::text("v1.0  •  чистый C++ и OpenGL", 16, h - 30.0f, 14, rgba(255, 255, 255, 110));
}

void Game::drawPause(int w, int h) {
    gfx::rect(Rect{0, 0, (float)w, (float)h}, rgba(0, 0, 0, 150));
    float cx = w * 0.5f, bw = 300, bh = 50, y = h * 0.3f;
    gfx::textCentered("Пауза", Rect{0, y - 80, (float)w, 60}, 44, theme::text, true);
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Продолжить", 20) || plat::keyPressed(plat::K_ESC)) {
        openMode(Mode::Play);
        return;
    }
    y += bh + 12;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Сохранить", 20, theme::panel2)) save();
    y += bh + 12;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Настройки", 20, theme::panel2)) {
        settingsBack = Mode::Pause;
        mode = Mode::Settings;
    }
    y += bh + 12;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Сохранить и выйти в меню", 20, theme::panel2)) {
        save();
        mode = Mode::Menu;
        plat::lockMouse(false);
    }
    y += bh + 12;
    if (button(Rect{cx - bw / 2, y, bw, bh}, "Сохранить и выйти", 20, theme::panel2)) {
        save();
        quit = true;
    }
}

void Game::drawSettings(int w, int h) {
    gfx::rect(Rect{0, 0, (float)w, (float)h}, rgba(0, 0, 0, 160));
    Rect p{w * 0.5f - 280, h * 0.5f - 250, 560, 500};
    gfx::roundRect(p, 16, theme::panel);
    gfx::textCentered("Настройки", Rect{p.x, p.y + 14, p.w, 40}, 30, theme::text, true);
    float x = p.x + 40, y = p.y + 80, sw = p.w - 80;
    gfx::text(fmt("Чувствительность мыши: %.2f", settings.sens), x, y, 17, theme::text);
    slider(Rect{x, y + 26, sw, 24}, settings.sens, 0.02f, 0.5f, 0.01f);
    y += 70;
    gfx::text(fmt("Поле зрения: %.0f°", settings.fov), x, y, 17, theme::text);
    slider(Rect{x, y + 26, sw, 24}, settings.fov, 55, 100, 1);
    y += 74;
    if (checkbox(Rect{x, y, sw, 30}, settings.fullscreen, "Полный экран (F11)", 20)) plat::setFullscreen(settings.fullscreen);
    y += 42;
    if (checkbox(Rect{x, y, sw, 30}, settings.vsync, "Вертикальная синхронизация", 20)) plat::setVsync(settings.vsync);
    y += 42;
    checkbox(Rect{x, y, sw, 30}, settings.showFps, "Показывать FPS", 20);
    y += 42;
    checkbox(Rect{x, y, sw, 30}, settings.sound, "Звуки", 20);
    if (button(Rect{p.x + p.w / 2 - 110, p.y + p.h - 70, 220, 48}, "Готово", 20) || plat::keyPressed(plat::K_ESC)) {
        saveSettings();
        mode = settingsBack;
        if (mode == Mode::Pause || mode == Mode::Menu) plat::lockMouse(false);
    }
}
