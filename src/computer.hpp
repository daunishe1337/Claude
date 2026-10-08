// Симуляция собранного ПК: проверка сборки, загрузка, температуры, разгон, бенчмарк
#pragma once
#include "core.hpp"
#include "world.hpp"
#include <deque>
#include <functional>
#include <string>
#include <vector>

struct Build {
    Entity* cs = nullptr;
    Entity* mb = nullptr;
    Entity* cpu = nullptr;
    Entity* cooler = nullptr;
    Entity* gpu = nullptr;
    Entity* psu = nullptr;
    Entity* usb = nullptr;
    std::vector<Entity*> ram, drives, fans;
    int ramGB = 0;
    Entity* bootDrive() const;       // накопитель с ОС (NVMe > SSD > HDD)
    bool gpuWorks() const { return gpu && gpu->def->supported; }
    bool hasVideo() const { return gpuWorks() || (cpu && cpu->def->igpu > 0); }
};

Build gatherBuild(World& w, Entity* cs);

enum class PCState { Off, NoVideo, Post, Bios, NoBoot, Installer, Installing, Booting, Desktop, Crash };

enum OsApp { APP_NONE = -1, APP_BENCH, APP_SENSORS, APP_OC, APP_MINER, APP_RGB, APP_SHOP, APP_SNAKE, APP_INFO, APP_COUNT };

struct OcSettings {
    int cpuOC = 0;        // +100 МГц шагов
    float vcore = 1.20f;
    int gpuCore = 0;      // +МГц
    int gpuMem = 0;
    bool xmp = false;
    Color rgb = rgba(0, 200, 255);
    int rgbMode = 1;      // 0 — статичный, 1 — радуга, 2 — дыхание, 3 — выкл.
    int fanProfile = 1;   // BIOS: 0 — тихий, 1 — стандарт, 2 — турбо
    bool bootUsbFirst = false;  // BIOS: первым загрузочным устройством стоит USB
};
float fanProfileAirflow(int profile);
const char* fanProfileName(int profile);

struct Snake {
    std::vector<std::pair<int, int>> body;
    int dx = 1, dy = 0, ndx = 1, ndy = 0;
    int fx = 10, fy = 8;
    float step = 0;
    bool dead = false;
    int score = 0;
    void reset();
};

struct Computer {
    int caseId = 0;
    PCState st = PCState::Off;
    float t = 0;
    std::string msg;
    float cpuTemp = 24, gpuTemp = 24, cpuLoad = 0, gpuLoad = 0, power = 0;
    OcSettings oc;
    OcSettings ocEdit;    // редактируемая копия в OC Tuner (видеокарта)
    OcSettings biosEdit;  // редактируемая копия в BIOS (процессор, память, вентиляторы, загрузка)
    int biosTab = 0;
    bool biosDirty = false;
    bool rebootToBios = false;
    bool startMenu = false;
    // ОС
    int app = APP_NONE;
    bool benchRunning = false;
    float benchT = 0;
    float benchFps = 0;
    int lastScore = 0;
    bool mining = false;
    double minedSession = 0;
    float crashTimer = -1, burnTimer = 0, overloadTimer = 0, hotTimer = 0;
    int installDrive = 0;
    float installProgress = 0;
    std::deque<float> cpuHist, gpuHist;
    float histT = 0;
    Snake snake;
    int shopCat = 0, shopScroll = 0;
    std::string crashReason;

    bool on() const { return st != PCState::Off; }
    bool hasScreen() const { return st != PCState::Off && st != PCState::NoVideo; }
};

// События, которые симуляция сообщает игре
struct SimEvents {
    std::function<void(const std::string&, Color)> toast;
    std::function<void(double)> earn;
    std::function<void(int)> benchDone;
    std::function<void(float, float)> beep;
};

struct Perf {
    float cpuEff = 0, gpuEff = 0, ramF = 1;
    int score = 0;
};
Perf estimatePerf(const Build& b, const OcSettings& oc);
float estimatePower(const Build& b, const OcSettings& oc, float cpuLoad, float gpuLoad, float* cpuP = nullptr,
                    float* gpuP = nullptr);
bool cpuStable(const Build& b, const OcSettings& oc);
bool gpuStable(const Build& b, const OcSettings& oc);
float requiredVcore(const Build& b, int cpuOC);
std::vector<std::string> buildProblems(const Build& b);   // почему не загрузится (пусто — всё ок)
int ramMhz(const Build& b, bool xmp);

void pressPower(World& w, Computer& c, const SimEvents& ev);
void shutdownPC(Computer& c);
void enterBios(Computer& c);
void rebootPC(Computer& c, bool toBios);
void updateComputer(World& w, Computer& c, float dt, const SimEvents& ev);
float bootTime(const Entity* drive);
