// ConsoleMiniGame.cpp
#include "ConsoleMiniGame.h"
#include <cstdio>
#include <cstring>

namespace lrh {

namespace {
// Спрайты: '0'..'3' - индекс палитры, '.' - прозрачно.
const char* const kPlayerCar[] = {"..333..", ".32223.", ".30003.", ".32223.", "3322233", "3322233",
                                  ".32223.", ".32223.", ".30003.", ".32223.", "..333.."};
const char* const kEnemyCar[]  = {"..333..", ".33333.", ".30003.", ".33333.", "3333333", "3333333",
                                  ".33333.", ".33333.", ".30003.", ".33333.", "..333.."};
const char* const kRock[] = {"..33..", ".3223.", "322223", "322223", ".3333."};
const char* const kCone[] = {".333.", "33033", "30003", "33033", ".333."};
const char* const kOil[]  = {"..2222..", ".222222.", "22202222", "22222222", ".222222.", "..2222.."};
const char* const kFuel[] = {"..333..", "..3.3..", "3333333", "3000003", "3033303", "3000003", "3333333"};
const char* const kFilm[] = {"..333..", "3333333", "3300033", "3303033", "3300033", "3333333"};

const uint32_t kPal[4] = {0xD8F0B0, 0x9CC878, 0x4C8058, 0x143828};

uint32_t hash2(int a, int b) {
    uint32_t h = uint32_t(a) * 73856093u ^ uint32_t(b) * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
}  // namespace

void ConsoleMiniGame::reset() {
    objs_.clear();
    x_ = 32.0f; vx_ = 0.0f; scroll_ = 0.0f;
    spawnT_ = 1.5f; sinceFuel_ = 0.0f; sinceFilm_ = 0.0f;
    slowT_ = invulnT_ = skidT_ = shakeT_ = popupT_ = clock_ = 0.0f;
    popup_.clear();
    std::memset(lcd_, 0, sizeof(lcd_));
}

void ConsoleMiniGame::spawn(const SharedState& s) {
    Type type;
    float fuelGap = s.fuel < 0.3f ? 8.0f : (s.fuel < 0.6f ? 15.0f : 28.0f);
    if (sinceFuel_ > fuelGap) type = Type::Fuel;
    else if ((s.film == 0 && sinceFilm_ > 12.0f) || sinceFilm_ > 28.0f) type = Type::Film;
    else {
        float r = g_rng.next01();
        type = r < 0.35f ? Type::Rock : (r < 0.60f ? Type::Cone : (r < 0.80f ? Type::Oil : Type::Car));
    }
    Obj o{type, 0, -12.0f, 6, 5};
    switch (type) {
        case Type::Fuel: o.w = 7; o.h = 7; break;
        case Type::Film: o.w = 7; o.h = 6; break;
        case Type::Rock: o.w = 6; o.h = 5; break;
        case Type::Cone: o.w = 5; o.h = 5; break;
        case Type::Oil:  o.w = 8; o.h = 6; break;
        case Type::Car:  o.w = 7; o.h = 11; break;
    }
    for (int attempt = 0; attempt < 6; ++attempt) {
        o.x = g_rng.range(float(kRoadL + 3), float(kRoadR - 3 - o.w));
        bool clash = false;
        for (const Obj& other : objs_)
            if (other.y < 14.0f && std::fabs(other.x - o.x) < 10.0f) clash = true;
        if (!clash) break;
    }
    if (type == Type::Fuel) sinceFuel_ = 0.0f;
    if (type == Type::Film) sinceFilm_ = 0.0f;
    objs_.push_back(o);
}

void ConsoleMiniGame::update(float dt, SharedState& s, Audio& audio, bool hasControl, bool left, bool right) {
    clock_ += dt;
    slowT_ = std::max(0.0f, slowT_ - dt);
    invulnT_ = std::max(0.0f, invulnT_ - dt);
    shakeT_ = std::max(0.0f, shakeT_ - dt);
    popupT_ = std::max(0.0f, popupT_ - dt);
    sinceFuel_ += dt; sinceFilm_ += dt;

    const float diff = saturate(s.time / 200.0f);
    const float v = (34.0f + 26.0f * diff) * clampf(s.carSpeed, 0.0f, 1.0f);  // пикс/с
    scroll_ += v * dt;

    // --- управление ---
    float dir = hasControl ? float((right ? 1 : 0) - (left ? 1 : 0)) : 0.0f;
    if (skidT_ > 0.0f) { skidT_ -= dt; vx_ = skidDir_ * 36.0f; }
    else vx_ += (dir * 52.0f - vx_) * saturate(dt * 14.0f);
    x_ = clampf(x_ + vx_ * dt, float(kRoadL + 5), float(kRoadR - 5));

    // --- спавн ---
    if (!s.stalled) {
        spawnT_ -= dt * (s.carSpeed > 0.2f ? 1.0f : 0.0f);
        if (spawnT_ <= 0.0f) { spawn(s); spawnT_ = lerp(1.5f, 0.85f, diff) * g_rng.range(0.8f, 1.25f); }
    }

    // --- объекты и столкновения ---
    const float carTop = float(LH - 20);
    for (size_t i = 0; i < objs_.size();) {
        Obj& o = objs_[i];
        o.y += (v - (o.type == Type::Car ? v * 0.5f : 0.0f)) * dt;
        bool remove = o.y > float(LH + 2);

        float cx0 = x_ - 2.5f, cx1 = x_ + 2.5f, cy0 = carTop + 1, cy1 = carTop + 10;
        bool hit = !remove && cx0 < o.x + o.w && cx1 > o.x && cy0 < o.y + o.h && cy1 > o.y;
        if (hit) {
            switch (o.type) {
                case Type::Fuel:
                    s.fuel = std::min(1.0f, s.fuel + kFuelCan);
                    popup_ = "+FUEL"; popupT_ = 1.2f; audio.play(Sfx::Pickup);
                    remove = true; break;
                case Type::Film:
                    if (s.film < kMaxFilm) { ++s.film; popup_ = "+FILM"; } else popup_ = "FILM FULL";
                    popupT_ = 1.2f; audio.play(Sfx::Pickup);
                    remove = true; break;
                case Type::Oil:
                    skidT_ = 0.9f; skidDir_ = (vx_ >= 0.0f) ? 1.0f : -1.0f;
                    audio.play(Sfx::Skid, 0.8f);
                    remove = true; break;
                default:  // камень, конус, машина
                    remove = true;
                    if (invulnT_ <= 0.0f) {
                        s.fuel = std::max(0.0f, s.fuel - 0.03f);
                        slowT_ = 1.4f; invulnT_ = 1.2f; shakeT_ = 0.35f; s.bump = 1.0f;
                        popup_ = "CRASH!"; popupT_ = 1.0f;
                        audio.play(Sfx::Crash, 0.9f);
                    }
                    break;
            }
        }
        if (remove) { objs_[i] = objs_.back(); objs_.pop_back(); }
        else ++i;
    }
}

void ConsoleMiniGame::lcdRect(int x, int y, int w, int h, uint8_t c) {
    for (int j = std::max(0, y); j < std::min(LH, y + h); ++j)
        for (int i = std::max(0, x); i < std::min(LW, x + w); ++i) lcd_[j * LW + i] = c;
}

void ConsoleMiniGame::spr(const char* const* rows, int h, int x, int y) {
    for (int j = 0; j < h; ++j) {
        for (int i = 0; rows[j][i]; ++i) {
            char ch = rows[j][i];
            int px = x + i, py = y + j;
            if (ch == '.' || px < 0 || px >= LW || py < 0 || py >= LH) continue;
            lcd_[py * LW + px] = uint8_t(ch - '0');
        }
    }
}

void ConsoleMiniGame::drawLcd(const SharedState& s) {
    const int sc = int(scroll_);
    // трава и дорога
    for (int y = 0; y < LH; ++y) {
        int row = y - sc;
        for (int x = 0; x < LW; ++x) {
            uint8_t c;
            if (x < kRoadL || x >= kRoadR) c = (hash2(x, row) % 23 == 0) ? 2 : 1;
            else if (x == kRoadL || x == kRoadR - 1) c = 2;
            else if (x == 32 && (((row % 14) + 14) % 14) < 7) c = 1;
            else c = 0;
            lcd_[y * LW + x] = c;
        }
    }
    // кусты по краям
    for (int k = 0; k < 4; ++k) {
        int by = ((k * 23 + sc) % (LH + 12)) - 6;
        int bx = (k & 1) ? 54 : 3;
        for (int j = -2; j <= 2; ++j)
            for (int i = -2; i <= 2; ++i)
                if (i * i + j * j <= 5) {
                    int px = bx + i, py = by + j;
                    if (px >= 0 && px < LW && py >= 0 && py < LH) lcd_[py * LW + px] = (i + j) & 1 ? 3 : 2;
                }
    }
    // объекты
    for (const Obj& o : objs_) {
        int ox = int(o.x + 0.5f), oy = int(o.y + 0.5f);
        switch (o.type) {
            case Type::Fuel: spr(kFuel, 7, ox, oy + int(std::sin(clock_ * 8.0f) * 1.0f + 0.5f)); break;
            case Type::Film: spr(kFilm, 6, ox, oy + int(std::sin(clock_ * 8.0f) * 1.0f + 0.5f)); break;
            case Type::Rock: spr(kRock, 5, ox, oy); break;
            case Type::Cone: spr(kCone, 5, ox, oy); break;
            case Type::Oil:  spr(kOil, 6, ox, oy); break;
            case Type::Car:  spr(kEnemyCar, 11, ox, oy); break;
        }
    }
    // игрок (мигает после аварии)
    bool blink = invulnT_ > 0.0f && (int(clock_ * 14.0f) & 1);
    if (!blink) spr(kPlayerCar, 11, int(x_ - 3.0f + 0.5f), LH - 20);

    // верхняя плашка (HUD рисуется текстом поверх при выводе)
    lcdRect(0, 0, LW, 8, 3);
    // нижний индикатор пути
    lcdRect(0, LH - 3, LW, 3, 3);
    lcdRect(1, LH - 2, int((LW - 2) * saturate(s.progress)), 1, 0);
}

void ConsoleMiniGame::render(Renderer& r, const SharedState& s, float slide, float threat) {
    if (slide <= 0.001f) return;
    drawLcd(s);
    const int oy = int((1.0f - smoothstep(slide)) * float(kH + 12));
    const int bodyX = 84, bodyW = 152;
    const int scrX = 96, scrY = 16 + oy;

    // подсветка от экрана
    r.ellipse(160, 100.0f + oy, 135, 105, 0xA8D890, 0.10f * slide);
    // корпус
    r.rect(bodyX, oy, bodyW, kH + 8, 0x3A3C44);
    r.rect(bodyX, oy, 2, kH + 8, 0x5A5D68);
    r.rect(bodyX + bodyW - 2, oy, 2, kH + 8, 0x20222A);
    r.text(bodyX + 10, oy + 2, "A/D STEER  TAB: BACK", 0x70747F);
    // рамка экрана
    r.rect(90, 10 + oy, 140, 168, 0x14181A);
    // светодиод опасности: чем выше угроза, тем чаще мигает (периферийное предупреждение)
    float blinkRate = 2.0f + threat * 10.0f;
    bool on = threat > 0.28f && (std::sin(clock_ * blinkRate * kPi) > 0.0f);
    r.ellipse(220, 12.0f + oy, 2, 2, on ? 0xFF2A2A : 0x3A1214);

    // экран консоли: 2x масштаб с лёгкой тряской при аварии
    int jx = 0, jy = 0;
    if (shakeT_ > 0.0f) { jx = g_rng.irange(-1, 1); jy = g_rng.irange(-1, 1); }
    for (int y = 0; y < LH; ++y)
        for (int x = 0; x < LW; ++x) {
            uint32_t c = kPal[lcd_[y * LW + x]];
            int px = scrX + x * 2 + jx, py = scrY + y * 2 + jy;
            r.rect(px, py, 2, 2, c);
        }
    // сетка пикселей LCD (тонкие линии)
    for (int y = 0; y < LH; ++y) r.rect(scrX, scrY + y * 2 + 1, 128, 1, 0x143828, 0.10f);

    // HUD поверх экрана
    r.text(scrX + 3, scrY + 4, "FUEL", kPal[0]);
    r.rectOutline(scrX + 30, scrY + 3, 52, 8, kPal[0]);
    uint32_t fc = (s.fuel < 0.2f && (int(clock_ * 6.0f) & 1)) ? kPal[1] : kPal[0];
    r.rect(scrX + 32, scrY + 5, int(48 * saturate(s.fuel)), 4, fc);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "CAM x%d", s.film);
    r.text(scrX + 88, scrY + 4, buf, kPal[0]);
    if (popupT_ > 0.0f) r.textCentered(scrX + 64, scrY + 30, popup_.c_str(), kPal[3]);
}

}  // namespace lrh
