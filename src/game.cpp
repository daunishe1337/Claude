#include "game.hpp"
#include "platform.hpp"
#include "render.hpp"
#include <algorithm>
#include <sstream>

namespace {
constexpr float kReach = 2.6f;
constexpr float kPlayerR = 0.28f;
}  // namespace

const std::vector<OrderTemplate>& orderTemplates() {
    static const std::vector<OrderTemplate> t = {
        {"Офисный ПК", "Бухгалтерия «Ромашка»", "Нужен простой компьютер для документов. Главное — чтобы работал.",
         320, 0, 8, RamType::None, 0, false, false, 0, false},
        {"ПК для учёбы", "Студентка Аня", "Учёба, браузер и немного игр. Хотя бы 16 ГБ памяти.", 650, 2500, 16,
         RamType::None, 0, false, false, 0, false},
        {"Тихий ПК с NVMe", "Писатель Олег", "Быстрая загрузка с NVMe и хороший обдув: минимум 2 вентилятора.", 750,
         0, 16, RamType::None, 0, false, true, 2, false},
        {"Игровой ПК начального уровня", "Школьник Дима", "Видеокарта RTX 40-й серии и BenchMax от 7000.", 1150, 7000,
         16, RamType::None, 52, false, false, 0, false},
        {"RGB-сборка", "Стример Лиза", "Чтобы всё светилось! RGB-корпус или 2+ RGB-вентилятора и DDR5.", 1600, 6000,
         16, RamType::DDR5, 28, false, false, 2, true},
        {"Игровой ПК", "Киберспортсмен Макс", "RTX 4070 или мощнее, 32 ГБ памяти и BenchMax от 14000.", 2300, 14000,
         32, RamType::None, 80, false, false, 0, false},
        {"Монстр для стримера", "Блогер ProGamer", "Только топ: X3D-процессор, 32+ ГБ DDR5, BenchMax от 22000.", 4700,
         22000, 32, RamType::DDR5, 115, true, false, 0, false},
    };
    return t;
}

Game::Game() {
    loadSettings();
    std::string tmp;
    hasSave = plat::readFile(savePath(), tmp) && !tmp.empty();
}

std::string Game::savePath() const { return plat::dataDir() + "pcsim2_save.txt"; }

void Game::toast(const std::string& s, Color c) {
    toasts.push_back(Toast{s, c, 0});
    while (toasts.size() > 5) toasts.pop_front();
}

void Game::beep(float f, float d) {
    if (settings.sound) plat::beep(f, d);
}

void Game::earn(double v) {
    money += v;
    totalEarned += v;
}

SimEvents Game::simEvents() {
    SimEvents ev;
    ev.toast = [this](const std::string& s, Color c) { toast(s, c); };
    ev.earn = [this](double v) { earn(v); };
    ev.benchDone = [this](int score) {
        if (score > bestScore) {
            bestScore = score;
            toast(fmt("Новый рекорд BenchMax: %d очков!", score), ui::theme::good);
        } else {
            toast(fmt("BenchMax: %d очков", score));
        }
    };
    ev.beep = [this](float f, float d) { beep(f, d); };
    return ev;
}

Computer& Game::comp(int caseId) {
    Computer& c = comps[caseId];
    c.caseId = caseId;
    return c;
}

void Game::newGame() {
    world.clear();
    comps.clear();
    money = 1500;
    bestScore = 0;
    ordersDone = 0;
    totalEarned = 0;
    deliveries.clear();
    toasts.clear();
    orders.clear();
    for (int i = 0; i < 3; i++) orders.push_back(randomOrder());
    world.spawn(findPart("usb_os"), v3(1.25f, 0.9f, -2.45f), false);
    world.spawn(findPart("paste"), v3(1.45f, 0.9f, -2.45f), false);
    pos = v3(0, 0, 1.0f);
    yaw = 0;
    pitch = -10;
    velY = 0;
    heldId = 0;
    phoneTab = phoneCat = phoneScroll = 0;
    openMode(Mode::Play);
    toast("Добро пожаловать в PC Simulator 2!", ui::theme::accent);
    toast("Откройте телефон [Tab], чтобы купить комплектующие.");
}

void Game::openMode(Mode m) {
    mode = m;
    plat::lockMouse(m == Mode::Play);
}

Order Game::randomOrder() {
    const auto& t = orderTemplates();
    int idx = 0;
    for (int tries = 0; tries < 20; tries++) {
        idx = rndi(0, (int)t.size() - 1);
        bool dup = false;
        for (auto& o : orders) dup |= o.tmpl == idx;
        if (!dup) break;
    }
    Order o;
    o.tmpl = idx;
    o.reward = (int)(t[idx].reward * (0.92f + rndf() * 0.22f) / 10) * 10;
    return o;
}

bool Game::buy(const PartDef* d) {
    if (money < d->price) {
        toast("Не хватает денег на " + d->name, ui::theme::bad);
        beep(220, 0.15f);
        return false;
    }
    money -= d->price;
    deliveries.push_back(Delivery{d, 1.5f + rndf()});
    toast("Куплено: " + d->name + " — доставка к двери", ui::theme::good);
    beep(990, 0.06f);
    return true;
}

// ---------------------------------------------------------------------------
// Главный цикл

void Game::frame(float dt) {
    dt = std::min(dt, 0.05f);
    fps = fps * 0.95f + (dt > 0 ? 1.0f / dt : 60) * 0.05f;
    if (plat::keyPressed(plat::K_F11)) {
        settings.fullscreen = !settings.fullscreen;
        plat::setFullscreen(settings.fullscreen);
    }
    if (debugShots) setupShots();

    bool simulate = mode == Mode::Play || mode == Mode::Phone || mode == Mode::Screen;
    if (simulate) {
        time += dt;
        if (mode == Mode::Play) updatePlay(dt);
        else if (mode == Mode::Phone) {
            if (plat::keyPressed(plat::K_TAB) || plat::keyPressed(plat::K_ESC)) openMode(Mode::Play);
        } else if (mode == Mode::Screen) {
            Computer* c = monitorComputer();
            if (plat::keyPressed(plat::K_ESC) || !c || !c->hasScreen()) openMode(Mode::Play);
            else if (c->st == PCState::Post && plat::keyPressed(plat::K_DELETE)) enterBios(*c);
            else if (c->st == PCState::Desktop && c->app == APP_SNAKE) {
                Snake& s = c->snake;
                if (plat::keyPressed(plat::K_UP) || plat::keyPressed(plat::K_W)) s.ndx = 0, s.ndy = -1;
                if (plat::keyPressed(plat::K_DOWN) || plat::keyPressed(plat::K_S)) s.ndx = 0, s.ndy = 1;
                if (plat::keyPressed(plat::K_LEFT) || plat::keyPressed(plat::K_A)) s.ndx = -1, s.ndy = 0;
                if (plat::keyPressed(plat::K_RIGHT) || plat::keyPressed(plat::K_D)) s.ndx = 1, s.ndy = 0;
                if (s.dead && (plat::keyPressed(plat::K_SPACE) || plat::keyPressed(plat::K_ENTER))) s.reset();
            }
        }
        if (mode != Mode::Play) updateHeld();

        world.update(dt);
        // Корпуса, которых больше нет (проданы/отправлены), забываем
        for (auto it = comps.begin(); it != comps.end();) {
            if (!world.get(it->first)) it = comps.erase(it);
            else ++it;
        }
        SimEvents ev = simEvents();
        for (auto& kv : comps) updateComputer(world, kv.second, dt, ev);

        for (auto& d : deliveries) d.t -= dt;
        for (auto it = deliveries.begin(); it != deliveries.end();) {
            if (it->t <= 0) {
                Rect z = world.deliveryZone;
                Vec3 p = v3(z.x + 0.25f + rndf() * (z.w - 0.5f), 0.9f + rndf() * 0.4f, z.y + 0.25f + rndf() * (z.h - 0.5f));
                Entity& e = world.spawn(it->def, p, true);
                e.yaw = rndi(0, 3);
                toast("Доставлено: " + it->def->name);
                it = deliveries.erase(it);
            } else {
                ++it;
            }
        }
        autosaveT += dt;
        if (autosaveT > 60) {
            autosaveT = 0;
            save();
        }
    }
    for (auto& t : toasts) t.t += dt;
    while (!toasts.empty() && toasts.front().t > 6) toasts.pop_front();
    if (saveFlash > 0) saveFlash -= dt;

    // Монитор показывает ближайший к нему собранный корпус
    screenComp = 0;
    float bestD = 2.6f;
    for (auto& up : world.ents) {
        Entity& e = *up;
        if (e.parent || e.held || e.inBox || e.def->cat != Cat::Case) continue;
        float dx = e.pos.x - world.monitorPos.x, dz = e.pos.z - world.monitorPos.z;
        float d = std::sqrt(dx * dx + dz * dz);
        if (d < bestD) {
            bool hasMb = false;
            for (Entity* ch : world.children(e.id)) hasMb |= ch->def->cat == Cat::Motherboard;
            if (hasMb) bestD = d, screenComp = e.id;
        }
    }
    if (screenComp) comp(screenComp);

    render(plat::width(), plat::height());
    if (!shotName.empty()) {
        gfx::screenshot(plat::dataDir() + shotName, plat::width(), plat::height());
        shotName.clear();
    }
}

Computer* Game::monitorComputer() {
    if (!screenComp) return nullptr;
    auto it = comps.find(screenComp);
    return it == comps.end() ? nullptr : &it->second;
}

bool Game::caseIsOn(Entity* e) {
    if (!e) return false;
    auto it = comps.find(e->id);
    return it != comps.end() && it->second.on();
}

// ---------------------------------------------------------------------------
// Игрок

Vec3 Game::forward() const {
    float y = yaw * kDeg, p = pitch * kDeg;
    return v3(std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p));
}

void Game::updatePlay(float dt) {
    if (plat::keyPressed(plat::K_ESC)) {
        openMode(Mode::Pause);
        return;
    }
    if (plat::keyPressed(plat::K_TAB)) {
        openMode(Mode::Phone);
        return;
    }
    if (!plat::focused()) return;
    updatePlayer(dt);
    computeTarget();
    handleInteraction();
    updateHeld();
}

void Game::updatePlayer(float dt) {
    Vec2 md = plat::mouseDelta();
    yaw += md.x * settings.sens;
    pitch = clampf(pitch - md.y * settings.sens, -88, 88);
    yaw = std::fmod(yaw, 360.0f);

    float sp = plat::keyDown(plat::K_SHIFT) ? 4.2f : 2.4f;
    bool crouch = plat::keyDown(plat::K_C) || plat::keyDown(plat::K_CTRL);
    if (crouch) sp = 1.3f;
    eye = approach(eye, crouch ? 1.05f : 1.62f, 12, dt);
    float y = yaw * kDeg;
    Vec3 f = v3(std::sin(y), 0, -std::cos(y)), r = v3(std::cos(y), 0, std::sin(y));
    Vec3 mv{0, 0, 0};
    if (plat::keyDown(plat::K_W)) mv += f;
    if (plat::keyDown(plat::K_S)) mv += -f;
    if (plat::keyDown(plat::K_D)) mv += r;
    if (plat::keyDown(plat::K_A)) mv += -r;
    if (length(mv) > 0) mv = normalize(mv) * (sp * dt);
    pos.x += mv.x;
    pos.z += mv.z;

    for (auto& s : world.solids) {
        float cx = clampf(pos.x, s.x0, s.x1), cz = clampf(pos.z, s.z0, s.z1);
        float dx = pos.x - cx, dz = pos.z - cz, d2 = dx * dx + dz * dz;
        if (d2 < kPlayerR * kPlayerR) {
            if (d2 > 1e-8f) {
                float d = std::sqrt(d2), push = kPlayerR - d;
                pos.x += dx / d * push;
                pos.z += dz / d * push;
            } else {
                // Центр внутри препятствия — выталкиваем по ближайшей стороне
                float l = pos.x - s.x0, rr = s.x1 - pos.x, b = pos.z - s.z0, t = s.z1 - pos.z;
                float m = std::min(std::min(l, rr), std::min(b, t));
                if (m == l) pos.x = s.x0 - kPlayerR;
                else if (m == rr) pos.x = s.x1 + kPlayerR;
                else if (m == b) pos.z = s.z0 - kPlayerR;
                else pos.z = s.z1 + kPlayerR;
            }
        }
    }
    pos.x = clampf(pos.x, world.roomX0 + kPlayerR, world.roomX1 - kPlayerR);
    pos.z = clampf(pos.z, world.roomZ0 + kPlayerR, world.roomZ1 - kPlayerR);

    if (onGround && plat::keyPressed(plat::K_SPACE)) {
        velY = 4.0f;
        onGround = false;
    }
    velY -= 14.0f * dt;
    pos.y += velY * dt;
    if (pos.y <= 0) {
        pos.y = 0;
        velY = 0;
        onGround = true;
    }
}

bool Game::isHeldOrInside(Entity* e) {
    Entity* r = world.rootOf(e);
    return r && r->held;
}

void Game::computeTarget() {
    Target t;
    Ray ray{eyePos(), forward()};
    float staticT = 1e9f;
    // Статика: поверхности (как тонкие плиты) и стены комнаты
    for (auto& s : world.surfaces) {
        BBox b{v3(s.x0, s.y - 0.04f, s.z0), v3(s.x1, s.y, s.z1)};
        float tt;
        if (rayBox(ray, b, tt) && tt < staticT) staticT = tt;
    }
    {
        const float lo[3] = {world.roomX0, 0, world.roomZ0}, hi[3] = {world.roomX1, world.roomH, world.roomZ1};
        const float o[3] = {ray.pos.x, ray.pos.y, ray.pos.z}, d[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
        for (int i = 0; i < 3; i++) {
            if (std::fabs(d[i]) < 1e-6f) continue;
            float tt = ((d[i] > 0 ? hi[i] : lo[i]) - o[i]) / d[i];
            if (tt > 0 && tt < staticT) staticT = tt;
        }
    }
    float tt;
    if (rayBox(ray, world.monitorBox, tt) && tt < staticT) {
        staticT = tt;
        t.monitor = tt < kReach;
    }
    float bestE = 1e9f;
    for (auto& up : world.ents) {
        Entity& e = *up;
        if (isHeldOrInside(&e)) continue;
        for (const BBox& b : world.hitBoxes(e)) {
            if (rayBox(ray, b, tt) && tt < bestE) {
                bestE = tt;
                t.entity = e.id;
            }
        }
    }
    if (bestE < staticT) {
        t.monitor = false;
        t.t = bestE;
        if (bestE > kReach) t.entity = 0;
    } else {
        t.entity = 0;
        t.t = staticT;
        t.hitStatic = staticT < kReach;
    }
    t.point = ray.pos + ray.dir * t.t;

    Entity* held = world.get(heldId);
    if (held && !held->inBox) {
        float limit = std::min(kReach, t.t + 0.35f);
        if (held->def->cat == Cat::Paste) {
            float best = 0.07f;
            for (auto& up : world.ents) {
                Entity& c = *up;
                if (c.def->cat != Cat::CPU || isHeldOrInside(&c)) continue;
                Vec3 p = xformPoint(world.worldMatrix(c), v3(0, 0.003f, 0)) - ray.pos;
                float along = dot(p, ray.dir);
                if (along < 0.05f || along > limit) continue;
                float d = length(p - ray.dir * along);
                if (d < best) {
                    best = d;
                    t.pasteCpu = c.id;
                    t.slotError.clear();
                    if (c.parent && world.childInSlot(c.parent, 1)) t.slotError = "Сначала снимите кулер";
                    else if (c.pasted) t.slotError = "Термопаста уже нанесена";
                }
            }
        } else {
            float bestOk = 0.1f, bestBad = 0.1f;
            int okP = 0, okS = -1, badP = 0, badS = -1;
            std::string badErr;
            for (auto& up : world.ents) {
                Entity& p = *up;
                if (p.inBox || isHeldOrInside(&p)) continue;
                if (p.def->cat != Cat::Case && p.def->cat != Cat::Motherboard) continue;
                auto slots = World::slotsFor(p.def);
                for (int i = 0; i < (int)slots.size(); i++) {
                    if (slots[i].accepts != held->def->cat) continue;
                    if (held->def->cat == Cat::Storage && slots[i].m2 != (held->def->stor == StorType::NVME)) continue;
                    if (world.childInSlot(p.id, i)) continue;
                    Vec3 sp = xformPoint(world.slotWorld(p, i), v3(0, 0, 0)) - ray.pos;
                    float along = dot(sp, ray.dir);
                    if (along < 0.05f || along > limit) continue;
                    float d = length(sp - ray.dir * along);
                    std::string err = canInstall(*held, p, i);
                    if (err.empty() && d < bestOk) bestOk = d, okP = p.id, okS = i;
                    if (!err.empty() && d < bestBad) bestBad = d, badP = p.id, badS = i, badErr = err;
                }
            }
            if (okP) {
                t.slotParent = okP;
                t.slot = okS;
            } else if (badP) {
                t.slotParent = badP;
                t.slot = badS;
                t.slotError = badErr;
            }
        }
    }
    target = t;
}

std::string Game::canInstall(Entity& item, Entity& parent, int slot) {
    if (item.inBox) return "Сначала распакуйте коробку [E]";
    auto slots = World::slotsFor(parent.def);
    if (slot < 0 || slot >= (int)slots.size()) return "Сюда это не ставится";
    const SlotDef& s = slots[slot];
    if (s.accepts != item.def->cat) return "Сюда это не ставится";
    if (item.def->cat != Cat::USB && caseIsOn(world.rootOf(&parent))) return "Сначала выключите компьютер";
    if (world.childInSlot(parent.id, slot)) return "Слот занят";
    if (item.def->cat == Cat::Storage && s.m2 != (item.def->stor == StorType::NVME))
        return item.def->stor == StorType::NVME ? "NVMe ставится в слот M.2 на плате" : "SATA-накопитель ставится в отсек корпуса";
    switch (item.def->cat) {
        case Cat::CPU:
            if (parent.def->socket != item.def->socket)
                return "Не тот сокет: процессор " + item.def->socket + ", а плата " + parent.def->socket;
            if (world.childInSlot(parent.id, 1)) return "Сначала снимите кулер";
            break;
        case Cat::Cooler:
            if (!world.childInSlot(parent.id, 0)) return "Сначала установите процессор";
            break;
        case Cat::RAM:
            if (parent.def->ram != item.def->ram)
                return std::string("Плата поддерживает только ") + ramName(parent.def->ram) + ", а это " +
                       ramName(item.def->ram);
            break;
        default:
            break;
    }
    return "";
}

void Game::install(Entity& item, Entity& parent, int slot) {
    item.held = false;
    item.parent = parent.id;
    item.slot = slot;
    item.resting = true;
    heldId = 0;
    beep(1300, 0.04f);
    if (item.def->cat == Cat::Cooler) {
        Entity* cpu = world.childInSlot(parent.id, 0);
        if (cpu && !cpu->pasted) toast("Внимание: на процессоре нет термопасты — он будет перегреваться!", ui::theme::warn);
    }
    world.wakeAll();
}

void Game::takeOut(Entity& e) {
    Entity* root = world.rootOf(&e);
    if (e.def->cat != Cat::USB && caseIsOn(root)) {
        toast("Выключите компьютер, прежде чем вынимать детали", ui::theme::warn);
        return;
    }
    if (e.def->cat == Cat::CPU && world.childInSlot(e.parent, 1)) {
        toast("Сначала снимите кулер", ui::theme::warn);
        return;
    }
    if (e.def->cat == Cat::Cooler) {
        Entity* cpu = world.childInSlot(e.parent, 0);
        if (cpu && cpu->pasted) {
            cpu->pasted = false;
            toast("Старая термопаста стёрлась — перед установкой кулера нанесите новую");
        }
    }
    e.parent = 0;
    e.slot = -1;
    e.held = true;
    heldId = e.id;
    heldYaw = 0;
    beep(800, 0.04f);
}

void Game::updateHeld() {
    Entity* e = world.get(heldId);
    if (!e) {
        heldId = 0;
        return;
    }
    BBox lb = world.localBounds(*e);
    float size = std::max(lb.max.x - lb.min.x, std::max(lb.max.y - lb.min.y, lb.max.z - lb.min.z));
    Vec3 f = forward();
    Vec3 hp = eyePos() + f * (0.35f + size * 0.8f) + v3(0, -0.12f - size * 0.25f, 0);
    BBox wb = world.worldBounds(*e);
    e->pos = v3(hp.x, hp.y - (wb.max.y - wb.min.y) * 0.5f, hp.z);
    e->yaw = ((int)std::lround((-90.0f - yaw) / 90.0f) + heldYaw) & 3;
    e->velY = 0;
}

void Game::placeHeld(Vec3 at) {
    Entity* e = world.get(heldId);
    if (!e) return;
    BBox wb = world.worldBounds(*e);
    float r = std::max(wb.max.x - wb.min.x, wb.max.z - wb.min.z) * 0.5f;
    Vec3 f = forward();
    Vec3 back = normalize(v3(f.x, 0, f.z)) * (r * 0.6f);
    e->pos = v3(at.x - back.x, at.y + 0.02f, at.z - back.z);
    e->pos.x = clampf(e->pos.x, world.roomX0 + r, world.roomX1 - r);
    e->pos.z = clampf(e->pos.z, world.roomZ0 + r, world.roomZ1 - r);
    e->held = false;
    e->resting = false;
    e->velY = 0;
    heldId = 0;
    world.wakeAll();
}

void Game::dropHeld() {
    Entity* e = world.get(heldId);
    if (!e) return;
    Vec3 f = forward();
    Vec3 p = eyePos() + normalize(v3(f.x, 0, f.z)) * 0.6f;
    placeHeld(v3(p.x, std::max(0.1f, p.y - 0.6f), p.z));
}

void Game::handleInteraction() {
    Entity* held = world.get(heldId);
    if (plat::mousePressed(0)) {
        if (held) {
            if (target.pasteCpu) {
                Entity* cpu = world.get(target.pasteCpu);
                if (!target.slotError.empty()) toast(target.slotError, ui::theme::warn);
                else if (cpu) {
                    cpu->pasted = true;
                    held->uses--;
                    toast(fmt("Термопаста нанесена (осталось %d)", held->uses), ui::theme::good);
                    beep(700, 0.05f);
                    if (held->uses <= 0) {
                        world.remove(held->id);
                        heldId = 0;
                    }
                }
            } else if (target.slot >= 0) {
                Entity* p = world.get(target.slotParent);
                if (p) {
                    std::string err = canInstall(*held, *p, target.slot);
                    if (err.empty()) install(*held, *p, target.slot);
                    else {
                        toast(err, ui::theme::warn);
                        beep(240, 0.12f);
                    }
                }
            } else if (target.entity || target.hitStatic) {
                placeHeld(target.point);
            }
        } else if (target.entity) {
            Entity* e = world.get(target.entity);
            if (e && e->parent) takeOut(*e);
            else if (e) {
                e->held = true;
                heldId = e->id;
                heldYaw = 0;
                world.wakeAll();
            }
        }
    }
    if (held && plat::mousePressed(1)) heldYaw = (heldYaw + 1) & 3;
    if (held && (plat::keyPressed(plat::K_G) || plat::keyPressed(plat::K_Q))) dropHeld();

    if (plat::keyPressed(plat::K_E)) {
        if (target.monitor) {
            Computer* c = monitorComputer();
            if (c && c->hasScreen()) openMode(Mode::Screen);
            else if (c && c->on()) toast("Монитор: нет сигнала. " + c->msg, ui::theme::warn);
            else toast("Монитор: нет сигнала. Поставьте собранный ПК на стол рядом и включите его [E].", ui::theme::warn);
        } else if (target.entity) {
            Entity* e = world.get(target.entity);
            Entity* root = world.rootOf(e);
            if (e && !e->parent && e->inBox) {
                e->inBox = false;
                e->resting = false;
                e->pos.y += 0.02f;
                toast("Распаковано: " + e->def->name);
                beep(500, 0.05f);
                world.wakeAll();
            } else if (root && root->def->cat == Cat::Case && !root->inBox) {
                pressPower(world, comp(root->id), simEvents());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Продажа и заказы

std::vector<Entity*> Game::shipItems() {
    std::vector<Entity*> out;
    for (auto& up : world.ents)
        if (!up->parent && !up->held && world.inZone(world.shipZone, *up)) out.push_back(up.get());
    return out;
}

double Game::sellValue(Entity* e) {
    std::vector<Entity*> all{e};
    world.descendants(e->id, all);
    double v = 0;
    for (Entity* x : all) {
        double k = x->inBox ? 0.85 : 0.6;
        if (x->broken) k = 0.05;
        double p = x->def->price;
        if (x->def->cat == Cat::Paste && x->def->uses > 0) p = p * x->uses / x->def->uses;
        v += p * k;
    }
    return std::floor(v);
}

void Game::sellAll() {
    auto items = shipItems();
    if (items.empty()) {
        toast("На поддоне у двери ничего нет — положите туда то, что хотите продать", ui::theme::warn);
        return;
    }
    double total = 0;
    std::vector<int> ids;
    for (Entity* e : items) {
        total += sellValue(e);
        ids.push_back(e->id);
    }
    for (int id : ids) {
        comps.erase(id);
        world.remove(id);
    }
    earn(total);
    toast("Продано на " + ::money(total), ui::theme::good);
    beep(1200, 0.08f);
}

std::vector<std::string> Game::checkOrder(const OrderTemplate& t, Entity* cs) {
    Build b = gatherBuild(world, cs);
    std::vector<std::string> p = buildProblems(b);
    if (!p.empty()) return p;
    const OcSettings& oc = comp(cs->id).oc;
    if (!cpuStable(b, oc) || !gpuStable(b, oc)) p.push_back("нестабильный разгон — сбросьте его в OC Tuner");
    if (b.cpu && !b.cpu->pasted) p.push_back("на процессоре нет термопасты");
    Perf perf = estimatePerf(b, oc);
    if (perf.score < t.minScore) p.push_back(fmt("BenchMax %d, а нужно %d", perf.score, t.minScore));
    if (b.ramGB < t.minRam) p.push_back(fmt("памяти %d ГБ, а нужно %d ГБ", b.ramGB, t.minRam));
    if (t.ramType != RamType::None && (b.ram.empty() || b.ram[0]->def->ram != t.ramType))
        p.push_back(std::string("нужна память ") + ramName(t.ramType));
    if (t.minGpuPerf > 0 && (!b.gpuWorks() || b.gpu->def->gpuPerf < t.minGpuPerf)) {
        std::string need = "RTX 40";
        for (auto& d : catalog())
            if (d.cat == Cat::GPU && d.supported && d.gpuPerf >= t.minGpuPerf) {
                need = d.name;
                break;
            }
        p.push_back("нужна видеокарта не слабее " + need);
    }
    if (t.needX3d && (!b.cpu || !b.cpu->def->x3d)) p.push_back("нужен процессор X3D");
    if (t.needNvme && (!b.bootDrive() || b.bootDrive()->def->stor != StorType::NVME))
        p.push_back("система должна стоять на NVMe");
    if ((int)b.fans.size() < t.minFans) p.push_back(fmt("нужно вентиляторов: %d", t.minFans));
    if (t.needRgb) {
        int rgbFans = 0;
        for (Entity* f : b.fans) rgbFans += f->def->fanRgb ? 1 : 0;
        if (!cs->def->rgb && rgbFans < 2) p.push_back("нужен RGB-корпус или 2 RGB-вентилятора");
    }
    return p;
}

void Game::completeOrder(int idx) {
    if (idx < 0 || idx >= (int)orders.size()) return;
    Entity* cs = nullptr;
    int count = 0;
    for (Entity* e : shipItems())
        if (e->def->cat == Cat::Case && !e->inBox) cs = e, count++;
    if (!cs) {
        toast("Поставьте собранный ПК на поддон отправки у двери", ui::theme::warn);
        return;
    }
    if (count > 1) {
        toast("На поддоне несколько корпусов — оставьте один", ui::theme::warn);
        return;
    }
    const OrderTemplate& t = orderTemplates()[orders[idx].tmpl];
    auto probs = checkOrder(t, cs);
    if (!probs.empty()) {
        std::string s = "Клиент недоволен: " + probs[0];
        if (probs.size() > 1) s += fmt(" (и ещё %d)", (int)probs.size() - 1);
        toast(s, ui::theme::bad);
        beep(240, 0.15f);
        return;
    }
    int reward = orders[idx].reward;
    comps.erase(cs->id);
    world.remove(cs->id);
    earn(reward);
    ordersDone++;
    orders[idx] = randomOrder();
    toast(fmt("Заказ «%s» выполнен! +%s", t.title, ::money(reward).c_str()), ui::theme::good);
    beep(1046, 0.1f);
}

// ---------------------------------------------------------------------------
// Отрисовка

static Color rgbNow(const Computer& c, float time) {
    switch (c.oc.rgbMode) {
        case 0: return c.oc.rgb;
        case 1: return hsv(time * 70.0f, 0.85f, 1.0f);
        case 2: {
            float k = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * 2.5f));
            Color r = c.oc.rgb;
            return rgba((int)(r.r * k), (int)(r.g * k), (int)(r.b * k));
        }
        default: return rgba(0, 0, 0, 0);
    }
}

void Game::renderWorld(const Mat4& proj, const Mat4& view, bool withOs) {
    gfx::begin3D(proj, view);
    world.drawRoom(time, true);
    bool hideHeld = mode == Mode::Play && heldId && (target.slot >= 0);
    for (auto& up : world.ents) {
        Entity& e = *up;
        Entity* root = world.rootOf(&e);
        if (hideHeld && root && root->id == heldId) continue;
        bool powered = false;
        Color rgb = rgba(0, 0, 0, 0);
        float fan = 0;
        if (root && root->def->cat == Cat::Case) {
            auto it = comps.find(root->id);
            if (it != comps.end() && it->second.on()) {
                powered = true;
                rgb = rgbNow(it->second, time);
                fan = 0.45f + 0.55f * clampf((it->second.cpuTemp - 35) / 45, 0, 1);
            }
        }
        bool hl = mode == Mode::Play && (e.id == target.entity || e.id == target.pasteCpu);
        if (hl) gfx::setTint(rgba(255, 255, 255), 0.22f);
        world.drawEntity(e, time, powered, rgb, fan);
        if (hl) gfx::setTint(rgba(0, 0, 0), 0);
    }
    // «Призрак» устанавливаемой детали
    Entity* held = world.get(heldId);
    if (mode == Mode::Play && held && target.slot >= 0) {
        Entity* p = world.get(target.slotParent);
        if (p) {
            gfx::setDepthWrite(false);
            gfx::setAlpha(0.5f);
            gfx::setTint(target.slotError.empty() ? rgba(80, 255, 120) : rgba(255, 70, 70), 0.55f);
            world.drawEntityAt(*held, world.slotWorld(*p, target.slot), time);
            gfx::setTint(rgba(0, 0, 0), 0);
            gfx::setAlpha(1);
            gfx::setDepthWrite(true);
        }
    }
    // Экран монитора
    Computer* mc = monitorComputer();
    gfx::beginCanvas3D(world.screenCanvasMatrix(1024, 640));
    if (mc && mc->hasScreen() && withOs) {
        ui::set(ui::Ctx{});
        drawOS(*mc, 1024, 640);
    } else if (mc && mc->on()) {
        gfx::rect(Rect{0, 0, 1024, 640}, rgba(0, 0, 0));
        float bx = 380 + std::sin(time * 0.7f) * 260, by = 290 + std::cos(time * 0.9f) * 220;
        gfx::text("Нет сигнала", bx, by, 44, rgba(200, 200, 210), true);
    }
    gfx::endCanvas();
    // Стекло — в последнюю очередь
    gfx::setDepthWrite(false);
    for (auto& up : world.ents) world.drawGlass(*up);
    gfx::setDepthWrite(true);
    gfx::end3D();
}

void Game::render(int w, int h) {
    gfx::beginFrame(w, h, rgba(20, 22, 28));
    float aspect = (float)w / h;
    Mat4 proj = perspective(settings.fov, aspect, 0.03f, 60.0f);
    Mat4 view;
    if (mode == Mode::Menu || (mode == Mode::Settings && settingsBack == Mode::Menu)) {
        float a = time * 0.08f;
        Vec3 eyeP = v3(0.6f + std::sin(a) * 1.4f, 1.75f, 0.6f + std::cos(a) * 0.9f);
        view = lookAt(eyeP, v3(-0.4f, 0.95f, -2.5f), v3(0, 1, 0));
        time += 1.0f / 60.0f;
    } else {
        Vec3 e = eyePos();
        view = lookAt(e, e + forward(), v3(0, 1, 0));
    }
    renderWorld(proj, view, true);

    gfx::beginScreen2D(w, h);
    ui::Ctx in;
    in.mouse = plat::mousePos();
    in.pressed = plat::mousePressed(0);
    in.down = plat::mouseDown(0);
    in.released = plat::mouseReleased(0);
    in.wheel = plat::wheel();
    in.active = true;

    switch (mode) {
        case Mode::Menu:
            ui::set(in);
            drawMenu(w, h);
            break;
        case Mode::Play:
            ui::set(ui::Ctx{});
            drawHUD(w, h);
            break;
        case Mode::Phone: {
            drawHUD(w, h);
            ui::set(in);
            float ph = std::min((float)h - 40, 780.0f), pw = ph * 0.52f;
            drawPhone(Rect{w - pw - 30, (h - ph) * 0.5f, pw, ph});
            break;
        }
        case Mode::Screen: {
            gfx::rect(Rect{0, 0, (float)w, (float)h}, rgba(0, 0, 0, 170));
            Computer* c = monitorComputer();
            if (c) {
                float s = std::min((w - 60.0f) / 1024.0f, (h - 90.0f) / 640.0f);
                float ox = (w - 1024 * s) * 0.5f, oy = (h - 640 * s) * 0.5f - 12;
                gfx::roundRect(Rect{ox - 14, oy - 14, 1024 * s + 28, 640 * s + 28}, 10, rgba(18, 18, 22));
                ui::Ctx sc = in;
                sc.mouse = Vec2{(in.mouse.x - ox) / s, (in.mouse.y - oy) / s};
                ui::set(sc);
                gfx::setCanvas2D(ox, oy, s);
                drawOS(*c, 1024, 640);
                gfx::resetCanvas2D();
                gfx::textCentered("Esc — встать из-за компьютера", Rect{0, h - 34.0f, (float)w, 30}, 16,
                                  rgba(200, 200, 210));
            }
            break;
        }
        case Mode::Pause:
            ui::set(in);
            drawPause(w, h);
            break;
        case Mode::Settings:
            ui::set(in);
            drawSettings(w, h);
            break;
    }
    // Тосты поверх всего
    float ty = 18;
    for (auto& t : toasts) {
        float a = clampf(6 - t.t, 0, 1) * clampf(t.t * 6, 0, 1);
        float tw = gfx::textWidth(t.text, 18) + 36;
        Rect r{(w - tw) * 0.5f, ty, tw, 36};
        gfx::roundRect(r, 8, rgba(20, 24, 32, (int)(225 * a)));
        gfx::rect(Rect{r.x, r.y + 6, 4, r.h - 12}, withAlpha(t.color, (int)(255 * a)));
        gfx::text(t.text, r.x + 18, r.y + 7, 18, withAlpha(ui::theme::text, (int)(255 * a)));
        ty += 42;
    }
    if (saveFlash > 0) gfx::text("Сохранено ✓", w - 150.0f, h - 40.0f, 18, withAlpha(ui::theme::good, 220));
    if (settings.showFps) gfx::text(fmt("%.0f FPS", fps), w - 90.0f, 8, 16, rgba(255, 255, 120));
    gfx::flush();
}

// ---------------------------------------------------------------------------
// Сохранение

bool Game::save() {
    if (mode == Mode::Menu && !hasSave && world.ents.empty()) return false;
    // Предмет из рук кладём под ноги, а доставки выдаём сразу
    if (Entity* h = world.get(heldId)) {
        h->held = false;
        h->pos = v3(pos.x, 0.0f, pos.z);
        h->resting = false;
        heldId = 0;
    }
    for (auto& d : deliveries) {
        Rect z = world.deliveryZone;
        world.spawn(d.def, v3(z.x + z.w * 0.5f, 0.5f, z.y + z.h * 0.5f), true);
    }
    deliveries.clear();

    std::ostringstream o;
    o << "PCSIM2 1\n";
    o << fmt("money %.2f\nbest %d\ndone %d\nearned %.2f\nnextid %d\n", money, bestScore, ordersDone, totalEarned,
             world.nextId);
    o << fmt("player %.3f %.3f %.3f %.2f %.2f\n", pos.x, pos.y, pos.z, yaw, pitch);
    for (auto& ord : orders) o << fmt("order %d %d\n", ord.tmpl, ord.reward);
    for (auto& up : world.ents) {
        Entity& e = *up;
        o << fmt("ent %d %s %.4f %.4f %.4f %d %d %d %d %d %d %d %.4f %d\n", e.id, e.def->id.c_str(), e.pos.x, e.pos.y,
                 e.pos.z, e.yaw, e.parent, e.slot, e.inBox ? 1 : 0, e.osInstalled ? 1 : 0, e.pasted ? 1 : 0,
                 e.broken ? 1 : 0, e.quality, e.uses);
    }
    for (auto& kv : comps) {
        const OcSettings& s = kv.second.oc;
        o << fmt("oc %d %d %.3f %d %d %d %d %d %d %d\n", kv.first, s.cpuOC, s.vcore, s.gpuCore, s.gpuMem, s.xmp ? 1 : 0,
                 s.rgb.r, s.rgb.g, s.rgb.b, s.rgbMode);
    }
    bool ok = plat::writeFile(savePath(), o.str());
    if (ok) {
        hasSave = true;
        saveFlash = 2;
    } else {
        toast("Не удалось сохранить игру в " + savePath(), ui::theme::bad);
    }
    return ok;
}

bool Game::load() {
    std::string data;
    if (!plat::readFile(savePath(), data)) return false;
    std::istringstream in(data);
    std::string line;
    if (!std::getline(in, line) || line.rfind("PCSIM2", 0) != 0) return false;
    world.clear();
    comps.clear();
    orders.clear();
    deliveries.clear();
    toasts.clear();
    heldId = 0;
    int maxId = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string k;
        ls >> k;
        if (k == "money") ls >> money;
        else if (k == "best") ls >> bestScore;
        else if (k == "done") ls >> ordersDone;
        else if (k == "earned") ls >> totalEarned;
        else if (k == "nextid") ls >> world.nextId;
        else if (k == "player") ls >> pos.x >> pos.y >> pos.z >> yaw >> pitch;
        else if (k == "order") {
            Order o;
            ls >> o.tmpl >> o.reward;
            if (o.tmpl >= 0 && o.tmpl < (int)orderTemplates().size()) orders.push_back(o);
        } else if (k == "ent") {
            auto e = std::make_unique<Entity>();
            std::string pid;
            int inBox = 0, os = 0, pasted = 0, broken = 0;
            ls >> e->id >> pid >> e->pos.x >> e->pos.y >> e->pos.z >> e->yaw >> e->parent >> e->slot >> inBox >> os >>
                pasted >> broken >> e->quality >> e->uses;
            e->def = findPart(pid);
            if (!e->def || ls.fail()) continue;
            e->inBox = inBox;
            e->osInstalled = os;
            e->pasted = pasted;
            e->broken = broken;
            maxId = std::max(maxId, e->id);
            world.ents.push_back(std::move(e));
        } else if (k == "oc") {
            int id, xmp, r, g, b;
            OcSettings s;
            ls >> id >> s.cpuOC >> s.vcore >> s.gpuCore >> s.gpuMem >> xmp >> r >> g >> b >> s.rgbMode;
            if (ls.fail()) continue;
            s.xmp = xmp;
            s.rgb = rgba(r, g, b);
            comp(id).oc = s;
            comp(id).ocEdit = s;
        }
    }
    world.nextId = std::max(world.nextId, maxId + 1);
    // Детали, чей «родитель» пропал, выпадают на пол
    for (auto& up : world.ents)
        if (up->parent && !world.get(up->parent)) up->parent = 0, up->slot = -1, up->pos.y += 0.3f;
    while (orders.size() < 3) orders.push_back(randomOrder());
    world.wakeAll();
    openMode(Mode::Play);
    toast("Игра загружена");
    return true;
}

void Game::loadSettings() {
    std::string data;
    if (!plat::readFile(plat::dataDir() + "pcsim2_settings.txt", data)) return;
    std::istringstream in(data);
    int fs = 0, vs = 1, fp = 0, snd = 1;
    in >> settings.sens >> settings.fov >> fs >> vs >> fp >> snd;
    if (in.fail()) {
        settings = Settings{};
        return;
    }
    settings.sens = clampf(settings.sens, 0.02f, 0.6f);
    settings.fov = clampf(settings.fov, 50, 110);
    settings.fullscreen = fs;
    settings.vsync = vs;
    settings.showFps = fp;
    settings.sound = snd;
}

void Game::saveSettings() {
    plat::writeFile(plat::dataDir() + "pcsim2_settings.txt",
                    fmt("%.3f %.1f %d %d %d %d\n", settings.sens, settings.fov, settings.fullscreen ? 1 : 0,
                        settings.vsync ? 1 : 0, settings.showFps ? 1 : 0, settings.sound ? 1 : 0));
}

// ---------------------------------------------------------------------------
// Отладка: --shots делает серию скриншотов (используется для проверки картинки)

void Game::setupShots() {
    shotFrames++;
    auto put = [&](Entity& parent, int slot, const char* id) -> Entity& {
        Entity& e = world.spawn(findPart(id), v3(0, 0, 0), false);
        e.parent = parent.id;
        e.slot = slot;
        return e;
    };
    auto look = [&](Vec3 p, Vec3 at) {
        pos = v3(p.x, 0, p.z);
        eye = p.y;
        Vec3 d = normalize(at - p);
        yaw = std::atan2(d.x, -d.z) / kDeg;
        pitch = std::asin(d.y) / kDeg;
    };
    auto next = [&](const char* name) {
        if (name) shotName = name;
        shotStep++;
        shotFrames = 0;
    };
    Computer* mc = monitorComputer();
    Entity* mb = mc ? gatherBuild(world, world.get(mc->caseId)).mb : nullptr;
    switch (shotStep) {
        case 0:
            if (shotFrames > 20) next("shot_01_menu.bmp");
            break;
        case 1: {
            newGame();
            toasts.clear();
            Entity& cs = world.spawn(findPart("case_tower"), v3(-0.45f, 0.78f, -2.62f), false);
            cs.yaw = 3;
            Entity& m = put(cs, 0, "mb_b650");
            Entity& cpu = put(m, 0, "cpu_r7_9800x3d");
            cpu.pasted = true;
            put(m, 1, "cool_tower");
            put(m, 2, "gpu_4070");
            put(m, 3, "ram_ddr5_16");
            put(m, 5, "ram_ddr5_16");
            Entity& nv = put(m, 7, "nvme_1000");
            nv.osInstalled = true;
            put(cs, 1, "psu_750");
            for (int i = 0; i < 3; i++) put(cs, 5 + i, "fan_120_rgb");
            // Вторая сборка на верстаке, в процессе
            Entity& cs2 = world.spawn(findPart("case_air"), v3(1.3f, 0.79f, -2.6f), false);
            cs2.yaw = 3;
            Entity& m2 = put(cs2, 0, "mb_b550");
            put(m2, 0, "cpu_r7_5800x3d");
            put(m2, 3, "ram_ddr4_16");
            put(cs2, 1, "psu_550");
            world.spawn(findPart("gpu_4060"), v3(1.9f, 0.8f, -2.5f), false);
            world.spawn(findPart("mb_x670p"), v3(2.15f, 0.8f, -2.75f), false);
            for (const char* id : {"case_office", "gpu_4090", "ram_ddr5_32", "psu_1000", "cool_aio"}) {
                Entity& e = world.spawn(findPart(id), v3(-3.3f + rndf() * 0.8f, 0.3f, 1.8f + rndf() * 0.7f), true);
                e.yaw = rndi(0, 3);
            }
            world.spawn(findPart("gpu_3060"), v3(-1.6f, 0.3f, 2.4f), false);
            pressPower(world, comp(cs.id), simEvents());
            look(v3(-0.2f, 1.6f, -0.6f), v3(-0.6f, 0.9f, -2.6f));
            next(nullptr);
            break;
        }
        case 2:
            if (shotFrames > 40) next("shot_02_room.bmp");
            break;
        case 3:
            look(v3(-0.3f, 1.15f, -1.95f), v3(-0.45f, 0.98f, -2.62f));
            if (shotFrames > 5) next("shot_03_case_closeup.bmp");
            break;
        case 4:
            look(v3(-1.0f, 1.35f, -1.9f), v3(-1.0f, 1.15f, -2.8f));
            if (mc && mc->st == PCState::Post && shotFrames > 3) next("shot_04_post.bmp");
            break;
        case 5:
            if (mc) {
                enterBios(*mc);
                mc->biosTab = 1;
            }
            openMode(Mode::Screen);
            if (shotFrames > 2) next("shot_05_bios_raven_oc.bmp");
            break;
        case 6:
            if (mb) mb->def = findPart("mb_b650p");
            if (mc) mc->biosTab = 2;
            if (shotFrames > 2) next("shot_06_bios_polaris.bmp");
            break;
        case 7:
            if (mb) mb->def = findPart("mb_h510");
            if (mc) mc->biosTab = 3;
            if (shotFrames > 2) next("shot_07_bios_nova_boot.bmp");
            break;
        case 8:
            if (mb) mb->def = findPart("mb_b650");
            if (mc && shotFrames == 1) rebootPC(*mc, false);
            if (mc && mc->st == PCState::Desktop) {
                mc->app = APP_BENCH;
                mc->benchRunning = true;
                next(nullptr);
            }
            break;
        case 9:
            if (shotFrames > 30) next("shot_08_desktop_bench.bmp");
            break;
        case 10:
            if (mc) mc->app = APP_OC;
            if (shotFrames > 3) next("shot_09_oc_tuner.bmp");
            break;
        case 11:
            openMode(Mode::Play);
            look(v3(-1.0f, 1.45f, -1.6f), v3(-0.9f, 1.05f, -2.75f));
            if (shotFrames > 3) next("shot_10_monitor_3d.bmp");
            break;
        case 12:
            phoneTab = 0;
            phoneCat = 5;
            openMode(Mode::Phone);
            if (shotFrames > 3) next("shot_11_phone_shop.bmp");
            break;
        case 13:
            phoneTab = 1;
            if (shotFrames > 3) next("shot_12_phone_orders.bmp");
            break;
        case 14: {
            openMode(Mode::Play);
            // Держим видеокарту и целимся в слот платы на верстаке
            if (shotFrames == 1) {
                for (auto& up : world.ents)
                    if (up->def->id == "gpu_4060") {
                        up->held = true;
                        heldId = up->id;
                    }
            }
            look(v3(1.0f, 1.25f, -1.95f), v3(1.32f, 1.03f, -2.6f));
            computeTarget();
            if (shotFrames > 4) next("shot_13_install_ghost.bmp");
            break;
        }
        case 15:
            look(v3(-2.0f, 1.6f, 0.6f), v3(-2.6f, 0.2f, 2.2f));
            if (shotFrames > 4) next("shot_14_delivery.bmp");
            break;
        default:
            quit = true;
            break;
    }
}
