// ============================================================================
//  Platform.cpp — реализация окна на чистом WinAPI.
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "Platform.h"

#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#endif

struct Platform::Impl {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    Input* input = nullptr; // действителен только внутри pumpMessages()
    bool quit = false;
    bool focusLost = false;
    bool minimized = false;
    bool fullscreen = false;
    WINDOWPLACEMENT savedPlacement{};
    DWORD savedStyle = 0;

    int gameW = 320;
    int gameH = 180;
    // Куда в клиентской области выводится картинка (для пересчёта мыши).
    int destX = 0, destY = 0, destW = 320, destH = 180;
    std::vector<uint32_t> lastFrame; // для перерисовки по WM_PAINT

    LARGE_INTEGER freq{};
    LARGE_INTEGER start{};

    void computeDestRect();
    void blit(HDC dc);
    void mouseToGame(LPARAM lp);
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
};

namespace {

const wchar_t* const kClassName = L"BackSeatWindowClass";

// Перевод виртуальных кодов клавиш WinAPI в клавиши игры.
bool mapKey(WPARAM vk, Key& out) {
    switch (vk) {
    case VK_LEFT: out = Key::Left; return true;
    case VK_RIGHT: out = Key::Right; return true;
    case VK_UP: out = Key::Up; return true;
    case VK_DOWN: out = Key::Down; return true;
    case 'A': out = Key::A; return true;
    case 'D': out = Key::D; return true;
    case 'W': out = Key::W; return true;
    case 'S': out = Key::S; return true;
    case 'F': out = Key::F; return true;
    case 'E': out = Key::E; return true;
    case 'R': out = Key::R; return true;
    case 'M': out = Key::M; return true;
    case 'Q': out = Key::Q; return true;
    case VK_TAB: out = Key::Tab; return true;
    case VK_SPACE: out = Key::Space; return true;
    case VK_RETURN: out = Key::Enter; return true;
    case VK_ESCAPE: out = Key::Escape; return true;
    case VK_F11: out = Key::F11; return true;
    default: return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------
//  Вывод изображения
// ---------------------------------------------------------------------------
void Platform::Impl::computeDestRect() {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) {
        destX = destY = 0;
        destW = gameW;
        destH = gameH;
        return;
    }
    // Целый масштаб даёт идеально чёткие пиксели; если окно меньше
    // исходного размера — дробный масштаб с сохранением пропорций.
    int scale = std::min(cw / gameW, ch / gameH);
    if (scale >= 1) {
        destW = gameW * scale;
        destH = gameH * scale;
    } else {
        const double s = std::min(static_cast<double>(cw) / gameW, static_cast<double>(ch) / gameH);
        destW = std::max(1, static_cast<int>(gameW * s));
        destH = std::max(1, static_cast<int>(gameH * s));
    }
    destX = (cw - destW) / 2;
    destY = (ch - destH) / 2;
}

void Platform::Impl::blit(HDC dc) {
    if (lastFrame.empty()) return;
    computeDestRect();
    RECT rc{};
    GetClientRect(hwnd, &rc);

    // Чёрные поля вокруг картинки.
    PatBlt(dc, 0, 0, rc.right, destY, BLACKNESS);
    PatBlt(dc, 0, destY + destH, rc.right, rc.bottom - destY - destH, BLACKNESS);
    PatBlt(dc, 0, destY, destX, destH, BLACKNESS);
    PatBlt(dc, destX + destW, destY, rc.right - destX - destW, destH, BLACKNESS);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = gameW;
    bmi.bmiHeader.biHeight = -gameH; // отрицательная высота: строки сверху вниз
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(dc, COLORONCOLOR); // ближайший сосед — без размытия
    StretchDIBits(dc, destX, destY, destW, destH, 0, 0, gameW, gameH, lastFrame.data(), &bmi,
                  DIB_RGB_COLORS, SRCCOPY);
}

void Platform::Impl::mouseToGame(LPARAM lp) {
    if (!input) return;
    const int mx = static_cast<short>(LOWORD(lp));
    const int my = static_cast<short>(HIWORD(lp));
    const int gx = destW > 0 ? (mx - destX) * gameW / destW : -1;
    const int gy = destH > 0 ? (my - destY) * gameH / destH : -1;
    const bool inside = mx >= destX && my >= destY && mx < destX + destW && my < destY + destH;
    input->onMouseMove(clampi(gx, 0, gameW - 1), clampi(gy, 0, gameH - 1), inside);
}

// ---------------------------------------------------------------------------
//  Оконная процедура
// ---------------------------------------------------------------------------
LRESULT CALLBACK Platform::Impl::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
        Impl* created = static_cast<Impl*>(cs->lpCreateParams);
        if (created) created->hwnd = hwnd; // сообщения создания тоже идут в handle()
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
    }
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->hwnd == hwnd) return self->handle(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Platform::Impl::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        quit = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        quit = true;
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1; // фон рисуем сами — без мерцания
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        blit(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        minimized = (wp == SIZE_MINIMIZED);
        computeDestRect();
        return 0;
    case WM_KILLFOCUS:
        focusLost = true;
        if (input) input->releaseAll();
        return 0;
    case WM_SYSKEYDOWN:
        // Alt+Enter — полноэкранный режим. Нажатие и отпускание сразу: нажатие
        // защёлкнется до ближайшего тика, а клавиша не «залипнет», даже если Alt
        // отпустили раньше Enter. Автоповтор (бит 30) игнорируем.
        if (wp == VK_RETURN && (lp & (1 << 29)) && !(lp & (1 << 30))) {
            if (input) {
                input->onKey(Key::F11, true);
                input->onKey(Key::F11, false);
            }
            return 0;
        }
        break; // Alt+F4 и прочее — стандартная обработка
    case WM_SYSKEYUP:
        if (wp == VK_RETURN) return 0;
        break;
    case WM_SYSCOMMAND:
        // Alt или F10 сами по себе не должны открывать системное меню: его модальный
        // цикл остановил бы игру до следующего нажатия.
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE); // Alt+буква — без системного «бипа»
    case WM_KEYDOWN:
    case WM_KEYUP: {
        Key k;
        if (input && mapKey(wp, k)) input->onKey(k, msg == WM_KEYDOWN);
        return 0;
    }
    case WM_MOUSEMOVE:
        mouseToGame(lp);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP: {
        mouseToGame(lp);
        const bool isDown = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN);
        const MouseButton b =
            (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? MouseButton::Left : MouseButton::Right;
        if (input) input->onMouseButton(b, isDown);
        // Захват мыши, пока зажата хотя бы одна кнопка: отпускание за пределами
        // окна тоже дойдёт до нас, и кнопка не «залипнет».
        if (isDown) {
            if (GetCapture() != hwnd) SetCapture(hwnd);
        } else if ((wp & (MK_LBUTTON | MK_RBUTTON)) == 0) {
            ReleaseCapture();
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        // Захват отобрало другое окно — считаем все кнопки отпущенными.
        if (reinterpret_cast<HWND>(lp) != hwnd && input) {
            input->onMouseButton(MouseButton::Left, false);
            input->onMouseButton(MouseButton::Right, false);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_CROSS)));
            return TRUE;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
//  Platform
// ---------------------------------------------------------------------------
Platform::Platform() : impl_(std::make_unique<Impl>()) {
    QueryPerformanceFrequency(&impl_->freq);
    QueryPerformanceCounter(&impl_->start);
}

Platform::~Platform() {
    if (impl_->hwnd) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
    if (impl_->instance) UnregisterClassW(kClassName, impl_->instance);
}

bool Platform::create(const wchar_t* title, int gameW, int gameH) {
    Impl& s = *impl_;
    s.instance = GetModuleHandleW(nullptr);
    s.gameW = gameW;
    s.gameH = gameH;
    s.lastFrame.assign(static_cast<size_t>(gameW) * gameH, 0u);

    // Чёткие пиксели на мониторах с масштабированием (иначе Windows размоет окно).
    SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Impl::wndProc;
    wc.hInstance = s.instance;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return false;

    // Наибольший целый масштаб, при котором окно помещается в 85% рабочей области.
    RECT work{0, 0, 1280, 720};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workW = work.right - work.left;
    const int workH = work.bottom - work.top;
    int scale = cfg::kWindowScale;
    while (scale > 1 && (gameW * scale > workW * 85 / 100 || gameH * scale > workH * 85 / 100)) --scale;
    while (gameW * (scale + 1) <= workW * 70 / 100 && gameH * (scale + 1) <= workH * 70 / 100) ++scale;

    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r{0, 0, gameW * scale, gameH * scale};
    AdjustWindowRect(&r, style, FALSE);
    const int ww = r.right - r.left;
    const int wh = r.bottom - r.top;
    const int wx = work.left + std::max(0, (workW - ww) / 2);
    const int wy = work.top + std::max(0, (workH - wh) / 2);

    s.hwnd = CreateWindowExW(0, kClassName, title, style, wx, wy, ww, wh, nullptr, nullptr, s.instance,
                             &s);
    if (!s.hwnd) return false;
    // WM_NCCREATE уже сохранил указатель, но на всякий случай — явно.
    SetWindowLongPtrW(s.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&s));
    ShowWindow(s.hwnd, SW_SHOW);
    UpdateWindow(s.hwnd);
    s.computeDestRect();
    return true;
}

bool Platform::pumpMessages(Input& input) {
    Impl& s = *impl_;
    s.input = &input;
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) s.quit = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    s.input = nullptr;
    return !s.quit;
}

void Platform::present(const Canvas& frame) {
    Impl& s = *impl_;
    if (!s.hwnd || frame.width() != s.gameW || frame.height() != s.gameH) return;
    std::copy(frame.data(), frame.data() + s.lastFrame.size(), s.lastFrame.begin());
    HDC dc = GetDC(s.hwnd);
    if (dc) {
        s.blit(dc);
        ReleaseDC(s.hwnd, dc);
    }
}

void Platform::toggleFullscreen() {
    Impl& s = *impl_;
    if (!s.hwnd) return;
    if (!s.fullscreen) {
        // Запоминаем положение окна и растягиваем его на весь монитор без рамки.
        s.savedStyle = static_cast<DWORD>(GetWindowLongPtrW(s.hwnd, GWL_STYLE));
        s.savedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(s.hwnd, &s.savedPlacement);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(s.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongPtrW(s.hwnd, GWL_STYLE, static_cast<LONG_PTR>((s.savedStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP));
        SetWindowPos(s.hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s.fullscreen = true;
    } else {
        SetWindowLongPtrW(s.hwnd, GWL_STYLE, static_cast<LONG_PTR>(s.savedStyle));
        SetWindowPlacement(s.hwnd, &s.savedPlacement);
        SetWindowPos(s.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED |
                         SWP_SHOWWINDOW);
        s.fullscreen = false;
    }
    s.computeDestRect();
    InvalidateRect(s.hwnd, nullptr, FALSE);
}

bool Platform::takeFocusLost() {
    const bool v = impl_->focusLost;
    impl_->focusLost = false;
    return v;
}

bool Platform::minimized() const { return impl_->minimized; }

double Platform::now() const {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart - impl_->start.QuadPart) / static_cast<double>(impl_->freq.QuadPart);
}

void Platform::sleepMs(int ms) const {
    if (ms > 0) Sleep(static_cast<DWORD>(ms));
}
