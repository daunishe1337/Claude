#include "parts.hpp"
#include <cstdio>

namespace {

PartDef mk(const char* id, const char* name, Cat cat, int price, Color col) {
    PartDef d;
    d.id = id;
    d.name = name;
    d.cat = cat;
    d.price = price;
    d.color = col;
    return d;
}

PartDef pcCase(const char* id, const char* name, int price, Color col, int fans, bool glass, bool rgb) {
    PartDef d = mk(id, name, Cat::Case, price, col);
    d.fanSlots = fans;
    d.glass = glass;
    d.rgb = rgb;
    return d;
}

PartDef mobo(const char* id, const char* name, int price, const char* socket, const char* chipset,
             RamType ram, int ramSlots, int m2, bool oc, Color col, int bios) {
    PartDef d = mk(id, name, Cat::Motherboard, price, col);
    d.biosStyle = bios;
    d.brand = bios == 1 ? "Raven" : (bios == 2 ? "Polaris" : "");
    d.socket = socket;
    d.chipset = chipset;
    d.ram = ram;
    d.ramSlots = ramSlots;
    d.m2Slots = m2;
    d.oc = oc;
    return d;
}

PartDef cpu(const char* id, const char* name, int price, const char* socket, int cores, int threads,
            float ghz, int tdp, float perf, float igpu, bool x3d) {
    PartDef d = mk(id, name, Cat::CPU, price, Color{170, 170, 175, 255});
    d.socket = socket;
    d.cores = cores;
    d.threads = threads;
    d.ghz = ghz;
    d.tdp = tdp;
    d.cpuPerf = perf;
    d.igpu = igpu;
    d.x3d = x3d;
    return d;
}

PartDef ram(const char* id, const char* name, int price, RamType t, int gb, int mhz, int xmp, Color col) {
    PartDef d = mk(id, name, Cat::RAM, price, col);
    d.ram = t;
    d.gb = gb;
    d.mhz = mhz;
    d.xmpMhz = xmp;
    return d;
}

PartDef gpu(const char* id, const char* name, int price, int series, float perf, int tdp, int vram,
            bool supported, Color col) {
    PartDef d = mk(id, name, Cat::GPU, price, col);
    d.series = series;
    d.gpuPerf = perf;
    d.tdp = tdp;
    d.vram = vram;
    d.supported = supported;
    return d;
}

PartDef psu(const char* id, const char* name, int price, int watts) {
    PartDef d = mk(id, name, Cat::PSU, price, Color{40, 40, 44, 255});
    d.watts = watts;
    return d;
}

PartDef cooler(const char* id, const char* name, int price, int kind, int w, Color col) {
    PartDef d = mk(id, name, Cat::Cooler, price, col);
    d.coolerKind = kind;
    d.coolW = w;
    return d;
}

PartDef storage(const char* id, const char* name, int price, StorType t, int cap, Color col) {
    PartDef d = mk(id, name, Cat::Storage, price, col);
    d.stor = t;
    d.capGB = cap;
    return d;
}

std::vector<PartDef> build() {
    std::vector<PartDef> v;
    const Color black{30, 30, 34, 255};

    // Корпуса
    v.push_back(pcCase("case_office", "Корпус Office Basic", 40, Color{150, 152, 158, 255}, 1, false, false));
    v.push_back(pcCase("case_air", "Корпус AirFlow M", 80, Color{36, 36, 40, 255}, 3, true, false));
    v.push_back(pcCase("case_tower", "Корпус Glass Tower X (RGB)", 150, Color{225, 225, 230, 255}, 3, true, true));

    // Материнские платы: Raven (красный «игровой» BIOS), Polaris (синий BIOS), без бренда (NovaBIOS)
    const Color pcbGreen{30, 90, 50, 255};
    const Color pcbBlack{28, 30, 34, 255};
    const Color pcbBlue{25, 45, 90, 255};
    const Color pcbRaven{34, 22, 24, 255};
    const Color pcbPolaris{22, 30, 48, 255};
    v.push_back(mobo("mb_h61", "Мат. плата H61M (LGA1155)", 35, "LGA1155", "H61", RamType::DDR3, 2, 0, false, pcbGreen, 0));
    v.push_back(mobo("mb_h510", "Мат. плата H510M (LGA1200)", 70, "LGA1200", "H510", RamType::DDR4, 2, 1, false, pcbBlue, 0));
    v.push_back(mobo("mb_h610", "Polaris H610M-E DDR4 (LGA1700)", 85, "LGA1700", "H610", RamType::DDR4, 2, 1, false, pcbPolaris, 2));
    v.push_back(mobo("mb_b760", "Polaris B760M Frost DDR5 (LGA1700)", 140, "LGA1700", "B760", RamType::DDR5, 4, 2, false, pcbPolaris, 2));
    v.push_back(mobo("mb_z790", "Raven Z790 Blade (LGA1700)", 230, "LGA1700", "Z790", RamType::DDR5, 4, 2, true, pcbRaven, 1));
    v.push_back(mobo("mb_a520", "Polaris A520M-K (AM4)", 65, "AM4", "A520", RamType::DDR4, 2, 1, false, pcbPolaris, 2));
    v.push_back(mobo("mb_b550", "Raven B550 Strike (AM4)", 120, "AM4", "B550", RamType::DDR4, 4, 2, true, pcbRaven, 1));
    v.push_back(mobo("mb_b650p", "Polaris B650 Steel (AM5)", 160, "AM5", "B650", RamType::DDR5, 4, 2, true, pcbPolaris, 2));
    v.push_back(mobo("mb_b650", "Raven B650 Strike WiFi (AM5)", 170, "AM5", "B650", RamType::DDR5, 4, 2, true, pcbBlack, 1));
    v.push_back(mobo("mb_x670p", "Polaris X670E Titan (AM5)", 330, "AM5", "X670E", RamType::DDR5, 4, 2, true, pcbPolaris, 2));
    v.push_back(mobo("mb_x670", "Raven X670E Inferno (AM5)", 350, "AM5", "X670E", RamType::DDR5, 4, 2, true, Color{60, 20, 24, 255}, 1));

    // Процессоры
    v.push_back(cpu("cpu_g2020", "Intel Pentium G2020", 15, "LGA1155", 2, 2, 2.9f, 55, 8, 3, false));
    v.push_back(cpu("cpu_i5_3470", "Intel Core i5-3470", 25, "LGA1155", 4, 4, 3.6f, 77, 14, 4, false));
    v.push_back(cpu("cpu_g6405", "Intel Pentium Gold G6405", 55, "LGA1200", 2, 4, 4.1f, 58, 16, 5, false));
    v.push_back(cpu("cpu_g7400", "Intel Pentium Gold G7400", 70, "LGA1700", 2, 4, 3.7f, 46, 20, 6, false));
    v.push_back(cpu("cpu_i5_12400f", "Intel Core i5-12400F", 140, "LGA1700", 6, 12, 4.4f, 65, 45, 0, false));
    v.push_back(cpu("cpu_i7_14700k", "Intel Core i7-14700K", 380, "LGA1700", 20, 28, 5.6f, 125, 85, 8, false));
    v.push_back(cpu("cpu_r5_5600", "AMD Ryzen 5 5600", 120, "AM4", 6, 12, 4.4f, 65, 42, 0, false));
    v.push_back(cpu("cpu_r7_5800x3d", "AMD Ryzen 7 5800X3D", 280, "AM4", 8, 16, 4.5f, 105, 58, 0, true));
    v.push_back(cpu("cpu_r5_7600", "AMD Ryzen 5 7600", 190, "AM5", 6, 12, 5.1f, 65, 55, 4, false));
    v.push_back(cpu("cpu_r7_7800x3d", "AMD Ryzen 7 7800X3D", 500, "AM5", 8, 16, 5.0f, 120, 80, 4, true));
    v.push_back(cpu("cpu_r7_9800x3d", "AMD Ryzen 7 9800X3D", 550, "AM5", 8, 16, 5.2f, 120, 92, 4, true));
    v.push_back(cpu("cpu_r9_9800x3d", "AMD Ryzen 9 9800X3D", 449, "AM5", 16, 32, 5.7f, 170, 105, 4, true));

    // Кулеры
    v.push_back(cooler("cool_box", "Кулер боксовый Stock 65", 15, 0, 70, Color{60, 60, 64, 255}));
    v.push_back(cooler("cool_tower", "Кулер башня Frost 120", 35, 1, 160, Color{185, 190, 198, 255}));
    v.push_back(cooler("cool_twin", "Кулер Frost Twin 240", 70, 2, 230, black));
    v.push_back(cooler("cool_aio", "СЖО AquaCool 360", 140, 3, 320, black));

    // ОЗУ
    const Color ddr3{40, 110, 60, 255};
    const Color ddr4{35, 35, 40, 255};
    const Color ddr5{70, 70, 78, 255};
    v.push_back(ram("ram_ddr3_4", "DDR3 4 ГБ 1600 МГц", 10, RamType::DDR3, 4, 1333, 1600, ddr3));
    v.push_back(ram("ram_ddr3_8", "DDR3 8 ГБ 1600 МГц", 20, RamType::DDR3, 8, 1333, 1600, ddr3));
    v.push_back(ram("ram_ddr4_8", "DDR4 8 ГБ 3200 МГц", 50, RamType::DDR4, 8, 2133, 3200, ddr4));
    v.push_back(ram("ram_ddr4_16", "DDR4 16 ГБ 3200 МГц", 150, RamType::DDR4, 16, 2133, 3200, ddr4));
    v.push_back(ram("ram_ddr4_32", "DDR4 32 ГБ 3200 МГц", 200, RamType::DDR4, 32, 2133, 3200, ddr4));
    v.push_back(ram("ram_ddr4_64", "DDR4 64 ГБ 3200 МГц", 250, RamType::DDR4, 64, 2133, 3200, ddr4));
    v.push_back(ram("ram_ddr5_8", "DDR5 8 ГБ 6000 МГц", 200, RamType::DDR5, 8, 4800, 6000, ddr5));
    v.push_back(ram("ram_ddr5_16", "DDR5 16 ГБ 6000 МГц", 400, RamType::DDR5, 16, 4800, 6000, ddr5));
    v.push_back(ram("ram_ddr5_32", "DDR5 32 ГБ 6000 МГц", 1000, RamType::DDR5, 32, 4800, 6000, ddr5));
    v.push_back(ram("ram_ddr5_64", "DDR5 64 ГБ 6000 МГц", 2000, RamType::DDR5, 64, 4800, 6000, ddr5));

    // Видеокарты: всё ниже 40-й серии — NOT SUPPORTED
    const Color gOld{45, 45, 50, 255};
    v.push_back(gpu("gpu_750ti", "GeForce GTX 750 Ti", 40, 7, 6, 60, 2, false, gOld));
    v.push_back(gpu("gpu_1050ti", "GeForce GTX 1050 Ti", 60, 10, 9, 75, 4, false, gOld));
    v.push_back(gpu("gpu_1660s", "GeForce GTX 1660 Super", 90, 16, 16, 125, 6, false, gOld));
    v.push_back(gpu("gpu_2060", "GeForce RTX 2060", 120, 20, 20, 160, 6, false, gOld));
    v.push_back(gpu("gpu_3060", "GeForce RTX 3060", 160, 30, 27, 170, 12, false, gOld));
    v.push_back(gpu("gpu_3080", "GeForce RTX 3080", 300, 30, 45, 320, 10, false, gOld));
    const Color g40{30, 30, 34, 255};
    v.push_back(gpu("gpu_4040", "GeForce RTX 4040", 200, 40, 28, 90, 6, true, g40));
    v.push_back(gpu("gpu_4050", "GeForce RTX 4050", 250, 40, 38, 115, 6, true, g40));
    v.push_back(gpu("gpu_4060", "GeForce RTX 4060", 300, 40, 52, 115, 8, true, g40));
    v.push_back(gpu("gpu_4070", "GeForce RTX 4070", 500, 40, 80, 200, 12, true, g40));
    v.push_back(gpu("gpu_4080", "GeForce RTX 4080", 999, 40, 115, 320, 16, true, g40));
    v.push_back(gpu("gpu_4090", "GeForce RTX 4090", 1299, 40, 150, 450, 24, true, Color{20, 20, 22, 255}));

    // Блоки питания
    v.push_back(psu("psu_400", "Блок питания 400 Вт", 35, 400));
    v.push_back(psu("psu_550", "Блок питания 550 Вт Bronze", 55, 550));
    v.push_back(psu("psu_750", "Блок питания 750 Вт Gold", 90, 750));
    v.push_back(psu("psu_1000", "Блок питания 1000 Вт Gold", 160, 1000));
    v.push_back(psu("psu_1300", "Блок питания 1300 Вт Platinum", 250, 1300));

    // Накопители
    v.push_back(storage("hdd_500", "HDD 500 ГБ (SATA)", 25, StorType::HDD, 500, Color{120, 124, 130, 255}));
    v.push_back(storage("hdd_1000", "HDD 1 ТБ (SATA)", 40, StorType::HDD, 1000, Color{120, 124, 130, 255}));
    v.push_back(storage("ssd_512", "SSD 512 ГБ (SATA)", 45, StorType::SATA_SSD, 512, Color{40, 40, 46, 255}));
    v.push_back(storage("nvme_1000", "NVMe SSD 1 ТБ (M.2)", 70, StorType::NVME, 1000, Color{25, 25, 30, 255}));
    v.push_back(storage("nvme_2000", "NVMe SSD 2 ТБ (M.2)", 130, StorType::NVME, 2000, Color{25, 25, 30, 255}));

    // Вентиляторы
    {
        PartDef f = mk("fan_120", "Вентилятор 120 мм", Cat::Fan, 8, Color{30, 30, 34, 255});
        v.push_back(f);
        PartDef r = mk("fan_120_rgb", "RGB-вентилятор 120 мм", Cat::Fan, 15, Color{230, 230, 235, 255});
        r.fanRgb = true;
        v.push_back(r);
    }

    // Расходники
    {
        PartDef p = mk("paste", "Термопаста (5 нанесений)", Cat::Paste, 5, Color{200, 200, 205, 255});
        p.uses = 5;
        v.push_back(p);
        PartDef u = mk("usb_os", "Флешка с NovaOS", Cat::USB, 20, Color{220, 60, 50, 255});
        v.push_back(u);
    }
    return v;
}

}  // namespace

const std::vector<PartDef>& catalog() {
    static std::vector<PartDef> c = build();
    return c;
}

const PartDef* findPart(const std::string& id) {
    for (auto& d : catalog())
        if (d.id == id) return &d;
    return nullptr;
}

const char* catName(Cat c) {
    switch (c) {
        case Cat::Case: return "Корпуса";
        case Cat::Motherboard: return "Мат. платы";
        case Cat::CPU: return "Процессоры";
        case Cat::Cooler: return "Охлаждение";
        case Cat::RAM: return "Оперативная память";
        case Cat::GPU: return "Видеокарты";
        case Cat::PSU: return "Блоки питания";
        case Cat::Storage: return "Накопители";
        case Cat::Fan: return "Вентиляторы";
        case Cat::Paste: return "Термопаста";
        case Cat::USB: return "Флешки";
        default: return "?";
    }
}

const char* ramName(RamType r) {
    switch (r) {
        case RamType::DDR3: return "DDR3";
        case RamType::DDR4: return "DDR4";
        case RamType::DDR5: return "DDR5";
        default: return "-";
    }
}

const char* storName(StorType s) {
    switch (s) {
        case StorType::HDD: return "HDD";
        case StorType::SATA_SSD: return "SATA SSD";
        case StorType::NVME: return "NVMe M.2";
        default: return "-";
    }
}

std::string partSpecs(const PartDef& d) {
    char b[256];
    switch (d.cat) {
        case Cat::Case:
            snprintf(b, sizeof b, "Мест для вентиляторов: %d%s%s", d.fanSlots, d.glass ? ", стекло" : "",
                     d.rgb ? ", RGB-подсветка" : "");
            break;
        case Cat::Motherboard:
            snprintf(b, sizeof b, "Сокет %s, %s, %s x%d, M.2 x%d%s, BIOS %s", d.socket.c_str(), d.chipset.c_str(),
                     ramName(d.ram), d.ramSlots, d.m2Slots, d.oc ? ", разгон" : "",
                     d.biosStyle == 1 ? "Raven" : (d.biosStyle == 2 ? "Polaris" : "NovaBIOS"));
            break;
        case Cat::CPU:
            snprintf(b, sizeof b, "Сокет %s, %d/%d потоков, %.1f ГГц, %d Вт%s%s", d.socket.c_str(), d.cores,
                     d.threads, d.ghz, d.tdp, d.igpu > 0 ? ", есть графика" : ", без графики",
                     d.x3d ? ", 3D V-Cache" : "");
            break;
        case Cat::Cooler:
            snprintf(b, sizeof b, "Рассеивает до %d Вт", d.coolW);
            break;
        case Cat::RAM:
            snprintf(b, sizeof b, "%s, %d ГБ, %d МГц (XMP/EXPO %d МГц)", ramName(d.ram), d.gb, d.mhz, d.xmpMhz);
            break;
        case Cat::GPU:
            if (d.supported)
                snprintf(b, sizeof b, "%d ГБ видеопамяти, %d Вт", d.vram, d.tdp);
            else
                snprintf(b, sizeof b, "%d ГБ, %d Вт — NOT SUPPORTED (ниже 40-й серии)", d.vram, d.tdp);
            break;
        case Cat::PSU:
            snprintf(b, sizeof b, "Мощность %d Вт", d.watts);
            break;
        case Cat::Storage:
            snprintf(b, sizeof b, "%s, %d ГБ", storName(d.stor), d.capGB);
            break;
        case Cat::Fan:
            snprintf(b, sizeof b, "%s", d.fanRgb ? "Улучшает обдув, RGB-подсветка" : "Улучшает обдув корпуса");
            break;
        case Cat::Paste:
            snprintf(b, sizeof b, "Нанести на процессор перед установкой кулера");
            break;
        case Cat::USB:
            snprintf(b, sizeof b, "Вставить в USB корпуса, чтобы установить ОС");
            break;
        default:
            b[0] = 0;
    }
    return b;
}
