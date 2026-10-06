// RealWorldScene.cpp
#include "RealWorldScene.h"
#include <cstdio>

namespace lrh {

namespace {
// ---- палитра ночи ----
const uint32_t kBody     = 0x0E1016;  // салон
const uint32_t kHeadlin  = 0x161820;  // потолок
const uint32_t kSeat     = 0x090A0F;
const uint32_t kSilhou   = 0x040509;
const uint32_t kSkyTop   = 0x04060F;
const uint32_t kSkyHor   = 0x141A30;
const uint32_t kGround   = 0x06070A;
const uint32_t kLampCol  = 0xFFB060;
const uint32_t kFog      = 0x0A0E1E;

// ---- геометрия вида ----
constexpr float kVpX = 160.0f, kVpY = 58.0f;   // точка схода в лобовом стекле
constexpr float kRoadLat = 3.4f;               // полуширина дороги в «единицах»
constexpr float kScreenK = 36.0f;              // пикселей на единицу на z = 1

// Центры окон, где появляется монстр.
constexpr float kLeftWinX = 41.0f, kRightWinX = 278.0f, kWinY = 66.0f;

float slotX(Slot s) { return s == Slot::Left ? kLeftWinX : (s == Slot::Right ? kRightWinX : 160.0f); }

struct Rect { int x, y, w, h; };
const Rect kWindshield = {88, 26, 144, 68};
const Rect kLeftWin    = {8, 24, 66, 94};
const Rect kRightWin   = {246, 24, 66, 94};

// ---- вспомогательное рисование монстра ----
void drawFace(Renderer& r, float cx, float cy, float s, float mouth, float fade, bool flip) {
    const float d = flip ? -1.0f : 1.0f;
    uint32_t skin = scaleColor(rgb(178, 180, 166), fade);
    uint32_t shade = scaleColor(rgb(110, 112, 104), fade);
    uint32_t dark = scaleColor(rgb(3, 3, 5), fade);
    uint32_t glint = scaleColor(rgb(255, 255, 230), fade);
    r.ellipse(cx, cy - 3 * s * d, 17 * s, 22 * s, dark);                 // волосы
    r.ellipse(cx, cy, 12.5f * s, 16.0f * s, skin);                        // лицо
    r.ellipse(cx, cy + 7 * s * d, 10.0f * s, 8.0f * s, shade, 0.55f);     // тень под скулами
    for (int sx = -1; sx <= 1; sx += 2) {
        float ex = cx + sx * 5.2f * s, ey = cy - 3.0f * s * d;
        r.ellipse(ex, ey, 3.4f * s, 5.0f * s, dark);
        r.put(int(ex), int(ey - 1.0f * s * d), glint);
        if (s > 1.4f) r.put(int(ex) + 1, int(ey - 1.0f * s * d), glint);
    }
    r.line(int(cx), int(cy + 1 * s * d), int(cx), int(cy + 5 * s * d), dark);  // нос
    float my = cy + 10.0f * s * d, mh = (1.3f + mouth * 6.0f) * s;
    r.ellipse(cx, my, 7.5f * s, mh, dark);
    for (int i = -3; i <= 3; ++i) {  // зубы
        float px = cx + i * 2.0f * s;
        r.line(int(px), int(my - mh * 0.8f), int(px), int(my - mh * 0.8f + 2.0f * s), glint);
    }
}

void drawHand(Renderer& r, float x, float y, float angle, float s, float fade) {
    uint32_t col = scaleColor(rgb(120, 122, 112), fade);
    r.ellipse(x, y, 4.0f * s, 5.0f * s, col);
    for (int k = -2; k <= 2; ++k) {
        float a = angle + k * 0.30f, len = 12.0f * s * (1.0f - std::fabs(float(k)) * 0.10f);
        r.thickLine(x, y, x + std::cos(a) * len, y + std::sin(a) * len, 2.6f * s, 1.2f * s, col);
    }
}

void drawArm(Renderer& r, float x0, float y0, float x1, float y1, float w0, float w1, float fade) {
    r.thickLine(x0, y0, x1, y1, w0, w1, scaleColor(rgb(60, 62, 58), fade));
}
}  // namespace

// ============================================================================
void RealWorldScene::reset() {
    monster_.reset();
    props_.clear();
    scroll_ = clock_ = 0; spawnZ_ = 0; propCounter_ = poleCounter_ = 0;
    lightPh_ = 1.0f; lightSide_ = -1; oncT_ = 0; oncNext_ = 18.0f;
    flashT_ = shutterT_ = 0; shakeX_ = shakeY_ = 0; thud_ = 0; stepPh_ = 0;
    talkT_ = 1.0f; speaker_ = 0; talkLevel_ = 0; silenceT_ = 0; argue_ = 0.5f; hintT_ = 0;
    // стартовый ряд придорожных объектов
    for (float z = 1.0f; z < 14.0f; z += 3.3f) {
        int side = (propCounter_++ & 1) ? 1 : -1;
        props_.push_back({z, side, g_rng.chance(0.5f) ? 0 : g_rng.irange(1, 2)});
    }
    spawnZ_ = 14.0f;
    // звёзды (фиксированные)
    Rng sr; sr.s = 777;
    starCount_ = 0;
    for (int i = 0; i < 70; ++i) {
        if (i < 45) stars_[starCount_++] = {sr.range(100, 220), sr.range(28, 54)};  // лобовое
        else        stars_[starCount_++] = {sr.range(126, 192), sr.range(4, 19)};   // люк
    }
}

bool RealWorldScene::fireCamera(SharedState& s, Audio& audio) {
    if (s.camCooldown > 0.0f || s.film <= 0) {
        audio.play(Sfx::Click, 0.6f);
        return false;
    }
    --s.film;
    s.camCooldown = kCamCooldown;
    flashT_ = 1.0f; shutterT_ = 1.0f;
    audio.play(Sfx::Flash, 0.9f);
    monster_.onFlash();
    silenceT_ = 2.5f;  // родители на секунду замолкают от неожиданной вспышки
    return true;
}

void RealWorldScene::update(float dt, SharedState& s, Audio& audio, bool threats) {
    clock_ += dt;
    hintT_ += dt;
    flashT_ = std::max(0.0f, flashT_ - dt * 2.6f);
    shutterT_ = std::max(0.0f, shutterT_ - dt * 3.0f);
    thud_ = std::max(0.0f, thud_ - dt * 5.0f);
    silenceT_ = std::max(0.0f, silenceT_ - dt);

    // --- монстр и его события ---
    if (threats) monster_.update(dt, s.time);
    for (const MEvent& e : monster_.events()) {
        switch (e.type) {
            case MEventType::Step:   audio.play(Sfx::Step, 0.9f, e.pan * 0.8f); thud_ = 1.0f; stepPh_ += 1.0f; break;
            case MEventType::Arrive: audio.play(Sfx::Step, 1.0f, e.pan); thud_ = 1.0f; break;
            case MEventType::Knock:  audio.play(Sfx::Knock, 0.9f, e.pan * 0.9f); break;
            case MEventType::Hiss:   audio.play(Sfx::Hiss, 0.8f, e.pan * 0.8f); break;
            case MEventType::Crack:  audio.play(Sfx::Crack, 0.9f, e.pan * 0.9f); break;
            case MEventType::Scared: audio.play(Sfx::Hiss, 1.0f, e.pan); thud_ = 1.0f; break;
            case MEventType::Reached: break;  // обрабатывает Game
        }
    }
    monster_.events().clear();
    audio.scrapeVol = threats ? monster_.scrapeLevel() : 0.0f;
    audio.scrapePan = monster_.scrapePan();

    // --- придорожные объекты ---
    const float speed = 7.0f * s.carSpeed;
    scroll_ += speed * dt;
    for (Prop& p : props_) {
        float before = p.z;
        p.z -= speed * dt;
        if (p.kind == 0 && before > 0.3f && p.z <= 0.3f) {  // фонарь проехал мимо: свет в салоне
            lightPh_ = 0.0f; lightSide_ = p.side;
            if ((++poleCounter_ & 1) == 0) audio.play(Sfx::Whoosh, 0.10f, p.side * 0.8f);
        }
    }
    props_.erase(std::remove_if(props_.begin(), props_.end(), [](const Prop& p) { return p.z < -6.0f; }), props_.end());
    spawnZ_ -= speed * dt;
    if (spawnZ_ < 14.0f - 3.3f) {
        int side = (propCounter_++ & 1) ? 1 : -1;
        props_.push_back({14.0f, side, g_rng.chance(0.5f) ? 0 : g_rng.irange(1, 2)});
        spawnZ_ = 14.0f;
    }
    lightPh_ = std::min(1.0f, lightPh_ + dt * 1.4f);

    // --- встречные машины ---
    if (s.carSpeed > 0.3f) {
        if (oncT_ > 0.0f) {
            float before = oncT_;
            oncT_ += dt / 3.2f;
            if (before < 0.78f && oncT_ >= 0.78f) audio.play(Sfx::Whoosh, 0.55f, -0.8f);
            if (oncT_ >= 1.0f) oncT_ = 0.0f;
        } else {
            oncNext_ -= dt;
            if (oncNext_ <= 0.0f) { oncT_ = 0.001f; oncNext_ = g_rng.range(16.0f, 34.0f); }
        }
    }

    // --- тряска кадра ---
    float amp = (0.25f + 0.55f * s.carSpeed) + s.bump * 2.5f + thud_ * 1.2f;
    if (s.stalled) amp = 0.0f;
    shakeX_ = int(std::lround(std::sin(clock_ * 31.0f) * amp * 0.7f + g_rng.range(-0.5f, 0.5f) * amp));
    shakeY_ = int(std::lround(std::sin(clock_ * 23.0f + 1.0f) * amp * 0.8f + g_rng.range(-0.5f, 0.5f) * amp));

    // --- ссора родителей (звук) ---
    argue_ = 0.45f + 0.35f * saturate(s.time / 150.0f) + 0.2f * std::sin(clock_ * 0.2f);
    talkT_ -= dt;
    if (talkT_ <= 0.0f) {
        if (talkLevel_ > 0.0f) { talkLevel_ = 0.0f; talkT_ = g_rng.range(0.3f, 1.1f); }
        else { speaker_ = g_rng.irange(0, 1); talkLevel_ = g_rng.range(0.35f, 0.9f) * argue_ + 0.15f; talkT_ = g_rng.range(1.5f, 4.0f); }
    }
    bool mute = s.stalled || silenceT_ > 0.0f;
    audio.voiceDad = (!mute && speaker_ == 0) ? talkLevel_ : 0.0f;
    audio.voiceMom = (!mute && speaker_ == 1) ? talkLevel_ : 0.0f;
}

// ============================================================================
//  Мир за окнами
// ============================================================================
void RealWorldScene::drawWindshield(Renderer& r, const SharedState& s) {
    (void)s;
    r.setClip(kWindshield.x, kWindshield.y, kWindshield.w, kWindshield.h);
    r.vgradient(88, 26, 144, 32, kSkyTop, kSkyHor);
    r.rect(88, 58, 144, 40, kGround);
    for (int i = 0; i < 45; ++i) {  // звёзды
        float tw = 0.55f + 0.45f * std::sin(clock_ * 2.0f + i * 1.7f);
        r.put(int(stars_[i].x), int(stars_[i].y), scaleColor(rgb(200, 210, 255), 0.55f * tw));
    }
    // луна
    r.ellipse(200, 38, 9, 9, 0x6A7090, 0.18f);
    r.ellipse(200, 38, 4.5f, 4.5f, 0xB8BED0);

    // дорога
    const float botY = 94.0f, hwBot = kRoadLat * (botY - kVpY);
    Pt road[3] = {{kVpX, kVpY}, {kVpX + hwBot, botY}, {kVpX - hwBot, botY}};
    r.poly(road, 3, 0x181A20);
    r.thickLine(kVpX, kVpY, kVpX - hwBot, botY, 0.6f, 3.0f, 0x6A6E78);   // обочины
    r.thickLine(kVpX, kVpY, kVpX + hwBot, botY, 0.6f, 3.0f, 0x6A6E78);
    // осевая (пунктир)
    const float period = 2.0f;
    for (float zb = period - fract(scroll_ / period) * period; zb < 16.0f; zb += period) {
        float z0 = std::max(zb, 0.5f), z1 = zb + 0.9f;
        float y0 = kVpY + kScreenK / z0, y1 = kVpY + kScreenK / z1;
        float w0 = 0.09f * kScreenK / z0, w1 = 0.09f * kScreenK / z1;
        Pt d[4] = {{kVpX - w1, y1}, {kVpX + w1, y1}, {kVpX + w0, y0}, {kVpX - w0, y0}};
        r.poly(d, 4, scaleColor(rgb(150, 150, 130), 1.0f - saturate(z0 / 16.0f) * 0.7f));
    }
    // придорожные объекты, от дальних к ближним
    std::vector<const Prop*> order;
    for (const Prop& p : props_) if (p.z > 0.45f) order.push_back(&p);
    std::sort(order.begin(), order.end(), [](const Prop* a, const Prop* b) { return a->z > b->z; });
    for (const Prop* p : order) {
        float sc = kScreenK / p->z;
        float x = kVpX + p->side * 5.0f * sc, by = kVpY + sc;
        float fog = saturate(p->z / 14.0f) * 0.75f;
        if (p->kind == 0) {
            uint32_t col = mixColor(0x0B0D12, kFog, fog);
            float h = 3.4f * sc, th = std::max(1.0f, 0.10f * sc);
            r.thickLine(x, by, x, by - h, th, th, col);
            float ax = x - p->side * 1.3f * sc;
            r.thickLine(x, by - h, ax, by - h - 0.1f * sc, th, th, col);
            r.ellipse(ax, by - h, std::max(1.0f, 0.22f * sc), std::max(1.0f, 0.10f * sc), kLampCol);
            r.ellipse(ax, by - h + 0.3f * sc, 0.9f * sc, 0.8f * sc, kLampCol, 0.16f * (1.0f - fog));
        } else if (p->kind == 1) {
            uint32_t col = mixColor(0x050A08, kFog, fog);
            r.thickLine(x, by, x, by - 1.5f * sc, 0.25f * sc, 0.2f * sc, col);
            r.ellipse(x, by - 2.2f * sc, 1.2f * sc, 1.7f * sc, col);
        } else {
            r.ellipse(x, by - 0.35f * sc, 1.0f * sc, 0.5f * sc, mixColor(0x060B09, kFog, fog));
        }
    }
    // встречная машина (фары)
    if (oncT_ > 0.0f) {
        float p = oncT_;
        float z = lerp(14.0f, 0.7f, p * p);
        float sc = kScreenK / z;
        float cx = kVpX - 1.7f * sc, cy = kVpY + 0.45f * sc;
        float sep = 0.62f * sc, rad = std::max(1.0f, 0.15f * sc);
        for (int k = -1; k <= 1; k += 2) {
            r.ellipse(cx + k * sep, cy, rad * 4.0f, rad * 3.0f, 0xFFF0C0, 0.18f);
            r.ellipse(cx + k * sep, cy, rad, rad * 0.8f, 0xFFFFF0);
        }
        if (p > 0.82f) r.tint(0xFFF4D8, saturate((p - 0.82f) / 0.12f) * saturate((1.0f - p) / 0.06f) * 0.65f);
    }
    r.clearClip();
}

void RealWorldScene::drawSideWindow(Renderer& r, bool right) {
    const Rect& R = right ? kRightWin : kLeftWin;
    auto mx = [&](float x) { return right ? 319.0f - x : x; };
    r.setClip(R.x, R.y, R.w, R.h);
    r.vgradient(R.x, R.y, R.w, 50, kSkyTop, kSkyHor);
    r.rect(R.x, 74, R.w, 50, kGround);
    // далёкие огни на горизонте (параллакс)
    for (int i = 0; i < 6; ++i) {
        float u = fract(scroll_ * 0.045f + i * 0.17f);
        float x = right ? lerp(246.0f, 312.0f, u) : lerp(74.0f, 8.0f, u);
        r.put(int(x), 74 + (i * 3) % 5, scaleColor(rgb(255, 200, 120), 0.35f + 0.2f * std::sin(clock_ * 3 + i)));
    }
    const int want = right ? 1 : -1;
    std::vector<const Prop*> order;
    for (const Prop& p : props_) if (p.side == want) order.push_back(&p);
    std::sort(order.begin(), order.end(), [](const Prop* a, const Prop* b) { return a->z > b->z; });
    for (const Prop* p : order) {
        float z = p->z;
        float f = 0.5f - std::atan(z / 1.8f) / kPi;           // 0 - у лобового, 1 - сзади
        float xl = lerp(74.0f, 8.0f, f);
        float x = mx(xl);
        float h = 70.0f * 2.0f / std::sqrt(z * z + 4.0f);
        float by = 74.0f + h * 0.30f;
        float fog = saturate(z / 14.0f) * 0.7f;
        if (p->kind == 0) {
            uint32_t col = mixColor(0x0B0D12, kFog, fog);
            float th = std::max(1.0f, h * 0.06f);
            r.thickLine(x, by, x, by - h, th, th, col);
            r.ellipse(x, by - h, std::max(1.0f, h * 0.07f), std::max(1.0f, h * 0.04f), kLampCol);
            r.ellipse(x, by - h + 2, h * 0.30f, h * 0.22f, kLampCol, 0.14f * (1.0f - fog));
        } else if (p->kind == 1) {
            uint32_t col = mixColor(0x040908, kFog, fog);
            r.thickLine(x, by, x, by - h * 0.5f, h * 0.06f, h * 0.05f, col);
            r.ellipse(x, by - h * 0.65f, h * 0.30f, h * 0.38f, col);
        } else {
            r.ellipse(x, by - h * 0.12f, h * 0.28f, h * 0.14f, mixColor(0x050A08, kFog, fog));
        }
    }
    // отражение на стекле
    Pt glare[4] = {{mx(20), 24}, {mx(34), 28}, {mx(14), 118}, {mx(2), 118}};
    r.poly(glare, 4, 0x9AB0D8, 0.035f);
    r.clearClip();
    // «ножки» окна: закрываем всё, что вне трапеции
    Pt top[3]  = {{mx(8), 24}, {mx(74), 34}, {mx(74), 24}};
    Pt bot[3]  = {{mx(8), 118}, {mx(74), 104}, {mx(74), 118}};
    r.setClip(R.x, R.y, R.w, R.h);
    r.poly(top, 3, kBody);
    r.poly(bot, 3, kBody);
    r.clearClip();
}

void RealWorldScene::drawSunroof(Renderer& r) {
    Pt sr[4] = {{130, 3}, {190, 3}, {198, 21}, {122, 21}};
    r.poly(sr, 4, 0x070A16);
    r.setClip(122, 3, 76, 18);
    for (int i = 45; i < starCount_; ++i) {
        float tw = 0.5f + 0.5f * std::sin(clock_ * 1.7f + i);
        r.put(int(stars_[i].x), int(stars_[i].y), scaleColor(rgb(200, 210, 255), 0.6f * tw));
    }
    r.clearClip();
    r.line(130, 3, 190, 3, 0x2A2D38); r.line(122, 21, 198, 21, 0x2A2D38);
    r.line(130, 3, 122, 21, 0x2A2D38); r.line(190, 3, 198, 21, 0x2A2D38);
}

void RealWorldScene::drawInterior(Renderer& r, const SharedState& s) {
    (void)s;
    // потолок
    r.vgradient(0, 0, kW, 26, 0x1A1C26, kHeadlin);
    for (int x = 0; x < kW; x += 40) r.rect(x, 0, 1, 24, 0x12141C);  // швы обивки
    // стойки: A-стойки и боковые панели
    Pt aL[4] = {{74, 34}, {88, 26}, {88, 94}, {74, 104}};
    Pt aR[4] = {{245, 34}, {231, 26}, {231, 94}, {245, 104}};
    r.poly(aL, 4, kBody); r.poly(aR, 4, kBody);
    // рамки окон
    r.line(8, 24, 74, 34, 0x2E3140); r.line(74, 34, 74, 104, 0x2E3140); r.line(74, 104, 8, 118, 0x2E3140);
    r.line(311, 24, 245, 34, 0x2E3140); r.line(245, 34, 245, 104, 0x2E3140); r.line(245, 104, 311, 118, 0x2E3140);
    r.line(104, 26, 216, 26, 0x2E3140); r.line(88, 94, 104, 26, 0x2E3140); r.line(232, 94, 216, 26, 0x2E3140);
    // зеркало заднего вида
    r.rect(147, 27, 26, 9, 0x1B1D26);
    r.rect(148, 28, 24, 7, 0x0C1018);
    r.rect(158, 24, 4, 3, 0x1B1D26);
    if (monster_.state() != MState::Dormant && monster_.slot() == Slot::Top && monster_.atWindow()) {
        r.put(157, 31, 0xD8D8C8); r.put(163, 31, 0xD8D8C8);  // глаза в зеркале
    }
    // приборная панель между сиденьями (слабое свечение)
    r.ellipse(160, 96, 30, 9, 0x2C5058, 0.22f);
    for (int i = 0; i < 5; ++i) r.put(150 + i * 5, 94, (i == 2 && (int(clock_ * 2) & 1)) ? 0xFF8040 : 0x40E0B0);

    // --- родители (силуэты) ---
    float swayD = std::sin(clock_ * 1.3f) * 1.2f + std::sin(clock_ * 4.1f) * 0.5f * (speaker_ == 0 ? talkLevel_ : 0.0f);
    float swayM = std::sin(clock_ * 1.1f + 2.0f) * 1.0f + std::sin(clock_ * 4.7f) * 0.5f * (speaker_ == 1 ? talkLevel_ : 0.0f);
    float turnD = (speaker_ == 1 && talkLevel_ > 0 && silenceT_ <= 0) ? 3.0f : 0.0f;   // отец поворачивается к матери
    float turnM = (speaker_ == 0 && talkLevel_ > 0 && silenceT_ <= 0) ? -3.0f : 0.0f;
    if (silenceT_ > 0) { turnD = 4.0f; turnM = -4.0f; }

    // спинки сидений
    Pt seatL[4] = {{62, 104}, {148, 104}, {152, 180}, {52, 180}};
    Pt seatR[4] = {{171, 104}, {257, 104}, {267, 180}, {167, 180}};
    r.poly(seatL, 4, kSeat); r.poly(seatR, 4, kSeat);
    // центральная консоль
    r.rect(150, 118, 20, 62, 0x0B0C11);
    r.rect(158, 112, 4, 8, 0x14161C);
    // голова и плечи отца (слева, за рулём)
    float dx = 122 + swayD + turnD;
    r.ellipse(122, 112, 40, 22, kSilhou);                  // плечи
    r.ellipse(dx, 68, 14, 17.5f, kSilhou);                 // голова
    r.rect(114, 80, 16, 8, kSilhou);                       // шея
    Pt hrD[4] = {{106, 84}, {138, 84}, {140, 104}, {104, 104}};
    r.poly(hrD, 4, kSeat);                                 // подголовник
    // жест рукой во время реплики
    if (speaker_ == 0 && talkLevel_ > 0 && silenceT_ <= 0) {
        float gy = 92 - 10 * std::fabs(std::sin(clock_ * 5.0f));
        r.thickLine(144, 108, 150 + std::sin(clock_ * 5.0f) * 3, gy, 7, 5, kSilhou);
        r.ellipse(150 + std::sin(clock_ * 5.0f) * 3, gy - 2, 4.5f, 5.5f, kSilhou);
    }
    // голова и плечи матери (справа)
    float mxp = 198 + swayM + turnM;
    r.ellipse(198, 114, 38, 22, kSilhou);
    r.ellipse(mxp, 70, 13, 17.5f, kSilhou);
    r.ellipse(mxp, 80, 16, 21, kSilhou, 0.9f);             // волосы
    r.ellipse(mxp + 1, 51, 6, 5.5f, kSilhou);              // пучок
    Pt hrM[4] = {{182, 86}, {214, 86}, {216, 106}, {180, 106}};
    r.poly(hrM, 4, kSeat);
    if (speaker_ == 1 && talkLevel_ > 0 && silenceT_ <= 0) {
        float gy = 96 - 9 * std::fabs(std::sin(clock_ * 4.3f + 1.0f));
        r.thickLine(176, 110, 170 + std::sin(clock_ * 4.3f) * 3, gy, 7, 5, kSilhou);
        r.ellipse(170 + std::sin(clock_ * 4.3f) * 3, gy - 2, 4.5f, 5.5f, kSilhou);
    }
    // светящаяся консоль в руках ребёнка (низ кадра)
    r.ellipse(160, 188, 100, 36, 0x7CC8A0, 0.12f);
    r.rect(118, 172, 84, 10, 0x1C1E26);
    r.rect(132, 174, 56, 3, 0x6AA880);
}

// ---- тень на потолке: монстр ползёт по крыше ----
void RealWorldScene::drawCrawlShadow(Renderer& r) {
    const MState st = monster_.state();
    if (st != MState::Crawling && st != MState::Retreating) return;
    float c = smoothstep(monster_.crawl());
    float tx = slotX(monster_.slot());
    float x, y, sc;
    if (monster_.slot() == Slot::Top) { x = 160.0f + std::sin(clock_ * 2.0f) * 4; y = lerp(18.0f, 10.0f, c); sc = lerp(1.4f, 0.9f, c); }
    else { x = lerp(160.0f, tx, c); y = lerp(14.0f, 17.0f, c) + std::sin(c * kPi) * 3.0f; sc = lerp(1.3f, 0.8f, c); }
    r.setClip(0, 0, kW, 26);
    r.ellipse(x, y, 22 * sc, 7 * sc, 0x020306, 0.85f);
    // лапы: поочерёдно «давят» на обивку
    float ph = stepPh_ + (std::sin(clock_ * 9.0f) * 0.5f);
    for (int k = 0; k < 4; ++k) {
        float off = ((k + int(ph)) & 1) ? 3.0f : -1.0f;
        float lx = x + (k - 1.5f) * 11.0f * sc;
        r.ellipse(lx, y + off, 5 * sc, 3.2f * sc, 0x020306, 0.9f);
        r.ellipse(lx, y + off - 3.5f * sc, 5.5f * sc, 1.6f * sc, 0x2A2D3A, 0.45f);  // вмятина-блик
    }
    r.clearClip();
}

void RealWorldScene::drawCracks(Renderer& r, float cx, float cy, float stage, float seed) {
    static const float ang[8] = {0.2f, 0.9f, 1.7f, 2.5f, 3.3f, 4.1f, 5.0f, 5.8f};
    int lines = stage >= 2.0f ? 8 : 4;
    float len = stage >= 2.0f ? 30.0f : 16.0f;
    for (int i = 0; i < lines; ++i) {
        float a = ang[i] + seed, l1 = len * (0.5f + 0.5f * float((i * 37) % 10) / 10.0f);
        float x1 = cx + std::cos(a) * l1 * 0.5f, y1 = cy + std::sin(a) * l1 * 0.5f + ((i & 1) ? 2.0f : -2.0f);
        float x2 = x1 + std::cos(a + 0.4f) * l1 * 0.5f, y2 = y1 + std::sin(a + 0.4f) * l1 * 0.5f;
        r.line(int(cx), int(cy), int(x1), int(y1), 0xB0BCD0);
        r.line(int(x1), int(y1), int(x2), int(y2), 0x8090A8);
    }
}

void RealWorldScene::drawMonster(Renderer& r) {
    const MState st = monster_.state();
    drawCrawlShadow(r);
    if (st == MState::Dormant || st == MState::Crawling) return;

    const Slot slot = monster_.slot();
    const float b = monster_.breach();
    float fade = 1.0f, lift = 0.0f;
    if (st == MState::Retreating) {
        float u = saturate(monster_.stateTime() / 0.7f);
        fade = 1.0f - u; lift = u * 22.0f;
        if (fade <= 0.01f) return;
    }
    const float stage = st == MState::Breaching ? (b >= 0.66f ? 2.0f : (b >= 0.33f ? 1.0f : 0.0f)) : 0.0f;

    if (slot == Slot::Top) {
        // люк открывается: чёрная щель растёт
        if (st == MState::Breaching || st == MState::Retreating) {
            float g = (st == MState::Breaching ? b : 0.3f * fade) * 14.0f;
            Pt gap[4] = {{130, 3}, {190, 3}, {190 + g * 0.4f, 3 + g}, {130 - g * 0.4f, 3 + g}};
            r.poly(gap, 4, 0x010103, 0.9f);
        }
        float fy = 4.0f + b * 20.0f - lift * 0.4f, fs = 0.85f + b * 0.55f;
        if (st != MState::Breaching) r.setClip(122, 3, 76, 18);
        drawFace(r, 160, fy, fs, b, fade, true);
        r.clearClip();
        float hx0 = 132 - b * 10, hx1 = 188 + b * 10;
        float hy = 18 + b * 10;
        drawHand(r, hx0, hy, 1.4f, 0.9f + b * 0.8f, fade);
        drawHand(r, hx1, hy, 1.75f, 0.9f + b * 0.8f, fade);
        if (b > 0.5f) {  // руки тянутся к зрителю
            drawArm(r, hx0, hy, lerp(hx0, 150.0f, b), lerp(hy, 150.0f, b), 5.0f, 8.0f + b * 8.0f, fade);
            drawArm(r, hx1, hy, lerp(hx1, 172.0f, b), lerp(hy, 150.0f, b), 5.0f, 8.0f + b * 8.0f, fade);
        }
        return;
    }

    const bool right = slot == Slot::Right;
    const float sgn = right ? -1.0f : 1.0f;      // направление «внутрь» салона
    const float wx = slotX(slot);
    // стекло опускается
    if (st == MState::Breaching) {
        float g = b * 55.0f;
        Pt gap[4];
        if (!right) { gap[0] = {8, 24}; gap[1] = {74, 34}; gap[2] = {74, 34 + g}; gap[3] = {8, 24 + g}; }
        else        { gap[0] = {311, 24}; gap[1] = {245, 34}; gap[2] = {245, 34 + g}; gap[3] = {311, 24 + g}; }
        r.poly(gap, 4, 0x020307, 0.8f);
        r.line(int(gap[3].x), int(gap[3].y), int(gap[2].x), int(gap[2].y), 0x6A7088);
    }
    float fx = wx, fy = kWinY - 2 - lift + b * 12.0f, fs = 1.25f + b * 0.6f;
    drawFace(r, fx, fy, fs, b, fade, false);
    // руки на раме
    float hx0 = wx - sgn * 24.0f, hx1 = wx + sgn * 26.0f, hy = kWinY + 22 - lift;
    drawHand(r, hx0, hy, right ? -1.9f : -1.2f, 1.0f + b * 0.6f, fade);
    drawHand(r, hx1, hy + 6, right ? -1.2f : -1.9f, 1.0f + b * 0.6f, fade);
    if (stage > 0) drawCracks(r, wx, kWinY + 10, stage, right ? 0.6f : 0.0f);
    if (b > 0.35f) {  // рука пролезает внутрь, к ребёнку
        float ex = lerp(hx1, 158.0f, saturate((b - 0.35f) / 0.65f));
        float ey = lerp(hy + 6, 150.0f, saturate((b - 0.35f) / 0.65f));
        drawArm(r, hx1, hy + 6, ex, ey, 6.0f, 6.0f + 12.0f * b, fade);
        drawHand(r, ex, ey, 1.57f, 1.0f + b * 1.4f, fade);
    }
}

// ============================================================================
void RealWorldScene::renderWorld(Renderer& r, const SharedState& s) {
    r.clear(kBody);
    drawWindshield(r, s);
    drawSideWindow(r, false);
    drawSideWindow(r, true);
    drawSunroof(r);
    drawInterior(r, s);
    drawMonster(r);

    // свет фонарей, пробегающий по салону
    if (lightPh_ < 1.0f) {
        float cx = lightSide_ < 0 ? lerp(120.0f, -20.0f, lightPh_) : lerp(200.0f, 340.0f, lightPh_);
        r.addLightX(0.30f * std::sin(lightPh_ * kPi), 0xFFAA55, cx, 140.0f);
    }
    if (oncT_ > 0.8f) r.addLightX(0.35f * std::sin(saturate((oncT_ - 0.78f) / 0.22f) * kPi), 0xFFF0D0, 130.0f, 200.0f);

    r.vignette(0.85f);
    r.grain(3);
    if (s.stalled) r.tint(0x000000, 0.35f);
    r.shift(shakeX_, shakeY_);
}

void RealWorldScene::renderHud(Renderer& r, const SharedState& s, bool showHints) {
    // путь до дома
    r.rect(60, 3, 200, 7, 0x000000, 0.45f);
    r.rectOutline(60, 3, 200, 7, 0x6A7088);
    r.rect(62, 5, int(196 * saturate(s.progress)), 3, 0x8FD8A0);
    float mx = 62.0f + 196.0f * saturate(s.progress);
    r.rect(int(mx) - 2, 1, 5, 3, 0xE0E0F0);  // машинка
    r.rect(264, 1, 8, 5, 0xC8A060); r.rect(266, 0, 4, 1, 0xA06040); r.rect(267, 3, 2, 3, 0x402010);  // домик
    r.text(30, 3, "HOME", 0x808598);
    // топливо
    r.text(4, 164, "FUEL", s.fuel < 0.2f && (int(clock_ * 5) & 1) ? 0xFF6060 : 0xA0A5B8);
    r.rectOutline(4, 172, 42, 6, 0x6A7088);
    r.rect(5, 173, int(40 * saturate(s.fuel)), 4, s.fuel < 0.2f ? 0xE04040 : 0xE0B040);
    // камера: кадры и перезарядка
    r.text(250, 164, "CAMERA", 0xA0A5B8);
    for (int i = 0; i < kMaxFilm; ++i) r.rect(250 + i * 9, 173, 7, 5, i < s.film ? 0xE8E8F0 : 0x2A2D38);
    if (s.camCooldown > 0.0f) r.rect(250, 171, int(34 * (1.0f - s.camCooldown / kCamCooldown)), 1, 0xFF8040);
    // угроза: глаз-индикатор
    float th = saturate(s.threat);
    r.rectOutline(122, 173, 76, 5, 0x4A4E60);
    r.rect(123, 174, int(74 * th), 3, mixColor(0x806020, 0xFF2020, th));
    if (th > 0.55f && (int(clock_ * (4.0f + 8.0f * th)) & 1)) r.text(150, 163, "DANGER", 0xFF4040);

    if (showHints) {
        r.textCentered(160, 150, "F: CAMERA FLASH    TAB: GAME CONSOLE", 0xC0C4D8);
    }
    // видоискатель камеры
    if (shutterT_ > 0.0f) {
        uint32_t c = 0xFFFFFF;
        int m = 14;
        r.rect(m, m, 14, 1, c); r.rect(m, m, 1, 14, c);
        r.rect(kW - m - 14, m, 14, 1, c); r.rect(kW - m - 1, m, 1, 14, c);
        r.rect(m, kH - m - 1, 14, 1, c); r.rect(m, kH - m - 14, 1, 14, c);
        r.rect(kW - m - 14, kH - m - 1, 14, 1, c); r.rect(kW - m - 1, kH - m - 14, 1, 14, c);
    }
    if (flashT_ > 0.0f) r.tint(0xFFFFFF, std::min(1.0f, std::pow(flashT_, 1.4f) * 1.1f));
}

void RealWorldScene::renderJumpscare(Renderer& r, float t) {
    r.clear(0x020204);
    float k = saturate(t / 0.25f);
    float s = lerp(2.0f, 7.0f, k * k) + t * 1.2f;
    float jx = g_rng.range(-4.0f, 4.0f) * (1.0f - saturate(t / 1.6f) * 0.5f);
    float jy = g_rng.range(-3.0f, 3.0f);
    drawFace(r, 160 + jx, 78 + jy, s, 1.0f, 1.0f, false);
    if (t < 0.3f) r.tint(0xFFFFFF, (0.3f - t) / 0.3f * 0.9f);
    if ((int(t * 30.0f) & 3) == 0) r.tint(0xC00000, 0.25f);
    r.grain(14);
}

}  // namespace lrh
