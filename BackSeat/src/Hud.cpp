// ============================================================================
//  Hud.cpp — виджеты интерфейса (см. Hud.h): шкалы, значки, подсказки клавиш,
//  субтитры и панели.
//
//  Значки — маленькие спрайты из символьных массивов (Canvas::drawSprite),
//  палитру к ним каждый виджет собирает сам (мигание, «выключенное» состояние).
//  Полупрозрачные надписи рисуются через промежуточный холст: виджет рисуется
//  на «ключевом» фоне, затем все не-фоновые пиксели смешиваются с экраном.
// ============================================================================
#include "Hud.h"

#include "Font.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace hud {
namespace {

// ---------------------------------------------------------------------------
//  Значки. '.' — прозрачный пиксель, цифры — индексы палитры виджета:
//  1 — основной цвет, 2 — тёмный контур, 3 — деталь/блик, 4 — акцент.
// ---------------------------------------------------------------------------
// clang-format off
const char* const kCanIcon[] = {   // канистра 10x11: ручка, носик, тиснёный крест
    "..222.....",
    ".2...2..22",
    ".2...2.212",
    "2222222212",
    "2111111112",
    "2131111312",
    "2113113112",
    "2111331112",
    "2113113112",
    "2131111312",
    "2222222222",
};
const char* const kHouseIcon[] = { // домик 9x8 с горящими окнами
    "....2....",
    "...212...",
    "..21112..",
    ".2111112.",
    "222222222",
    ".2323322.",
    ".2222242.",
    ".2222242.",
};
const char* const kEyeIcon[] = {   // глаз 11x7 (угроза): радужка 3, зрачок 4
    "...22222...",
    ".22.111.22.",
    "2.1133311.2",
    "2.1334331.2",
    "2.1133311.2",
    ".22.111.22.",
    "...22222...",
};
const char* const kEyeClosed[] = { // закрытый глаз с ресницами (угрозы нет)
    "...........",
    "...........",
    "2.........2",
    ".22.....22.",
    "...22222...",
    "..2.2.2.2..",
    "...........",
};
const char* const kCameraIcon[] = { // фотоаппарат 13x9: объектив 3, окошко вспышки 4
    "..2222.......",
    ".211112.444..",
    "2222222222222",
    "2111112221112",
    "2111123332112",
    "2111123132112",
    "2111123332112",
    "2111112221112",
    "2222222222222",
};
const char* const kLockIcon[] = {  // навесной замок 7x9
    "..222..",
    ".2...2.",
    ".2...2.",
    "2222222",
    "2111112",
    "2113112",
    "2113112",
    "2111112",
    "2222222",
};
const char* const kBoltIcon[] = {  // молния 5x7 (усиленная вспышка)
    "...11",
    "..11.",
    ".11..",
    "11111",
    "..11.",
    ".11..",
    "11...",
};
const char* const kCarMarker[] = { // машинка-отметка на шкале пути 5x3
    ".111.",
    "11111",
    ".2.2.",
};
const char* const kInfinityIcon[] = { // знак бесконечности 9x5 (в шрифте его нет)
    ".11...11.",
    "1..1.1..1",
    "1...1...1",
    "1..1.1..1",
    ".11...11.",
};
// clang-format on

template <size_t N>
constexpr int rowsOf(const char* const (&)[N]) {
    return static_cast<int>(N);
}

// Цвет-«ключ» промежуточного холста: в интерфейсе нигде не используется.
constexpr uint32_t kKeyColor = rgb(255, 0, 255);

// Общие цвета рамок и подложек.
constexpr uint32_t kBarEmpty = rgb(30, 26, 38);
constexpr uint32_t kFrameLight = rgb(98, 92, 114);
constexpr uint32_t kFrameMid = rgb(44, 40, 56);
constexpr uint32_t kCapFace = rgb(60, 56, 74);
constexpr uint32_t kCapEdge = rgb(22, 20, 30);

// Палитра значка из четырёх цветов (индексы 1..4, остальные не используются).
std::array<uint32_t, 16> iconPalette(uint32_t main, uint32_t dark, uint32_t detail, uint32_t accent) {
    std::array<uint32_t, 16> p{};
    p[1] = main;
    p[2] = dark;
    p[3] = detail;
    p[4] = accent;
    return p;
}

// Промежуточный холст для полупрозрачных надписей (растёт по мере надобности,
// поэтому после первых кадров память больше не выделяется).
Canvas& scratchCanvas(int w, int h) {
    static Canvas s;
    if (s.width() < w || s.height() < h) s.resize(std::max(w, s.width()), std::max(h, s.height()));
    return s;
}

// Рисует виджет с прозрачностью alpha. draw(холст, dx, dy) рисует в координатах
// экрана, сдвинутых на (dx, dy); (rx, ry, rw, rh) — прямоугольник, который
// гарантированно содержит весь виджет.
template <typename DrawFn>
void drawBlended(Canvas& c, int rx, int ry, int rw, int rh, float alpha, DrawFn draw) {
    if (alpha <= 0.004f || rw <= 0 || rh <= 0) return;
    if (alpha >= 0.996f) {
        draw(c, 0, 0);
        return;
    }
    Canvas& s = scratchCanvas(rw, rh);
    s.setOffset(0, 0);
    s.resetStencilModes();
    s.resetClip();
    s.fillRect(0, 0, rw, rh, kKeyColor);
    s.setClip(0, 0, rw, rh);
    draw(s, -rx, -ry);
    s.resetClip();
    for (int j = 0; j < rh; ++j) {
        for (int i = 0; i < rw; ++i) {
            const uint32_t p = s.get(i, j);
            if (p != kKeyColor) c.blendPixel(rx + i, ry + j, p, alpha);
        }
    }
}

// Горизонтальная шкала (x, y, w, h — вместе с рамкой): тёмная рамка, пустая
// часть, заливка с бликом сверху и тенью снизу, деления каждые tick пикселей.
void drawBar(Canvas& c, int x, int y, int w, int h, float v, uint32_t fill, int tick) {
    if (w < 3 || h < 3) return;
    // Рамка и пустая часть полупрозрачные: под шкалой видно, что творится в окне.
    c.blendRect(x, y, w, 1, kShadow, 0.85f);
    c.blendRect(x, y + h - 1, w, 1, kShadow, 0.85f);
    c.blendRect(x, y + 1, 1, h - 2, kShadow, 0.85f);
    c.blendRect(x + w - 1, y + 1, 1, h - 2, kShadow, 0.85f);
    c.blendRect(x + 1, y + 1, w - 2, h - 2, kBarEmpty, 0.6f);
    const int inner = w - 2;
    const int fw = clampi(roundi(saturate(v) * static_cast<float>(inner)), 0, inner);
    if (fw > 0) {
        c.fillRect(x + 1, y + 1, fw, h - 2, fill);
        c.fillRect(x + 1, y + 1, fw, 1, scaleColor(fill, 1.3f));
        if (h > 4) c.fillRect(x + 1, y + h - 2, fw, 1, scaleColor(fill, 0.62f));
        // Яркий край заливки — «уровень».
        c.fillRect(x + fw, y + 1, 1, h - 2, scaleColor(fill, 1.45f));
    }
    if (tick > 1) {
        for (int i = tick; i < inner; i += tick) {
            if (i < fw) c.fillRect(x + 1 + i, y + 1, 1, h - 2, scaleColor(fill, 0.55f));
            else c.blendRect(x + 1 + i, y + 1, 1, h - 2, rgb(40, 36, 50), 0.6f);
        }
    }
}

// Рисует одну кодовую точку UTF-8 (от p до next) с тенью.
void drawGlyphRange(Canvas& c, int x, int y, const char* p, const char* next, uint32_t color) {
    char buf[8] = {};
    const size_t n = std::min<size_t>(static_cast<size_t>(next - p), sizeof(buf) - 1);
    std::memcpy(buf, p, n);
    font::drawTextShadow(c, x, y, buf, color, kShadow);
}

// Мигание с частотой hz (true в первой половине периода).
bool blinkOn(float time, float hz) {
    const float ph = time * hz;
    return ph - std::floor(ph) < 0.5f;
}

} // namespace

// ---------------------------------------------------------------------------
//  Бензин: [канистра] БЕНЗИН  65%
//                     [=========    ]
// ---------------------------------------------------------------------------
void drawFuelGauge(Canvas& c, int x, int y, float fuel01, bool blink, float time) {
    fuel01 = saturate(fuel01);
    const bool alarm = blink && blinkOn(time, 2.5f);
    const uint32_t main = blink ? (alarm ? kFuelLow : scaleColor(kFuelLow, 0.6f)) : kFuel;
    const auto pal = iconPalette(main, kShadow, scaleColor(main, 0.62f), main);
    c.drawSprite(kCanIcon, rowsOf(kCanIcon), x, y + 1, pal.data());

    font::drawTextShadow(c, x + 12, y + 1, T8("БЕНЗИН"), blink && alarm ? kFuelLow : kText, kShadow);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", roundi(fuel01 * 100.0f));
    const int tw = font::textWidth(buf);
    font::drawTextShadow(c, x + 70 - tw, y + 1, buf, blink ? main : kTextDim, kShadow);

    drawBar(c, x + 12, y + 9, 58, 5, fuel01, blink ? main : kFuel, 7);
}

// ---------------------------------------------------------------------------
//  Путь домой: ДОМ 6.2 КМ
//              [-----🚗---------]⌂
// ---------------------------------------------------------------------------
void drawTripBar(Canvas& c, int x, int y, int w, float progress01, float kmLeft) {
    progress01 = saturate(progress01);
    w = std::max(w, 24);
    char buf[48];
    if (progress01 >= 0.999f) {
        std::snprintf(buf, sizeof(buf), "%s", T8("ПРИЕХАЛИ"));
    } else {
        const float km = std::max(0.0f, kmLeft);
        if (km >= 9.95f) std::snprintf(buf, sizeof(buf), "%s %d %s", T8("ДОМ"), roundi(km), T8("КМ"));
        else std::snprintf(buf, sizeof(buf), "%s %.1f %s", T8("ДОМ"), static_cast<double>(km), T8("КМ"));
    }
    font::drawTextShadow(c, x, y + 1, buf, kText, kShadow);

    // Дорога-шкала: тонкая, со «штрихами» разметки; дом справа.
    const int bw = w - 11;
    const int by = y + 10;
    c.fillRect(x, by - 1, bw, 4, kShadow);
    c.fillRect(x, by, bw, 2, kBarEmpty);
    const int done = clampi(roundi(progress01 * static_cast<float>(bw - 1)), 0, bw - 1);
    c.fillRect(x, by, done + 1, 2, scaleColor(kTrip, 0.75f));
    c.fillRect(x, by, done + 1, 1, kTrip);
    for (int i = 3; i < bw; i += 6) {
        if (i > done) c.plot(x + i, by, rgb(70, 74, 92));
    }
    const auto carPal = iconPalette(kText, kShadow, kText, kText);
    c.drawSprite(kCarMarker, rowsOf(kCarMarker), x + done - 2, by - 3, carPal.data());

    const auto housePal =
        iconPalette(rgb(70, 82, 110), rgb(28, 30, 44), rgb(255, 196, 96), rgb(150, 110, 70));
    c.drawSprite(kHouseIcon, rowsOf(kHouseIcon), x + w - 9, y + 6, housePal.data());
}

// ---------------------------------------------------------------------------
//  Бесконечный режим: ДОРОГА ∞
//                     [ - - -🚗- - - - ]   (разметка бежит без конца)
//                                12.4 КМ
//  Дома в конце шкалы нет: оба края растворяются в темноте. Изредка одна
//  цифра счётчика на долю секунды «сбивается» — дорога будто повторяется.
// ---------------------------------------------------------------------------
void drawEndlessBar(Canvas& c, int x, int y, int w, float kmDriven, float time) {
    w = std::max(w, 24);
    const float km = std::max(0.0f, kmDriven);
    font::drawTextShadow(c, x, y + 1, T8("ДОРОГА"), kText, kShadow);
    // Знак бесконечности после подписи; медленно «дышит».
    const int ix = x + font::textWidth(T8("ДОРОГА")) + 3;
    const float breathe = 0.5f + 0.5f * std::sin(time * 1.3f);
    c.drawSpriteSolid(kInfinityIcon, rowsOf(kInfinityIcon), ix + 1, y + 3, kShadow);
    c.drawSpriteSolid(kInfinityIcon, rowsOf(kInfinityIcon), ix, y + 2,
                      lerpColor(scaleColor(kTrip, 0.7f), kTrip, breathe));

    // Дорога-шкала во всю ширину: разметка ползёт влево вместе с пройденным
    // путём, машинка стоит на месте.
    const int by = y + 11;
    c.blendRect(x, by - 1, w, 4, kShadow, 0.85f);
    c.fillRect(x, by, w, 2, kBarEmpty);
    const int scroll = static_cast<int>(std::fmod(km * 420.0f, 6.0f));
    for (int i = 0; i < w; ++i) {
        if (((i + scroll) % 6) < 3) c.plot(x + i, by, scaleColor(kTrip, 0.62f));
    }
    // Края уходят в темноту (плавное затемнение по 8 пикселей): дороге нет конца.
    for (int i = 0; i < 8; ++i) {
        const float a = 1.0f - static_cast<float>(i) / 8.0f;
        c.blendRect(x + i, by - 1, 1, 4, kShadow, a);
        c.blendRect(x + w - 1 - i, by - 1, 1, 4, kShadow, a);
    }
    const auto carPal = iconPalette(kText, kShadow, kText, kText);
    c.drawSprite(kCarMarker, rowsOf(kCarMarker), x + w - 24, by - 2, carPal.data());

    // Пройденные километры (по правому краю, под шкалой).
    char buf[32];
    if (km < 99.95f) std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(km));
    else std::snprintf(buf, sizeof(buf), "%d", std::min(99999, static_cast<int>(km)));
    // Сбой: в редкий отрезок времени одна цифра подменяется другой.
    const uint32_t slot = static_cast<uint32_t>(std::max(0.0f, time) * 9.0f);
    uint32_t hsh = slot * 2654435761u;
    hsh ^= hsh >> 15;
    hsh *= 2246822519u;
    hsh ^= hsh >> 13;
    int glitchAt = -1;
    if ((hsh & 63u) == 7u) {
        const int len = static_cast<int>(std::strlen(buf));
        int pick = static_cast<int>((hsh >> 8) % static_cast<uint32_t>(std::max(1, len)));
        if (buf[pick] == '.') pick = (pick + 1) % std::max(1, len); // точку не трогаем
        if (buf[pick] >= '0' && buf[pick] <= '9') {
            const int shift = 1 + static_cast<int>((hsh >> 16) % 8u);
            buf[pick] = static_cast<char>('0' + ((buf[pick] - '0') + shift) % 10);
            glitchAt = pick;
        }
    }
    const char* unit = T8(" КМ");
    const int numW = font::textWidth(buf), unitW = font::textWidth(unit);
    const int tx = x + w - (numW + 1) - unitW;
    const int ty = y + 16;
    for (int i = 0; buf[i] != 0; ++i) {
        const char d[2] = {buf[i], 0};
        const uint32_t col = i == glitchAt ? rgb(196, 92, 84) : kText;
        font::drawTextShadow(c, tx + i * font::kAdvance, ty, d, col, kShadow);
    }
    font::drawTextShadow(c, tx + numW + 1, ty, unit, kTextDim, kShadow);
}

// ---------------------------------------------------------------------------
//  Угроза: [глаз] УГРОЗА
//                 [=====       ]   (пульсирует выше 60%)
// ---------------------------------------------------------------------------
void drawThreatBar(Canvas& c, int x, int y, float threat01, float time) {
    threat01 = saturate(threat01);
    float pulse = 0.0f;
    if (threat01 > 0.6f) {
        const float k = (threat01 - 0.6f) / 0.4f;
        pulse = (0.5f + 0.5f * std::sin(time * (5.0f + 7.0f * k))) * (0.45f + 0.55f * k);
    }
    const uint32_t bar = lerpColor(kThreat, rgb(255, 72, 64), pulse);

    // Пока угрозы почти нет, глаз закрыт; дальше открыт, радужка пульсирует
    // вместе со шкалой, на пике значок дрожит.
    int jx = 0;
    if (threat01 > 0.85f) jx = (static_cast<int>(time * 30.0f) & 1) ? 1 : 0;
    if (threat01 < 0.2f) {
        // Закрытый глаз рисуется светлыми линиями с тенью — иначе пропадает на тёмном потолке.
        c.drawSpriteSolid(kEyeClosed, rowsOf(kEyeClosed), x + jx + 1, y + 4, kShadow);
        c.drawSpriteSolid(kEyeClosed, rowsOf(kEyeClosed), x + jx, y + 3, kTextDim);
    } else {
        const auto pal = iconPalette(rgb(150, 140, 130), kShadow, bar, rgb(6, 4, 8));
        c.drawSprite(kEyeIcon, rowsOf(kEyeIcon), x + jx, y + 3, pal.data());
    }

    font::drawTextShadow(c, x + 13, y + 1, T8("УГРОЗА"), pulse > 0.5f ? lerpColor(kText, bar, 0.6f) : kText,
                         kShadow);
    if (pulse > 0.0f) c.addRect(x + 12, y + 8, 59, 7, kThreat, 0.22f * pulse);
    drawBar(c, x + 13, y + 9, 57, 5, threat01, bar, 0);
}

// ---------------------------------------------------------------------------
//  Камера: значок в кольце зарядки, под ним подпись. Блок 44x30 от (x, y);
//  подпись выравнивается по правому краю блока (может уходить левее x).
// ---------------------------------------------------------------------------
void drawCameraStatus(Canvas& c, int x, int y, bool hasCamera, float charge01, bool superFlash,
                      float time) {
    charge01 = saturate(charge01);
    const int cx = x + 32, cy = y + 11;
    const int right = x + 44;
    if (!hasCamera) {
        const auto pal = iconPalette(rgb(46, 44, 54), rgb(22, 20, 28), rgb(30, 30, 38), rgb(52, 50, 60));
        c.drawSprite(kCameraIcon, rowsOf(kCameraIcon), cx - 6, cy - 4, pal.data());
        // Пунктирное пустое кольцо.
        for (int i = 0; i < 16; ++i) {
            const float a = static_cast<float>(i) * (kTau / 16.0f);
            c.plot(cx + roundi(std::sin(a) * 10.0f), cy - roundi(std::cos(a) * 10.0f), rgb(44, 42, 52));
        }
        const char* label = T8("НЕТ КАМЕРЫ");
        font::drawTextShadow(c, right - font::textWidth(label), y + 23, label, kTextDim, kShadow);
        return;
    }

    const bool ready = charge01 >= 0.999f;
    const uint32_t ringOn = superFlash ? rgb(255, 214, 110) : kCamera;
    const float glowK = ready ? 0.65f + 0.35f * std::sin(time * 4.0f) : 0.0f;
    if (ready) c.glow(cx, cy, 14, superFlash ? rgb(255, 190, 80) : rgb(150, 170, 255), 0.10f + 0.06f * glowK);

    // Кольцо зарядки: по часовой стрелке от верхней точки, 12 делений.
    for (int dy = -11; dy <= 11; ++dy) {
        for (int dx = -11; dx <= 11; ++dx) {
            const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (d < 8.6f || d > 10.7f) continue;
            float a = std::atan2(static_cast<float>(dx), static_cast<float>(-dy)) / kTau;
            if (a < 0.0f) a += 1.0f;
            const float seg = a * 12.0f - std::floor(a * 12.0f);
            if (seg < 0.12f) continue; // промежуток между делениями
            const bool on = a <= charge01;
            uint32_t col = on ? ringOn : rgb(48, 46, 60);
            if (on && d > 9.9f) col = scaleColor(col, 0.7f);
            if (on && ready) col = lerpColor(col, rgb(255, 255, 255), 0.25f * glowK);
            c.plot(cx + dx, cy + dy, col);
        }
    }

    const uint32_t body = ready ? rgb(196, 196, 214) : rgb(120, 120, 136);
    const uint32_t flashCol =
        ready ? (superFlash ? rgb(255, 220, 120) : rgb(230, 240, 255)) : rgb(70, 70, 84);
    const auto pal = iconPalette(body, rgb(18, 16, 24), rgb(40, 60, 90), flashCol);
    c.drawSprite(kCameraIcon, rowsOf(kCameraIcon), cx - 6, cy - 4, pal.data());

    if (superFlash) {
        const auto boltPal =
            iconPalette(blinkOn(time, 3.0f) ? rgb(255, 230, 120) : rgb(230, 170, 60), 0, 0, 0);
        c.drawSprite(kBoltIcon, rowsOf(kBoltIcon), x + 14, y + 7, boltPal.data());
    }

    const char* label = ready ? (superFlash ? T8("СУПЕР") : T8("ГОТОВО")) : T8("ЗАРЯДКА");
    const uint32_t lc = ready ? (superFlash ? rgb(255, 214, 110) : kCamera) : kTextDim;
    font::drawTextShadow(c, right - font::textWidth(label), y + 23, label, lc, kShadow);
}

// ---------------------------------------------------------------------------
//  Замок: [замок] ЗАМОК 12, под надписью — полоска оставшегося времени.
//  Блок 56x11 от (x, y). Последние 3 секунды мигает.
// ---------------------------------------------------------------------------
void drawLockStatus(Canvas& c, int x, int y, float secondsLeft) {
    if (secondsLeft <= 0.0f) return;
    const bool ending = secondsLeft < 3.0f;
    const bool dim = ending && !blinkOn(secondsLeft, 3.0f);
    const uint32_t col = dim ? scaleColor(kLock, 0.45f) : kLock;
    const auto pal = iconPalette(col, kShadow, rgb(20, 40, 26), col);
    c.drawSprite(kLockIcon, rowsOf(kLockIcon), x, y, pal.data());
    font::drawTextShadow(c, x + 10, y, T8("ЗАМОК"), col, kShadow);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::ceil(secondsLeft)));
    font::drawTextShadow(c, x + 56 - font::textWidth(buf), y, buf, col, kShadow);
    // Полоска: полный замок длится 15 секунд.
    const int bw = 46;
    const int fw = clampi(roundi(saturate(secondsLeft / 15.0f) * static_cast<float>(bw)), 0, bw);
    c.fillRect(x + 10, y + 9, bw, 1, kBarEmpty);
    c.fillRect(x + 10, y + 9, fw, 1, scaleColor(col, 0.8f));
}

// ---------------------------------------------------------------------------
//  Подсказка клавиши: клавиша-«колпачок» высотой 9 px и подпись справа.
// ---------------------------------------------------------------------------
int drawKeyHint(Canvas& c, int x, int y, const char* key, const char* label, float alpha) {
    if (!key) key = "";
    if (!label) label = "";
    const int capW = font::textWidth(key) + 6;
    const int labelW = font::textWidth(label);
    const int total = capW + (labelW > 0 ? 3 + labelW : 0);
    drawBlended(c, x - 1, y - 1, total + 3, 12, alpha, [&](Canvas& t, int dx, int dy) {
        const int bx = x + dx, by = y + dy;
        // Колпачок: светлая верхняя грань, тёмная «толщина» снизу, срезанные углы.
        t.fillRect(bx + 1, by, capW - 2, 8, kCapFace);
        t.fillRect(bx, by + 1, 1, 6, kCapFace);
        t.fillRect(bx + capW - 1, by + 1, 1, 6, kCapFace);
        t.fillRect(bx + 1, by, capW - 2, 1, kFrameLight);
        t.fillRect(bx + 1, by + 8, capW - 2, 1, kCapEdge);
        t.plot(bx, by + 7, kCapEdge);
        t.plot(bx + capW - 1, by + 7, kCapEdge);
        font::drawText(t, bx + 3, by + 1, key, kText);
        if (labelW > 0) font::drawTextShadow(t, bx + capW + 3, by + 1, label, kTextDim, kShadow);
    });
    return total;
}

// ---------------------------------------------------------------------------
//  Субтитры: «ИМЯ: текст» — имя цветом говорящего, текст основным цветом.
//  Раскладка по словам считается по ПОЛНОМУ тексту (строки не «прыгают»
//  во время печати), печатная машинка открывает только символы текста.
// ---------------------------------------------------------------------------
namespace {

struct SubWord {
    const char* begin = nullptr; // байты слова в исходной строке
    const char* end = nullptr;
    int cols = 0;                // ширина в символах
    int textIndex = 0;           // индекс первого символа в тексте реплики (-1 — имя)
    bool forceBreak = false;     // перед словом был явный перенос строки
};

struct SubLine {
    int firstWord = 0;
    int wordCount = 0;
    int cols = 0;
};

constexpr int kMaxWords = 160;
constexpr int kMaxLines = 12;

} // namespace

void drawSubtitle(Canvas& c, int cx, int y, int maxWidth, const char* speaker, const char* text,
                  int visibleChars, float alpha, uint32_t speakerColor) {
    if (alpha <= 0.004f) return;
    if (!text) text = "";
    const bool hasName = speaker && *speaker;
    if (!hasName && !*text) return;

    // ---- Разбиение на слова (без выделения памяти) ----
    std::array<SubWord, kMaxWords> words{};
    int wordCount = 0;
    char nameBuf[64] = {};
    if (hasName) {
        std::snprintf(nameBuf, sizeof(nameBuf), "%s:", speaker);
        SubWord& w = words[wordCount++];
        w.begin = nameBuf;
        w.end = nameBuf + std::strlen(nameBuf);
        w.cols = font::countChars(nameBuf);
        w.textIndex = -1;
    }
    int charIndex = 0;
    bool pendingBreak = false;
    const char* p = text;
    while (*p && wordCount < kMaxWords) {
        if (*p == ' ') {
            ++p;
            ++charIndex;
            continue;
        }
        if (*p == '\n') {
            ++p;
            ++charIndex;
            pendingBreak = true;
            continue;
        }
        SubWord& w = words[wordCount++];
        w.begin = p;
        w.textIndex = charIndex;
        w.forceBreak = pendingBreak;
        pendingBreak = false;
        while (*p && *p != ' ' && *p != '\n') {
            font::decodeUtf8(p);
            ++w.cols;
            ++charIndex;
        }
        w.end = p;
    }

    // ---- Строки ----
    const int maxCols = std::max(1, (maxWidth + 1) / font::kAdvance);
    std::array<SubLine, kMaxLines> lines{};
    int lineCount = 0;
    for (int i = 0; i < wordCount; ++i) {
        const SubWord& w = words[static_cast<size_t>(i)];
        SubLine* cur = lineCount > 0 ? &lines[static_cast<size_t>(lineCount - 1)] : nullptr;
        const bool fits = cur && cur->wordCount > 0 && cur->cols + 1 + w.cols <= maxCols;
        if (cur && cur->wordCount > 0 && fits && !w.forceBreak) {
            cur->cols += 1 + w.cols;
            ++cur->wordCount;
            continue;
        }
        if (lineCount >= kMaxLines) break;
        SubLine& nl = lines[static_cast<size_t>(lineCount++)];
        nl.firstWord = i;
        nl.wordCount = 1;
        nl.cols = w.cols;
    }
    if (lineCount == 0) return;
    int widest = 0;
    for (int i = 0; i < lineCount; ++i) widest = std::max(widest, lines[static_cast<size_t>(i)].cols);
    const int blockW = widest * font::kAdvance - 1;
    const int blockH = lineCount * font::kLineHeight;

    // ---- Подложка: тёмная полупрозрачная полоса со срезанными углами ----
    const int sx = cx - blockW / 2 - 5, sy = y - 3;
    const int sw = blockW + 10, sh = blockH + 4;
    const float stripA = 0.62f * saturate(alpha);
    c.blendRect(sx + 1, sy, sw - 2, sh, kPanel, stripA);
    c.blendRect(sx, sy + 1, 1, sh - 2, kPanel, stripA);
    c.blendRect(sx + sw - 1, sy + 1, 1, sh - 2, kPanel, stripA);

    // ---- Текст ----
    const int vis = visibleChars < 0 ? 1 << 30 : visibleChars;
    drawBlended(c, sx - 1, sy - 1, sw + 3, sh + 3, alpha, [&](Canvas& t, int dx, int dy) {
        for (int li = 0; li < lineCount; ++li) {
            const SubLine& ln = lines[static_cast<size_t>(li)];
            int px = cx - (ln.cols * font::kAdvance - 1) / 2 + dx;
            const int py = y + li * font::kLineHeight + dy;
            for (int wi = ln.firstWord; wi < ln.firstWord + ln.wordCount; ++wi) {
                const SubWord& w = words[static_cast<size_t>(wi)];
                const bool isName = w.textIndex < 0;
                const char* q = w.begin;
                int idx = w.textIndex;
                while (q < w.end) {
                    const char* start = q;
                    font::decodeUtf8(q);
                    if (isName || idx < vis)
                        drawGlyphRange(t, px, py, start, q, isName ? speakerColor : kText);
                    px += font::kAdvance;
                    if (!isName) ++idx;
                }
                px += font::kAdvance; // пробел между словами
            }
        }
    });
}

// ---------------------------------------------------------------------------
//  Панель: затемнение, рамка со срезанными углами, светлая кромка сверху.
// ---------------------------------------------------------------------------
void drawPanel(Canvas& c, int x, int y, int w, int h, float alpha) {
    if (w < 4 || h < 4) return;
    alpha = saturate(alpha);
    c.blendRect(x + 1, y + 1, w - 2, h - 2, kPanel, alpha);
    const float fa = std::min(1.0f, alpha + 0.1f);
    // Рамка без угловых пикселей (скруглённый вид).
    c.blendRect(x + 1, y, w - 2, 1, kFrameLight, fa);
    c.blendRect(x + 1, y + h - 1, w - 2, 1, kFrameMid, fa);
    c.blendRect(x, y + 1, 1, h - 2, kFrameMid, fa);
    c.blendRect(x + w - 1, y + 1, 1, h - 2, kFrameMid, fa);
    // Внутренняя кромка и уголки-заклёпки.
    c.blendRect(x + 2, y + 1, w - 4, 1, rgb(32, 29, 42), fa);
    c.blendPixel(x + 2, y + 2, kFrameLight, fa);
    c.blendPixel(x + w - 3, y + 2, kFrameLight, fa);
    c.blendPixel(x + 2, y + h - 3, kFrameMid, fa);
    c.blendPixel(x + w - 3, y + h - 3, kFrameMid, fa);
}

} // namespace hud
