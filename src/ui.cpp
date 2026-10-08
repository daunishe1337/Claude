#include "ui.hpp"
#include "render.hpp"

namespace ui {
namespace {
Ctx g_ctx;
const void* g_drag = nullptr;
}  // namespace

void set(const Ctx& c) {
    g_ctx = c;
    if (!c.down) g_drag = nullptr;
}
Ctx& ctx() { return g_ctx; }
void consume() { g_ctx.pressed = false; }

bool hover(Rect r) { return g_ctx.active && r.contains(g_ctx.mouse); }

bool click(Rect r) {
    if (g_ctx.active && g_ctx.pressed && r.contains(g_ctx.mouse)) {
        consume();
        return true;
    }
    return false;
}

bool button(Rect r, const std::string& label, float size, Color bg, Color fg, bool enabled) {
    bool h = enabled && hover(r);
    Color c = bg;
    if (!enabled) c = lerpColor(bg, rgba(60, 60, 66, bg.a), 0.7f);
    else if (h) c = lerpColor(bg, rgba(255, 255, 255, bg.a), g_ctx.down ? 0.05f : 0.18f);
    gfx::roundRect(r, std::min(8.0f, r.h * 0.3f), c);
    gfx::textCentered(label, r, size, enabled ? fg : withAlpha(fg, 120), true);
    return enabled && click(r);
}

bool slider(Rect r, float& v, float lo, float hi, float step, const void* id) {
    if (!id) id = &v;
    float old = v;
    if (g_ctx.active && g_ctx.pressed && r.contains(g_ctx.mouse)) {
        g_drag = id;
        consume();
    }
    if (g_drag == id && g_ctx.down && g_ctx.active) {
        float f = clampf((g_ctx.mouse.x - r.x) / r.w, 0, 1);
        v = lo + f * (hi - lo);
        if (step > 0) v = lo + std::round((v - lo) / step) * step;
        v = clampf(v, lo, hi);
    }
    float f = (v - lo) / (hi - lo);
    gfx::roundRect(Rect{r.x, r.y + r.h * 0.4f, r.w, r.h * 0.2f}, r.h * 0.1f, rgba(255, 255, 255, 40));
    gfx::roundRect(Rect{r.x, r.y + r.h * 0.4f, r.w * f, r.h * 0.2f}, r.h * 0.1f, theme::accent);
    gfx::circle(Vec2{r.x + r.w * f, r.y + r.h * 0.5f}, r.h * 0.38f,
                (g_drag == id || hover(r)) ? rgba(255, 255, 255) : rgba(220, 225, 235), 16);
    return v != old;
}

bool checkbox(Rect r, bool& v, const std::string& label, float size) {
    Rect b{r.x, r.y + (r.h - size) * 0.5f, size, size};
    gfx::roundRect(b, 4, v ? theme::accent : rgba(255, 255, 255, 40));
    if (v) gfx::textCentered("✓", b, size * 0.8f, theme::text, true);
    gfx::text(label, b.x + size + 10, r.y + (r.h - size * 1.15f) * 0.5f, size, theme::text);
    if (click(r)) {
        v = !v;
        return true;
    }
    return false;
}

void bar(Rect r, float frac, Color fg, Color bg) {
    frac = clampf(frac, 0, 1);
    gfx::roundRect(r, r.h * 0.4f, bg);
    if (frac > 0) gfx::roundRect(Rect{r.x, r.y, std::max(r.h, r.w * frac), r.h}, r.h * 0.4f, fg);
}

}  // namespace ui
