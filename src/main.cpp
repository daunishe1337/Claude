// PC Simulator 2 — точка входа
#include "game.hpp"
#include "platform.hpp"
#include "render.hpp"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int runSelfTest();   // selftest.cpp

static void fatal(const char* msg) {
#ifdef _WIN32
    MessageBoxA(nullptr, msg, "PC Simulator 2", MB_ICONERROR | MB_OK);
#else
    fprintf(stderr, "%s\n", msg);
#endif
}

int main(int argc, char** argv) {
    bool shots = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) return runSelfTest();
        if (!strcmp(argv[i], "--shots")) shots = true;
    }
    if (!plat::init("PC Simulator 2", 1280, 720)) {
        fatal("Не удалось создать окно OpenGL. Обновите драйвер видеокарты.");
        return 1;
    }
    gfx::init();
    Game game;
    game.debugShots = shots;
    plat::setVsync(game.settings.vsync);
    if (game.settings.fullscreen) plat::setFullscreen(true);

    double last = plat::time();
    while (!game.quit && plat::pollEvents()) {
        double now = plat::time();
        float dt = (float)(now - last);
        last = now;
        game.frame(dt);
        plat::swap();
        // Без вертикальной синхронизации не грузим процессор на 100%
        if (!game.settings.vsync && plat::time() - now < 1.0 / 240) plat::sleepMs(1);
        if (!plat::focused()) plat::sleepMs(10);
    }
    if (game.mode != Mode::Menu && !shots) game.save();
    game.saveSettings();
    plat::shutdown();
    return 0;
}
