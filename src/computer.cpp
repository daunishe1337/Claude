#include "computer.hpp"
#include <algorithm>

void Snake::reset() {
    body = {{6, 8}, {5, 8}, {4, 8}};
    dx = ndx = 1;
    dy = ndy = 0;
    fx = 14;
    fy = 8;
    step = 0;
    dead = false;
    score = 0;
}

float fanProfileAirflow(int p) { return p == 0 ? 0.92f : (p == 2 ? 1.1f : 1.0f); }
const char* fanProfileName(int p) { return p == 0 ? "Тихий" : (p == 2 ? "Турбо" : "Стандарт"); }

Entity* Build::bootDrive() const {
    Entity* best = nullptr;
    int bestRank = -1;
    for (Entity* d : drives) {
        if (!d->osInstalled) continue;
        int rank = d->def->stor == StorType::NVME ? 3 : (d->def->stor == StorType::SATA_SSD ? 2 : 1);
        if (rank > bestRank) best = d, bestRank = rank;
    }
    return best;
}

Build gatherBuild(World& w, Entity* cs) {
    Build b;
    b.cs = cs;
    if (!cs) return b;
    for (Entity* e : w.children(cs->id)) {
        switch (e->def->cat) {
            case Cat::Motherboard: b.mb = e; break;
            case Cat::PSU: b.psu = e; break;
            case Cat::Storage: b.drives.push_back(e); break;
            case Cat::Fan: b.fans.push_back(e); break;
            case Cat::USB: b.usb = e; break;
            default: break;
        }
    }
    if (b.mb) {
        for (Entity* e : w.children(b.mb->id)) {
            switch (e->def->cat) {
                case Cat::CPU: b.cpu = e; break;
                case Cat::Cooler: b.cooler = e; break;
                case Cat::GPU: b.gpu = e; break;
                case Cat::RAM:
                    b.ram.push_back(e);
                    b.ramGB += e->def->gb;
                    break;
                case Cat::Storage: b.drives.push_back(e); break;
                default: break;
            }
        }
    }
    return b;
}

int ramMhz(const Build& b, bool xmp) {
    int mhz = 0;
    for (Entity* r : b.ram) {
        int m = xmp ? r->def->xmpMhz : r->def->mhz;
        mhz = mhz == 0 ? m : std::min(mhz, m);
    }
    return mhz;
}

Perf estimatePerf(const Build& b, const OcSettings& oc) {
    Perf p;
    if (!b.cpu || b.ram.empty()) return p;
    RamType rt = b.ram[0]->def->ram;
    float typeF = rt == RamType::DDR3 ? 0.8f : (rt == RamType::DDR5 ? 1.12f : 1.0f);
    float capF = b.ramGB < 8 ? 0.7f : (b.ramGB < 16 ? 0.9f : (b.ramGB < 32 ? 1.0f : 1.03f));
    float chanF = b.ram.size() >= 2 ? 1.0f : 0.9f;
    p.ramF = typeF * capF * chanF * (oc.xmp ? 1.06f : 1.0f);
    bool canOC = b.mb && b.mb->def->oc;
    int cpuOC = canOC ? oc.cpuOC : 0;
    p.cpuEff = b.cpu->def->cpuPerf * (1 + 0.025f * cpuOC) * (b.cpu->def->x3d ? 1.15f : 1.0f);
    if (b.gpuWorks())
        p.gpuEff = b.gpu->def->gpuPerf * (1 + oc.gpuCore / 1500.0f + oc.gpuMem / 6000.0f);
    else
        p.gpuEff = b.cpu->def->igpu;
    p.score = (int)((p.gpuEff * 140 + p.cpuEff * 35) * p.ramF);
    return p;
}

float estimatePower(const Build& b, const OcSettings& oc, float cpuLoad, float gpuLoad, float* cpuPOut,
                    float* gpuPOut) {
    float cpuP = 0, gpuP = 0;
    if (b.cpu) {
        bool canOC = b.mb && b.mb->def->oc;
        float v = canOC ? oc.vcore / 1.2f : 1.0f;
        cpuP = b.cpu->def->tdp * (0.15f + 0.85f * cpuLoad) * (1 + 0.03f * (canOC ? oc.cpuOC : 0)) * v * v;
    }
    if (b.gpuWorks()) gpuP = b.gpu->def->tdp * (0.1f + 0.9f * gpuLoad) * (1 + oc.gpuCore / 1000.0f);
    else if (b.gpu) gpuP = 15;   // неподдерживаемая карта просто греется в простое
    if (cpuPOut) *cpuPOut = cpuP;
    if (gpuPOut) *gpuPOut = gpuP;
    return cpuP + gpuP + 35 + 4.0f * b.fans.size() + 5.0f * b.drives.size();
}

float requiredVcore(const Build& b, int cpuOC) {
    float q = b.cpu ? b.cpu->quality : 0.5f;
    return 1.15f + 0.025f * cpuOC - (q - 0.5f) * 0.1f;
}

bool cpuStable(const Build& b, const OcSettings& oc) {
    if (!b.cpu || !b.mb || !b.mb->def->oc || oc.cpuOC <= 0) return true;
    return oc.vcore + 1e-4f >= requiredVcore(b, oc.cpuOC);
}

bool gpuStable(const Build& b, const OcSettings& oc) {
    if (!b.gpuWorks()) return true;
    float q = b.gpu->quality;
    return oc.gpuCore <= 100 + q * 250 && oc.gpuMem <= 400 + q * 800;
}

std::vector<std::string> buildProblems(const Build& b) {
    std::vector<std::string> p;
    if (!b.psu) p.push_back("нет блока питания");
    if (!b.mb) {
        p.push_back("нет материнской платы");
        return p;
    }
    if (!b.cpu) p.push_back("нет процессора");
    else if (b.cpu->broken) p.push_back("процессор сгорел");
    if (!b.cooler) p.push_back("нет кулера процессора");
    if (b.ram.empty()) p.push_back("нет оперативной памяти");
    if (b.cpu && !b.hasVideo())
        p.push_back(b.gpu ? "видеокарта NOT SUPPORTED и нет встроенной графики" : "нет видеокарты и встроенной графики");
    if (!b.bootDrive()) p.push_back("нет накопителя с установленной NovaOS");
    return p;
}

float bootTime(const Entity* d) {
    if (!d) return 3;
    switch (d->def->stor) {
        case StorType::NVME: return 2.5f;
        case StorType::SATA_SSD: return 4.0f;
        default: return 8.0f;
    }
}

static void enter(Computer& c, PCState s) {
    c.st = s;
    c.t = 0;
}

void shutdownPC(Computer& c) {
    enter(c, PCState::Off);
    c.app = APP_NONE;
    c.benchRunning = false;
    c.mining = false;
    c.crashTimer = -1;
    c.burnTimer = c.overloadTimer = c.hotTimer = 0;
    c.cpuLoad = c.gpuLoad = 0;
    c.msg.clear();
}

void enterBios(Computer& c) {
    enter(c, PCState::Bios);
    c.biosEdit = c.oc;
    c.biosTab = 0;
    c.biosDirty = false;
    c.rebootToBios = false;
}

void rebootPC(Computer& c, bool toBios) {
    c.app = APP_NONE;
    c.benchRunning = false;
    c.mining = false;
    c.crashTimer = -1;
    c.rebootToBios = toBios;
    enter(c, PCState::Post);
}

void pressPower(World& w, Computer& c, const SimEvents& ev) {
    if (c.on()) {
        shutdownPC(c);
        return;
    }
    Build b = gatherBuild(w, w.get(c.caseId));
    if (!b.psu) {
        ev.toast("ПК не включается: нет блока питания", col::red);
        return;
    }
    if (!b.mb) {
        ev.toast("ПК не включается: нет материнской платы", col::red);
        return;
    }
    c.cpuTemp = c.gpuTemp = 26;
    c.cpuHist.clear();
    c.gpuHist.clear();
    c.app = APP_NONE;
    c.msg.clear();
    if (ev.beep) ev.beep(880, 0.12f);
    auto noVideo = [&](const std::string& why) {
        enter(c, PCState::NoVideo);
        c.msg = why;
        ev.toast("Монитор: нет сигнала. Похоже, " + why, col::orange);
        if (ev.beep) ev.beep(300, 0.6f);
    };
    if (!b.cpu) return noVideo("нет процессора");
    if (b.cpu->broken) return noVideo("процессор неисправен (сгорел)");
    if (b.ram.empty()) return noVideo("нет оперативной памяти");
    if (!b.hasVideo()) {
        if (b.gpu)
            return noVideo("видеокарта " + b.gpu->def->name + " — NOT SUPPORTED, а у процессора нет встроенной графики");
        return noVideo("нет видеокарты, а у процессора нет встроенной графики");
    }
    c.rebootToBios = false;
    enter(c, PCState::Post);
}

void updateComputer(World& w, Computer& c, float dt, const SimEvents& ev) {
    Entity* cs = w.get(c.caseId);
    if (!cs) return;
    Build b = gatherBuild(w, cs);
    // Если во время работы пропало питание/плата (например, корпус разобрали) — выключаемся
    if (c.on() && (!b.psu || !b.mb)) {
        shutdownPC(c);
        return;
    }
    c.t += dt;

    // --- Нагрузка ---
    float cpuL = 0.04f, gpuL = 0.03f;
    switch (c.st) {
        case PCState::Off: cpuL = gpuL = 0; break;
        case PCState::Bios: cpuL = 0.08f; gpuL = 0.04f; break;
        case PCState::Post:
        case PCState::Booting:
        case PCState::Installing: cpuL = 0.35f; gpuL = 0.05f; break;
        default: break;
    }
    if (c.st == PCState::Desktop) {
        if (c.benchRunning) cpuL = gpuL = 1.0f;
        else if (c.mining) cpuL = 0.15f, gpuL = 1.0f;
        else if (c.app == APP_SNAKE) cpuL = 0.1f, gpuL = 0.12f;
    }
    c.cpuLoad = approach(c.cpuLoad, cpuL, 4, dt);
    c.gpuLoad = approach(c.gpuLoad, gpuL, 4, dt);

    // --- Мощность и температуры ---
    float cpuP = 0, gpuP = 0;
    c.power = c.on() ? estimatePower(b, c.oc, c.cpuLoad, c.gpuLoad, &cpuP, &gpuP) : 0;
    float airflow = b.fans.empty() ? 0.85f : (b.fans.size() == 1 ? 1.0f : (b.fans.size() == 2 ? 1.07f : 1.12f));
    airflow *= fanProfileAirflow(c.oc.fanProfile);
    if (c.on() && b.cpu) {
        float cap = b.cooler ? (float)b.cooler->def->coolW : 12.0f;
        if (b.cooler && !b.cpu->pasted) cap *= 0.55f;   // без термопасты охлаждение хуже почти вдвое
        float target = 24 + 62 * cpuP / (cap * airflow);
        c.cpuTemp = approach(c.cpuTemp, target, b.cooler ? 0.45f : 0.9f, dt);
    } else {
        c.cpuTemp = approach(c.cpuTemp, 24, 0.2f, dt);
    }
    if (c.on() && b.gpu) {
        float target = 24 + 55 * gpuP / (b.gpu->def->tdp * 1.05f * airflow);
        c.gpuTemp = approach(c.gpuTemp, target, 0.4f, dt);
    } else {
        c.gpuTemp = approach(c.gpuTemp, 24, 0.2f, dt);
    }
    if (!c.on()) return;

    c.histT += dt;
    if (c.histT > 0.5f) {
        c.histT = 0;
        c.cpuHist.push_back(c.cpuTemp);
        c.gpuHist.push_back(c.gpuTemp);
        while (c.cpuHist.size() > 120) c.cpuHist.pop_front();
        while (c.gpuHist.size() > 120) c.gpuHist.pop_front();
    }

    auto fail = [&](const std::string& why) {
        shutdownPC(c);
        ev.toast(why, col::red);
        if (ev.beep) ev.beep(200, 0.5f);
    };

    // --- Защиты ---
    if (b.psu && c.power > b.psu->def->watts) {
        c.overloadTimer += dt;
        if (c.overloadTimer > 1.2f)
            return fail(fmt("Блок питания %d Вт не выдержал нагрузку %.0f Вт — ПК выключился!", b.psu->def->watts,
                            c.power));
    } else {
        c.overloadTimer = 0;
    }
    if (c.cpuTemp > 100) return fail("Перегрев процессора (100°C) — аварийное выключение! Проверьте кулер и термопасту.");
    if (c.gpuTemp > 95) return fail("Перегрев видеокарты — аварийное выключение!");
    bool canOC = b.mb && b.mb->def->oc;
    if (b.cpu && canOC && c.oc.vcore >= 1.45f && c.cpuTemp > 92) {
        c.burnTimer += dt;
        if (c.burnTimer > 4) {
            b.cpu->broken = true;
            return fail("Процессор сгорел от высокого напряжения и перегрева! Его можно только продать за копейки.");
        }
    } else {
        c.burnTimer = 0;
    }

    // --- Стабильность разгона ---
    bool stable = cpuStable(b, c.oc) && gpuStable(b, c.oc);
    if (c.st == PCState::Desktop && !stable && (c.cpuLoad > 0.5f || c.gpuLoad > 0.5f)) {
        if (c.crashTimer < 0) c.crashTimer = 1.5f + rndf() * 4.0f;
        c.crashTimer -= dt;
        if (c.crashTimer <= 0) {
            enter(c, PCState::Crash);
            c.crashReason = !cpuStable(b, c.oc) ? "CPU_VOLTAGE_TOO_LOW" : "GPU_CLOCK_UNSTABLE";
            c.benchRunning = false;
            c.mining = false;
            c.app = APP_NONE;
            c.crashTimer = -1;
            if (ev.beep) ev.beep(160, 0.4f);
        }
    } else {
        c.crashTimer = -1;
    }

    // --- Состояния загрузки ---
    switch (c.st) {
        case PCState::Post:
            if (c.rebootToBios && c.t > 1.0f) {
                enterBios(c);
                break;
            }
            if (c.t > 3.5f) {
                if (c.oc.bootUsbFirst && b.usb && !b.drives.empty()) {
                    enter(c, PCState::Installer);
                    c.installDrive = 0;
                } else if (b.bootDrive()) {
                    enter(c, PCState::Booting);
                } else if (b.usb) {
                    enter(c, PCState::Installer);
                    c.installDrive = 0;
                } else {
                    enter(c, PCState::NoBoot);
                    c.msg = b.drives.empty() ? "Накопитель не найден. Установите SSD или HDD."
                                             : "Нет загрузочного устройства. Вставьте флешку с NovaOS в USB-порт.";
                }
            }
            break;
        case PCState::NoBoot:
            // Как только вставили флешку — предлагаем установку
            if (b.usb && !b.drives.empty() && c.t > 0.5f) enter(c, PCState::Installer);
            break;
        case PCState::Installing: {
            Entity* d = w.get(c.installDrive);
            if (!b.usb || !d || std::find(b.drives.begin(), b.drives.end(), d) == b.drives.end()) {
                enter(c, PCState::NoBoot);
                c.msg = "Установка прервана: флешку или накопитель извлекли.";
                break;
            }
            float total = d->def->stor == StorType::HDD ? 14.0f : (d->def->stor == StorType::SATA_SSD ? 8.0f : 6.0f);
            c.installProgress = std::min(1.0f, c.t / total);
            if (c.t >= total) {
                d->osInstalled = true;
                ev.toast("NovaOS установлена! Флешку можно извлечь.", col::green);
                enter(c, PCState::Booting);
            }
            break;
        }
        case PCState::Booting: {
            Entity* d = b.bootDrive();
            if (!d) {
                enter(c, PCState::NoBoot);
                c.msg = "Нет загрузочного устройства.";
                break;
            }
            // Сильно недовольный разгоном процессор падает прямо при загрузке
            if (!cpuStable(b, c.oc) && c.oc.vcore < requiredVcore(b, c.oc.cpuOC) - 0.06f && c.t > 1.0f) {
                enter(c, PCState::Crash);
                c.crashReason = "BOOT_FAILURE_OVERCLOCK";
                break;
            }
            if (c.t >= bootTime(d)) {
                enter(c, PCState::Desktop);
                c.ocEdit = c.oc;
                if (ev.beep) ev.beep(660, 0.1f);
            }
            break;
        }
        case PCState::Desktop: {
            if (c.benchRunning) {
                c.benchT += dt;
                Perf p = estimatePerf(b, c.oc);
                float throttle = (c.cpuTemp > 95 || c.gpuTemp > 88) ? 0.85f : 1.0f;
                c.benchFps = approach(c.benchFps, p.score / 100.0f * throttle * (0.95f + 0.1f * rndf()), 3, dt);
                if (c.benchT >= 12.0f) {
                    c.benchRunning = false;
                    c.lastScore = (int)(p.score * throttle * (0.97f + 0.06f * rndf()));
                    if (ev.benchDone) ev.benchDone(c.lastScore);
                }
            }
            if (c.mining) {
                if (!b.gpuWorks()) {
                    c.mining = false;
                } else {
                    Perf p = estimatePerf(b, c.oc);
                    double inc = p.gpuEff * 0.004 * dt;
                    c.minedSession += inc;
                    ev.earn(inc);
                }
            }
            if (c.app == APP_SNAKE) {
                Snake& s = c.snake;
                if (!s.dead) {
                    s.step += dt;
                    float speed = std::max(0.06f, 0.14f - s.score * 0.003f);
                    while (s.step > speed && !s.dead) {
                        s.step -= speed;
                        if (!(s.ndx == -s.dx && s.ndy == -s.dy)) s.dx = s.ndx, s.dy = s.ndy;
                        auto head = s.body.front();
                        head.first += s.dx;
                        head.second += s.dy;
                        if (head.first < 0 || head.second < 0 || head.first >= 28 || head.second >= 16) {
                            s.dead = true;
                            break;
                        }
                        for (auto& p : s.body)
                            if (p == head) s.dead = true;
                        if (s.dead) break;
                        s.body.insert(s.body.begin(), head);
                        if (head.first == s.fx && head.second == s.fy) {
                            s.score++;
                            ev.earn(1.0);
                            for (int tries = 0; tries < 50; tries++) {
                                s.fx = rndi(0, 27);
                                s.fy = rndi(0, 15);
                                bool onBody = false;
                                for (auto& p : s.body) onBody |= p.first == s.fx && p.second == s.fy;
                                if (!onBody) break;
                            }
                        } else {
                            s.body.pop_back();
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}
