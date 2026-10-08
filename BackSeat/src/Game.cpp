// ============================================================================
//  Game.cpp — главный конечный автомат «Заднего сиденья» (см. Game.h).
//
//  Game владеет всеми модулями (сцена, мини-игра, монстр, родители) и связывает
//  их: события превращаются в звук, картинку и последствия; здесь же считаются
//  бензин, путь, камера и замок, рисуются меню, вступление, пауза и экраны итога.
//
//  Порядок тика поездки (фиксированный шаг 60 Гц):
//    ввод -> машина (бензин, скорость, путь) -> камера и замок -> мини-игра
//    -> монстр -> сцена -> родители -> подсказки -> переходы состояний.
//  В конце каждого тика, в любом состоянии, выставляются непрерывные слои звука.
//
//  Переходы между экранами идут через затемнение (fade): кадр уходит в чёрное,
//  состояние меняется, кадр проявляется. Без общего затемнения — только пауза
//  (её панель проявляется сама за 0.15 с) и скример (он сам уходит в черноту).
// ============================================================================
#include "Game.h"

#include "Audio.h"
#include "Common.h"
#include "ConsoleMiniGame.h"
#include "Font.h"
#include "Hud.h"
#include "Input.h"
#include "Monster.h"
#include "Parents.h"
#include "RealWorldScene.h"
#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

// ---- Поездка -----------------------------------------------------------------
constexpr float kTripSeconds = 300.0f;  // путь при полной скорости, с
constexpr float kTripKm = 7.5f;         // длина пути, км
constexpr float kNearHome = 0.92f;      // «скоро дом»: родители мирятся
constexpr float kArriveDrive = 4.0f;    // подъезд к дому до остановки, с
constexpr float kArriveHold = 1.8f;     // стоим у дома до экрана победы, с

// ---- Бензин и двигатель --------------------------------------------------------
constexpr float kFuelStart = 65.0f;
constexpr float kFuelMax = 100.0f;
constexpr float kFuelDrain = 1.5f;      // расход, ед./с
constexpr float kCanister = 22.0f;      // канистра из мини-игры
constexpr float kLowFuel = 25.0f;       // предупреждение о бензине
constexpr float kLowFuelRearm = 35.0f;  // новое предупреждение — только после заправки выше
constexpr float kSputterTime = 4.0f;    // двигатель чихает; канистра ещё спасает
constexpr float kStallDark = 2.0f;      // заглох: темнота и тишина перед скримером
constexpr float kSputterSpeed = 0.55f;  // скорость машины, пока двигатель чихает
constexpr float kCoughMin = 0.45f;      // интервал «чихов», с
constexpr float kCoughMax = 0.85f;

// ---- Камера и предметы ----------------------------------------------------------
constexpr float kRecharge = 4.0f;       // перезарядка вспышки, с
constexpr float kFlashTime = 0.35f;     // белая вспышка на экране гаснет за...
constexpr float kSuperFlashTime = 0.5f; // ...усиленная — чуть дольше и теплее
constexpr float kLockTime = 15.0f;      // замок: длительность, с
constexpr float kLockBreakMul = 0.35f;  // ...множитель скорости взлома
constexpr float kLockThreat = 15.0f;    // ...и мгновенное снижение угрозы

// ---- Монстр ---------------------------------------------------------------------
constexpr float kFirstVisit = 22.0f;      // первый визит, с от начала поездки
constexpr float kAggrPerProgress = 0.9f;  // агрессия = прогресс * 0.9 + аварии
constexpr float kCrashBoostStep = 0.02f;  // каждая авария в мини-игре злит монстра...
constexpr float kCrashBoostCap = 0.15f;   // ...но не больше этого
constexpr float kAttract = 4.0f;          // писк консоли сокращает ожидание монстра, с
constexpr float kCueThudChance = 0.7f;    // родители реагируют на удар о крышу...
constexpr float kCueBangChance = 0.3f;    // ...на удар по стеклу...
constexpr float kCueCrashChance = 0.4f;   // ...и на писк консоли (не каждый раз)

// ---- Уровни сложности (выбираются в меню стрелками) --------------------------
struct DifficultyPreset {
    const char* name;  // название для меню и экрана итогов (UTF-8)
    float fuelDrain;   // расход бензина, ед./с
    float canister;    // сколько даёт канистра
    float firstVisit;  // первый визит монстра, с от начала поездки
    float breakMul;    // множитель скорости взлома
    float recharge;    // перезарядка вспышки, с
};
const DifficultyPreset kDifficulties[] = {
    {T8("ЛЁГКАЯ"), 1.1f, 28.0f, 35.0f, 0.7f, 3.0f},
    {T8("НОРМАЛЬНАЯ"), kFuelDrain, kCanister, kFirstVisit, 1.0f, kRecharge},
    {T8("СЛОЖНАЯ"), 2.0f, 10.0f, 5.0f, 2.0f, 4.0f},
};
constexpr int kDifficultyCount = static_cast<int>(sizeof(kDifficulties) / sizeof(kDifficulties[0]));
constexpr int kDefaultDifficulty = 1;

// ---- Виды и экраны ----------------------------------------------------------------
constexpr int kW = cfg::kScreenW;
constexpr int kH = cfg::kScreenH;
constexpr float kRaiseTime = 0.2f;        // подъём/опускание консоли, с
constexpr float kJumpscareTime = 1.75f;   // renderJumpscare уходит в черноту к 1.7 с
constexpr float kResultInputDelay = 0.8f; // экран итога не реагирует на клавиши сразу
constexpr float kRaiseHintAt = 2.0f;      // подсказка про TAB после старта, с...
constexpr float kRaiseHintUntil = 40.0f;  // ...и напоминание, пока консоль не поднимали
constexpr float kToastTime = 1.6f;        // сообщение «звук вкл/выкл», с
constexpr float kVictoryEerie = 7.0f;     // одинокий шаг по крыше на экране победы, с

// ---- Цвета ---------------------------------------------------------------------------
constexpr uint32_t kDadColor = rgb(132, 178, 236);  // имя папы в субтитрах
constexpr uint32_t kMomColor = rgb(238, 152, 162);  // имя мамы
constexpr uint32_t kTitleRed = rgb(176, 22, 30);    // заголовок меню
constexpr uint32_t kTitleDeep = rgb(86, 8, 14);     // его «подтёк»
constexpr uint32_t kKeyColor = rgb(232, 190, 116);  // клавиши в панели управления
constexpr uint32_t kWarm = rgb(255, 200, 118);      // победа
constexpr uint32_t kFlashCold = rgb(240, 244, 255); // вспышка камеры
constexpr uint32_t kFlashWarm = rgb(255, 238, 204); // усиленная вспышка

// ---- Вступление ---------------------------------------------------------------------
const char* const kIntroLines[] = {
    T8("Мы опять выехали от бабушки слишком поздно."),
    T8("Мама с папой снова ругаются. Думают, что я сплю."),
    T8("Короткая дорога через лес. Ни фонарей, ни машин."),
    T8("Светится только экран моей консоли."),
};
constexpr int kIntroCount = static_cast<int>(sizeof(kIntroLines) / sizeof(kIntroLines[0]));
constexpr float kIntroFirst = 0.9f;   // первая строка после проявления, с
constexpr float kIntroType = 24.0f;   // скорость печати, символов в секунду
constexpr float kIntroPause = 1.1f;   // пауза между строками
constexpr float kIntroHold = 2.6f;    // после последней строки — в путь
constexpr int kIntroY = 58;           // верх первой строки
constexpr int kIntroStep = 16;        // шаг строк

// Приглушённая ссора за кадром вступления: {время, папа?, длительность}.
struct IntroMumble {
    float at = 0.0f;
    bool dad = true;
    float dur = 1.0f;
};
constexpr IntroMumble kIntroMumbles[] = {
    {2.4f, true, 2.3f}, {5.0f, false, 1.9f}, {7.3f, true, 1.5f}, {9.2f, false, 2.4f},
};

// ---- Внутренние перечисления ----------------------------------------------------------
enum class Goto { Menu, Intro, NewTrip, GameOver, Victory }; // куда ведёт затемнение
enum class EngineState { Running, Sputtering, Dead };
enum class Hint { None, Raise, Camera, Fuel, NoCamera };     // подсказки новичку

// ---- Вспомогательные функции -----------------------------------------------------------

// Перемешивание битов (зерно поездки из счётчика, зёрна модулей из зерна поездки).
uint32_t mixSeed(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x ? x : 0x9E3779B9u;
}

// Ширина подсказки-клавиши (так же, как считает hud::drawKeyHint).
int keyHintWidth(const char* key, const char* label) {
    const int labelW = font::textWidth(label);
    return font::textWidth(key) + 6 + (labelW > 0 ? 3 + labelW : 0);
}

// Однострочный текст с тенью, центрированный по cx (без выделения памяти).
void textCentered(Canvas& c, int cx, int y, const char* s, uint32_t col, int scale = 1) {
    font::drawTextShadow(c, cx - font::textWidth(s, scale) / 2, y, s, col, hud::kShadow, scale);
}

// Однострочный текст, выровненный по правому краю.
void textRight(Canvas& c, int right, int y, const char* s, uint32_t col) {
    font::drawTextShadow(c, right - font::textWidth(s), y, s, col, hud::kShadow);
}

// Упаковка канала без вызова rgb() — для горячих циклов отладочной сборки.
inline uint32_t pack(int r, int g, int b) {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// Таблица виньетки: 0 в центре кадра .. 256 в углах.
std::vector<uint16_t> buildVignette() {
    std::vector<uint16_t> v(static_cast<size_t>(kW * kH));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - 160.0f) / 160.0f;
            const float ny = (static_cast<float>(y) + 0.5f - 90.0f) / 90.0f;
            const float d = std::sqrt(nx * nx * 0.8f + ny * ny * 0.7f);
            v[static_cast<size_t>(y * kW + x)] = static_cast<uint16_t>(roundi(256.0f * smoothstep(0.42f, 1.22f, d)));
        }
    }
    return v;
}

// Затемнение кадра (k — общая яркость) с виньеткой силы vigStrength (0..1).
void shadeFrame(Canvas& c, const std::vector<uint16_t>& vig, float k, float vigStrength) {
    const size_t n = static_cast<size_t>(c.width()) * static_cast<size_t>(c.height());
    if (n == 0 || vig.size() != n) {
        c.darken(k);
        return;
    }
    const int kq = clampi(roundi(k * 256.0f), 0, 256);
    const int sq = clampi(roundi(vigStrength * 256.0f), 0, 256);
    uint32_t* px = c.data();
    const uint16_t* vg = vig.data();
    for (size_t i = 0; i < n; ++i) {
        const uint32_t f = static_cast<uint32_t>((kq * (256 - ((static_cast<int>(vg[i]) * sq) >> 8))) >> 8);
        const uint32_t p = px[i];
        px[i] = ((((p & 0xFF00FFu) * f) >> 8) & 0xFF00FFu) | ((((p & 0x00FF00u) * f) >> 8) & 0x00FF00u);
    }
}

// Размытие прямоугольным окном радиуса radius (два прохода) с затемнением k.
// tmp — рабочий буфер, растёт один раз (в кадре память не выделяется).
void blurDarken(Canvas& c, std::vector<uint32_t>& tmp, int radius, float k) {
    const int w = c.width(), h = c.height();
    if (w <= 0 || h <= 0) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    const int kq = clampi(roundi(k * 256.0f), 0, 256);
    uint32_t* px = c.data();
    if (radius <= 0) {
        if (kq >= 256) return;
        const uint32_t f = static_cast<uint32_t>(kq);
        for (size_t i = 0; i < n; ++i) {
            const uint32_t p = px[i];
            px[i] = ((((p & 0xFF00FFu) * f) >> 8) & 0xFF00FFu) | ((((p & 0x00FF00u) * f) >> 8) & 0x00FF00u);
        }
        return;
    }
    if (tmp.size() < n) tmp.resize(n);
    const int taps = 2 * radius + 1;
    // Горизонтальный проход: скользящая сумма, края повторяют крайний пиксель.
    for (int y = 0; y < h; ++y) {
        const uint32_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(w);
        uint32_t* dst = tmp.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        int sr = 0, sg = 0, sb = 0;
        for (int i = -radius; i <= radius; ++i) {
            const uint32_t p = row[i < 0 ? 0 : (i >= w ? w - 1 : i)];
            sr += static_cast<int>((p >> 16) & 255u);
            sg += static_cast<int>((p >> 8) & 255u);
            sb += static_cast<int>(p & 255u);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = pack(sr / taps, sg / taps, sb / taps);
            const int ia = x - radius, ib = x + radius + 1;
            const uint32_t pa = row[ia < 0 ? 0 : ia];
            const uint32_t pb = row[ib >= w ? w - 1 : ib];
            sr += static_cast<int>((pb >> 16) & 255u) - static_cast<int>((pa >> 16) & 255u);
            sg += static_cast<int>((pb >> 8) & 255u) - static_cast<int>((pa >> 8) & 255u);
            sb += static_cast<int>(pb & 255u) - static_cast<int>(pa & 255u);
        }
    }
    // Вертикальный проход сразу с затемнением.
    const int div = taps * 256;
    for (int x = 0; x < w; ++x) {
        const uint32_t* col = tmp.data() + x;
        int sr = 0, sg = 0, sb = 0;
        for (int i = -radius; i <= radius; ++i) {
            const uint32_t p = col[static_cast<size_t>(i < 0 ? 0 : (i >= h ? h - 1 : i)) * static_cast<size_t>(w)];
            sr += static_cast<int>((p >> 16) & 255u);
            sg += static_cast<int>((p >> 8) & 255u);
            sb += static_cast<int>(p & 255u);
        }
        for (int y = 0; y < h; ++y) {
            px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                pack(sr * kq / div, sg * kq / div, sb * kq / div);
            const int ia = y - radius, ib = y + radius + 1;
            const uint32_t pa = col[static_cast<size_t>(ia < 0 ? 0 : ia) * static_cast<size_t>(w)];
            const uint32_t pb = col[static_cast<size_t>(ib >= h ? h - 1 : ib) * static_cast<size_t>(w)];
            sr += static_cast<int>((pb >> 16) & 255u) - static_cast<int>((pa >> 16) & 255u);
            sg += static_cast<int>((pb >> 8) & 255u) - static_cast<int>((pa >> 8) & 255u);
            sb += static_cast<int>(pb & 255u) - static_cast<int>(pa & 255u);
        }
    }
}

// Красное «давление» по краям кадра (монстр ломится, пока смотришь в консоль).
void edgeRed(Canvas& c, const std::vector<uint16_t>& vig, float k) {
    const size_t n = static_cast<size_t>(c.width()) * static_cast<size_t>(c.height());
    const int kq = clampi(roundi(k * 256.0f), 0, 256);
    if (kq <= 0 || vig.size() != n) return;
    uint32_t* px = c.data();
    const uint16_t* vg = vig.data();
    for (size_t i = 0; i < n; ++i) {
        const int a = (static_cast<int>(vg[i]) * kq) >> 8; // 0..256
        if (a <= 0) continue;
        const uint32_t p = px[i];
        const int r = static_cast<int>((p >> 16) & 255u), g = static_cast<int>((p >> 8) & 255u),
                  b = static_cast<int>(p & 255u);
        // Разность может быть отрицательной: делим, а не сдвигаем.
        px[i] = pack(r + (150 - r) * a / 512, g - ((g * a) >> 9), b - ((b * a) >> 9));
    }
}

// Тёмно-красный монохром с виньеткой — фон экрана проигрыша.
void bloodTone(Canvas& c, const std::vector<uint16_t>& vig) {
    const size_t n = static_cast<size_t>(c.width()) * static_cast<size_t>(c.height());
    if (vig.size() != n) return;
    uint32_t* px = c.data();
    const uint16_t* vg = vig.data();
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = px[i];
        const int lum = static_cast<int>((((p >> 16) & 255u) * 77u + ((p >> 8) & 255u) * 150u + (p & 255u) * 29u) >> 8);
        const int f = 256 - ((static_cast<int>(vg[i]) * 220) >> 8);
        const int l = (lum * f) >> 8;
        px[i] = pack(std::min(255, 6 + ((l * 150) >> 8)), (l * 26) >> 8, 2 + ((l * 30) >> 8));
    }
}

// Сплошной цвет поверх всего кадра с прозрачностью a.
void overlay(Canvas& c, uint32_t color, float a) {
    const int aq = clampi(roundi(a * 256.0f), 0, 256);
    if (aq <= 0) return;
    const size_t n = static_cast<size_t>(c.width()) * static_cast<size_t>(c.height());
    uint32_t* px = c.data();
    const int cr = colR(color), cg = colG(color), cb = colB(color);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = px[i];
        const int r = static_cast<int>((p >> 16) & 255u), g = static_cast<int>((p >> 8) & 255u),
                  b = static_cast<int>(p & 255u);
        px[i] = pack(r + (cr - r) * aq / 256, g + (cg - g) * aq / 256, b + (cb - b) * aq / 256);
    }
}

// Полупрозрачная тёмная полоса под подсказкой (срезанные углы, светлая кромка сверху).
void hintStrip(Canvas& c, int x, int y, int w, int h, float a) {
    if (w < 4 || h < 4 || a <= 0.0f) return;
    c.blendRect(x + 1, y, w - 2, h, hud::kPanel, 0.72f * a);
    c.blendRect(x, y + 1, 1, h - 2, hud::kPanel, 0.72f * a);
    c.blendRect(x + w - 1, y + 1, 1, h - 2, hud::kPanel, 0.72f * a);
    c.blendRect(x + 2, y, w - 4, 1, rgb(98, 92, 114), 0.5f * a);
}

// Подсказка из одной или двух клавиш с подписями, центрированная по cx.
void keyBanner(Canvas& c, int cx, int y, const char* k1, const char* l1, const char* k2, const char* l2,
               float a) {
    const int w1 = keyHintWidth(k1, l1);
    const int w2 = k2 ? keyHintWidth(k2, l2) : 0;
    const int gap = k2 ? 12 : 0;
    const int total = w1 + gap + w2;
    const int x0 = cx - total / 2;
    hintStrip(c, x0 - 7, y - 3, total + 14, 15, a);
    hud::drawKeyHint(c, x0, y, k1, l1, a);
    if (k2) hud::drawKeyHint(c, x0 + w1 + gap, y, k2, l2, a);
}

// Текстовая подсказка: цвет «проявляется» из подложки вместо смешивания по пикселям.
void textBanner(Canvas& c, int cx, int y, const char* text, uint32_t color, float a) {
    const int w = font::textWidth(text);
    hintStrip(c, cx - w / 2 - 7, y - 3, w + 14, 15, a);
    const uint32_t col = lerpColor(hud::kPanel, color, a);
    const uint32_t sh = lerpColor(hud::kPanel, hud::kShadow, a);
    font::drawTextShadow(c, cx - w / 2, y + 1, text, col, sh);
}

// Мерцание «перегорающей лампы»: обычно ~1, изредка короткие провалы.
float flickerAt(float t) {
    const int slot = static_cast<int>(std::floor(std::fmod(std::max(0.0f, t), 10000.0f) * 14.0f));
    const float h = hash01(static_cast<uint32_t>(slot) * 2654435761u + 0x7171u);
    float k = 0.93f + 0.07f * std::sin(t * 1.7f);
    if (h < 0.035f) k *= 0.42f;
    else if (h < 0.075f) k *= 0.74f;
    return k;
}

// Длительность печати строки вступления.
float introTypeTime(int i) {
    return static_cast<float>(font::countChars(kIntroLines[i])) / kIntroType;
}

} // namespace

// ============================================================================
//  Состояние игры
// ============================================================================
struct Game::State {
    explicit State(Audio& a) : audio(a) {}

    // ---- Модули ----
    Audio& audio;
    RealWorldScene scene;
    ConsoleMiniGame mini;
    Monster monster;
    Parents parents;
    Rng rng;

    // ---- Буферы кадра ----
    Canvas screen{cfg::kConsoleW, cfg::kConsoleH}; // кадр мини-игры
    Canvas photoFrame{kW, kH};                      // чистый кадр для полароида
    Canvas consoleBg{kW, kH};                       // размытый мир за поднятой консолью
    bool consoleBgValid = false;
    unsigned consoleBgPhase = 0;
    std::vector<uint32_t> blurTmp = std::vector<uint32_t>(static_cast<size_t>(kW * kH));
    std::vector<uint16_t> vig = buildVignette();

    // ---- Автомат состояний ----
    GameState state = GameState::Menu;
    float stateT = 0.0f;        // секунд в текущем состоянии
    float animTime = 0.0f;      // монотонное время анимаций (стоит на паузе)
    float fade = 1.0f;          // чёрный слой поверх кадра, 1 — полностью чёрный
    float fadeOutRate = 1.0f;
    float fadeInRate = 1.0f;
    bool fadePending = false;   // идёт затемнение перед сменой экрана
    Goto fadeGoto = Goto::Menu;
    bool quit = false;
    bool fullscreenReq = false;
    uint32_t tripCounter = 0;
    float toastT = kToastTime;  // сообщение «звук вкл/выкл»
    bool toastMuted = false;

    // ---- Поездка ----
    ViewMode view = ViewMode::RealWorld;
    LoseReason lose = LoseReason::None;
    float tripTime = 0.0f;
    float progress = 0.0f;
    float fuel = kFuelStart;
    EngineState engine = EngineState::Running;
    float sputterT = 0.0f;
    float coughT = 0.0f;        // до следующего «чиха»
    float coughDip = 0.0f;      // провал оборотов от последнего «чиха» (затухает)
    float carSpeed = 1.0f;      // 0..1
    float engineVis = 1.0f;     // «жизнь» приборки и фар для сцены
    float arriveT = 0.0f;
    float arrive = 0.0f;        // 0..1 — дом приближается
    float arriveSpeed0 = 1.0f;  // скорость в момент начала подъезда
    bool jinglePlayed = false;
    bool eeriePlayed = false;
    float stallDelay = 0.0f;    // темнота перед скримером (только бензин)
    bool jumpscareOn = false;
    bool lowFuelArmed = true;
    bool nearHomeCued = false;

    // ---- Камера и предметы ----
    bool hasCamera = false;
    float charge = 1.0f;
    bool superFlash = false;
    float flashT = 10.0f;       // секунд с последней вспышки
    float flashDur = kFlashTime;
    bool flashSuper = false;
    bool capturePending = false;
    bool captureHit = false;
    Entry aim = Entry::None;
    float lockLeft = 0.0f;
    float crashBoost = 0.0f;

    // ---- Консоль ----
    float raise = 0.0f;         // 0 — на коленях, 1 — у лица
    int steer = 0;
    float jolt = 0.0f;          // встряска консоли при аварии в мини-игре
    bool raisedOnce = false;

    // ---- Подсказки ----
    Hint hint = Hint::None;
    float hintT = 0.0f;
    float hintDur = 0.0f;
    Hint hintQueued = Hint::None; // ждёт, пока догорит текущая
    float hintQueuedDur = 0.0f;
    bool hintRaiseShown = false;
    bool hintCameraShown = false;
    bool hintFuelShown = false;

    // ---- Статистика ----
    int difficulty = kDefaultDifficulty; // индекс в kDifficulties; переживает рестарты
    const DifficultyPreset& diff() const { return kDifficulties[difficulty]; }
    int photos = 0;
    int hits = 0;

    // ---- Сглаженные слои звука ----
    float ambMusic = 0.0f;
    float ambDread = 0.0f;
    float ambHeart = 0.0f;
    float ambMaster = 0.9f;

    // ---- Вступление ----
    std::array<float, kIntroCount> introStart{};
    float introEnd = 0.0f;
    int introMumbleNext = 0;

    // ---- Переходы ----
    void go(Goto g, float outTime, float inTime);
    void perform(Goto g);
    void updateFade(float dt);
    void enterMenu();
    void enterIntro();
    void startTrip(uint32_t seed);
    void enterPause();
    void enterDying(LoseReason r);
    void startJumpscare();
    void enterArriving();
    uint32_t nextSeed();

    // ---- Тики состояний ----
    void updateMenu(float dt, const Input& in, bool accept);
    void updateIntro(float dt, const Input& in, bool accept);
    void updatePlaying(float dt, const Input& in, bool accept);
    void updatePaused(float dt, const Input& in, bool accept);
    void updateDying(float dt);
    void updateArriving(float dt);
    void updateResult(float dt, const Input& in, bool accept);
    void updateAmbient(float dt);

    // ---- Механики ----
    void handleAim(const Input& in);
    void toggleView();
    void shoot();
    void updateCar(float dt);
    void updateItems(float dt);
    void updateMini(float dt, bool live);
    void onMiniEvent(MiniEvent e);
    void updateMonster(float dt, float aggression, float breakMul, bool live);
    void playMonsterSound(const MonsterEventInfo& e);
    void updateWorld(float dt);
    void updateParents(float dt);
    void updateHints(float dt);
    void showHint(Hint h, float dur);
    void dismissHint(Hint h);
    float hintAlpha() const;

    // ---- Отрисовка ----
    RealWorldView makeView(bool hud) const;
    float raiseEased() const { return smoothstep(0.0f, 1.0f, raise); }
    void capturePhoto();
    void renderMenu(Canvas& out);
    void renderIntro(Canvas& out);
    void renderTrip(Canvas& out, bool hud, bool overlays);
    void renderConsoleView(Canvas& out, float r, bool hud);
    void renderSubtitle(Canvas& out, bool consoleView) const;
    void renderHint(Canvas& out, bool consoleView) const;
    void renderPause(Canvas& out);
    void renderDying(Canvas& out);
    void renderGameOver(Canvas& out);
    void renderVictory(Canvas& out);
    void renderResult(Canvas& out, bool victory);
    void renderToast(Canvas& out) const;
};

// ============================================================================
//  Переходы между состояниями
// ============================================================================
void Game::State::go(Goto g, float outTime, float inTime) {
    if (fadePending) return; // переход уже идёт
    fadeGoto = g;
    fadeInRate = 1.0f / std::max(0.05f, inTime);
    if (outTime <= 0.0f) {
        fade = 1.0f;
        perform(g);
        return;
    }
    fadeOutRate = 1.0f / outTime;
    fadePending = true;
}

void Game::State::perform(Goto g) {
    switch (g) {
    case Goto::Menu: enterMenu(); break;
    case Goto::Intro: enterIntro(); break;
    case Goto::NewTrip: startTrip(nextSeed()); break;
    case Goto::GameOver:
        state = GameState::GameOver;
        stateT = 0.0f;
        audio.play(Sfx::LoseSting);
        break;
    case Goto::Victory:
        state = GameState::Victory;
        stateT = 0.0f;
        eeriePlayed = false;
        break;
    }
}

void Game::State::updateFade(float dt) {
    if (fadePending) {
        fade += dt * fadeOutRate;
        if (fade >= 1.0f) {
            fade = 1.0f;
            fadePending = false;
            perform(fadeGoto);
        }
    } else {
        fade = std::max(0.0f, fade - dt * fadeInRate);
    }
}

uint32_t Game::State::nextSeed() {
    ++tripCounter;
    return mixSeed(tripCounter * 2654435761u + 0xB4C5EA7u);
}

// Меню: за титулом едет та же машина — без монстра, без интерфейса.
void Game::State::enterMenu() {
    state = GameState::Menu;
    stateT = 0.0f;
    const uint32_t seed = nextSeed();
    scene.reset(seed);
    mini.reset(mixSeed(seed ^ 0x6D696E69u));
    monster.reset(mixSeed(seed ^ 0x6D6F6E73u), 1.0e6f);
    monster.flee(true);
    parents.reset(mixSeed(seed ^ 0x70617265u));
    view = ViewMode::RealWorld;
    raise = 0.0f;
    carSpeed = 1.0f;
    engineVis = 1.0f;
    engine = EngineState::Running;
    fuel = kFuelStart;
    progress = 0.0f;
    arrive = 0.0f;
    hasCamera = false;
    lockLeft = 0.0f;
    flashT = 10.0f;
    capturePending = false;
    hint = Hint::None;
    lose = LoseReason::None;
    audio.stopAllSfx();
}

void Game::State::enterIntro() {
    state = GameState::Intro;
    stateT = 0.0f;
    float t = kIntroFirst;
    for (int i = 0; i < kIntroCount; ++i) {
        introStart[static_cast<size_t>(i)] = t;
        t += introTypeTime(i) + kIntroPause;
    }
    introEnd = t - kIntroPause + kIntroHold;
    introMumbleNext = 0;
}

void Game::State::startTrip(uint32_t seed) {
    scene.reset(seed);
    mini.reset(mixSeed(seed ^ 0x6D696E69u));
    monster.reset(mixSeed(seed ^ 0x6D6F6E73u), diff().firstVisit);
    parents.reset(mixSeed(seed ^ 0x70617265u));
    rng.reseed(mixSeed(seed ^ 0x67616D65u));

    state = GameState::Playing;
    stateT = 0.0f;
    view = ViewMode::RealWorld;
    lose = LoseReason::None;
    tripTime = 0.0f;
    progress = 0.0f;
    fuel = kFuelStart;
    engine = EngineState::Running;
    sputterT = 0.0f;
    coughT = 0.0f;
    coughDip = 0.0f;
    carSpeed = 1.0f;
    engineVis = 1.0f;
    arriveT = 0.0f;
    arrive = 0.0f;
    arriveSpeed0 = 1.0f;
    jinglePlayed = false;
    eeriePlayed = false;
    stallDelay = 0.0f;
    jumpscareOn = false;
    lowFuelArmed = true;
    nearHomeCued = false;

    hasCamera = false;
    charge = 1.0f;
    superFlash = false;
    flashT = 10.0f;
    flashDur = kFlashTime;
    flashSuper = false;
    capturePending = false;
    captureHit = false;
    aim = Entry::None;
    lockLeft = 0.0f;
    crashBoost = 0.0f;

    raise = 0.0f;
    steer = 0;
    jolt = 0.0f;
    raisedOnce = false;
    consoleBgValid = false;
    hint = Hint::None;
    hintT = 0.0f;
    hintDur = 0.0f;
    hintQueued = Hint::None;
    hintRaiseShown = false;
    hintCameraShown = false;
    hintFuelShown = false;
    photos = 0;
    hits = 0;
    ambDread = 0.0f;
    ambHeart = 0.0f;

    audio.stopAllSfx();
    audio.play(Sfx::ConsoleBoot);
}

void Game::State::enterPause() {
    state = GameState::Paused;
    stateT = 0.0f;
    audio.play(Sfx::MenuMove, 0.8f);
}

void Game::State::enterDying(LoseReason r) {
    state = GameState::Dying;
    stateT = 0.0f;
    lose = r;
    view = ViewMode::RealWorld;
    hint = Hint::None;
    hintQueued = Hint::None;
    jumpscareOn = false;
    stallDelay = r == LoseReason::Fuel ? kStallDark : 0.0f;
    if (r == LoseReason::Fuel) {
        // Двигатель уже заглох (EngineDie прозвучал в updateCar): машина катится
        // в темноте, музыка консоли гаснет, потом — скример.
        scene.addShake(0.6f);
    } else {
        startJumpscare();
    }
}

void Game::State::startJumpscare() {
    jumpscareOn = true;
    audio.stopAllSfx();
    if (lose == LoseReason::Monster) audio.play(Sfx::GlassShatter);
    audio.play(Sfx::Jumpscare);
}

void Game::State::enterArriving() {
    state = GameState::Arriving;
    stateT = 0.0f;
    arriveT = 0.0f;
    arriveSpeed0 = std::max(carSpeed, 0.35f);
    progress = 1.0f;
    view = ViewMode::RealWorld;
    monster.flee(true);
    parents.cue(ParentCue::Arrived);
    dismissHint(hint);
    hintQueued = Hint::None;
}

// ============================================================================
//  Тики состояний
// ============================================================================
void Game::State::updateMenu(float dt, const Input& in, bool accept) {
    stateT += dt;
    // Машина едет, консоль на коленях сама крутит демо-заезд.
    scene.update(dt, 1.0f, monster);
    scene.takeStreetlightPassed();
    mini.update(dt, 0, false, 0.15f, false);
    mini.takeEvents();
    if (!accept) return;
    if (in.pressed(Key::Escape)) {
        quit = true;
        return;
    }
    // Выбор сложности: влево/вправо по кругу.
    int step = 0;
    if (in.pressed(Key::Left) || in.pressed(Key::A)) step = -1;
    if (in.pressed(Key::Right) || in.pressed(Key::D)) step = 1;
    if (step != 0) {
        difficulty = (difficulty + step + kDifficultyCount) % kDifficultyCount;
        audio.play(Sfx::MenuMove);
    }
    if (in.pressed(Key::Enter)) {
        audio.play(Sfx::MenuSelect);
        go(Goto::Intro, 0.7f, 0.6f);
    }
}

void Game::State::updateIntro(float dt, const Input& in, bool accept) {
    stateT += dt;
    scene.update(dt, 1.0f, monster);
    scene.takeStreetlightPassed();
    mini.update(dt, 0, false, 0.15f, false);
    mini.takeEvents();
    // Ссора за кадром: приглушённый «бубнёж» из-за спинок кресел.
    const int mumbles = static_cast<int>(sizeof(kIntroMumbles) / sizeof(kIntroMumbles[0]));
    while (introMumbleNext < mumbles && stateT >= kIntroMumbles[introMumbleNext].at) {
        const IntroMumble& m = kIntroMumbles[introMumbleNext++];
        audio.play(m.dad ? Sfx::MumbleDad : Sfx::MumbleMom, 0.75f, m.dad ? -0.35f : 0.35f, m.dur);
    }
    if (!accept) return;
    const bool skip = in.anyPressed() && !in.pressed(Key::M) && !in.pressed(Key::F11);
    if (skip || stateT >= introEnd) go(Goto::NewTrip, skip ? 0.45f : 0.9f, 0.8f);
}

void Game::State::updatePlaying(float dt, const Input& in, bool accept) {
    // ---- Ввод ----
    int steerNow = 0;
    if (accept) {
        if (in.pressed(Key::Escape)) {
            enterPause();
            return;
        }
        if (in.pressed(Key::Tab) || in.pressed(Key::Space) || in.mousePressed(MouseButton::Right)) toggleView();
        if (view == ViewMode::Console) {
            steerNow = in.horizontal();
        } else {
            handleAim(in);
            if (in.pressed(Key::F) || in.pressed(Key::E) || in.mousePressed(MouseButton::Left)) shoot();
        }
    }
    steer = steerNow;
    tripTime += dt;

    // ---- Мир ----
    updateCar(dt);
    updateItems(dt);
    updateMini(dt, true);
    const float aggression = clampf(progress * kAggrPerProgress + crashBoost, 0.0f, 1.0f);
    updateMonster(dt, aggression, diff().breakMul * (lockLeft > 0.0f ? kLockBreakMul : 1.0f), true);
    updateWorld(dt);
    updateParents(dt);
    updateHints(dt);

    // ---- Переходы ----
    if (monster.state() == MonsterState::Entered) {
        enterDying(LoseReason::Monster);
    } else if (engine == EngineState::Dead) {
        enterDying(LoseReason::Fuel);
    } else if (progress >= 1.0f) {
        enterArriving();
    }
}

void Game::State::updatePaused(float dt, const Input& in, bool accept) {
    stateT += dt; // мир стоит; время идёт только для появления панели
    if (!accept) return;
    if (in.pressed(Key::Escape)) {
        state = GameState::Playing;
        audio.play(Sfx::MenuMove, 0.8f);
    } else if (in.pressed(Key::R)) {
        audio.play(Sfx::MenuSelect);
        go(Goto::NewTrip, 0.35f, 0.6f);
    } else if (in.pressed(Key::Q)) {
        audio.play(Sfx::MenuSelect);
        go(Goto::Menu, 0.5f, 0.8f);
    }
}

void Game::State::updateDying(float dt) {
    stateT += dt;
    if (lose == LoseReason::Fuel && !jumpscareOn) {
        // Машина катится по инерции, приборка и фары гаснут, консоль ещё светится.
        carSpeed = approach(carSpeed, 0.0f, dt * 0.7f);
        engineVis = approach(engineVis, 0.0f, dt * 4.0f);
        raise = approach(raise, 0.0f, dt / kRaiseTime);
        flashT += dt;
        scene.update(dt, carSpeed, monster);
        scene.takeStreetlightPassed();
        updateMini(dt, false);
    }
    if (!jumpscareOn && stateT >= stallDelay) startJumpscare();
    if (stateT >= stallDelay + kJumpscareTime) go(Goto::GameOver, 0.0f, 1.2f);
}

void Game::State::updateArriving(float dt) {
    stateT += dt;
    arriveT += dt;
    const float u = saturate(arriveT / kArriveDrive);
    // Равномерное торможение: пройденная доля пути 2u - u^2.
    carSpeed = arriveSpeed0 * (1.0f - u);
    arrive = 2.0f * u - u * u;
    engineVis = approach(engineVis, 1.0f, dt * 4.0f);
    coughDip = 0.0f;
    steer = 0;
    updateItems(dt);
    updateMini(dt, false);
    updateMonster(dt, 0.0f, 1.0f, false);
    updateWorld(dt);
    updateParents(dt);
    updateHints(dt);
    if (!jinglePlayed && arriveT >= kArriveDrive) {
        jinglePlayed = true;
        audio.play(Sfx::WinJingle);
    }
    if (arriveT >= kArriveDrive + kArriveHold) go(Goto::Victory, 1.0f, 1.0f);
}

// Экраны проигрыша и победы.
void Game::State::updateResult(float dt, const Input& in, bool accept) {
    stateT += dt;
    if (state == GameState::Victory) {
        // Машина стоит у дома, родители договаривают.
        carSpeed = 0.0f;
        flashT += dt;
        updateMonster(dt, 0.0f, 1.0f, false);
        scene.update(dt, 0.0f, monster);
        scene.takeStreetlightPassed();
        updateParents(dt);
        // «...Наверное.» — один далёкий шаг по крыше.
        if (!eeriePlayed && stateT >= kVictoryEerie) {
            eeriePlayed = true;
            audio.play(Sfx::RoofStep, 0.28f, 0.55f, 0.8f);
        }
    }
    if (!accept || stateT < kResultInputDelay) return;
    if (in.pressed(Key::R)) {
        audio.play(Sfx::MenuSelect);
        go(Goto::NewTrip, 0.4f, 0.7f);
    } else if (in.pressed(Key::Escape)) {
        audio.play(Sfx::MenuSelect);
        go(Goto::Menu, 0.5f, 0.8f);
    }
}

// ============================================================================
//  Механики поездки
// ============================================================================
void Game::State::handleAim(const Input& in) {
    const Entry before = aim;
    if (in.pressed(Key::A) || in.pressed(Key::Left)) aim = Entry::Left;
    else if (in.pressed(Key::D) || in.pressed(Key::Right)) aim = Entry::Right;
    else if (in.pressed(Key::W) || in.pressed(Key::Up)) aim = Entry::Sunroof;
    else if (in.pressed(Key::S) || in.pressed(Key::Down)) aim = Entry::None;
    // Мышь перехватывает прицел, только когда её действительно двигали.
    if (in.mouseMoved() && in.mouseInside()) aim = scene.aimFromPoint(in.mouseX(), in.mouseY());
    if (aim != before && hasCamera) audio.play(Sfx::MenuMove, 0.3f, 0.0f, 1.3f);
}

void Game::State::toggleView() {
    view = view == ViewMode::RealWorld ? ViewMode::Console : ViewMode::RealWorld;
    if (view == ViewMode::Console) {
        raisedOnce = true;
        dismissHint(Hint::Raise);
    }
}

void Game::State::shoot() {
    if (!hasCamera) {
        audio.play(Sfx::CameraEmpty);
        showHint(Hint::NoCamera, 2.8f);
        return;
    }
    if (charge < 1.0f) {
        audio.play(Sfx::CameraEmpty, 0.8f);
        return;
    }
    const bool super = superFlash;
    const bool hit = monster.tryRepel(aim, super);
    charge = 0.0f;
    superFlash = false;
    flashT = 0.0f;
    flashSuper = super;
    flashDur = super ? kSuperFlashTime : kFlashTime;
    capturePending = true;
    captureHit = hit;
    ++photos;
    if (hit) ++hits;
    audio.play(Sfx::CameraShutter);
    parents.cue(ParentCue::CameraFlash);
}

void Game::State::updateCar(float dt) {
    if (engine != EngineState::Dead) fuel = std::max(0.0f, fuel - diff().fuelDrain * dt);
    switch (engine) {
    case EngineState::Running:
        if (fuel <= 0.0f) {
            engine = EngineState::Sputtering;
            sputterT = 0.0f;
            coughT = 0.0f;
            parents.cue(ParentCue::Stalling);
        }
        break;
    case EngineState::Sputtering:
        if (fuel > 0.0f) { // канистра успела
            engine = EngineState::Running;
            break;
        }
        sputterT += dt;
        coughT -= dt;
        if (coughT <= 0.0f) {
            coughT = rng.range(kCoughMin, kCoughMax);
            coughDip = 1.0f;
            audio.play(Sfx::EngineSputter, 1.0f, -0.2f, rng.range(0.9f, 1.1f));
            scene.addShake(rng.range(0.3f, 0.55f));
        }
        parents.cue(ParentCue::Stalling);
        if (sputterT >= kSputterTime) {
            engine = EngineState::Dead;
            audio.play(Sfx::EngineDie);
        }
        break;
    case EngineState::Dead: break;
    }

    float target = 1.0f;
    if (engine == EngineState::Sputtering) target = kSputterSpeed - 0.25f * coughDip;
    else if (engine == EngineState::Dead) target = 0.0f;
    carSpeed = approach(carSpeed, target, dt * (engine == EngineState::Running ? 0.25f : 0.6f));
    coughDip = approach(coughDip, 0.0f, dt * 3.0f);
    float alive = 1.0f;
    if (engine == EngineState::Sputtering) alive = 1.0f - 0.65f * coughDip;
    else if (engine == EngineState::Dead) alive = 0.0f;
    engineVis = approach(engineVis, alive, dt * 6.0f);

    progress = std::min(1.0f, progress + dt * carSpeed / kTripSeconds);

    if (fuel < kLowFuel && lowFuelArmed) {
        lowFuelArmed = false;
        parents.cue(ParentCue::LowFuel);
        if (!hintFuelShown) {
            hintFuelShown = true;
            showHint(Hint::Fuel, 4.5f);
        }
    }
    if (fuel > kLowFuelRearm) lowFuelArmed = true;
    if (!nearHomeCued && progress >= kNearHome) {
        nearHomeCued = true;
        parents.cue(ParentCue::NearHome);
    }
}

void Game::State::updateItems(float dt) {
    if (hasCamera && charge < 1.0f) {
        charge = std::min(1.0f, charge + dt / diff().recharge);
        if (charge >= 1.0f) audio.play(Sfx::CameraReady);
    }
    lockLeft = std::max(0.0f, lockLeft - dt);
    flashT += dt;
    jolt = approach(jolt, 0.0f, dt * 4.0f);
    raise = approach(raise, view == ViewMode::Console ? 1.0f : 0.0f, dt / kRaiseTime);
}

// live = false: мини-игра едет сама, её события ничего не меняют (финал/меню).
void Game::State::updateMini(float dt, bool live) {
    mini.update(dt, live ? steer : 0, live && view == ViewMode::Console, progress, hasCamera);
    for (MiniEvent e : mini.takeEvents()) {
        if (live) onMiniEvent(e);
    }
}

void Game::State::onMiniEvent(MiniEvent e) {
    switch (e) {
    case MiniEvent::PickedFuel:
        fuel = std::min(kFuelMax, fuel + diff().canister);
        audio.play(Sfx::ConsolePickupFuel);
        break;
    case MiniEvent::PickedCamera:
        if (!hasCamera) {
            hasCamera = true;
            charge = 1.0f;
            if (!hintCameraShown) {
                hintCameraShown = true;
                showHint(Hint::Camera, 6.0f);
            }
        }
        audio.play(Sfx::ConsolePickupItem);
        break;
    case MiniEvent::PickedBattery:
        if (hasCamera) {
            charge = 1.0f;
            superFlash = true;
        }
        audio.play(Sfx::ConsolePickupItem);
        break;
    case MiniEvent::PickedLock:
        lockLeft = kLockTime;
        monster.reduceThreat(kLockThreat);
        audio.play(Sfx::ConsolePickupItem);
        break;
    case MiniEvent::Crashed:
        audio.play(Sfx::ConsoleCrash);
        monster.attract(kAttract);
        crashBoost = std::min(kCrashBoostCap, crashBoost + kCrashBoostStep);
        if (rng.chance(kCueCrashChance)) parents.cue(ParentCue::ConsoleCrash);
        jolt = 1.0f;
        break;
    case MiniEvent::Skid: audio.play(Sfx::ConsoleSkid); break;
    }
}

// live = false: монстр только уходит (финал) — реплики родителей не нужны.
void Game::State::updateMonster(float dt, float aggression, float breakMul, bool live) {
    monster.update(dt, aggression, breakMul, scene.lightLevel());
    for (const MonsterEventInfo& e : monster.takeEvents()) {
        scene.onMonsterEvent(e);
        playMonsterSound(e);
        if (!live) continue;
        if (e.type == MonsterEvent::Landed && rng.chance(kCueThudChance)) parents.cue(ParentCue::RoofThud);
        if (e.type == MonsterEvent::Bang && rng.chance(kCueBangChance)) parents.cue(ParentCue::WindowBang);
    }
}

void Game::State::playMonsterSound(const MonsterEventInfo& e) {
    const float k = saturate(e.intensity);
    const float pan = clampf(e.pan, -1.0f, 1.0f);
    switch (e.type) {
    case MonsterEvent::Landed: audio.play(Sfx::RoofThud, 0.75f + 0.25f * k, pan); break;
    case MonsterEvent::Step: audio.play(Sfx::RoofStep, 0.45f + 0.5f * k, pan); break;
    case MonsterEvent::Scrape: audio.play(Sfx::RoofScrape, 0.5f + 0.5f * k, pan); break;
    case MonsterEvent::Appear:
    case MonsterEvent::Growl: audio.play(Sfx::MonsterGrowl, 0.5f + 0.5f * k, pan); break;
    case MonsterEvent::Knock: audio.play(Sfx::GlassKnock, 0.55f + 0.45f * k, pan); break;
    case MonsterEvent::Bang: audio.play(Sfx::GlassBang, 0.6f + 0.4f * k, pan); break;
    case MonsterEvent::Crack: audio.play(Sfx::GlassCrack, 0.7f + 0.3f * k, pan); break;
    case MonsterEvent::Screech: audio.play(Sfx::MonsterScreech, 0.7f + 0.3f * k, pan); break;
    case MonsterEvent::LeftRoof: audio.play(Sfx::RoofStep, 1.0f, pan, 0.85f); break;
    case MonsterEvent::BrokeIn: break; // звон стекла играет startJumpscare (после stopAllSfx)
    }
}

void Game::State::updateWorld(float dt) {
    scene.update(dt, carSpeed, monster);
    if (scene.takeStreetlightPassed()) audio.play(Sfx::StreetlightWhoosh);
}

void Game::State::updateParents(float dt) {
    parents.update(dt);
    Speaker who = Speaker::Dad;
    float dur = 0.0f;
    if (parents.takeLineStarted(who, dur)) {
        const bool dad = who == Speaker::Dad;
        audio.play(dad ? Sfx::MumbleDad : Sfx::MumbleMom, 0.8f + 0.25f * parents.tension(), dad ? -0.35f : 0.35f,
                   dur);
    }
}

// ---- Подсказки ----
// Важная подсказка не перебивает другую важную, а ждёт своей очереди; напоминание
// про TAB уступает сразу, «нет камеры» — мгновенный ответ на нажатие F.
void Game::State::showHint(Hint h, float dur) {
    const bool busy = hint != Hint::None && hint != Hint::Raise && hint != h;
    if (busy && h != Hint::NoCamera) {
        hintQueued = h;
        hintQueuedDur = dur;
        return;
    }
    hint = h;
    hintT = 0.0f;
    hintDur = dur;
}

// Плавно убрать подсказку h, если она сейчас на экране.
void Game::State::dismissHint(Hint h) {
    if (hint != h || hint == Hint::None) return;
    hintDur = std::min(hintDur, hintT + 0.35f);
}

float Game::State::hintAlpha() const {
    if (hint == Hint::None) return 0.0f;
    return saturate(std::min(hintT / 0.25f, (hintDur - hintT) / 0.35f));
}

void Game::State::updateHints(float dt) {
    if (!hintRaiseShown && tripTime >= kRaiseHintAt) {
        hintRaiseShown = true;
        if (!raisedOnce && hint == Hint::None) showHint(Hint::Raise, 9.0f);
    }
    if (hint != Hint::None) {
        hintT += dt;
        if (hintT >= hintDur) hint = Hint::None;
    }
    if (hint == Hint::None && hintQueued != Hint::None) {
        showHint(hintQueued, hintQueuedDur);
        hintQueued = Hint::None;
    }
    // Подсказку про TAB перебила другая — вернуть, пока консоль так и не поднимали.
    if (hint == Hint::None && hintRaiseShown && !raisedOnce && tripTime < kRaiseHintUntil) {
        showHint(Hint::Raise, 6.0f);
    }
}

// ============================================================================
//  Звук: непрерывные слои (каждый тик, в любом состоянии)
// ============================================================================
void Game::State::updateAmbient(float dt) {
    AmbientParams p;
    float music = 0.0f, dread = 0.0f, heart = 0.0f, master = 1.0f;
    const float threat = monster.threat();
    switch (state) {
    case GameState::Menu:
    case GameState::Intro:
        p.engine = 0.55f;
        p.engineRpm = 0.42f;
        p.road = 0.5f;
        p.wind = 0.32f;
        p.rain = 0.12f;
        dread = state == GameState::Menu ? 0.14f : 0.08f;
        master = 0.9f;
        break;
    case GameState::Playing:
    case GameState::Paused:
    case GameState::Arriving: {
        const float sputter = engine == EngineState::Sputtering ? coughDip : 0.0f;
        const float idle = state == GameState::Arriving ? 0.45f : 0.8f;
        p.engine = engine == EngineState::Dead ? 0.0f : lerpf(idle, 0.8f, carSpeed) * (1.0f - 0.6f * sputter);
        p.engineRpm = 0.25f + 0.3f * carSpeed + 0.15f * sputter;
        p.road = 0.75f * carSpeed;
        p.wind = 0.2f + 0.25f * carSpeed;
        p.rain = 0.1f + 0.22f * smoothstep(0.3f, 0.55f, progress) * (1.0f - smoothstep(0.8f, 0.95f, progress));
        if (state == GameState::Paused) {
            master = 0.45f;
        } else {
            music = state == GameState::Playing ? 0.55f : 0.0f;
            heart = smoothstep(35.0f, 100.0f, threat);
            if (monster.onRoof()) dread = 0.45f + 0.55f * smoothstep(0.0f, 60.0f, threat);
        }
        break;
    }
    case GameState::Dying:
        if (lose == LoseReason::Fuel && !jumpscareOn) {
            // Тишина: только шорох колёс затихающей машины.
            p.road = 0.75f * carSpeed;
            p.wind = 0.2f * carSpeed;
        } else {
            ambMusic = 0.0f; // скример — в полной тишине
        }
        break;
    case GameState::GameOver:
        p.wind = 0.15f;
        dread = 0.3f;
        break;
    case GameState::Victory:
        p.engine = std::max(0.0f, 0.35f - 0.15f * stateT); // мотор заглушили у дома
        p.engineRpm = 0.2f;
        p.wind = 0.18f;
        p.rain = 0.08f;
        break;
    }
    ambMusic = approach(ambMusic, music, dt * 1.0f);
    ambDread = approach(ambDread, dread, dt * (dread > ambDread ? 0.6f : 0.25f));
    ambHeart = approach(ambHeart, heart, dt * 0.8f);
    ambMaster = approach(ambMaster, master, dt * 2.0f);
    p.consoleMusic = ambMusic;
    p.dread = ambDread;
    p.heartbeat = ambHeart;
    p.consoleMuffle = 1.0f - raiseEased();
    p.musicTempo = 1.0f + 0.25f * progress;
    p.master = ambMaster;
    audio.setAmbient(p);
}

// ============================================================================
//  Отрисовка
// ============================================================================
RealWorldView Game::State::makeView(bool hud) const {
    RealWorldView v;
    v.time = animTime;
    v.tripProgress = progress;
    v.kmLeft = kTripKm * (1.0f - progress);
    v.carSpeed = carSpeed;
    v.engineAlive = engineVis;
    v.fuel = fuel;
    v.lowFuelWarning = fuel < kLowFuel;
    v.hasCamera = hasCamera;
    v.cameraCharge = charge;
    v.superFlash = superFlash;
    v.aim = aim;
    v.showAim = hud && view == ViewMode::RealWorld && state == GameState::Playing;
    v.lockActive = lockLeft > 0.0f;
    v.lockLeft = lockLeft;
    v.threat = monster.threat();
    v.arrive = arrive;
    v.showHud = hud;
    v.showConsole = true;
    v.consoleDpad = 0;
    return v;
}

// Снимок для полароида: чистый кадр (без интерфейса и консоли) в момент вспышки.
void Game::State::capturePhoto() {
    capturePending = false;
    RealWorldView v = makeView(false);
    v.showConsole = false;
    scene.render(photoFrame, v, monster, parents, screen);
    scene.capturePhoto(photoFrame, captureHit);
}

void Game::State::renderMenu(Canvas& out) {
    RealWorldView v = makeView(false);
    v.showConsole = false; // консоль «оживёт» во вступлении
    scene.render(out, v, monster, parents, screen);
    shadeFrame(out, vig, 0.72f, 0.85f);

    // ---- Заголовок: тёмно-красный, «подтёк» вниз, мерцает как старая лампа ----
    const char* title = T8("ЗАДНЕЕ СИДЕНЬЕ");
    const float fl = flickerAt(animTime);
    const int tw = font::textWidth(title, 2);
    const int tx = 160 - tw / 2, ty = 16;
    out.blendRect(tx - 10, ty - 5, tw + 20, 24, rgb(0, 0, 0), 0.35f);
    font::drawText(out, tx + 2, ty + 3, title, rgb(4, 2, 4), 2);
    font::drawText(out, tx, ty + 1, title, scaleColor(kTitleDeep, 0.6f + 0.4f * fl), 2);
    font::drawText(out, tx, ty, title, scaleColor(kTitleRed, fl), 2);
    out.fillRect(tx + 2, ty + 18, tw - 4, 1, scaleColor(kTitleDeep, 0.8f * fl));

    textCentered(out, 160, 41, T8("Ночь. Дорога домой."), hud::kTextDim);
    textCentered(out, 160, 51, T8("Не смотри в окно слишком долго."), rgb(170, 150, 150));

    // ---- Управление ----
    const int px = 44, py = 64, pw = 232, ph = 74;
    hud::drawPanel(out, px, py, pw, ph, 0.72f);
    textCentered(out, 160, py + 5, T8("УПРАВЛЕНИЕ"), hud::kText);
    struct Row {
        const char* keys;
        const char* what;
    };
    const Row rows[] = {
        {T8("TAB/ПРОБЕЛ/ПКМ"), T8("консоль: вверх/вниз")},
        {T8("A D / < >"), T8("руль в мини-игре")},
        {T8("A W D S / мышь"), T8("камера: выбор окна")},
        {T8("F / E / ЛКМ"), T8("снимок со вспышкой")},
        {T8("ESC / M / F11"), T8("пауза / звук / экран")},
    };
    const int split = px + 98;
    int ry = py + 17;
    for (const Row& r : rows) {
        textRight(out, split - 6, ry, r.keys, kKeyColor);
        font::drawTextShadow(out, split + 2, ry, r.what, hud::kTextDim, hud::kShadow);
        ry += 11;
    }

    // ---- Сложность ----
    {
        char line[64];
        std::snprintf(line, sizeof(line), "%s  < %s >", T8("СЛОЖНОСТЬ:"), diff().name);
        const uint32_t col = difficulty == 2 ? rgb(214, 96, 90) : (difficulty == 0 ? rgb(150, 196, 150) : kKeyColor);
        textCentered(out, 160, 141, line, col);
    }

    // ---- Старт и выход ----
    const float pulse = 0.55f + 0.45f * std::sin(animTime * 3.2f);
    const int w1 = keyHintWidth("ENTER", T8("НАЧАТЬ"));
    const int w2 = keyHintWidth("ESC", T8("ВЫХОД"));
    const int gap = 22;
    const int bx = 160 - (w1 + gap + w2) / 2, by = 153;
    hud::drawKeyHint(out, bx, by, "ENTER", T8("НАЧАТЬ"), 0.55f + 0.45f * pulse);
    hud::drawKeyHint(out, bx + w1 + gap, by, "ESC", T8("ВЫХОД"), 0.7f);
    if (toastT >= kToastTime) textCentered(out, 160, 168, T8("Лучше играть в наушниках"), rgb(84, 80, 92));
}

void Game::State::renderIntro(Canvas& out) {
    RealWorldView v = makeView(false);
    scene.render(out, v, monster, parents, screen);
    // Салон медленно проступает из темноты; светится только консоль.
    shadeFrame(out, vig, 0.05f + 0.2f * smoothstep(0.0f, 8.0f, stateT), 1.0f);

    for (int i = 0; i < kIntroCount; ++i) {
        const float t0 = introStart[static_cast<size_t>(i)];
        if (stateT < t0) break;
        const char* line = kIntroLines[i];
        const int total = font::countChars(line);
        const int shown = std::min(total, static_cast<int>((stateT - t0) * kIntroType) + 1);
        const bool typing = shown < total;
        const bool latest = i + 1 >= kIntroCount || stateT < introStart[static_cast<size_t>(i + 1)];
        const uint32_t col = latest ? hud::kText : rgb(120, 116, 112);
        const int w = font::textWidth(line);
        const int x = 160 - w / 2, y = kIntroY + i * kIntroStep;
        font::drawTextShadow(out, x, y, line, col, hud::kShadow, 1, shown);
        // Мигающий курсор печатной машинки.
        if (latest && (typing || std::fmod(stateT, 0.8f) < 0.4f)) {
            out.fillRect(x + shown * font::kAdvance, y + 7, 4, 1, typing ? hud::kText : hud::kTextDim);
        }
    }
    const float skipA = smoothstep(1.0f, 2.0f, stateT) * 0.6f;
    if (skipA > 0.0f) {
        textCentered(out, 160, 168, T8("любая клавиша — пропустить"), lerpColor(rgb(0, 0, 0), hud::kTextDim, skipA));
    }
}

// hud — шкалы и подсказки клавиш; overlays — субтитры и подсказки новичку.
void Game::State::renderTrip(Canvas& out, bool hud, bool overlays) {
    const float r = raiseEased();
    if (r <= 0.0f) {
        RealWorldView v = makeView(hud);
        scene.render(out, v, monster, parents, screen);
    } else {
        renderConsoleView(out, r, hud);
    }
    if (!overlays) return;
    const bool consoleView = r >= 0.5f;
    renderSubtitle(out, consoleView);
    renderHint(out, consoleView);
}

// Консоль у лица (r = 1) или анимация подъёма (0 < r < 1).
void Game::State::renderConsoleView(Canvas& out, float r, bool hud) {
    // Мир — боковым зрением: размыт и затемнён. Когда консоль у лица, этот фон
    // обновляется через кадр (30 Гц): по размытым краям разницы не видно, а
    // отладочная сборка экономит целую отрисовку сцены на каждом втором кадре.
    const bool full = r >= 0.999f;
    const size_t px = static_cast<size_t>(kW) * static_cast<size_t>(kH);
    if (full && consoleBgValid && (++consoleBgPhase & 1u) != 0u) {
        std::copy(consoleBg.data(), consoleBg.data() + px, out.data());
    } else {
        RealWorldView v = makeView(false);
        v.showConsole = false;
        scene.render(out, v, monster, parents, screen);
        blurDarken(out, blurTmp, r < 0.35f ? 0 : (r < 0.7f ? 1 : 2), lerpf(1.0f, 0.42f, r));
        consoleBgValid = full;
        if (full) std::copy(out.data(), out.data() + px, consoleBg.data());
    }
    if (monster.state() == MonsterState::BreakingIn) {
        const float beat = std::pow(0.5f + 0.5f * std::sin(animTime * 6.0f), 2.0f);
        edgeRed(out, vig, r * (0.35f + 0.4f * beat));
    }

    // Консоль: из колен (160,150) x0.5 к лицу (160,90) x1. Масштаб квантуется
    // шагом 1/64, чтобы кэш корпуса не перестраивался из-за мелких отличий.
    const float scale = std::round(lerpf(0.5f, 1.0f, r) * 64.0f) / 64.0f;
    const int cy = roundi(lerpf(150.0f, 90.0f, r));
    const float light = scene.lightLevel();
    const float lapLit = clampf(0.3f + 0.45f * light + 0.15f * engineVis, 0.0f, 1.0f);
    const float faceLit = clampf(0.38f + 0.35f * light + 0.07f * engineVis, 0.0f, 1.0f);
    int jx = 0, jy = 0;
    if (jolt > 0.0f) {
        const int fr = static_cast<int>(std::fmod(animTime, 10000.0f) * 60.0f); // номер кадра для дрожи
        jx = roundi((hash01(static_cast<uint32_t>(fr) * 747796405u + 1u) - 0.5f) * 5.0f * jolt);
        jy = roundi((hash01(static_cast<uint32_t>(fr) * 747796405u + 2u) - 0.5f) * 4.0f * jolt);
    }
    out.setOffset(scene.shakeX() + jx, scene.shakeY() + jy);
    ConsoleMiniGame::drawHandheld(out, screen, 160, cy, scale, view == ViewMode::Console ? steer : 0,
                                  lerpf(lapLit, faceLit, r), lerpf(0.55f, 0.7f, r));
    out.setOffset(0, 0);

    if (hud && r >= 0.999f) {
        hud::drawFuelGauge(out, 4, 165, fuel / 100.0f, fuel < kLowFuel, animTime);
    }
}

void Game::State::renderSubtitle(Canvas& out, bool consoleView) const {
    if (!parents.hasLine()) return;
    const uint32_t col = parents.speaker() == Speaker::Dad ? kDadColor : kMomColor;
    if (consoleView) {
        // Вплотную к верхнему краю: две строки почти не заходят на экран консоли.
        hud::drawSubtitle(out, 160, 1, 316, parents.speakerName(), parents.text(), parents.visibleChars(),
                          parents.lineAlpha(), col);
    } else {
        hud::drawSubtitle(out, 160, 97, 200, parents.speakerName(), parents.text(), parents.visibleChars(),
                          parents.lineAlpha(), col);
    }
}

void Game::State::renderHint(Canvas& out, bool consoleView) const {
    const float a = hintAlpha();
    if (a <= 0.01f) return;
    const int y = consoleView ? 167 : 73;
    const int cx = consoleView ? 198 : 160; // в консоли не залезать на шкалу бензина
    switch (hint) {
    case Hint::Raise: keyBanner(out, cx, y, "TAB", T8("ПОДНЯТЬ КОНСОЛЬ"), nullptr, nullptr, a); break;
    case Hint::Camera: keyBanner(out, cx, y, "F", T8("ФОТО ВСПЫШКОЙ"), "A/W/D", T8("ЦЕЛЬ"), a); break;
    case Hint::Fuel: textBanner(out, cx, y, T8("БЕНЗИН! СОБИРАЙ КАНИСТРЫ В ИГРЕ"), hud::kFuel, a); break;
    case Hint::NoCamera: textBanner(out, cx, y, T8("У ТЕБЯ НЕТ КАМЕРЫ — НАЙДИ ЕЁ В ИГРЕ"), hud::kText, a); break;
    case Hint::None: break;
    }
}

void Game::State::renderPause(Canvas& out) {
    const float a = smoothstep(0.0f, 0.15f, stateT); // быстрое появление
    shadeFrame(out, vig, 1.0f - 0.45f * a, 0.6f * a);
    const int pw = 168, ph = 98;
    const int px = 160 - pw / 2, py = 90 - ph / 2;
    hud::drawPanel(out, px, py, pw, ph, 0.88f * a);
    if (a < 0.6f) return;
    textCentered(out, 160, py + 8, T8("ПАУЗА"), hud::kText, 2);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s %.1f %s", T8("ДО ДОМА"), static_cast<double>(kTripKm * (1.0f - progress)),
                  T8("КМ"));
    textCentered(out, 160, py + 26, buf, hud::kTextDim);
    struct Row {
        const char* key;
        const char* label;
    };
    const Row rows[] = {
        {"ESC", T8("ПРОДОЛЖИТЬ")},
        {"R", T8("ЗАНОВО")},
        {"Q", T8("В МЕНЮ")},
        {"M", audio.muted() ? T8("ЗВУК: ВЫКЛ") : T8("ЗВУК: ВКЛ")},
    };
    int ry = py + 40;
    for (const Row& r : rows) {
        hud::drawKeyHint(out, px + 46, ry, r.key, r.label);
        ry += 13;
    }
}

void Game::State::renderDying(Canvas& out) {
    if (!jumpscareOn) {
        // Заглохли: салон темнеет, остаётся только свет консоли на коленях.
        renderTrip(out, false, false);
        shadeFrame(out, vig, 1.0f - 0.7f * smoothstep(0.0f, stallDelay, stateT), 0.9f);
        return;
    }
    scene.renderJumpscare(out, stateT - stallDelay);
}

void Game::State::renderGameOver(Canvas& out) {
    RealWorldView v = makeView(false);
    v.showConsole = false;
    scene.render(out, v, monster, parents, screen);
    bloodTone(out, vig);
    renderResult(out, false);
}

void Game::State::renderVictory(Canvas& out) {
    RealWorldView v = makeView(false);
    v.showConsole = false;
    scene.render(out, v, monster, parents, screen);
    shadeFrame(out, vig, 0.8f, 0.9f);
    renderResult(out, true);
}

// Экран итога: заголовок и причина сверху, в окне между ними и панелью виден
// дом (победа) или тёмный салон с гостем в окне (проигрыш); внизу — статистика.
void Game::State::renderResult(Canvas& out, bool victory) {
    const float a = smoothstep(0.1f, 0.8f, stateT);
    if (a <= 0.0f) return;
    const char* title = victory ? T8("ПРИЕХАЛИ") : T8("КОНЕЦ ПУТИ");
    const char* reason = victory ? T8("Вы дома. Всё позади... наверное.")
                                 : (lose == LoseReason::Fuel ? T8("Машина заглохла. Бензин кончился.")
                                                             : T8("Оно добралось до тебя."));
    const uint32_t titleCol = lerpColor(rgb(0, 0, 0), victory ? kWarm : kTitleRed, a);
    const int tw = font::textWidth(title, 2);
    out.blendRect(160 - tw / 2 - 12, 5, tw + 24, 22, rgb(0, 0, 0), 0.45f * a);
    font::drawText(out, 160 - tw / 2 + 2, 10, title, rgb(4, 2, 4), 2);
    font::drawText(out, 160 - tw / 2, 8, title, titleCol, 2);
    textBanner(out, 160, 31, reason, victory ? hud::kText : rgb(226, 200, 194), a);

    const int pw = 216, ph = 96;
    const int px = 160 - pw / 2, py = 82;
    hud::drawPanel(out, px, py, pw, ph, 0.84f * a);
    if (a < 0.5f) return;

    char val[6][40];
    const int secs = static_cast<int>(tripTime);
    std::snprintf(val[0], sizeof(val[0]), "%d:%02d", secs / 60, secs % 60);
    std::snprintf(val[1], sizeof(val[1]), "%.1f %s %.1f %s", static_cast<double>(progress * kTripKm), T8("ИЗ"),
                  static_cast<double>(kTripKm), T8("КМ"));
    std::snprintf(val[2], sizeof(val[2]), "%d", mini.fuelCollected());
    std::snprintf(val[3], sizeof(val[3]), "%d (%s %d)", photos, T8("В ЦЕЛЬ"), hits);
    std::snprintf(val[4], sizeof(val[4]), "%d", mini.crashes());
    std::snprintf(val[5], sizeof(val[5]), "%s", diff().name);
    const char* labels[6] = {T8("Время в пути"), T8("Проехали"), T8("Канистры"), T8("Снимки"),
                             T8("Аварии в игре"), T8("Сложность")};
    int ry = py + 8;
    for (int i = 0; i < 6; ++i) {
        font::drawTextShadow(out, px + 20, ry, labels[i], hud::kTextDim, hud::kShadow);
        textRight(out, px + pw - 20, ry, val[i], hud::kText);
        ry += 11;
    }

    const float ka = smoothstep(kResultInputDelay - 0.2f, kResultInputDelay + 0.2f, stateT);
    if (ka > 0.0f) {
        const int w1 = keyHintWidth("R", T8("ЗАНОВО"));
        const int w2 = keyHintWidth("ESC", T8("В МЕНЮ"));
        const int gap = 22;
        const int bx = 160 - (w1 + gap + w2) / 2, by = py + ph - 16;
        hud::drawKeyHint(out, bx, by, "R", T8("ЗАНОВО"), ka);
        hud::drawKeyHint(out, bx + w1 + gap, by, "ESC", T8("В МЕНЮ"), ka);
    }
}

// «Звук вкл/выкл»: в реальном мире — под люком; в меню и с консолью у лица — внизу,
// чтобы не закрывать ни заголовок, ни экран мини-игры.
void Game::State::renderToast(Canvas& out) const {
    if (toastT >= kToastTime || state == GameState::Paused) return; // в паузе это видно на панели
    const float a = saturate(std::min(toastT / 0.15f, (kToastTime - toastT) / 0.4f));
    const bool consoleUp = (state == GameState::Playing || state == GameState::Arriving) && raiseEased() >= 0.5f;
    const int y = state == GameState::Menu || consoleUp ? 167 : 40;
    const int cx = consoleUp ? 198 : 160;
    textBanner(out, cx, y, toastMuted ? T8("ЗВУК ВЫКЛЮЧЕН") : T8("ЗВУК ВКЛЮЧЁН"), hud::kText, a);
}

// ============================================================================
//  Game
// ============================================================================
Game::Game(Audio& audio) : st_(std::make_unique<State>(audio)) {
    State& s = *st_;
    s.enterMenu();
    s.fade = 1.0f;
    s.fadeInRate = 1.0f / 1.2f;
}

Game::~Game() = default;

void Game::update(float dt, const Input& input) {
    State& s = *st_;
    if (!(dt > 0.0f)) return; // отсекает и NaN
    dt = std::min(dt, 0.1f);

    // ---- Клавиши, работающие везде ----
    if (input.pressed(Key::M)) {
        const bool m = !s.audio.muted();
        s.audio.setMuted(m);
        if (!m) s.audio.play(Sfx::MenuMove);
        s.toastMuted = m;
        s.toastT = 0.0f;
    }
    if (input.pressed(Key::F11)) s.fullscreenReq = true;

    const bool accept = !s.fadePending;
    if (s.state != GameState::Paused) s.animTime += dt;
    switch (s.state) {
    case GameState::Menu: s.updateMenu(dt, input, accept); break;
    case GameState::Intro: s.updateIntro(dt, input, accept); break;
    case GameState::Playing: s.updatePlaying(dt, input, accept); break;
    case GameState::Paused: s.updatePaused(dt, input, accept); break;
    case GameState::Dying: s.updateDying(dt); break;
    case GameState::Arriving: s.updateArriving(dt); break;
    case GameState::GameOver:
    case GameState::Victory: s.updateResult(dt, input, accept); break;
    }
    s.toastT = std::min(s.toastT + dt, kToastTime);
    s.updateFade(dt);
    s.updateAmbient(dt);
}

void Game::render(Canvas& out) {
    State& s = *st_;
    if (out.width() != kW || out.height() != kH) out.resize(kW, kH);
    out.resetClip();
    out.resetStencilModes();
    out.setOffset(0, 0);
    // Кадр мини-игры нужен везде, где видна консоль (на коленях или у лица).
    const bool consoleVisible = s.state == GameState::Intro || s.state == GameState::Playing ||
                                s.state == GameState::Paused || s.state == GameState::Arriving ||
                                (s.state == GameState::Dying && !s.jumpscareOn);
    if (consoleVisible) s.mini.render(s.screen);
    if (s.capturePending) s.capturePhoto();

    switch (s.state) {
    case GameState::Menu: s.renderMenu(out); break;
    case GameState::Intro: s.renderIntro(out); break;
    case GameState::Playing: s.renderTrip(out, true, true); break;
    case GameState::Arriving: s.renderTrip(out, false, true); break; // кино: без интерфейса
    case GameState::Paused:
        s.renderTrip(out, true, false);
        s.renderPause(out);
        break;
    case GameState::Dying: s.renderDying(out); break;
    case GameState::GameOver: s.renderGameOver(out); break;
    case GameState::Victory: s.renderVictory(out); break;
    }
    s.renderToast(out);

    // Вспышка камеры: один белый кадр и быстрое затухание (без стробоскопа).
    if (s.flashT < s.flashDur) {
        const float k = 1.0f - s.flashT / s.flashDur;
        overlay(out, s.flashSuper ? kFlashWarm : kFlashCold, k * k);
    }
    // Затемнение переходов.
    if (s.fade > 0.0f) overlay(out, rgb(0, 0, 0), smoothstep(0.0f, 1.0f, s.fade));
    out.resetClip();
    out.resetStencilModes();
    out.setOffset(0, 0);
}

void Game::onFocusLost() {
    State& s = *st_;
    if (s.state == GameState::Playing && !s.fadePending) s.enterPause();
}

bool Game::wantsQuit() const { return st_->quit; }

bool Game::takeFullscreenToggle() {
    const bool r = st_->fullscreenReq;
    st_->fullscreenReq = false;
    return r;
}

GameState Game::state() const { return st_->state; }
ViewMode Game::view() const { return st_->view; }
LoseReason Game::loseReason() const { return st_->lose; }
float Game::fuel() const { return st_->fuel; }
float Game::tripProgress() const { return st_->progress; }
float Game::tripTime() const { return st_->tripTime; }
bool Game::hasCamera() const { return st_->hasCamera; }
float Game::cameraCharge() const { return st_->charge; }
Entry Game::aim() const { return st_->aim; }
const Monster& Game::monster() const { return st_->monster; }
const ConsoleMiniGame& Game::miniGame() const { return st_->mini; }

void Game::debugStartTrip(uint32_t seed) {
    State& s = *st_;
    s.fadePending = false;
    s.fade = 0.0f;
    s.startTrip(seed);
}
