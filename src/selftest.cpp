// Самопроверка логики без окна: PCSimulator2 --selftest
#include "computer.hpp"
#include "game.hpp"
#include <cstdio>

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "[ OK ]" : "[FAIL]", what);
    if (!ok) g_fail++;
}

Entity& put(World& w, Entity& parent, int slot, const char* id) {
    Entity& e = w.spawn(findPart(id), v3(0, 0, 0), false);
    e.parent = parent.id;
    e.slot = slot;
    return e;
}

int price(const char* id) {
    const PartDef* d = findPart(id);
    return d ? d->price : -1;
}
}  // namespace

int runSelfTest() {
    // Цены, которые задал автор
    check(price("gpu_4040") == 200 && price("gpu_4050") == 250 && price("gpu_4060") == 300 &&
              price("gpu_4070") == 500 && price("gpu_4090") == 1299,
          "цены RTX 40");
    check(price("ram_ddr5_8") == 200 && price("ram_ddr5_16") == 400 && price("ram_ddr5_32") == 1000 &&
              price("ram_ddr5_64") == 2000,
          "цены DDR5");
    check(price("ram_ddr4_8") == 50 && price("ram_ddr4_16") == 150 && price("ram_ddr4_32") == 200 &&
              price("ram_ddr4_64") == 250,
          "цены DDR4");
    check(price("cpu_r7_7800x3d") == 500 && price("cpu_r7_9800x3d") == 550 && price("cpu_r9_9800x3d") == 449,
          "цены Ryzen");
    bool noRtx50 = true, oldUnsupported = true;
    for (auto& d : catalog()) {
        if (d.cat != Cat::GPU) continue;
        if (d.series >= 50) noRtx50 = false;
        if (d.series < 40 && d.supported) oldUnsupported = false;
        if (d.series >= 40 && !d.supported) oldUnsupported = false;
    }
    check(noRtx50, "нет 50-й серии");
    check(oldUnsupported, "всё ниже 40-й серии — NOT SUPPORTED");
    bool socketsOk = true;
    for (auto& mb : catalog()) {
        if (mb.cat != Cat::Motherboard) continue;
        bool hasCpu = false, hasRam = false;
        for (auto& d : catalog()) {
            hasCpu |= d.cat == Cat::CPU && d.socket == mb.socket;
            hasRam |= d.cat == Cat::RAM && d.ram == mb.ram;
        }
        socketsOk &= hasCpu && hasRam;
    }
    check(socketsOk, "у каждой платы есть процессор и память");
    int raven = 0, polaris = 0;
    for (auto& d : catalog())
        if (d.cat == Cat::Motherboard) raven += d.biosStyle == 1, polaris += d.biosStyle == 2;
    check(raven >= 3 && polaris >= 3, "платы Raven и Polaris в магазине");

    // Полная сборка
    World w;
    Entity& cs = w.spawn(findPart("case_air"), v3(0, 0, 0), false);
    Entity& mb = put(w, cs, 0, "mb_b650");
    Entity& cpu = put(w, mb, 0, "cpu_r7_7800x3d");
    cpu.pasted = true;
    put(w, mb, 1, "cool_tower");
    put(w, mb, 2, "gpu_4070");
    put(w, mb, 3, "ram_ddr5_16");
    put(w, mb, 5, "ram_ddr5_16");
    Entity& nv = put(w, mb, 7, "nvme_1000");
    put(w, cs, 1, "psu_750");
    put(w, cs, 4, "usb_os");
    Build b = gatherBuild(w, &cs);
    check(b.mb && b.cpu && b.cooler && b.gpu && b.psu && b.ram.size() == 2 && b.drives.size() == 1, "сборка собирается");
    check(buildProblems(b).size() == 1, "не хватает только ОС");
    Perf p = estimatePerf(b, OcSettings{});
    check(p.score > 12000 && p.score < 20000, "оценка BenchMax в разумных пределах");

    // Включение: POST → установщик → установка → рабочий стол
    Computer c;
    c.caseId = cs.id;
    SimEvents ev;
    ev.toast = [](const std::string& s, Color) { printf("       toast: %s\n", s.c_str()); };
    ev.earn = [](double) {};
    pressPower(w, c, ev);
    check(c.st == PCState::Post, "POST после включения");
    for (int i = 0; i < 100; i++) updateComputer(w, c, 0.05f, ev);
    check(c.st == PCState::Installer, "установщик с флешки");
    c.installDrive = nv.id;
    c.st = PCState::Installing;
    c.t = 0;
    for (int i = 0; i < 400; i++) updateComputer(w, c, 0.05f, ev);
    check(nv.osInstalled && c.st == PCState::Desktop, "ОС установлена и загружена");
    check(c.cpuTemp < 80, "температура в простое нормальная");

    // BIOS: вход и сохранение разгона
    enterBios(c);
    check(c.st == PCState::Bios, "вход в BIOS");
    rebootPC(c, false);
    check(c.st == PCState::Post, "перезагрузка из BIOS");

    // Без кулера — перегрев
    Entity* cooler = b.cooler;
    w.remove(cooler->id);
    shutdownPC(c);
    pressPower(w, c, ev);
    bool overheated = false;
    for (int i = 0; i < 1200 && !overheated; i++) {
        updateComputer(w, c, 0.05f, ev);
        overheated = c.st == PCState::Off;
    }
    check(overheated, "без кулера ПК выключается от перегрева");

    // Старая видеокарта без встройки — нет изображения
    World w2;
    Entity& cs2 = w2.spawn(findPart("case_office"), v3(0, 0, 0), false);
    Entity& mb2 = put(w2, cs2, 0, "mb_b550");
    put(w2, mb2, 0, "cpu_r5_5600");
    put(w2, mb2, 1, "cool_box");
    put(w2, mb2, 2, "gpu_3060");
    put(w2, mb2, 3, "ram_ddr4_8");
    put(w2, cs2, 1, "psu_550");
    Computer c2;
    c2.caseId = cs2.id;
    pressPower(w2, c2, ev);
    check(c2.st == PCState::NoVideo, "RTX 3060 (NOT SUPPORTED) без встройки — нет сигнала");

    // Слабый БП не тянет 4090
    World w3;
    Entity& cs3 = w3.spawn(findPart("case_air"), v3(0, 0, 0), false);
    Entity& mb3 = put(w3, cs3, 0, "mb_x670");
    Entity& cpu3 = put(w3, mb3, 0, "cpu_r7_9800x3d");
    cpu3.pasted = true;
    put(w3, mb3, 1, "cool_aio");
    put(w3, mb3, 2, "gpu_4090");
    put(w3, mb3, 3, "ram_ddr5_32");
    Entity& nv3 = put(w3, mb3, 7, "nvme_2000");
    nv3.osInstalled = true;
    put(w3, cs3, 1, "psu_400");
    Build b3 = gatherBuild(w3, &cs3);
    check(estimatePower(b3, OcSettings{}, 1, 1) > 400, "4090 + 9800X3D потребляют больше 400 Вт");

    // Заказ «Монстр для стримера» эта сборка выполняет (кроме БП это не проверяется заказом)
    Game g;
    g.world.ents.clear();
    {
        Entity& gcs = g.world.spawn(findPart("case_air"), v3(0, 0, 0), false);
        Entity& gmb = put(g.world, gcs, 0, "mb_x670");
        Entity& gcpu = put(g.world, gmb, 0, "cpu_r7_9800x3d");
        gcpu.pasted = true;
        put(g.world, gmb, 1, "cool_aio");
        put(g.world, gmb, 2, "gpu_4090");
        put(g.world, gmb, 3, "ram_ddr5_32");
        Entity& gnv = put(g.world, gmb, 7, "nvme_2000");
        gnv.osInstalled = true;
        put(g.world, gcs, 1, "psu_1000");
        auto probs = g.checkOrder(orderTemplates()[6], &gcs);
        for (auto& s : probs) printf("       проблема: %s\n", s.c_str());
        check(probs.empty(), "топовая сборка проходит заказ «Монстр для стримера»");
        auto probs2 = g.checkOrder(orderTemplates()[3], &gcs);
        check(probs2.empty(), "и заказ начального уровня тоже");
    }

    printf("\n%s: %d ошибок\n", g_fail ? "ПРОВАЛ" : "ВСЁ ХОРОШО", g_fail);
    return g_fail ? 1 : 0;
}
