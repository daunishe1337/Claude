// main.cpp - платформенный слой WinAPI: окно, цикл с фиксированным шагом, вывод буфера.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include "Game.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

using namespace lrh;

namespace {

struct App {
    Game* game = nullptr;
    bool fullscreen = false;
    WINDOWPLACEMENT prevPlacement = {sizeof(WINDOWPLACEMENT)};
    LONG prevStyle = 0;
};

// Вывод буфера 320x180 в окно с сохранением пропорций и без сглаживания.
void present(HWND hwnd, HDC dc, const Renderer& r) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) return;
    const float sc = std::min(float(cw) / kW, float(ch) / kH);
    const int w = int(kW * sc), h = int(kH * sc);
    const int x = (cw - w) / 2, y = (ch - h) / 2;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kW;
    bi.bmiHeader.biHeight = -kH;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    if (x > 0) { PatBlt(dc, 0, 0, x, ch, BLACKNESS); PatBlt(dc, x + w, 0, cw - x - w, ch, BLACKNESS); }
    if (y > 0) { PatBlt(dc, 0, 0, cw, y, BLACKNESS); PatBlt(dc, 0, y + h, cw, ch - y - h, BLACKNESS); }
    SetStretchBltMode(dc, COLORONCOLOR);  // nearest neighbour
    StretchDIBits(dc, x, y, w, h, 0, 0, kW, kH, r.pixels(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

void toggleFullscreen(HWND hwnd, App& app) {
    if (!app.fullscreen) {
        app.prevStyle = GetWindowLong(hwnd, GWL_STYLE);
        GetWindowPlacement(hwnd, &app.prevPlacement);
        MONITORINFO mi = {sizeof(MONITORINFO)};
        if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, app.prevStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            app.fullscreen = true;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, app.prevStyle);
        SetWindowPlacement(hwnd, &app.prevPlacement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        app.fullscreen = false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp == VK_F11 && app) { toggleFullscreen(hwnd, *app); return 0; }
            if (app && app->game) app->game->input().onKey(int(wp), true);
            if (msg == WM_SYSKEYDOWN && wp != VK_F10) return 0;  // не даём Alt/F10 уходить в меню окна
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (app && app->game) app->game->input().onKey(int(wp), false);
            break;
        case WM_KILLFOCUS:
            if (app && app->game) app->game->input().releaseAll();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (app && app->game) present(hwnd, dc, app->game->renderer());
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    timeBeginPeriod(1);  // точный Sleep(1)

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "LongRoadHomeWnd";
    if (!RegisterClassExA(&wc)) return 1;

    App app;
    Game* game = new Game();  // большой объект (буферы) - держим в куче
    app.game = game;

    RECT rc = {0, 0, 1280, 720};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "The Long Road Home", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, hInst, &app);
    if (!hwnd) { delete game; return 1; }
    ShowWindow(hwnd, nShow);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    game->init(uint32_t(last.QuadPart ^ (last.QuadPart >> 32)));

    double acc = 0.0;
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running) break;

        QueryPerformanceCounter(&now);
        double dt = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);
        last = now;
        acc += std::min(dt, 0.25);  // защита от «спирали смерти»

        int steps = 0;
        while (acc >= double(kDt) && steps < 6) {
            game->update(kDt);
            acc -= double(kDt);
            ++steps;
        }
        if (steps == 6) acc = 0.0;
        if (game->wantsQuit()) { DestroyWindow(hwnd); continue; }

        if (steps > 0) {
            game->render();
            HDC dc = GetDC(hwnd);
            present(hwnd, dc, game->renderer());
            ReleaseDC(hwnd, dc);
        } else {
            Sleep(1);
        }
    }

    game->shutdown();
    delete game;
    timeEndPeriod(1);
    return 0;
}
