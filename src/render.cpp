#include "render.hpp"
#include "font_data.hpp"
#include "platform.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <unordered_map>
#include <vector>

namespace gfx {
namespace {

struct Vtx {
    float x, y, z, u, v;
    uint8_t r, g, b, a;
};

std::vector<Vtx> g_batch;
GLuint g_atlas = 0;
float g_wu = 0, g_wv = 0;   // белый тексель

std::vector<Mat4> g_stack{identity()};
float g_alpha = 1.0f;
Color g_tint;
float g_tintK = 0.0f;
bool g_emissive = false;
const Vec3 kLight = normalize(v3(0.35f, 1.0f, 0.55f));

enum class Mode { None, World, Screen, Canvas3D } g_mode = Mode::None;
Mat4 g_canvas = identity();

std::unordered_map<int, int> g_glyphIndex;

void flushBatch() {
    if (g_batch.empty()) return;
    glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &g_batch[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), &g_batch[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vtx), &g_batch[0].r);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_batch.size());
    g_batch.clear();
}

inline void put(Vec3 p, float u, float v, Color c) { g_batch.push_back(Vtx{p.x, p.y, p.z, u, v, c.r, c.g, c.b, c.a}); }

Color shade(Color c, Vec3 n) {
    if (g_tintK > 0) c = lerpColor(c, withAlpha(g_tint, c.a), g_tintK);
    float k = 1.0f;
    if (!g_emissive) {
        float d = dot(n, kLight);
        k = 0.50f + 0.22f * (n.y * 0.5f + 0.5f) + 0.38f * (d > 0 ? d : 0);
    }
    auto ch = [k](uint8_t v) {
        float f = v * k;
        return (uint8_t)(f > 255 ? 255 : f);
    };
    return Color{ch(c.r), ch(c.g), ch(c.b), (uint8_t)(c.a * g_alpha)};
}

Vec3 xf(Vec3 p) { return xformPoint(g_stack.back(), p); }

void tri3(Vec3 a, Vec3 b, Vec3 c, Color col) {
    put(a, g_wu, g_wv, col);
    put(b, g_wu, g_wv, col);
    put(c, g_wu, g_wv, col);
}

void quad3(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 center, Color base) {
    Vec3 fc = (a + b + c + d) * 0.25f;
    Vec3 n = normalize(cross(b - a, d - a));
    if (dot(n, fc - center) < 0) n = -n;
    Color col = shade(base, n);
    tri3(a, b, c, col);
    tri3(a, c, d, col);
}

// 2D: точка холста -> мир/экран
Vec3 cv(float x, float y) { return xformPoint(g_canvas, v3(x, y, 0)); }

void put2(float x, float y, float u, float v, Color c) {
    c.a = (uint8_t)(c.a * g_alpha);
    put(cv(x, y), u, v, c);
}

void quad2uv(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c) {
    put2(x0, y0, u0, v0, c);
    put2(x1, y0, u1, v0, c);
    put2(x1, y1, u1, v1, c);
    put2(x0, y0, u0, v0, c);
    put2(x1, y1, u1, v1, c);
    put2(x0, y1, u0, v1, c);
}

const fontdata::Glyph* glyph(int cp, bool bold) {
    auto it = g_glyphIndex.find(cp);
    if (it == g_glyphIndex.end()) {
        it = g_glyphIndex.find('?');
        if (it == g_glyphIndex.end()) return nullptr;
    }
    return bold ? &fontdata::kBold[it->second] : &fontdata::kRegular[it->second];
}

int nextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    int cp = c, extra = 0;
    if (c >= 0xF0) cp = c & 0x07, extra = 3;
    else if (c >= 0xE0) cp = c & 0x0F, extra = 2;
    else if (c >= 0xC0) cp = c & 0x1F, extra = 1;
    i++;
    for (int k = 0; k < extra && i < s.size(); k++, i++) cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
    return cp;
}

void uploadAtlas() {
    using namespace fontdata;
    std::vector<uint8_t> lvl(kAtlasW * kAtlasH * 4);
    for (int i = 0; i < kAtlasW * kAtlasH; i++) {
        uint8_t b = kAtlas4[i / 2];
        uint8_t v = (i & 1) ? (b >> 4) : (b & 15);
        lvl[i * 4 + 0] = lvl[i * 4 + 1] = lvl[i * 4 + 2] = 255;
        lvl[i * 4 + 3] = (uint8_t)(v * 17);
    }
    glGenTextures(1, &g_atlas);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Мип-уровни считаем сами (в OpenGL 1.1 нет автогенерации)
    int w = kAtlasW, h = kAtlasH, level = 0;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, lvl.data());
    while (w > 1 || h > 1) {
        int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
        std::vector<uint8_t> next(nw * nh * 4);
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++) {
                int sum = 0;
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++) {
                        int sx = std::min(w - 1, x * 2 + dx), sy = std::min(h - 1, y * 2 + dy);
                        sum += lvl[(sy * w + sx) * 4 + 3];
                    }
                next[(y * nw + x) * 4 + 0] = next[(y * nw + x) * 4 + 1] = next[(y * nw + x) * 4 + 2] = 255;
                next[(y * nw + x) * 4 + 3] = (uint8_t)(sum / 4);
            }
        lvl.swap(next);
        w = nw;
        h = nh;
        glTexImage2D(GL_TEXTURE_2D, ++level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, lvl.data());
    }
    g_wu = (kWhite * 0.5f) / kAtlasW;
    g_wv = (kWhite * 0.5f) / kAtlasH;
}

void setMode(Mode m) {
    flushBatch();
    g_mode = m;
}
}  // namespace

bool init() {
    for (int i = 0; i < fontdata::kGlyphCount; i++) g_glyphIndex[fontdata::kRegular[i].cp] = i;
    uploadAtlas();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glDepthFunc(GL_LEQUAL);
    g_batch.reserve(1 << 16);
    return glGetError() == GL_NO_ERROR || true;
}

void beginFrame(int w, int h, Color c) {
    g_mode = Mode::None;
    glViewport(0, 0, w, h);
    glDepthMask(GL_TRUE);
    glClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void flush() { flushBatch(); }

void begin3D(const Mat4& proj, const Mat4& view) {
    setMode(Mode::World);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    resetXform();
    g_alpha = 1;
    g_tintK = 0;
    g_emissive = false;
}

void end3D() {
    flushBatch();
    glDepthMask(GL_TRUE);
}

void resetXform() { g_stack.assign(1, identity()); }
void push(const Mat4& m) { g_stack.push_back(g_stack.back() * m); }
void pop() {
    if (g_stack.size() > 1) g_stack.pop_back();
}
const Mat4& current() { return g_stack.back(); }
void setAlpha(float a) { g_alpha = a; }
void setTint(Color c, float k) {
    g_tint = c;
    g_tintK = k;
}
void setEmissive(bool e) { g_emissive = e; }
void setDepthWrite(bool on) {
    flushBatch();
    glDepthMask(on ? GL_TRUE : GL_FALSE);
}

void box(Vec3 c, Vec3 s, Color col) {
    float hx = s.x * 0.5f, hy = s.y * 0.5f, hz = s.z * 0.5f;
    Vec3 p[8];
    for (int i = 0; i < 8; i++)
        p[i] = xf(v3(c.x + ((i & 1) ? hx : -hx), c.y + ((i & 2) ? hy : -hy), c.z + ((i & 4) ? hz : -hz)));
    Vec3 wc = xf(c);
    quad3(p[0], p[1], p[3], p[2], wc, col);
    quad3(p[4], p[5], p[7], p[6], wc, col);
    quad3(p[0], p[2], p[6], p[4], wc, col);
    quad3(p[1], p[3], p[7], p[5], wc, col);
    quad3(p[0], p[1], p[5], p[4], wc, col);
    quad3(p[2], p[3], p[7], p[6], wc, col);
}

void cylinder(Vec3 base, float r, float h, Color col, int seg) {
    Vec3 wc = xf(v3(base.x, base.y + h * 0.5f, base.z));
    Vec3 bc = xf(base), tc = xf(v3(base.x, base.y + h, base.z));
    Vec3 up = normalize(tc - bc);
    Color ct = shade(col, up), cb = shade(col, -up);
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * kPi * i / seg, a1 = 2 * kPi * (i + 1) / seg;
        Vec3 b0 = xf(v3(base.x + std::cos(a0) * r, base.y, base.z + std::sin(a0) * r));
        Vec3 b1 = xf(v3(base.x + std::cos(a1) * r, base.y, base.z + std::sin(a1) * r));
        Vec3 t0 = xf(v3(base.x + std::cos(a0) * r, base.y + h, base.z + std::sin(a0) * r));
        Vec3 t1 = xf(v3(base.x + std::cos(a1) * r, base.y + h, base.z + std::sin(a1) * r));
        quad3(b0, b1, t1, t0, wc, col);
        tri3(tc, t0, t1, ct);
        tri3(bc, b1, b0, cb);
    }
}

// ---------- 2D ----------
void beginScreen2D(int w, int h) {
    setMode(Mode::Screen);
    glMatrixMode(GL_PROJECTION);
    Mat4 o = ortho(0, (float)w, (float)h, 0, -1, 1);
    glLoadMatrixf(o.m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    g_alpha = 1;
    g_canvas = identity();
}

void setCanvas2D(float ox, float oy, float s) { g_canvas = translate(ox, oy, 0) * scale(s, s, 1); }
void resetCanvas2D() { g_canvas = identity(); }

void beginCanvas3D(const Mat4& m) {
    flushBatch();
    g_mode = Mode::Canvas3D;
    g_canvas = m;
    glDepthMask(GL_FALSE);
}

void endCanvas() {
    flushBatch();
    if (g_mode == Mode::Canvas3D) {
        glDepthMask(GL_TRUE);
        g_mode = Mode::World;
    }
    g_canvas = identity();
}

void rect(Rect r, Color c) { quad2uv(r.x, r.y, r.x + r.w, r.y + r.h, g_wu, g_wv, g_wu, g_wv, c); }

void rectGrad(Rect r, Color top, Color bottom) {
    put2(r.x, r.y, g_wu, g_wv, top);
    put2(r.x + r.w, r.y, g_wu, g_wv, top);
    put2(r.x + r.w, r.y + r.h, g_wu, g_wv, bottom);
    put2(r.x, r.y, g_wu, g_wv, top);
    put2(r.x + r.w, r.y + r.h, g_wu, g_wv, bottom);
    put2(r.x, r.y + r.h, g_wu, g_wv, bottom);
}

void rectLines(Rect r, float t, Color c) {
    rect(Rect{r.x, r.y, r.w, t}, c);
    rect(Rect{r.x, r.y + r.h - t, r.w, t}, c);
    rect(Rect{r.x, r.y + t, t, r.h - 2 * t}, c);
    rect(Rect{r.x + r.w - t, r.y + t, t, r.h - 2 * t}, c);
}

void triangle(Vec2 a, Vec2 b, Vec2 c, Color col) {
    put2(a.x, a.y, g_wu, g_wv, col);
    put2(b.x, b.y, g_wu, g_wv, col);
    put2(c.x, c.y, g_wu, g_wv, col);
}

static void arc(Vec2 c, float r, float a0, float a1, Color col, int seg) {
    for (int i = 0; i < seg; i++) {
        float t0 = a0 + (a1 - a0) * i / seg, t1 = a0 + (a1 - a0) * (i + 1) / seg;
        triangle(c, Vec2{c.x + std::cos(t0) * r, c.y + std::sin(t0) * r},
                 Vec2{c.x + std::cos(t1) * r, c.y + std::sin(t1) * r}, col);
    }
}

void circle(Vec2 c, float r, Color col, int seg) { arc(c, r, 0, 2 * kPi, col, seg); }

void roundRect(Rect r, float rad, Color c) {
    rad = std::min(rad, std::min(r.w, r.h) * 0.5f);
    if (rad < 0.5f) {
        rect(r, c);
        return;
    }
    rect(Rect{r.x + rad, r.y, r.w - 2 * rad, r.h}, c);
    rect(Rect{r.x, r.y + rad, rad, r.h - 2 * rad}, c);
    rect(Rect{r.x + r.w - rad, r.y + rad, rad, r.h - 2 * rad}, c);
    arc(Vec2{r.x + rad, r.y + rad}, rad, kPi, 1.5f * kPi, c, 6);
    arc(Vec2{r.x + r.w - rad, r.y + rad}, rad, 1.5f * kPi, 2 * kPi, c, 6);
    arc(Vec2{r.x + r.w - rad, r.y + r.h - rad}, rad, 0, 0.5f * kPi, c, 6);
    arc(Vec2{r.x + rad, r.y + r.h - rad}, rad, 0.5f * kPi, kPi, c, 6);
}

void line(Vec2 a, Vec2 b, float t, Color c) {
    float dx = b.x - a.x, dy = b.y - a.y, l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * t * 0.5f, ny = dx / l * t * 0.5f;
    triangle(Vec2{a.x + nx, a.y + ny}, Vec2{b.x + nx, b.y + ny}, Vec2{b.x - nx, b.y - ny}, c);
    triangle(Vec2{a.x + nx, a.y + ny}, Vec2{b.x - nx, b.y - ny}, Vec2{a.x - nx, a.y - ny}, c);
}

float text(const std::string& s, float x, float y, float size, Color c, bool bold) {
    float sc = size / fontdata::kBake;
    float cx = x;
    size_t i = 0;
    while (i < s.size()) {
        int cp = nextCodepoint(s, i);
        if (cp == '\n') {
            cx = x;
            y += size * 1.25f;
            continue;
        }
        const fontdata::Glyph* g = glyph(cp, bold);
        if (!g) continue;
        if (g->w > 0 && g->h > 0 && cp != ' ') {
            float x0 = cx + g->xo * sc, y0 = y + g->yo * sc;
            float u0 = (float)g->x / fontdata::kAtlasW, v0 = (float)g->y / fontdata::kAtlasH;
            float u1 = (float)(g->x + g->w) / fontdata::kAtlasW, v1 = (float)(g->y + g->h) / fontdata::kAtlasH;
            quad2uv(x0, y0, x0 + g->w * sc, y0 + g->h * sc, u0, v0, u1, v1, c);
        }
        cx += g->adv * sc;
    }
    return cx - x;
}

float textWidth(const std::string& s, float size, bool bold) {
    float sc = size / fontdata::kBake, w = 0, best = 0;
    size_t i = 0;
    while (i < s.size()) {
        int cp = nextCodepoint(s, i);
        if (cp == '\n') {
            best = std::max(best, w);
            w = 0;
            continue;
        }
        const fontdata::Glyph* g = glyph(cp, bold);
        if (g) w += g->adv * sc;
    }
    return std::max(best, w);
}

void textCentered(const std::string& s, Rect r, float size, Color c, bool bold) {
    float w = textWidth(s, size, bold);
    text(s, r.x + (r.w - w) * 0.5f, r.y + (r.h - size * 1.15f) * 0.5f, size, c, bold);
}

void textRight(const std::string& s, float right, float y, float size, Color c, bool bold) {
    text(s, right - textWidth(s, size, bold), y, size, c, bold);
}

float textWrapped(const std::string& s, float x, float y, float w, float size, Color c, bool draw) {
    float lineH = size * 1.3f, cy = y;
    size_t start = 0;
    while (true) {
        size_t nl = s.find('\n', start);
        std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        std::string line;
        size_t p = 0;
        while (true) {
            size_t sp = para.find(' ', p);
            std::string word = para.substr(p, sp == std::string::npos ? std::string::npos : sp - p);
            std::string test = line.empty() ? word : line + " " + word;
            if (!line.empty() && textWidth(test, size) > w) {
                if (draw) text(line, x, cy, size, c);
                cy += lineH;
                line = word;
            } else {
                line = test;
            }
            if (sp == std::string::npos) break;
            p = sp + 1;
        }
        if (draw) text(line, x, cy, size, c);
        cy += lineH;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return cy - y;
}

bool screenshot(const std::string& path, int w, int h) {
    flushBatch();
    std::vector<unsigned char> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return plat::saveScreenshotBMP(path, w, h, px.data());
}

}  // namespace gfx
