// Каталог комплектующих PC Simulator 2
#pragma once
#include "core.hpp"
#include <string>
#include <vector>

enum class Cat { Case, Motherboard, CPU, Cooler, RAM, GPU, PSU, Storage, Fan, Paste, USB, Count };
enum class RamType { None, DDR3, DDR4, DDR5 };
enum class StorType { None, HDD, SATA_SSD, NVME };

struct PartDef {
    std::string id;
    std::string name;
    Cat cat = Cat::Case;
    int price = 0;
    Color color = rgba(130, 130, 130);

    // CPU
    std::string socket;
    int cores = 0, threads = 0;
    float ghz = 0;
    int tdp = 0;          // CPU и GPU
    float cpuPerf = 0;
    float igpu = 0;       // производительность встроенной графики (0 — нет)
    bool x3d = false;

    // Материнская плата
    std::string chipset;
    RamType ram = RamType::None;   // также тип модуля ОЗУ
    int ramSlots = 0;
    int m2Slots = 0;
    bool oc = false;
    std::string brand;   // производитель платы (вымышленный)
    int biosStyle = 0;   // 0 — NovaBIOS (без бренда), 1 — Raven, 2 — Polaris

    // ОЗУ
    int gb = 0;
    int mhz = 0;
    int xmpMhz = 0;

    // Видеокарта
    int series = 0;
    float gpuPerf = 0;
    int vram = 0;
    bool supported = true;

    // БП
    int watts = 0;

    // Кулер: 0 — боксовый, 1 — башня, 2 — двойная башня, 3 — СЖО
    int coolW = 0;
    int coolerKind = 0;

    // Накопитель
    StorType stor = StorType::None;
    int capGB = 0;

    // Корпус
    int fanSlots = 0;
    bool glass = false;
    bool rgb = false;

    // Вентилятор
    bool fanRgb = false;

    // Термопаста
    int uses = 0;
};

const std::vector<PartDef>& catalog();
const PartDef* findPart(const std::string& id);
const char* catName(Cat c);
const char* ramName(RamType r);
const char* storName(StorType s);
std::string partSpecs(const PartDef& d);   // короткое описание характеристик
