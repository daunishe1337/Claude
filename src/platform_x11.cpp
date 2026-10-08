// Linux: X11 + GLX (системные библиотеки libX11 и libGL)
#if !defined(_WIN32)
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <time.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include "platform.hpp"

namespace plat {
namespace {
Display* g_dpy = nullptr;
Window g_win = 0;
GLXContext g_ctx = nullptr;
Atom g_wmDelete;
Cursor g_blank = 0;
bool g_quit = false;
int g_w = 1280, g_h = 720;
bool g_keys[K_COUNT], g_prevKeys[K_COUNT];
bool g_mouse[3], g_prevMouse[3];
Vec2 g_mousePos, g_mouseDelta, g_accum;
float g_wheel = 0;
bool g_locked = false, g_focused = true, g_fullscreen = false;
timespec g_start;

int mapKey(KeySym k) {
    switch (k) {
        case XK_w: case XK_W: return K_W;
        case XK_a: case XK_A: return K_A;
        case XK_s: case XK_S: return K_S;
        case XK_d: case XK_D: return K_D;
        case XK_e: case XK_E: return K_E;
        case XK_f: case XK_F: return K_F;
        case XK_g: case XK_G: return K_G;
        case XK_q: case XK_Q: return K_Q;
        case XK_r: case XK_R: return K_R;
        case XK_c: case XK_C: return K_C;
        case XK_Tab: case XK_ISO_Left_Tab: return K_TAB;
        case XK_Escape: return K_ESC;
        case XK_space: return K_SPACE;
        case XK_Shift_L: case XK_Shift_R: return K_SHIFT;
        case XK_Control_L: case XK_Control_R: return K_CTRL;
        case XK_Up: return K_UP;
        case XK_Down: return K_DOWN;
        case XK_Left: return K_LEFT;
        case XK_Right: return K_RIGHT;
        case XK_Return: return K_ENTER;
        case XK_BackSpace: return K_BACKSPACE;
        case XK_Delete: case XK_KP_Delete: return K_DELETE;
        case XK_F10: return K_F10;
        case XK_F1: return K_F1;
        case XK_F2: return K_F2;
        case XK_F3: return K_F3;
        case XK_F11: return K_F11;
        case XK_F12: return K_F12;
        default: return -1;
    }
}

void applyCursor() {
    if (!g_dpy) return;
    if (g_locked) {
        XDefineCursor(g_dpy, g_win, g_blank);
        XWarpPointer(g_dpy, None, g_win, 0, 0, 0, 0, g_w / 2, g_h / 2);
    } else {
        XUndefineCursor(g_dpy, g_win);
    }
    XFlush(g_dpy);
}
}  // namespace

bool init(const char* title, int w, int h) {
    clock_gettime(CLOCK_MONOTONIC, &g_start);
    g_dpy = XOpenDisplay(nullptr);
    if (!g_dpy) return false;
    int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                   GLX_BLUE_SIZE, 8, None};
    XVisualInfo* vi = glXChooseVisual(g_dpy, DefaultScreen(g_dpy), attrs);
    if (!vi) return false;
    Window root = RootWindow(g_dpy, vi->screen);
    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(g_dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask;
    g_win = XCreateWindow(g_dpy, root, 0, 0, w, h, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask,
                          &swa);
    g_w = w;
    g_h = h;
    Atom utf8 = XInternAtom(g_dpy, "UTF8_STRING", False);
    XChangeProperty(g_dpy, g_win, XInternAtom(g_dpy, "_NET_WM_NAME", False), utf8, 8, PropModeReplace,
                    (const unsigned char*)title, (int)strlen(title));
    XStoreName(g_dpy, g_win, "PC Simulator 2");
    g_wmDelete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &g_wmDelete, 1);
    XMapWindow(g_dpy, g_win);
    g_ctx = glXCreateContext(g_dpy, vi, nullptr, True);
    XFree(vi);
    if (!g_ctx) return false;
    glXMakeCurrent(g_dpy, g_win, g_ctx);
    Bool supported;
    XkbSetDetectableAutoRepeat(g_dpy, True, &supported);

    char empty[8] = {0};
    Pixmap pm = XCreateBitmapFromData(g_dpy, g_win, empty, 8, 8);
    XColor black{};
    g_blank = XCreatePixmapCursor(g_dpy, pm, pm, &black, &black, 0, 0);
    XFreePixmap(g_dpy, pm);
    setVsync(true);
    return true;
}

void shutdown() {
    if (!g_dpy) return;
    glXMakeCurrent(g_dpy, None, nullptr);
    if (g_ctx) glXDestroyContext(g_dpy, g_ctx);
    XDestroyWindow(g_dpy, g_win);
    XCloseDisplay(g_dpy);
    g_dpy = nullptr;
}

bool pollEvents() {
    memcpy(g_prevKeys, g_keys, sizeof g_keys);
    memcpy(g_prevMouse, g_mouse, sizeof g_mouse);
    g_wheel = 0;
    g_accum = Vec2{0, 0};
    while (XPending(g_dpy)) {
        XEvent e;
        XNextEvent(g_dpy, &e);
        switch (e.type) {
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == g_wmDelete) g_quit = true;
                break;
            case ConfigureNotify:
                g_w = e.xconfigure.width;
                g_h = e.xconfigure.height;
                break;
            case FocusIn:
                g_focused = true;
                applyCursor();
                break;
            case FocusOut:
                g_focused = false;
                memset(g_keys, 0, sizeof g_keys);
                memset(g_mouse, 0, sizeof g_mouse);
                break;
            case KeyPress:
            case KeyRelease: {
                int k = mapKey(XLookupKeysym(&e.xkey, 0));
                if (k >= 0) g_keys[k] = e.type == KeyPress;
                break;
            }
            case ButtonPress:
            case ButtonRelease: {
                bool down = e.type == ButtonPress;
                int b = e.xbutton.button;
                if (b == 1) g_mouse[0] = down;
                else if (b == 3) g_mouse[1] = down;
                else if (b == 2) g_mouse[2] = down;
                else if (down && b == 4) g_wheel += 1;
                else if (down && b == 5) g_wheel -= 1;
                break;
            }
            case MotionNotify:
                if (g_locked) {
                    g_accum.x += e.xmotion.x - g_w / 2;
                    g_accum.y += e.xmotion.y - g_h / 2;
                }
                g_mousePos = Vec2{(float)e.xmotion.x, (float)e.xmotion.y};
                break;
        }
    }
    if (g_locked && g_focused) {
        g_mouseDelta = g_accum;
        if (g_accum.x != 0 || g_accum.y != 0) XWarpPointer(g_dpy, None, g_win, 0, 0, 0, 0, g_w / 2, g_h / 2);
        g_mousePos = Vec2{(float)(g_w / 2), (float)(g_h / 2)};
    } else {
        g_mouseDelta = Vec2{0, 0};
    }
    return !g_quit;
}

void swap() { glXSwapBuffers(g_dpy, g_win); }
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
}

void setFullscreen(bool fs) {
    if (fs == g_fullscreen) return;
    g_fullscreen = fs;
    XEvent e{};
    e.type = ClientMessage;
    e.xclient.window = g_win;
    e.xclient.message_type = XInternAtom(g_dpy, "_NET_WM_STATE", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = fs ? 1 : 0;
    e.xclient.data.l[1] = (long)XInternAtom(g_dpy, "_NET_WM_STATE_FULLSCREEN", False);
    XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(g_dpy);
}

bool fullscreen() { return g_fullscreen; }

void setVsync(bool on) {
    typedef void (*SwapFn)(Display*, GLXDrawable, int);
    SwapFn fn = (SwapFn)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
    if (fn) fn(g_dpy, g_win, on ? 1 : 0);
}

double time() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec - g_start.tv_sec) + (t.tv_nsec - g_start.tv_nsec) * 1e-9;
}

void sleepMs(int ms) { usleep(ms * 1000); }
void beep(float, float, float) {}

std::string dataDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return "./";
    std::string p(buf, n);
    size_t s = p.find_last_of('/');
    return s == std::string::npos ? "./" : p.substr(0, s + 1);
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (fclose(f) == 0) && ok;
    return ok && rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace plat
#endif
