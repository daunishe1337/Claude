// Windows: WinAPI + WGL. Только системные библиотеки (user32, gdi32, opengl32, winmm).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "platform.hpp"

namespace plat {
namespace {
HWND g_wnd = nullptr;
HDC g_dc = nullptr;
HGLRC g_rc = nullptr;
bool g_quit = false;
int g_w = 1280, g_h = 720;
bool g_keys[K_COUNT], g_prevKeys[K_COUNT];
bool g_mouse[3], g_prevMouse[3];
Vec2 g_mousePos, g_mouseDelta;
float g_wheel = 0;
bool g_locked = false, g_focused = true, g_fullscreen = false;
WINDOWPLACEMENT g_prevPlacement = {sizeof(WINDOWPLACEMENT)};
LARGE_INTEGER g_freq, g_start;

int mapKey(WPARAM vk) {
    switch (vk) {
        case 'W': return K_W;
        case 'A': return K_A;
        case 'S': return K_S;
        case 'D': return K_D;
        case 'E': return K_E;
        case 'F': return K_F;
        case 'G': return K_G;
        case 'Q': return K_Q;
        case 'R': return K_R;
        case 'C': return K_C;
        case VK_TAB: return K_TAB;
        case VK_ESCAPE: return K_ESC;
        case VK_SPACE: return K_SPACE;
        case VK_SHIFT: return K_SHIFT;
        case VK_CONTROL: return K_CTRL;
        case VK_UP: return K_UP;
        case VK_DOWN: return K_DOWN;
        case VK_LEFT: return K_LEFT;
        case VK_RIGHT: return K_RIGHT;
        case VK_RETURN: return K_ENTER;
        case VK_BACK: return K_BACKSPACE;
        case VK_DELETE: return K_DELETE;
        case VK_F10: return K_F10;
        case VK_F1: return K_F1;
        case VK_F2: return K_F2;
        case VK_F3: return K_F3;
        case VK_F11: return K_F11;
        case VK_F12: return K_F12;
        default: return -1;
    }
}

void applyCursor() {
    if (g_locked && g_focused) {
        while (ShowCursor(FALSE) >= 0) {
        }
        RECT r;
        GetClientRect(g_wnd, &r);
        POINT tl{r.left, r.top}, br{r.right, r.bottom};
        ClientToScreen(g_wnd, &tl);
        ClientToScreen(g_wnd, &br);
        RECT clip{tl.x, tl.y, br.x, br.y};
        ClipCursor(&clip);
        POINT c{(tl.x + br.x) / 2, (tl.y + br.y) / 2};
        SetCursorPos(c.x, c.y);
    } else {
        while (ShowCursor(TRUE) < 0) {
        }
        ClipCursor(nullptr);
    }
}

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            g_quit = true;
            return 0;
        case WM_SIZE:
            g_w = LOWORD(lp);
            g_h = HIWORD(lp);
            return 0;
        case WM_SETFOCUS:
            g_focused = true;
            applyCursor();
            return 0;
        case WM_KILLFOCUS:
            g_focused = false;
            memset(g_keys, 0, sizeof g_keys);
            memset(g_mouse, 0, sizeof g_mouse);
            applyCursor();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int k = mapKey(wp);
            if (k >= 0) g_keys[k] = true;
            if (wp == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) g_quit = true;
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int k = mapKey(wp);
            if (k >= 0) g_keys[k] = false;
            return 0;
        }
        case WM_LBUTTONDOWN: g_mouse[0] = true; SetCapture(h); return 0;
        case WM_LBUTTONUP: g_mouse[0] = false; ReleaseCapture(); return 0;
        case WM_RBUTTONDOWN: g_mouse[1] = true; return 0;
        case WM_RBUTTONUP: g_mouse[1] = false; return 0;
        case WM_MBUTTONDOWN: g_mouse[2] = true; return 0;
        case WM_MBUTTONUP: g_mouse[2] = false; return 0;
        case WM_MOUSEWHEEL:
            g_wheel += (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && g_locked && g_focused) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

std::wstring widen(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}
}  // namespace

bool init(const char* title, int w, int h) {
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    SetProcessDPIAware();

    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"PCSim2Window";
    if (!RegisterClassW(&wc)) return false;

    RECT r{0, 0, w, h};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&r, style, FALSE);
    std::wstring wt = widen(title);
    g_wnd = CreateWindowExW(0, wc.lpszClassName, wt.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    if (!g_wnd) return false;

    g_dc = GetDC(g_wnd);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_dc, &pfd);
    if (!pf || !SetPixelFormat(g_dc, pf, &pfd)) return false;
    g_rc = wglCreateContext(g_dc);
    if (!g_rc || !wglMakeCurrent(g_dc, g_rc)) return false;

    setVsync(true);
    ShowWindow(g_wnd, SW_SHOW);
    UpdateWindow(g_wnd);
    RECT cr;
    GetClientRect(g_wnd, &cr);
    g_w = cr.right;
    g_h = cr.bottom;
    return true;
}

void shutdown() {
    lockMouse(false);
    if (g_rc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_rc);
    }
    if (g_dc) ReleaseDC(g_wnd, g_dc);
    if (g_wnd) DestroyWindow(g_wnd);
}

bool pollEvents() {
    memcpy(g_prevKeys, g_keys, sizeof g_keys);
    memcpy(g_prevMouse, g_mouse, sizeof g_mouse);
    g_wheel = 0;
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(g_wnd, &p);
    if (g_locked && g_focused) {
        int cx = g_w / 2, cy = g_h / 2;
        g_mouseDelta = Vec2{(float)(p.x - cx), (float)(p.y - cy)};
        POINT c{cx, cy};
        ClientToScreen(g_wnd, &c);
        SetCursorPos(c.x, c.y);
        g_mousePos = Vec2{(float)cx, (float)cy};
    } else {
        g_mouseDelta = Vec2{0, 0};
        g_mousePos = Vec2{(float)p.x, (float)p.y};
    }
    return !g_quit;
}

void swap() { SwapBuffers(g_dc); }
int width() { return g_w > 0 ? g_w : 1; }
int height() { return g_h > 0 ? g_h : 1; }
bool keyDown(Key k) { return g_keys[k]; }
bool keyPressed(Key k) { return g_keys[k] && !g_prevKeys[k]; }
bool mouseDown(int b) { return g_mouse[b]; }
bool mousePressed(int b) { return g_mouse[b] && !g_prevMouse[b]; }
bool mouseReleased(int b) { return !g_mouse[b] && g_prevMouse[b]; }
Vec2 mousePos() { return g_mousePos; }
Vec2 mouseDelta() { return g_mouseDelta; }
float wheel() { return g_wheel; }
bool focused() { return g_focused; }

void lockMouse(bool lock) {
    if (lock == g_locked) return;
    g_locked = lock;
    applyCursor();
    if (!lock) {
        POINT c{g_w / 2, g_h / 2};
        ClientToScreen(g_wnd, &c);
        SetCursorPos(c.x, c.y);
    }
}

void setFullscreen(bool fs) {
    if (fs == g_fullscreen) return;
    g_fullscreen = fs;
    DWORD style = GetWindowLongW(g_wnd, GWL_STYLE);
    if (fs) {
        MONITORINFO mi{sizeof mi};
        GetWindowPlacement(g_wnd, &g_prevPlacement);
        GetMonitorInfoW(MonitorFromWindow(g_wnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongW(g_wnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(g_wnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(g_wnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_wnd, &g_prevPlacement);
        SetWindowPos(g_wnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    applyCursor();
}

bool fullscreen() { return g_fullscreen; }

void setVsync(bool on) {
    typedef BOOL(WINAPI * SwapFn)(int);
    SwapFn fn = (SwapFn)(void*)wglGetProcAddress("wglSwapIntervalEXT");
    if (fn) fn(on ? 1 : 0);
}

double time() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return double(t.QuadPart - g_start.QuadPart) / double(g_freq.QuadPart);
}

void sleepMs(int ms) { Sleep(ms); }

void beep(float freq, float seconds, float volume) {
    // WAV генерируется в памяти и играется асинхронно через PlaySound
    static std::vector<unsigned char> buffers[4];
    static int idx = 0;
    std::vector<unsigned char>& buf = buffers[idx];
    idx = (idx + 1) % 4;
    const int rate = 22050;
    int n = (int)(rate * seconds);
    buf.assign(44 + n, 0);
    auto put32 = [&](int off, unsigned v) { memcpy(&buf[off], &v, 4); };
    auto put16 = [&](int off, unsigned short v) { memcpy(&buf[off], &v, 2); };
    memcpy(&buf[0], "RIFF", 4);
    put32(4, 36 + n);
    memcpy(&buf[8], "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, 1);
    put32(24, rate);
    put32(28, rate);
    put16(32, 1);
    put16(34, 8);
    memcpy(&buf[36], "data", 4);
    put32(40, n);
    for (int i = 0; i < n; i++) {
        float t = (float)i / rate;
        float env = std::min(1.0f, std::min(t * 200.0f, (seconds - t) * 200.0f));
        float s = std::sin(2 * kPi * freq * t) > 0 ? 1.0f : -1.0f;
        buf[44 + i] = (unsigned char)(128 + 127 * volume * env * s);
    }
    PlaySoundA((LPCSTR)buf.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

std::string dataDir() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n);
    size_t s = p.find_last_of(L"\\/");
    if (s != std::wstring::npos) p = p.substr(0, s + 1);
    int len = WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len > 0 ? len - 1 : 0, 0);
    WideCharToMultiByte(CP_UTF8, 0, p.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = _wfopen(widen(path.c_str()).c_str(), L"rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::wstring tmp = widen((path + ".tmp").c_str()), dst = widen(path.c_str());
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (fclose(f) == 0) && ok;
    if (!ok) return false;
    return MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace plat
#endif
