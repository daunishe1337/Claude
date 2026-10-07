// ============================================================================
//  main.cpp — точка входа и главный цикл с фиксированным шагом 60 Гц.
//
//  Схема цикла («fix your timestep»):
//    1. разобрать сообщения окна (клавиатура/мышь -> Input);
//    2. накопить прошедшее реальное время;
//    3. пока накоплено >= 1/60 с — сделать шаг логики Game::update(1/60);
//    4. нарисовать кадр и вывести его в окно;
//    5. если до следующего шага есть время — немного поспать (экономия CPU).
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include "Audio.h"
#include "Common.h"
#include "Game.h"
#include "Input.h"
#include "Platform.h"
#include "Renderer.h"

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace {

int runGame() {
    // Точность Sleep() до 1 мс (иначе шаг планировщика ~15 мс даёт рывки).
    timeBeginPeriod(1);

    Platform platform;
    // Заголовок окна: «Заднее сиденье» (кодами Unicode — не зависит от кодировки файла).
    const wchar_t* title = L"Заднее сиденье";
    if (!platform.create(title, cfg::kScreenW, cfg::kScreenH)) {
        MessageBoxW(nullptr, L"Failed to create window.", L"BackSeat", MB_ICONERROR | MB_OK);
        timeEndPeriod(1);
        return 1;
    }

    Audio audio;
    audio.init(); // без звукового устройства игра просто будет беззвучной

    Input input;
    Canvas frame(cfg::kScreenW, cfg::kScreenH);
    {
        Game game(audio);

        const double dt = cfg::kFixedDt;
        double previous = platform.now();
        double accumulator = 0.0;

        while (platform.pumpMessages(input)) {
            if (platform.takeFocusLost()) {
                input.releaseAll();
                game.onFocusLost();
            }

            const double current = platform.now();
            double elapsed = current - previous;
            previous = current;
            // Защита от «спирали смерти» после долгой паузы (перетаскивание окна и т.п.).
            if (elapsed > 0.25) elapsed = 0.25;
            if (elapsed < 0.0) elapsed = 0.0;
            accumulator += elapsed;

            int steps = 0;
            while (accumulator >= dt && steps < 8) {
                input.beginTick();
                game.update(static_cast<float>(dt), input);
                accumulator -= dt;
                ++steps;
                if (game.wantsQuit()) break;
            }
            if (steps >= 8) accumulator = 0.0; // не догоняем бесконечно
            if (game.wantsQuit()) break;

            if (game.takeFullscreenToggle()) platform.toggleFullscreen();

            if (!platform.minimized()) {
                game.render(frame);
                platform.present(frame);
            }

            // Спим, если до следующего шага логики ещё далеко.
            const double untilNext = dt - accumulator;
            if (untilNext > 0.002) platform.sleepMs(static_cast<int>(untilNext * 1000.0) - 1);
            else if (platform.minimized()) platform.sleepMs(10);
        }
    } // Game уничтожается до остановки звука

    audio.shutdown();
    timeEndPeriod(1);
    return 0;
}

} // namespace

// Точка входа для подсистемы Windows (/SUBSYSTEM:WINDOWS).
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return runGame(); }

// Запасная точка входа: если проект случайно собран как консольный
// (/SUBSYSTEM:CONSOLE), игра всё равно запустится.
int main() { return runGame(); }
