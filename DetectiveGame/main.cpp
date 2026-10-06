// main.cpp - окно, захват мыши, игровой цикл с фиксированным шагом.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cwchar>
#include <string>

#include "Game.h"
#include "Player.h"
#include "Renderer.h"
#include "UI.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

const wchar_t kClassName[] = L"DetectiveWindowClass";
const wchar_t kTitle[] = L"Детектив (хоррор) - прототип";
const double kStep = 1.0 / 60.0; // фиксированный шаг симуляции

// Всё состояние приложения в одном месте
struct App {
    HWND hwnd = nullptr;
    bool running = true;
    bool mouseCaptured = false;
    bool cursorHidden = false;
    POINT center = { 0, 0 };   // центр окна в экранных координатах
    double mouseAccum = 0.0;   // накопленное смещение мыши между шагами
    double fps = 0.0;          // текущий FPS
    bool showFps = true;       // F3 - показать/скрыть счётчик

    Game game;
    Renderer renderer;
    UI ui;
    InputState input;
};

App g_app;

// ---------------- Захват мыши ----------------

void UpdateCenter() {
    RECT rc;
    GetClientRect(g_app.hwnd, &rc);
    POINT p = { rc.right / 2, rc.bottom / 2 };
    ClientToScreen(g_app.hwnd, &p);
    g_app.center = p;
}

void CaptureMouse() {
    if (g_app.mouseCaptured) return;
    RECT rc;
    GetClientRect(g_app.hwnd, &rc);
    POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
    ClientToScreen(g_app.hwnd, &tl);
    ClientToScreen(g_app.hwnd, &br);
    RECT clip = { tl.x, tl.y, br.x, br.y };
    ClipCursor(&clip);
    if (!g_app.cursorHidden) { ShowCursor(FALSE); g_app.cursorHidden = true; }
    UpdateCenter();
    SetCursorPos(g_app.center.x, g_app.center.y);
    g_app.mouseCaptured = true;
}

void ReleaseMouse() {
    if (!g_app.mouseCaptured) return;
    ClipCursor(nullptr);
    if (g_app.cursorHidden) { ShowCursor(TRUE); g_app.cursorHidden = false; }
    g_app.mouseCaptured = false;
}

// При изменении размера/позиции окна область ClipCursor нужно пересчитать
void RefreshMouseCapture() {
    if (!g_app.mouseCaptured) return;
    ReleaseMouse();
    CaptureMouse();
}

// ---------------- Ввод ----------------

bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

// Опрос клавиатуры и мыши раз в кадр
void PollInput() {
    InputState& in = g_app.input;
    in = InputState{};
    if (GetForegroundWindow() != g_app.hwnd) return;

    in.forward = KeyDown('W') || KeyDown(VK_UP);
    in.back = KeyDown('S') || KeyDown(VK_DOWN);
    in.strafeLeft = KeyDown('A');
    in.strafeRight = KeyDown('D');
    in.turnLeft = KeyDown(VK_LEFT);
    in.turnRight = KeyDown(VK_RIGHT);

    if (g_app.mouseCaptured) {
        POINT p;
        GetCursorPos(&p);
        g_app.mouseAccum += static_cast<double>(p.x - g_app.center.x);
        SetCursorPos(g_app.center.x, g_app.center.y);
    }
}

// ---------------- Отрисовка ----------------

void RenderFrame() {
    RECT rc;
    GetClientRect(g_app.hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return; // окно свёрнуто

    g_app.game.Render(g_app.renderer);

    HDC hdc = GetDC(g_app.hwnd);
    g_app.renderer.Present(hdc, w, h, [&](HDC dc) {
        g_app.game.DrawOverlay(dc, g_app.ui, w, h, g_app.mouseCaptured);
        if (g_app.showFps) {
            wchar_t fpsText[32];
            swprintf_s(fpsText, L"FPS: %.0f", g_app.fps);
            RECT rf = { w - 140, h - 26, w - 8, h - 4 };
            g_app.ui.DrawString(dc, fpsText, rf, RGB(120, 255, 120), DT_RIGHT, FontSize::Small);
        }
    });
    ReleaseDC(g_app.hwnd, hdc);
}

// ---------------- Оконная процедура ----------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1; // фон рисуем сами, без мерцания
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (g_app.game.InMission()) CaptureMouse(); // мышь нужна только при хождении по локации
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) ReleaseMouse();
        else if (wp == VK_F3) g_app.showFps = !g_app.showFps;
        else g_app.game.OnKey(static_cast<int>(wp), (lp & (1 << 30)) != 0); // бит 30 - автоповтор
        return 0;
    case WM_KILLFOCUS:
        ReleaseMouse();
        return 0;
    case WM_SIZE:
    case WM_MOVE:
        RefreshMouseCapture();
        return 0;
    case WM_DESTROY:
        ReleaseMouse();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

// ---------------- Точка входа ----------------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return 1;

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, 960, 600 }; // размер клиентской области
    AdjustWindowRect(&r, style, FALSE);
    g_app.hwnd = CreateWindowExW(0, kClassName, kTitle, style,
                                 CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                                 nullptr, nullptr, hInstance, nullptr);
    if (!g_app.hwnd) return 1;

    g_app.ui.Init();
    ShowWindow(g_app.hwnd, nCmdShow);
    UpdateWindow(g_app.hwnd);

    // Игровой цикл с фиксированным шагом
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    double accumulator = 0.0;
    double fpsTimer = 0.0;
    int fpsFrames = 0;

    while (g_app.running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_app.running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_app.running) break;

        QueryPerformanceCounter(&now);
        double frameTime = static_cast<double>(now.QuadPart - prev.QuadPart) / static_cast<double>(freq.QuadPart);
        prev = now;
        if (frameTime > 0.25) frameTime = 0.25; // защита от "спирали смерти" после паузы
        accumulator += frameTime;

        if (!g_app.game.InMission()) ReleaseMouse();
        PollInput();
        while (accumulator >= kStep) {
            InputState step = g_app.input;
            step.mouseDX = g_app.mouseAccum; // вся накопленная мышь уходит в первый шаг
            g_app.mouseAccum = 0.0;
            g_app.game.Update(kStep, step);
            accumulator -= kStep;
        }

        RenderFrame();

        // Счётчик FPS: усредняем за 0.5 секунды
        ++fpsFrames;
        fpsTimer += frameTime;
        if (fpsTimer >= 0.5) {
            g_app.fps = fpsFrames / fpsTimer;
            fpsFrames = 0;
            fpsTimer = 0.0;
        }
        Sleep(1); // не грузим процессор на 100%
    }
    return 0;
}
