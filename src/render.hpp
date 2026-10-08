// Рендер на OpenGL 1.1 (есть в любой Windows без драйверных расширений):
// пакеты вершин, низкополигональные фигуры с освещением, 2D-холсты и текст.
#pragma once
#include "core.hpp"
#include <string>

namespace gfx {

bool init();
void beginFrame(int w, int h, Color clear);
void flush();

// ---------- 3D ----------
void begin3D(const Mat4& proj, const Mat4& view);
void end3D();
void resetXform();
void push(const Mat4& m);   // m действует внутри текущей трансформации
void pop();
const Mat4& current();
void setAlpha(float a);
void setTint(Color c, float k);
void setEmissive(bool e);
void setDepthWrite(bool on);

void box(Vec3 center, Vec3 size, Color c);
void cylinder(Vec3 base, float radius, float height, Color c, int seg = 12);   // ось Y

// ---------- 2D-холст ----------
// Холст — плоскость с координатами в «пикселях». Он либо выводится на экран,
// либо кладётся на поверхность в 3D (экран монитора, телефон в руке).
void beginScreen2D(int w, int h);                  // ортографическая проекция на окно
void beginCanvas3D(const Mat4& canvasToWorld);      // внутри begin3D: холст на плоскости
void endCanvas();
void setCanvas2D(float offsetX, float offsetY, float scale);   // сдвиг/масштаб холста на экране
void resetCanvas2D();
void rect(Rect r, Color c);
void rectGrad(Rect r, Color top, Color bottom);
void rectLines(Rect r, float t, Color c);
void roundRect(Rect r, float radius, Color c);
void circle(Vec2 c, float r, Color col, int seg = 24);
void line(Vec2 a, Vec2 b, float t, Color c);
void triangle(Vec2 a, Vec2 b, Vec2 c, Color col);

float text(const std::string& s, float x, float y, float size, Color c, bool bold = false);
float textWidth(const std::string& s, float size, bool bold = false);
void textCentered(const std::string& s, Rect r, float size, Color c, bool bold = false);
void textRight(const std::string& s, float right, float y, float size, Color c, bool bold = false);
float textWrapped(const std::string& s, float x, float y, float w, float size, Color c, bool draw = true);

bool screenshot(const std::string& path, int w, int h);

}  // namespace gfx
