// Простейший immediate-mode интерфейс поверх gfx 2D (работает и на экране, и на мониторе в 3D)
#pragma once
#include "core.hpp"
#include <string>

namespace ui {

struct Ctx {
    Vec2 mouse{-1e9f, -1e9f};
    bool pressed = false, down = false, released = false;
    float wheel = 0;
    bool active = false;
};

void set(const Ctx& c);
Ctx& ctx();
void consume();   // клик уже обработан

namespace theme {
constexpr Color accent = rgba(64, 140, 255);
constexpr Color panel = rgba(28, 32, 40, 235);
constexpr Color panel2 = rgba(40, 46, 58, 240);
constexpr Color text = rgba(235, 238, 245);
constexpr Color dim = rgba(150, 158, 175);
constexpr Color good = rgba(80, 210, 110);
constexpr Color bad = rgba(240, 80, 80);
constexpr Color warn = rgba(255, 180, 60);
}

bool hover(Rect r);
bool click(Rect r);
bool button(Rect r, const std::string& label, float size, Color bg = theme::accent, Color fg = theme::text,
            bool enabled = true);
bool slider(Rect r, float& v, float lo, float hi, float step, const void* id = nullptr);
bool checkbox(Rect r, bool& v, const std::string& label, float size);
void bar(Rect r, float frac, Color fg, Color bg = rgba(255, 255, 255, 30));

}  // namespace ui
