// Платформенный слой: окно, OpenGL-контекст, ввод, время, звук.
// Реализации: platform_win32.cpp (WinAPI + WGL) и platform_x11.cpp (X11 + GLX).
#pragma once
#include "core.hpp"
#include <string>

namespace plat {

enum Key {
    K_W, K_A, K_S, K_D, K_E, K_F, K_G, K_Q, K_R, K_C, K_TAB, K_ESC, K_SPACE, K_SHIFT, K_CTRL,
    K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_BACKSPACE, K_DELETE, K_F1, K_F2, K_F3, K_F10, K_F11, K_F12, K_COUNT
};

bool init(const char* titleUtf8, int w, int h);
void shutdown();
bool pollEvents();      // false — окно закрыли
void swap();
int width();
int height();

bool keyDown(Key k);
bool keyPressed(Key k);
bool mouseDown(int button);      // 0 — левая, 1 — правая
bool mousePressed(int button);
bool mouseReleased(int button);
Vec2 mousePos();
Vec2 mouseDelta();               // при захваченной мыши
float wheel();

void lockMouse(bool lock);       // скрыть курсор и считать относительное движение
bool focused();
void setFullscreen(bool fs);
bool fullscreen();
void setVsync(bool on);

double time();
void sleepMs(int ms);
void beep(float freq, float seconds, float volume = 0.25f);
std::string dataDir();           // папка рядом с exe для сохранений
// Работа с файлами по UTF-8 путям (на Windows путь может содержать кириллицу)
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
bool saveScreenshotBMP(const std::string& path, int w, int h, const unsigned char* rgba);

}  // namespace plat
