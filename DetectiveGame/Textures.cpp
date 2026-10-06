// Textures.cpp
#include "Textures.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include "Renderer.h" // Rgb()

namespace {

// Детерминированный шум 0..255 по координатам
int Noise(int x, int y, int seed) {
    uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(seed) * 2246822519u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<int>((n ^ (n >> 16)) & 255u);
}

// Множитель яркости цвета
uint32_t Mul(uint32_t c, double k) {
    int r = static_cast<int>(((c >> 16) & 255) * k);
    int g = static_cast<int>(((c >> 8) & 255) * k);
    int b = static_cast<int>((c & 255) * k);
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return Rgb(r, g, b);
}

// Лёгкая случайная вариация яркости пикселя: 1 +- amount
double Jitter(int x, int y, int seed, double amount) {
    return 1.0 + ((Noise(x, y, seed) / 127.5) - 1.0) * amount;
}

Texture Make(const std::function<uint32_t(int, int)>& f) {
    Texture t;
    t.px.resize(Texture::kSize * Texture::kSize);
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x)
            t.px[y * Texture::kSize + x] = f(x, y);
    return t;
}

// ---------- Стены ----------

Texture MakeBrick() {
    return Make([](int x, int y) {
        int row = y / 8;
        int xs = x + (row % 2) * 8;
        int bx = xs % 16;
        if (y % 8 == 0 || bx == 0) return Mul(Rgb(88, 82, 76), Jitter(x, y, 1, 0.15));
        double tone = 0.8 + Noise(xs / 16, row, 7) / 255.0 * 0.4;
        return Mul(Rgb(150, 70, 55), tone * Jitter(x, y, 2, 0.12));
    });
}

Texture MakeWood() {
    return Make([](int x, int y) {
        int px = x % 16;
        if (px == 0) return Rgb(40, 24, 12);
        if (y < 3 || y > 60) return Mul(Rgb(80, 50, 26), Jitter(x, y, 3, 0.1));
        double grain = 0.9 + 0.12 * std::sin(y * 0.35 + (x / 16) * 3.1 + Noise(x / 16, 0, 4) * 0.05);
        return Mul(Rgb(118, 76, 38), grain * Jitter(x, y, 5, 0.08));
    });
}

Texture MakeStone() {
    return Make([](int x, int y) {
        int row = y / 16;
        int xs = x + (row % 2) * 16;
        if (y % 16 == 0 || xs % 32 == 0) return Mul(Rgb(60, 62, 68), Jitter(x, y, 6, 0.2));
        double tone = 0.85 + Noise(xs / 32, row, 8) / 255.0 * 0.3;
        double speck = Noise(x, y, 9) > 245 ? 0.7 : 1.0;
        return Mul(Rgb(125, 128, 135), tone * speck * Jitter(x, y, 10, 0.14));
    });
}

Texture MakeWallpaper() {
    return Make([](int x, int y) {
        if (y >= 46 && y <= 49) return Mul(Rgb(70, 45, 30), Jitter(x, y, 11, 0.1));
        if (y > 49) { // деревянная панель внизу
            if (x % 8 == 0) return Rgb(30, 20, 12);
            return Mul(Rgb(90, 58, 36), Jitter(x, y, 12, 0.1));
        }
        uint32_t c = Rgb(74, 112, 86);
        if (x % 16 < 3) c = Rgb(92, 132, 102);
        if (std::abs(x % 16 - 8) + std::abs(y % 16 - 8) < 3) c = Rgb(96, 136, 104);
        double stain = 0.8 + Noise(x / 8, y / 8, 13) / 255.0 * 0.3; // выцветшие пятна
        return Mul(c, stain * Jitter(x, y, 14, 0.06));
    });
}

Texture MakeFurniture() {
    return Make([](int x, int y) {
        bool border = x < 3 || x > 60 || y < 3 || y > 60;
        if (border) return Mul(Rgb(120, 80, 46), Jitter(x, y, 15, 0.1));
        int dx = x - 32, dy = y - 32;
        if (dx * dx + dy * dy < 12) return Rgb(200, 165, 70); // латунная ручка
        double grain = 0.88 + 0.12 * std::sin(x * 0.3 + Noise(0, y, 16) * 0.04);
        return Mul(Rgb(88, 56, 32), grain * Jitter(x, y, 17, 0.07));
    });
}

Texture MakeConcrete() {
    return Make([](int x, int y) {
        if (x % 32 == 0 || y % 32 == 0) return Mul(Rgb(70, 74, 82), Jitter(x, y, 18, 0.15));
        if (x % 32 == 1 || y % 32 == 1) return Mul(Rgb(135, 140, 148), 1.0);
        double streak = 1.0 - 0.25 * (Noise(x / 3, 0, 19) / 255.0) * (y / 64.0); // потёки грязи
        return Mul(Rgb(112, 116, 124), streak * Jitter(x, y, 20, 0.1));
    });
}

Texture MakeGlass() {
    return Make([](int x, int y) {
        if (x % 32 < 2 || y < 2 || y > 61) return Mul(Rgb(40, 45, 52), Jitter(x, y, 21, 0.1));
        uint32_t c = MixColor(Rgb(60, 105, 150), Rgb(95, 150, 190), y / 64.0);
        if ((x + y) % 24 < 4) c = Mul(c, 1.35); // блик
        return Mul(c, Jitter(x, y, 22, 0.04));
    });
}

// ---------- Полы и потолки ----------

Texture MakeParquet() {
    return Make([](int x, int y) {
        int row = y / 8;
        int xs = x + (row % 2) * 16;
        if (y % 8 == 0 || xs % 32 == 0) return Rgb(45, 28, 14);
        double tone = 0.8 + Noise(xs / 32, row, 23) / 255.0 * 0.35;
        double grain = 0.92 + 0.08 * std::sin(xs * 0.5);
        return Mul(Rgb(140, 92, 48), tone * grain * Jitter(x, y, 24, 0.06));
    });
}

Texture MakeOfficeTiles() {
    return Make([](int x, int y) {
        if (x % 32 == 0 || y % 32 == 0) return Rgb(80, 82, 88);
        bool alt = ((x / 32) + (y / 32)) % 2 == 0;
        return Mul(alt ? Rgb(150, 152, 158) : Rgb(132, 136, 146), Jitter(x, y, 25, 0.06));
    });
}

Texture MakePlanks() {
    return Make([](int x, int y) {
        int row = y / 16;
        int xs = x + (row * 23) % 64;
        if (y % 16 == 0 || xs % 64 == 0) return Rgb(30, 20, 14);
        double tone = 0.75 + Noise(xs / 64, row, 26) / 255.0 * 0.4;
        return Mul(Rgb(100, 74, 52), tone * Jitter(x, y, 27, 0.08));
    });
}

Texture MakeLinoleum() {
    return Make([](int x, int y) {
        bool alt = ((x / 16) + (y / 16)) % 2 == 0;
        double worn = 0.85 + Noise(x / 4, y / 4, 28) / 255.0 * 0.25;
        return Mul(alt ? Rgb(172, 166, 140) : Rgb(108, 120, 100), worn * Jitter(x, y, 29, 0.05));
    });
}

Texture MakePlaster(bool beams) {
    return Make([beams](int x, int y) {
        if (beams && x % 32 < 5) return Mul(Rgb(60, 40, 25), Jitter(x, y, 30, 0.12));
        return Mul(Rgb(150, 145, 135), Jitter(x, y, 31, 0.1) * (0.9 + Noise(x / 8, y / 8, 32) / 255.0 * 0.15));
    });
}

struct Pack {
    Texture wall[7];
    Texture floor[4];
    Texture ceiling[4];
    Pack() {
        wall[0] = MakeBrick();      // '1'
        wall[1] = MakeWood();       // '2'
        wall[2] = MakeStone();      // '3'
        wall[3] = MakeWallpaper();  // '4'
        wall[4] = MakeFurniture();  // '5'
        wall[5] = MakeConcrete();   // '6'
        wall[6] = MakeGlass();      // '7'
        floor[0] = MakeParquet();
        floor[1] = MakeOfficeTiles();
        floor[2] = MakePlanks();
        floor[3] = MakeLinoleum();
        ceiling[0] = MakePlaster(true);
        ceiling[1] = MakePlaster(false);
        ceiling[2] = MakePlaster(true);
        ceiling[3] = MakePlaster(false);
    }
};

const Pack& GetPack() {
    static Pack pack; // создаётся при первом вызове
    return pack;
}

} // namespace

namespace Textures {

const Texture& Wall(char cell) {
    int i = cell - '1';
    if (i < 0 || i > 6) i = 0;
    return GetPack().wall[i];
}

const Texture& Floor(int levelId) {
    if (levelId < 0 || levelId > 3) levelId = 0;
    return GetPack().floor[levelId];
}

const Texture& Ceiling(int levelId) {
    if (levelId < 0 || levelId > 3) levelId = 0;
    return GetPack().ceiling[levelId];
}

} // namespace Textures
