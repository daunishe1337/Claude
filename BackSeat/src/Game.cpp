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

#include "Achievements.h"

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
#include <cstring>
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

// ---- Бесконечная дорога (секрет: 5 нажатий F за 2 секунды в меню) ----------
constexpr int kSecretTaps = 5;
constexpr float kSecretWindow = 2.0f;     // за сколько секунд нужно успеть
constexpr float kFacelessAt = 180.0f;     // через 3 минуты родители замолкают и оборачиваются
constexpr float kFacelessTurn = 6.0f;     // поворот длится...
constexpr float kEndlessCueMin = 35.0f;   // «мы тут уже проезжали» — интервал реплик
constexpr float kEndlessCueMax = 50.0f;
constexpr float kEndlessKmGoal = 15.0f;   // достижение «Дальнобойщик»

// ---- «Притвориться спящим» (бесконечная дорога: удерживать F) ----------------
constexpr float kSleepHold = 0.45f;   // столько держать F, чтобы закрыть глаза (короче — снимок)
constexpr float kSleepZone = 0.32f;   // полуширина зелёной зоны дыхания (шкала -1..1)
constexpr float kSleepPush = 7.0f;    // сила A/D
constexpr float kSleepGrace = 0.7f;   // сколько можно пробыть вне зоны, пока не заметят
constexpr float kSleepCalm = 4.0f;    // угроза тает, пока дышишь ровно (ед./с)
constexpr float kSleepCooldown = 2.5f; // после «тебя заметили» глаза не закрыть
constexpr float kSleepRepel = 1.5f;   // столько ровного «сна» — и гость уходит с крыши
constexpr float kGlimpseTime = 0.7f;  // открыл глаза — а они смотрят на тебя...

// ---- Громкость (E) ---------------------------------------------------------------
constexpr float kVolumeStep = 0.1f;

// ---- Достижения ----------------------------------------------------------------
constexpr float kPopupTime = 3.2f;        // всплывающее «ДОСТИЖЕНИЕ», с
constexpr float kConsoleStreak = 60.0f;   // «Не отрываясь»: минута с поднятой консолью
// ---- Админ-панель (INS, пароль 1667) ---------------------------------------------
const char* const kAdminPassword = "1667";
constexpr int kAdminMaxDigits = 8;
enum class AdminItem : int {
    AutoSleep,   // сам «спит» и дышит ровно (бесконечная дорога)
    God,         // гость не может пролезть
    Fuel,        // бак всегда полный
    Autopilot,   // мини-игра рулит сама
    Camera,      // камера есть и всегда заряжена
    AutoWin,     // действие: сразу доехать
    Banish,      // действие: прогнать гостя
    Faceless,    // действие: перемотать к безликим (бесконечная дорога)
    Endless,     // переключить бесконечную дорогу
    UnlockAll,   // действие: открыть все достижения
    Count
};
constexpr int kAdminItems = static_cast<int>(AdminItem::Count);

constexpr int kAchRows = 13;              // строк в колонке списка достижений
constexpr int kAchPerPage = 2 * kAchRows; // две колонки на страницу

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
    float maxThreat = 0.0f;     // для «На волоске»
    float consoleRunT = 0.0f;   // сколько секунд подряд консоль у лица

    // ---- Бесконечная дорога ----
    bool endless = false;       // включена в меню (секретным кодом); переживает рестарты
    bool endlessTrip = false;   // текущая поездка — бесконечная
    float kmDriven = 0.0f;
    float endlessCueT = 0.0f;   // до следующей «петли» в разговоре родителей
    bool facelessStarted = false;
    float facelessT = 0.0f;     // 0..1 — родители оборачиваются
    std::array<float, kSecretTaps> secretTaps{};
    int secretIdx = 0;
    int secretCount = 0;

    float holdF = 0.0f;         // сколько держится F (короткое нажатие — снимок)
    bool sleeping = false;      // «притворяется спящим»
    float sleepEyes = 0.0f;     // 0..1 — веки закрыты (анимация)
    float sleepPos = 0.0f;      // бегунок дыхания -1..1
    float sleepVel = 0.0f;
    float sleepOut = 0.0f;      // сколько секунд бегунок вне зелёной зоны
    float sleepCooldown = 0.0f;
    float sleepSteadyT = 0.0f;  // сколько секунд подряд дыхание в зелёной зоне
    float overheardT = 0.0f;    // сколько слушали родителей «во сне»
    float glimpseT = 0.0f;      // родители «застигнуты» обернувшимися (после пробуждения)
    int hitStreak = 0;          // попадания подряд
    bool gotBattery = false;    // для «Запасливый»
    bool gotLock = false;
    float userVolume = 1.0f;    // громкость, выбранная клавишей E (переживает рестарты)
    bool toastVolume = false;   // тост показывает громкость, а не вкл/выкл звука

    // ---- Админ-панель ----
    bool adminOpen = false;
    bool adminUnlocked = false;    // пароль введён (до выхода из игры)
    char adminInput[kAdminMaxDigits + 1] = {};
    int adminLen = 0;
    float adminMsgT = 0.0f;        // показ сообщения («неверный пароль» и т.п.)
    const char* adminMsg = "";
    int adminCursor = 0;
    bool cheatAutoSleep = false;
    bool cheatGod = false;
    bool cheatFuel = false;
    bool cheatAutopilot = false;
    bool cheatCamera = false;
    bool cheatedTrip = false;   // в этой поездке работали читы (метка на экране итогов)
    bool anyCheat() const { return cheatAutoSleep || cheatGod || cheatFuel || cheatAutopilot || cheatCamera; }

    // ---- Достижения ----
    Achievements ach;
    bool achScreen = false;     // в меню открыт список достижений
    int achCursor = 0;
    Ach popupAch = Ach::FirstTrip;
    float popupT = kPopupTime;  // >= kPopupTime — уведомления нет

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
    void updateEndless(float dt);
    void updateSleep(float dt, const Input& in, bool accept);
    void wakeUp(bool caught);
    void trackAchievements(float dt);
    void onTripWon();
    void updatePopup(float dt);
    void updateAdmin(float dt, const Input& in);
    void applyAdmin(AdminItem item);
    void applyCheats();
    int autopilotSteer() const;

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
    void renderAchievements(Canvas& out) const;
    void renderPopup(Canvas& out) const;
    void renderSleep(Canvas& out) const;
    void renderAdmin(Canvas& out) const;
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
    achScreen = false;
    endlessTrip = false;
    facelessT = 0.0f;
    secretCount = 0;
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
    maxThreat = 0.0f;
    consoleRunT = 0.0f;
    endlessTrip = endless;
    kmDriven = 0.0f;
    endlessCueT = rng.range(kEndlessCueMin, kEndlessCueMax);
    facelessStarted = false;
    facelessT = 0.0f;
    holdF = 0.0f;
    sleeping = false;
    sleepEyes = 0.0f;
    sleepCooldown = 0.0f;
    sleepSteadyT = 0.0f;
    overheardT = 0.0f;
    glimpseT = 0.0f;
    hitStreak = 0;
    gotBattery = false;
    gotLock = false;
    cheatedTrip = false;
    ach.unlock(Ach::FirstTrip);
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
    ach.unlock(r == LoseReason::Fuel ? Ach::LoseFuel : Ach::LoseMonster);
    ach.addLoss();
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
    onTripWon();
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
    if (achScreen) {
        // Список достижений: страницы по две колонки. Вверх/вниз — по колонке,
        // влево/вправо — соседняя колонка (за краем — соседняя страница).
        const int pages = (kAchCount + kAchPerPage - 1) / kAchPerPage;
        int page = achCursor / kAchPerPage;
        int col = (achCursor % kAchPerPage) / kAchRows;
        int row = achCursor % kAchRows;
        if (in.pressed(Key::Up) || in.pressed(Key::W)) row = (row + kAchRows - 1) % kAchRows;
        if (in.pressed(Key::Down) || in.pressed(Key::S)) row = (row + 1) % kAchRows;
        if (in.pressed(Key::Right) || in.pressed(Key::D)) {
            if (col == 0) col = 1;
            else { col = 0; page = (page + 1) % pages; }
        }
        if (in.pressed(Key::Left) || in.pressed(Key::A)) {
            if (col == 1) col = 0;
            else { col = 1; page = (page + pages - 1) % pages; }
        }
        int idx = page * kAchPerPage + col * kAchRows + row;
        idx = std::min(idx, kAchCount - 1); // неполная последняя страница
        if (idx != achCursor) {
            achCursor = idx;
            audio.play(Sfx::MenuMove, 0.6f);
        }
        if (in.pressed(Key::Escape) || in.pressed(Key::Tab) || in.pressed(Key::Enter)) {
            achScreen = false;
            audio.play(Sfx::MenuMove);
        }
        return;
    }
    if (in.pressed(Key::Escape)) {
        quit = true;
        return;
    }
    if (in.pressed(Key::Tab)) {
        achScreen = true;
        achCursor = 0;
        audio.play(Sfx::MenuSelect);
        return;
    }
    // Секрет: 5 нажатий F за 2 секунды включают (и выключают) бесконечную дорогу.
    if (in.pressed(Key::F)) {
        secretTaps[static_cast<size_t>(secretIdx)] = stateT;
        secretIdx = (secretIdx + 1) % kSecretTaps;
        ++secretCount;
        const float oldest = secretTaps[static_cast<size_t>(secretIdx)]; // самое старое из пяти
        if (secretCount >= kSecretTaps && stateT - oldest <= kSecretWindow) {
            endless = !endless;
            secretCount = 0;
            audio.play(endless ? Sfx::MonsterGrowl : Sfx::MenuMove, 0.9f);
            if (endless) ach.unlock(Ach::EndlessFound);
        }
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
            // На бесконечной дороге F работает по отпусканию: долгое удержание — «уснуть».
            if ((in.pressed(Key::F) && !endlessTrip) || in.mousePressed(MouseButton::Left)) shoot();
        }
    }
    steer = steerNow;
    tripTime += dt;

    applyCheats();

    // ---- Мир ----
    updateCar(dt);
    updateItems(dt);
    updateMini(dt, true);
    const float aggression = clampf(progress * kAggrPerProgress + crashBoost, 0.0f, 1.0f);
    if (cheatGod && monster.threat() > 80.0f) monster.reduceThreat(monster.threat() - 80.0f);
    updateMonster(dt, aggression, diff().breakMul * (lockLeft > 0.0f ? kLockBreakMul : 1.0f), true);
    updateWorld(dt);
    updateParents(dt);
    updateHints(dt);
    updateSleep(dt, in, accept);
    if (endlessTrip) updateEndless(dt);
    trackAchievements(dt);

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
    if (sleeping) wakeUp(false);
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
    ach.unlock(Ach::FirstPhoto);
    if (hit) {
        ++hits;
        ach.addHits(1);
        if (super) ach.unlock(Ach::SuperHit);
        if (++hitStreak >= 5) ach.unlock(Ach::Streak5);
    } else {
        hitStreak = 0;
        ach.unlock(Ach::Miss);
    }
    if (aim == Entry::None && facelessT >= 0.9f) ach.unlock(Ach::FaceToFace); // вспышка в лица
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
            ach.unlock(Ach::SputterSave);
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

    // На бесконечной дороге дом не приближается: прогресс (он же агрессия) упирается
    // в потолок, а километры просто копятся.
    progress = std::min(endlessTrip ? 0.999f : 1.0f, progress + dt * carSpeed / kTripSeconds);
    kmDriven += dt * carSpeed * kTripKm / kTripSeconds;

    if (fuel < kLowFuel && lowFuelArmed) {
        lowFuelArmed = false;
        parents.cue(ParentCue::LowFuel);
        if (!hintFuelShown) {
            hintFuelShown = true;
            showHint(Hint::Fuel, 4.5f);
        }
    }
    if (fuel > kLowFuelRearm) lowFuelArmed = true;
    if (!nearHomeCued && !endlessTrip && progress >= kNearHome) {
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
    const bool pilot = live && cheatAutopilot;
    const int steerUsed = pilot ? autopilotSteer() : (live ? steer : 0);
    mini.update(dt, steerUsed, pilot || (live && view == ViewMode::Console), progress, hasCamera);
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
        gotBattery = true;
        if (hasCamera && gotLock) ach.unlock(Ach::Collector);
        audio.play(Sfx::ConsolePickupItem);
        break;
    case MiniEvent::PickedLock:
        lockLeft = kLockTime;
        monster.reduceThreat(kLockThreat);
        ach.unlock(Ach::LockUsed);
        gotLock = true;
        if (hasCamera && gotBattery) ach.unlock(Ach::Collector);
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
        // Страх сбивает дыхание «спящего».
        if (sleeping && (e.type == MonsterEvent::Step || e.type == MonsterEvent::Scrape)) sleepVel += rng.signedUnit() * 0.8f;
        if (sleeping && (e.type == MonsterEvent::Bang || e.type == MonsterEvent::Landed)) sleepVel += rng.signedUnit() * 2.0f;
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
        const float vol = facelessStarted ? 0.35f : 0.8f + 0.25f * parents.tension(); // шёпот — тише
        audio.play(dad ? Sfx::MumbleDad : Sfx::MumbleMom, vol, dad ? -0.35f : 0.35f, dur);
    }
}

// ---- Бесконечная дорога ----
// Родители всё чаще замечают, что дорога «повторяется»; через 3 минуты они
// замолкают и медленно оборачиваются к ребёнку. Лиц у них нет.
void Game::State::updateEndless(float dt) {
    if (!facelessStarted) {
        endlessCueT -= dt;
        if (endlessCueT <= 0.0f) {
            parents.cue(ParentCue::EndlessLoop);
            endlessCueT = rng.range(kEndlessCueMin, kEndlessCueMax);
        }
        if (tripTime >= kFacelessAt) {
            facelessStarted = true;
            parents.silence();
            audio.play(Sfx::LoseSting, 0.7f);
        }
    } else if (!sleeping && facelessT < 1.0f) {
        facelessT = std::min(1.0f, facelessT + dt / kFacelessTurn);
        if (facelessT >= 1.0f) ach.unlock(Ach::Faceless);
    }
    if (kmDriven >= kEndlessKmGoal) ach.unlock(Ach::Endless15km);
    if (kmDriven >= 50.0f) ach.unlock(Ach::Km50);
    if (tripTime >= 600.0f) ach.unlock(Ach::Marathon);
}

// ---- «Притвориться спящим» ----
// Удерживая F на бесконечной дороге, ребёнок закрывает глаза. Дыхание надо держать
// ровным (A/D — бегунок в зелёной зоне): тогда гость теряет интерес, а безликие
// родители отворачиваются. Сбился — тебя заметили.
void Game::State::updateSleep(float dt, const Input& in, bool accept) {
    const bool canSleep = endlessTrip && view == ViewMode::RealWorld && accept;
    const bool fDown = canSleep && (in.down(Key::F) || cheatAutoSleep); // автосон держит «F» сам
    sleepCooldown = std::max(0.0f, sleepCooldown - dt);
    if (!sleeping) {
        if (fDown) {
            holdF += dt;
            if (holdF >= kSleepHold && sleepCooldown <= 0.0f) {
                sleeping = true;
                sleepSteadyT = 0.0f;
                ach.unlock(Ach::Pretender);
                // Стоило закрыть глаза — родители становятся странными.
                parents.setKidAsleep(true);
                if (!facelessStarted) parents.cue(ParentCue::KidAsleep);
                sleepPos = 0.0f;
                sleepVel = 0.0f;
                sleepOut = 0.0f;
            }
        } else {
            if (canSleep && holdF > 0.0f && holdF < kSleepHold) shoot(); // короткое нажатие — снимок
            holdF = 0.0f;
        }
    } else if (!fDown) {
        wakeUp(false);
    } else {
        const float aggr = clampf(progress * kAggrPerProgress, 0.0f, 1.0f);
        sleepVel += rng.signedUnit() * (9.0f + 9.0f * aggr) * dt;       // дыхание само сбивается
        sleepVel += static_cast<float>(in.horizontal()) * kSleepPush * dt;
        sleepVel *= std::exp(-1.6f * dt);
        if (cheatAutoSleep) sleepVel = -sleepPos * 4.0f; // автосон: дыхание само возвращается в зону
        sleepPos += sleepVel * dt;
        if (sleepPos < -1.0f || sleepPos > 1.0f) {
            sleepPos = clampf(sleepPos, -1.0f, 1.0f);
            sleepVel = 0.0f;
        }
        if (facelessStarted) parents.cue(ParentCue::Whisper); // безликие шепчут «спящему»
        else parents.cue(ParentCue::AsleepTalk);               // странные разговоры
        if (parents.hasLine()) {
            overheardT += dt;
            if (facelessStarted) ach.unlock(Ach::Whispers);
            else if (overheardT >= 1.0f) ach.unlock(Ach::Overheard);
        }
        if (std::fabs(sleepPos) <= kSleepZone) {
            sleepOut = 0.0f;
            monster.reduceThreat(kSleepCalm * dt);
            // «Спящий» гостю неинтересен: он не прилетает, а с крыши уходит.
            monster.delay(dt);
            sleepSteadyT += dt;
            if (sleepSteadyT >= 30.0f) ach.unlock(Ach::DeepSleep);
            const MonsterState ms = monster.state();
            const bool pressing = ms == MonsterState::Landing || ms == MonsterState::Crawling ||
                                  ms == MonsterState::Peeking || ms == MonsterState::BreakingIn;
            if (pressing && sleepSteadyT >= kSleepRepel) {
                monster.flee(false);
                ach.unlock(Ach::Lullaby);
            }
            if (facelessStarted) facelessT = std::max(0.0f, facelessT - dt / kFacelessTurn);
        } else {
            sleepSteadyT = 0.0f;
            sleepOut += dt;
            if (sleepOut >= kSleepGrace) wakeUp(true);
        }
    }
    sleepEyes = approach(sleepEyes, sleeping ? 1.0f : 0.0f, dt * 3.0f);
    glimpseT = std::max(0.0f, glimpseT - dt);
}

void Game::State::wakeUp(bool caught) {
    sleeping = false;
    holdF = 0.0f;
    parents.setKidAsleep(false);
    // Открываешь глаза — а они уже обернулись к тебе и тут же отворачиваются.
    if (!facelessStarted) glimpseT = kGlimpseTime;
    if (!caught) return;
    ach.unlock(Ach::Caught);
    // Тебя заметили: гость возвращается быстрее, родители снова смотрят на тебя.
    sleepCooldown = kSleepCooldown;
    monster.attract(6.0f);
    if (facelessStarted) facelessT = 1.0f;
    scene.addShake(0.4f);
    audio.play(Sfx::MonsterGrowl, 0.9f);
}

// ---- Достижения, которые проверяются по ходу поездки ----
void Game::State::trackAchievements(float dt) {
    maxThreat = std::max(maxThreat, monster.threat());
    if (fuel >= kFuelMax - 0.01f) ach.unlock(Ach::FullTank);
    if (mini.fuelCollected() >= 30) ach.unlock(Ach::Cans30);
    if (mini.crashes() >= 20) ach.unlock(Ach::Crashes20);
    if (hits >= 10) ach.unlock(Ach::Hits10);
    consoleRunT = view == ViewMode::Console ? consoleRunT + dt : 0.0f;
    if (consoleRunT >= kConsoleStreak) ach.unlock(Ach::Console60);
}

void Game::State::onTripWon() {
    ach.unlock(Ach::WinAny);
    const Ach byLevel[] = {Ach::WinEasy, Ach::WinNormal, Ach::WinHard};
    ach.unlock(byLevel[clampi(difficulty, 0, 2)]);
    if (photos > 0 && photos == hits) ach.unlock(Ach::PerfectAim);
    if (mini.crashes() == 0) ach.unlock(Ach::CleanRun);
    if (maxThreat >= 90.0f) ach.unlock(Ach::CloseCall);
    if (maxThreat < 20.0f) ach.unlock(Ach::Untouched);
    if (audio.muted()) ach.unlock(Ach::SilentRide);
    ach.addWin();
}

// Всплывающие «ДОСТИЖЕНИЕ»: по одному, не во время скримера.
void Game::State::updatePopup(float dt) {
    popupT = std::min(popupT + dt, kPopupTime);
    if (popupT < kPopupTime || state == GameState::Dying) return;
    if (ach.takeUnlocked(popupAch)) {
        popupT = 0.0f;
        audio.play(Sfx::MenuSelect, 0.8f, 0.0f, 1.25f);
    }
}

// ---- Админ-панель ----
// INS открывает панель; сначала пароль (1667), потом список читов.
void Game::State::updateAdmin(float dt, const Input& in) {
    adminMsgT = std::max(0.0f, adminMsgT - dt);
    if (in.pressed(Key::Escape)) {
        adminOpen = false;
        audio.play(Sfx::MenuMove);
        return;
    }
    if (!adminUnlocked) {
        for (int d = 0; d < 10; ++d) {
            if (in.pressed(static_cast<Key>(static_cast<int>(Key::Digit0) + d)) && adminLen < kAdminMaxDigits) {
                adminInput[adminLen++] = static_cast<char>('0' + d);
                adminInput[adminLen] = '\0';
                audio.play(Sfx::ConsoleBlip, 0.7f);
            }
        }
        if (in.pressed(Key::Backspace) && adminLen > 0) adminInput[--adminLen] = '\0';
        if (in.pressed(Key::Enter)) {
            if (std::strcmp(adminInput, kAdminPassword) == 0) {
                adminUnlocked = true;
                adminCursor = 0;
                adminMsgT = 0.0f; // не тянуть «неверный пароль» с прошлой попытки
                audio.play(Sfx::MenuSelect);
            } else {
                adminMsg = T8("НЕВЕРНЫЙ ПАРОЛЬ");
                adminMsgT = 1.5f;
                audio.play(Sfx::CameraEmpty);
            }
            adminLen = 0;
            adminInput[0] = '\0';
        }
        return;
    }
    if (in.pressed(Key::Up) || in.pressed(Key::W)) adminCursor = (adminCursor + kAdminItems - 1) % kAdminItems;
    if (in.pressed(Key::Down) || in.pressed(Key::S)) adminCursor = (adminCursor + 1) % kAdminItems;
    if (in.pressed(Key::Enter) || in.pressed(Key::Space)) applyAdmin(static_cast<AdminItem>(adminCursor));
}

void Game::State::applyAdmin(AdminItem item) {
    const bool trip = state == GameState::Playing || state == GameState::Paused;
    if (trip && item != AdminItem::Endless) cheatedTrip = true; // любое вмешательство в поездку
    adminMsg = "";
    switch (item) {
    case AdminItem::AutoSleep: cheatAutoSleep = !cheatAutoSleep; break;
    case AdminItem::God: cheatGod = !cheatGod; break;
    case AdminItem::Fuel: cheatFuel = !cheatFuel; break;
    case AdminItem::Autopilot: cheatAutopilot = !cheatAutopilot; break;
    case AdminItem::Camera: cheatCamera = !cheatCamera; break;
    case AdminItem::AutoWin:
        if (!trip) adminMsg = T8("ТОЛЬКО В ПОЕЗДКЕ");
        else if (endlessTrip) adminMsg = T8("У ЭТОЙ ДОРОГИ НЕТ КОНЦА");
        else {
            progress = 1.0f; // на следующем тике — приезд
            adminMsg = T8("ПРИЕХАЛИ");
        }
        break;
    case AdminItem::Banish:
        if (!trip) adminMsg = T8("ТОЛЬКО В ПОЕЗДКЕ");
        else {
            monster.flee(false);
            monster.reduceThreat(100.0f);
            adminMsg = T8("ГОСТЬ УШЁЛ");
        }
        break;
    case AdminItem::Faceless:
        if (!trip || !endlessTrip) adminMsg = T8("ТОЛЬКО НА БЕСКОНЕЧНОЙ ДОРОГЕ");
        else {
            tripTime = std::max(tripTime, kFacelessAt);
            adminMsg = T8("ОНИ ОБОРАЧИВАЮТСЯ");
        }
        break;
    case AdminItem::Endless:
        endless = !endless;
        if (state == GameState::Playing || state == GameState::Paused) endlessTrip = endless;
        break;
    case AdminItem::UnlockAll:
        for (int i = 0; i < kAchCount; ++i) ach.unlock(static_cast<Ach>(i));
        adminMsg = T8("ВСЕ ДОСТИЖЕНИЯ ОТКРЫТЫ");
        break;
    case AdminItem::Count: break;
    }
    adminMsgT = adminMsg[0] ? 1.5f : 0.0f;
    audio.play(Sfx::MenuSelect, 0.7f);
}

// Постоянные читы — каждый тик поездки.
void Game::State::applyCheats() {
    if (anyCheat()) cheatedTrip = true;
    if (cheatFuel) fuel = kFuelMax;
    if (cheatCamera) {
        hasCamera = true;
        charge = 1.0f;
    }
}

// Автопилот мини-игры: к ближайшему предмету впереди, в обход препятствий,
// не съезжая с дороги.
int Game::State::autopilotSteer() const {
    const float px = mini.playerX(), py = mini.playerY();
    const float half = mini.roadHalfWidth();
    float target = mini.roadCenterAt(py - 30.0f);
    float bestDy = 1.0e9f;
    for (const MiniObject& o : mini.objects()) {
        if (!o.alive) continue;
        const bool pickup = o.type == MiniObjectType::Fuel || o.type == MiniObjectType::Camera ||
                            o.type == MiniObjectType::Battery || o.type == MiniObjectType::Lock;
        const float dy = py - o.y; // > 0 — впереди
        if (pickup && dy > -4.0f && dy < 90.0f && dy < bestDy) {
            bestDy = dy;
            target = o.x;
        }
    }
    const float center = mini.roadCenterAt(py - 20.0f);
    for (const MiniObject& o : mini.objects()) {
        if (!o.alive) continue;
        const bool pickup = o.type == MiniObjectType::Fuel || o.type == MiniObjectType::Camera ||
                            o.type == MiniObjectType::Battery || o.type == MiniObjectType::Lock;
        const float dy = py - o.y;
        if (pickup || dy < -6.0f || dy > 64.0f) continue;
        const float clear = o.w * 0.5f + 10.0f;
        if (std::fabs(o.x - target) < clear || std::fabs(o.x - px) < clear) {
            target = o.x < center ? o.x + clear + 2.0f : o.x - clear - 2.0f;
        }
    }
    target = clampf(target, center - half + 8.0f, center + half - 8.0f);
    if (target > px + 2.0f) return 1;
    if (target < px - 2.0f) return -1;
    return 0;
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
            heart = std::max(smoothstep(35.0f, 100.0f, threat), 0.55f * sleepEyes);
            if (monster.onRoof()) dread = 0.45f + 0.55f * smoothstep(0.0f, 60.0f, threat);
            if (facelessT > 0.0f) { // родители обернулись: музыка консоли стихает, гул растёт
                music *= 1.0f - facelessT;
                dread = std::max(dread, 0.6f + 0.4f * facelessT);
            }
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
    p.master = ambMaster * userVolume;
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
    v.endless = endlessTrip;
    v.kmDriven = kmDriven;
    v.facelessTurn = std::max(facelessT, 0.8f * saturate(glimpseT / 0.3f));
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
        {T8("F / ЛКМ"), T8("снимок со вспышкой")},
        {T8("ESC / M / E"), T8("пауза/звук/громкость")},
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
        char line[128];
        std::snprintf(line, sizeof(line), "%s  < %s >%s", T8("СЛОЖНОСТЬ:"), diff().name,
                      endless ? T8("  + БЕСКОНЕЧНАЯ ДОРОГА") : "");
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
    if (toastT >= kToastTime) {
        char achLabel[64];
        std::snprintf(achLabel, sizeof(achLabel), "%s %d/%d", T8("ДОСТИЖЕНИЯ"), ach.unlockedCount(), kAchCount);
        const int w3 = keyHintWidth("TAB", achLabel);
        hud::drawKeyHint(out, 160 - w3 / 2, 167, "TAB", achLabel, 0.6f);
    }
    if (anyCheat()) font::drawTextShadow(out, 4, 4, T8("ADMIN: ЧИТЫ ВКЛЮЧЕНЫ (INS)"), rgb(220, 70, 70), hud::kShadow);
    if (achScreen) renderAchievements(out);
}

// Список достижений поверх меню: две колонки, описание выбранного внизу.
void Game::State::renderAchievements(Canvas& out) const {
    out.blendRect(0, 0, kW, kH, rgb(0, 0, 0), 0.6f);
    hud::drawPanel(out, 6, 4, 308, 172, 0.92f);
    const int pages = (kAchCount + kAchPerPage - 1) / kAchPerPage;
    const int page = achCursor / kAchPerPage;
    char title[96];
    std::snprintf(title, sizeof(title), "%s  %d/%d   %s %d/%d", T8("ДОСТИЖЕНИЯ"), ach.unlockedCount(), kAchCount,
                  T8("СТР."), page + 1, pages);
    textCentered(out, 160, 9, title, kKeyColor);
    const int first = page * kAchPerPage;
    const int last = std::min(kAchCount, first + kAchPerPage);
    for (int i = first; i < last; ++i) {
        const Ach a = static_cast<Ach>(i);
        const AchievementInfo& inf = Achievements::info(a);
        const bool got = ach.has(a);
        const int local = i - first;
        const int x = 14 + (local / kAchRows) * 150;
        const int y = 22 + (local % kAchRows) * 9;
        if (i == achCursor) out.blendRect(x - 3, y - 1, 146, 9, rgb(120, 110, 150), 0.35f);
        font::drawText(out, x, y, got ? "+" : "-", got ? rgb(140, 210, 140) : rgb(70, 66, 80));
        const char* name = (!got && inf.secret) ? "???" : inf.title;
        font::drawTextShadow(out, x + 9, y, name, got ? hud::kText : rgb(96, 92, 106), hud::kShadow);
    }
    const AchievementInfo& sel = Achievements::info(static_cast<Ach>(achCursor));
    const bool selGot = ach.has(static_cast<Ach>(achCursor));
    const char* desc = (!selGot && sel.secret) ? T8("Секретное достижение.") : sel.desc;
    out.fillRect(14, 142, 292, 1, rgb(60, 56, 72));
    textCentered(out, 160, 147, desc, selGot ? rgb(170, 220, 170) : hud::kTextDim);
    const int w = keyHintWidth("ESC", T8("НАЗАД"));
    hud::drawKeyHint(out, 160 - w / 2, 161, "ESC", T8("НАЗАД"), 0.8f);
}

// Веки смыкаются сверху и снизу; когда глаза закрыты — шкала дыхания.
void Game::State::renderSleep(Canvas& out) const {
    if (endlessTrip && !sleeping && tripTime < 9.0f && state == GameState::Playing) {
        const float a = saturate(std::min(tripTime / 0.5f, (9.0f - tripTime) / 0.6f));
        textBanner(out, 160, 58, T8("ДЕРЖИ F - ПРИТВОРИТЬСЯ СПЯЩИМ"), rgb(200, 190, 220), a); // выше подсказки TAB
    }
    if (sleepEyes <= 0.0f) return;
    const float open = 90.0f * (1.0f - 0.97f * smoothstep(0.0f, 1.0f, sleepEyes));
    for (int y = 0; y < kH; ++y) {
        const float d = std::fabs(static_cast<float>(y) + 0.5f - 90.0f) - open; // > 0 — под веком
        if (d <= -8.0f) continue;
        const float k = d >= 0.0f ? 1.0f : 1.0f - (-d / 8.0f);
        out.blendRect(0, y, kW, 1, rgb(2, 1, 3), 0.97f * k);
    }
    // Глаза закрыты, но уши — нет: субтитры поверх век.
    if (sleepEyes > 0.3f && state == GameState::Playing) renderSubtitle(out, false);
    if (!sleeping) return;
    const bool inZone = std::fabs(sleepPos) <= kSleepZone;
    const int bx = 90, bw = 140, by = 148;
    textCentered(out, 160, by - 13, T8("ДЫШИ РОВНО: A / D"), inZone ? rgb(150, 200, 150) : rgb(220, 110, 100));
    out.fillRect(bx - 1, by - 1, bw + 2, 10, rgb(40, 36, 48));
    out.fillRect(bx, by, bw, 8, rgb(14, 12, 18));
    const int zw = roundi(kSleepZone * static_cast<float>(bw) * 0.5f);
    out.fillRect(160 - zw, by, 2 * zw, 8, inZone ? rgb(40, 120, 60) : rgb(30, 80, 40));
    const int mx = 160 + roundi(sleepPos * static_cast<float>(bw) * 0.5f);
    const bool blink = !inZone && static_cast<int>(animTime * 10.0f) % 2 == 0;
    out.fillRect(mx - 1, by - 3, 3, 14, blink ? rgb(230, 60, 50) : rgb(230, 226, 214));
}

// Админ-панель: сначала поле пароля, затем список читов.
void Game::State::renderAdmin(Canvas& out) const {
    out.blendRect(0, 0, kW, kH, rgb(0, 0, 0), 0.55f);
    const int pw = 236, ph = adminUnlocked ? 150 : 64;
    const int px = 160 - pw / 2, py = 90 - ph / 2;
    hud::drawPanel(out, px, py, pw, ph, 0.94f);
    out.fillRect(px + 1, py + 1, pw - 2, 1, rgb(200, 70, 70));
    textCentered(out, 160, py + 5, T8("АДМИН-ПАНЕЛЬ"), rgb(220, 90, 90));
    if (!adminUnlocked) {
        char stars[kAdminMaxDigits + 2] = {};
        for (int i = 0; i < adminLen; ++i) stars[i] = '*';
        if (static_cast<int>(animTime * 2.0f) % 2 == 0) stars[adminLen] = '_';
        char line[48];
        std::snprintf(line, sizeof(line), "%s %s", T8("ПАРОЛЬ:"), stars);
        textCentered(out, 160, py + 21, line, hud::kText);
        if (adminMsgT > 0.0f) textCentered(out, 160, py + 33, adminMsg, rgb(230, 80, 70));
        textCentered(out, 160, py + 47, T8("ЦИФРЫ, ENTER - ВОЙТИ, ESC - ЗАКРЫТЬ"), hud::kTextDim);
        return;
    }
    struct Row { const char* name; int toggle; }; // toggle: -1 действие, 0/1 — выкл/вкл
    const Row rows[kAdminItems] = {
        {T8("Автосон (сам держит дыхание)"), cheatAutoSleep ? 1 : 0},
        {T8("Бессмертие"), cheatGod ? 1 : 0},
        {T8("Бесконечный бензин"), cheatFuel ? 1 : 0},
        {T8("Автопилот мини-игры"), cheatAutopilot ? 1 : 0},
        {T8("Камера всегда готова"), cheatCamera ? 1 : 0},
        {T8("Авто-победа"), -1},
        {T8("Прогнать гостя"), -1},
        {T8("Сразу к безликим"), -1},
        {T8("Бесконечная дорога"), endless ? 1 : 0},
        {T8("Открыть все достижения"), -1},
    };
    for (int i = 0; i < kAdminItems; ++i) {
        const int y = py + 19 + i * 10;
        if (i == adminCursor) out.blendRect(px + 6, y - 2, pw - 12, 10, rgb(150, 60, 60), 0.4f);
        font::drawTextShadow(out, px + 12, y, rows[i].name, i == adminCursor ? hud::kText : hud::kTextDim, hud::kShadow);
        const char* val = rows[i].toggle < 0 ? "[ENTER]" : (rows[i].toggle ? T8("ВКЛ") : T8("ВЫКЛ"));
        const uint32_t vc = rows[i].toggle < 0 ? rgb(150, 140, 120) : (rows[i].toggle ? rgb(120, 220, 120) : rgb(110, 100, 110));
        textRight(out, px + pw - 12, y, val, vc);
    }
    if (adminMsgT > 0.0f) textCentered(out, 160, py + ph - 26, adminMsg, kKeyColor);
    textCentered(out, 160, py + ph - 13, T8("W/S - ВЫБОР, ENTER - ПРИМЕНИТЬ, ESC - ЗАКРЫТЬ"), hud::kTextDim);
}

// «ДОСТИЖЕНИЕ ПОЛУЧЕНО» — выезжает сверху по центру и уезжает обратно.
void Game::State::renderPopup(Canvas& out) const {
    if (popupT >= kPopupTime) return;
    const float in = smoothstep(0.0f, 0.3f, popupT);
    const float outK = smoothstep(kPopupTime - 0.4f, kPopupTime, popupT);
    const int slide = roundi(-26.0f * (1.0f - in) - 26.0f * outK);
    const char* name = Achievements::info(popupAch).title;
    const int w = std::max(font::textWidth(name), font::textWidth(T8("ДОСТИЖЕНИЕ ПОЛУЧЕНО"))) + 20;
    const int x = 160 - w / 2, y = 3 + slide;
    hud::drawPanel(out, x, y, w, 22, 0.9f);
    out.fillRect(x + 1, y + 1, w - 2, 1, kKeyColor);
    textCentered(out, 160, y + 4, T8("ДОСТИЖЕНИЕ ПОЛУЧЕНО"), kKeyColor);
    textCentered(out, 160, y + 13, name, hud::kText);
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
    uint32_t col = parents.speaker() == Speaker::Dad ? kDadColor : kMomColor;
    if (facelessStarted) col = rgb(176, 188, 200); // шёпот безликих — бесцветный
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
    if (cheatedTrip) textCentered(out, 160, 45, T8("ADMIN: ПОЕЗДКА С ЧИТАМИ"), scaleColor(rgb(220, 70, 70), a));

    const int pw = 216, ph = 96;
    const int px = 160 - pw / 2, py = 82;
    hud::drawPanel(out, px, py, pw, ph, 0.84f * a);
    if (a < 0.5f) return;

    char val[6][40];
    const int secs = static_cast<int>(tripTime);
    std::snprintf(val[0], sizeof(val[0]), "%d:%02d", secs / 60, secs % 60);
    if (endlessTrip) {
        std::snprintf(val[1], sizeof(val[1]), "%.1f %s", static_cast<double>(kmDriven), T8("КМ"));
    } else {
        std::snprintf(val[1], sizeof(val[1]), "%.1f %s %.1f %s", static_cast<double>(progress * kTripKm), T8("ИЗ"),
                      static_cast<double>(kTripKm), T8("КМ"));
    }
    std::snprintf(val[2], sizeof(val[2]), "%d", mini.fuelCollected());
    std::snprintf(val[3], sizeof(val[3]), "%d (%s %d)", photos, T8("В ЦЕЛЬ"), hits);
    std::snprintf(val[4], sizeof(val[4]), "%d", mini.crashes());
    std::snprintf(val[5], sizeof(val[5]), "%s%s", diff().name, endlessTrip ? T8(" + ДОРОГА") : "");
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
    if (toastT >= kToastTime || (state == GameState::Paused && !toastVolume)) return; // вкл/выкл видно на панели
    const float a = saturate(std::min(toastT / 0.15f, (kToastTime - toastT) / 0.4f));
    const bool consoleUp = (state == GameState::Playing || state == GameState::Arriving) && raiseEased() >= 0.5f;
    const int y = state == GameState::Menu || consoleUp ? 167 : 40;
    const int cx = consoleUp ? 198 : 160;
    char vol[48];
    std::snprintf(vol, sizeof(vol), "%s %d%%", T8("ГРОМКОСТЬ"), roundi(userVolume * 100.0f));
    const char* msg = toastVolume ? vol : (toastMuted ? T8("ЗВУК ВЫКЛЮЧЕН") : T8("ЗВУК ВКЛЮЧЁН"));
    textBanner(out, cx, y, msg, hud::kText, a);
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
        s.toastVolume = false;
        s.toastT = 0.0f;
    }
    // E — громкость +10% по кругу (после 100% снова 10%).
    if (input.pressed(Key::E)) {
        s.userVolume = s.userVolume >= 0.999f ? kVolumeStep : std::min(1.0f, s.userVolume + kVolumeStep);
        s.toastVolume = true;
        s.toastT = 0.0f;
        if (s.userVolume <= kVolumeStep + 0.001f) s.ach.unlock(Ach::Hush);
        s.audio.play(Sfx::MenuMove, 0.7f);
    }
    if (input.pressed(Key::F11)) s.fullscreenReq = true;

    // INS — админ-панель; пока открыта, игра стоит.
    if (input.pressed(Key::Insert)) {
        s.adminOpen = !s.adminOpen;
        s.adminLen = 0;
        s.adminInput[0] = '\0';
        s.adminMsgT = 0.0f;
        s.audio.play(Sfx::MenuMove);
    } else if (s.adminOpen) {
        s.updateAdmin(dt, input);
        // Панель закрыли клавишей ESC — этот же ESC не должен дойти до игры (пауза/выход).
        if (!s.adminOpen) {
            s.updateAmbient(dt);
            return;
        }
    }
    if (s.adminOpen) {
        s.updatePopup(dt);
        s.updateAmbient(dt);
        return;
    }

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
    s.updatePopup(dt);
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
    if (s.state == GameState::Playing || s.state == GameState::Paused) s.renderSleep(out);
    if (s.anyCheat() && s.state == GameState::Playing) textCentered(out, 160, 172, "ADMIN", rgb(200, 70, 70));
    if (s.adminOpen) s.renderAdmin(out);
    s.renderToast(out);
    s.renderPopup(out);

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

void Game::setSaveFile(const std::filesystem::path& file) { st_->ach.setSaveFile(file); }

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
