// Точка входа: WinMain, оконная процедура, цикл сообщений и таймеры.
// Одно полноэкранное окно WS_POPUP изображает весь рабочий стол; всё рисуется вручную.

#include "Common.h"
#include "Renderer.h"
#include "Shell.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <dwmapi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

namespace
{
constexpr UINT WM_APP_WINKEY = WM_APP + 1;   // одиночное нажатие Win
constexpr UINT WM_APP_WINCOMBO = WM_APP + 2; // Win + клавиша
constexpr UINT_PTR kTimerFrame = 1;
constexpr UINT_PTR kTimerClock = 2;
constexpr UINT_PTR kTimerCaret = 3;
constexpr DWORD kDwmWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kDwmDoNotRound = 1;              // DWMWCP_DONOTROUND

HWND g_hwnd = nullptr;
HHOOK g_hook = nullptr;
bool g_winDown = false;
bool g_winUsed = false;
bool g_tracking = false;
float g_scale = 1.0f;
std::unique_ptr<Renderer> g_renderer;
std::unique_ptr<Shell> g_shell;

template <class Fn>
Fn LoadProc(const wchar_t* module, const char* name)
{
    HMODULE h = GetModuleHandleW(module);
    if (!h)
        return nullptr;
    FARPROC p = GetProcAddress(h, name);
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(p));
}

void EnableDpiAwareness()
{
    using SetCtxFn = BOOL(WINAPI*)(HANDLE);
    if (auto fn = LoadProc<SetCtxFn>(L"user32.dll", "SetProcessDpiAwarenessContext"))
    {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
        if (fn(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4))))
            return;
    }
    SetProcessDPIAware();
}

float DpiScaleFor(HWND hwnd)
{
    using GetDpiFn = UINT(WINAPI*)(HWND);
    UINT dpi = 96;
    if (auto fn = LoadProc<GetDpiFn>(L"user32.dll", "GetDpiForWindow"))
        dpi = fn(hwnd);
    else
    {
        HDC dc = GetDC(nullptr);
        dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(nullptr, dc);
    }
    return static_cast<float>(dpi) / 96.0f;
}

Point ToDip(LPARAM lp)
{
    return {static_cast<float>(GET_X_LPARAM(lp)) / g_scale, static_cast<float>(GET_Y_LPARAM(lp)) / g_scale};
}

Point ToDip(POINT pt) { return {static_cast<float>(pt.x) / g_scale, static_cast<float>(pt.y) / g_scale}; }

// Низкоуровневый перехват клавиатуры: пока наше окно активно, клавиша Win открывает
// собственное меню «Пуск», а не системное.
LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION && g_hwnd)
    {
        const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        const bool ours = GetForegroundWindow() == g_hwnd;
        const bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        if (!ours)
        {
            g_winDown = false;
        }
        else if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN)
        {
            if (down)
            {
                if (!g_winDown)
                {
                    g_winDown = true;
                    g_winUsed = false;
                }
            }
            else
            {
                if (g_winDown && !g_winUsed)
                    PostMessageW(g_hwnd, WM_APP_WINKEY, 0, 0);
                g_winDown = false;
            }
            return 1;
        }
        else if (g_winDown)
        {
            if (k->vkCode == 'L')
                return CallNextHookEx(g_hook, code, wp, lp);
            if (down)
            {
                g_winUsed = true;
                PostMessageW(g_hwnd, WM_APP_WINCOMBO, static_cast<WPARAM>(k->vkCode), 0);
            }
            return 1;
        }
        else if (down && k->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                 !(GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            PostMessageW(g_hwnd, WM_APP_WINKEY, 0, 0); // Ctrl+Esc
            return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

void ResizeToClient(HWND hwnd)
{
    RECT cr{};
    GetClientRect(hwnd, &cr);
    if (g_shell && cr.right > 0 && cr.bottom > 0)
        g_shell->Resize(cr.right, cr.bottom, g_scale);
}

void FitToMonitor(HWND hwnd)
{
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    const RECT& r = mi.rcMonitor;
    SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED)
            ResizeToClient(hwnd);
        return 0;
    case WM_DPICHANGED:
    {
        g_scale = static_cast<float>(HIWORD(wp)) / 96.0f;
        const RECT* rc = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        ResizeToClient(hwnd);
        return 0;
    }
    case WM_DISPLAYCHANGE:
        FitToMonitor(hwnd);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_shell)
            g_shell->Paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!g_tracking)
        {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            g_tracking = true;
        }
        if (g_shell)
            g_shell->MouseMove(ToDip(lp));
        return 0;
    case WM_MOUSELEAVE:
        g_tracking = false;
        if (g_shell)
            g_shell->MouseLeftWindow();
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        if (g_shell)
            g_shell->MouseDown(ToDip(lp), MouseButton::Left, msg == WM_LBUTTONDBLCLK ? 2 : 1);
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        SetCapture(hwnd);
        if (g_shell)
            g_shell->MouseDown(ToDip(lp), MouseButton::Right, 1);
        return 0;
    case WM_MBUTTONDOWN:
        SetCapture(hwnd);
        if (g_shell)
            g_shell->MouseDown(ToDip(lp), MouseButton::Middle, 1);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        const MouseButton b = msg == WM_LBUTTONUP ? MouseButton::Left : (msg == WM_RBUTTONUP ? MouseButton::Right : MouseButton::Middle);
        if (g_shell)
            g_shell->MouseUp(ToDip(lp), b);
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)))
            ReleaseCapture();
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd, &pt);
        if (g_shell)
            g_shell->MouseWheel(ToDip(pt), static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA));
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_shell)
        {
            POINT pt{};
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            SetCursor(g_shell->CursorAt(ToDip(pt)));
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_LWIN || wp == VK_RWIN)
            return 0;
        if (g_shell)
            g_shell->KeyDown(static_cast<UINT>(wp), (lp & (1 << 30)) != 0);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (g_shell)
            g_shell->KeyUp(static_cast<UINT>(wp));
        return 0;
    case WM_CHAR:
        if (g_shell)
            g_shell->Char(static_cast<wchar_t>(wp));
        return 0;
    case WM_SYSCHAR:
        return 0; // без системного звука на Alt+буква
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;
    case WM_TIMER:
        if (g_shell)
            g_shell->Timer(wp);
        return 0;
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lp) != hwnd && g_shell)
            g_shell->CaptureLost();
        return 0;
    case WM_ACTIVATEAPP:
        if (!wp)
            g_winDown = false;
        return 0;
    case WM_APP_WINKEY:
        if (g_shell)
            g_shell->WinKey();
        return 0;
    case WM_APP_WINCOMBO:
        if (g_shell)
            g_shell->WinCombo(static_cast<UINT>(wp));
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrev*/, PWSTR /*cmdLine*/, int /*show*/)
{
    EnableDpiAwareness();
    if (!Renderer::Startup())
    {
        MessageBoxW(nullptr, L"Не удалось инициализировать GDI+.", L"Windows 11", MB_ICONERROR);
        return 1;
    }

    const wchar_t* className = L"Win11ShellDesktop";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    POINT origin{0, 0};
    HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    const RECT r = mi.rcMonitor;

    g_renderer = std::make_unique<Renderer>();
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, className, L"Windows 11 — интерфейс", WS_POPUP, r.left, r.top,
                                r.right - r.left, r.bottom - r.top, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd)
    {
        g_renderer.reset();
        Renderer::Shutdown();
        return 1;
    }
    g_hwnd = hwnd;
    const DWORD corner = kDwmDoNotRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner));

    g_scale = DpiScaleFor(hwnd);
    g_shell = std::make_unique<Shell>(hwnd, *g_renderer);
    ResizeToClient(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    SetTimer(hwnd, kTimerFrame, 15, nullptr);
    SetTimer(hwnd, kTimerClock, 1000, nullptr);
    SetTimer(hwnd, kTimerCaret, GetCaretBlinkTime(), nullptr);
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hook)
        UnhookWindowsHookEx(g_hook);
    g_shell.reset();
    g_renderer.reset();
    Renderer::Shutdown();
    return static_cast<int>(msg.wParam);
}
