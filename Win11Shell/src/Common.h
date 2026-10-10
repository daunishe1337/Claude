#pragma once
// Общие подключения WinAPI/GDI+ и базовые типы: геометрия, цвет, время, анимации, строки.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// gdiplus.h использует min/max без квалификации, а NOMINMAX убирает макросы.
namespace Gdiplus
{
using std::max;
using std::min;
}
#include <gdiplus.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// ---------------------------------------------------------------------------
// Математика
// ---------------------------------------------------------------------------
constexpr float kPi = 3.14159265358979f;

template <class T>
inline T Clamp(T v, T lo, T hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Saturate(float v) { return Clamp(v, 0.0f, 1.0f); }
inline int RoundToInt(float v) { return static_cast<int>(std::floor(v + 0.5f)); }
inline float SmoothStep(float e0, float e1, float x)
{
    const float t = Saturate((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

// ---------------------------------------------------------------------------
// Геометрия (в логических пикселях, DIP)
// ---------------------------------------------------------------------------
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};
inline Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
inline Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
inline float Distance(Point a, Point b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }
    Point Center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    bool Empty() const { return w <= 0.0f || h <= 0.0f; }
    bool Contains(Point p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    bool Intersects(const Rect& o) const { return o.x < x + w && x < o.x + o.w && o.y < y + h && y < o.y + o.h; }
    Rect Inflated(float d) const { return {x - d, y - d, w + 2.0f * d, h + 2.0f * d}; }
    Rect Inflated(float dx, float dy) const { return {x - dx, y - dy, w + 2.0f * dx, h + 2.0f * dy}; }
    Rect Translated(float dx, float dy) const { return {x + dx, y + dy, w, h}; }
    Rect Inset(float l, float t, float r, float b) const { return {x + l, y + t, w - l - r, h - t - b}; }
    Rect Intersect(const Rect& o) const
    {
        const float l = std::max(x, o.x), t = std::max(y, o.y);
        const float r = std::min(Right(), o.Right()), b = std::min(Bottom(), o.Bottom());
        return {l, t, std::max(0.0f, r - l), std::max(0.0f, b - t)};
    }
    static Rect LTRB(float l, float t, float r, float b) { return {l, t, r - l, b - t}; }
};
inline Rect LerpRect(const Rect& a, const Rect& b, float t)
{
    return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.w, b.w, t), Lerp(a.h, b.h, t)};
}

// ---------------------------------------------------------------------------
// Цвет (RGBA, неумноженная альфа)
// ---------------------------------------------------------------------------
struct Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa = 255) : r(rr), g(gg), b(bb), a(aa) {}

    static constexpr Color Hex(uint32_t rgb, uint8_t alpha = 255)
    {
        return Color(static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF),
                     static_cast<uint8_t>(rgb & 0xFF), alpha);
    }
    constexpr Color WithAlpha(uint8_t na) const { return Color(r, g, b, na); }
    Color MulAlpha(float f) const { return Color(r, g, b, static_cast<uint8_t>(Clamp(a * f, 0.0f, 255.0f))); }
    constexpr bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    constexpr bool operator!=(const Color& o) const { return !(*this == o); }
};

inline uint8_t LerpByte(uint8_t a, uint8_t b, float t)
{
    return static_cast<uint8_t>(Clamp(a + (static_cast<float>(b) - static_cast<float>(a)) * t + 0.5f, 0.0f, 255.0f));
}
inline Color LerpColor(Color a, Color b, float t)
{
    return Color(LerpByte(a.r, b.r, t), LerpByte(a.g, b.g, t), LerpByte(a.b, b.b, t), LerpByte(a.a, b.a, t));
}
// Наложение полупрозрачного цвета на непрозрачный (результат непрозрачный).
inline Color Over(Color base, Color top)
{
    const float t = top.a / 255.0f;
    return Color(LerpByte(base.r, top.r, t), LerpByte(base.g, top.g, t), LerpByte(base.b, top.b, t), 255);
}

// ---------------------------------------------------------------------------
// Время и анимации
// ---------------------------------------------------------------------------
double NowSeconds();
void RequestFrame();        // попросить ещё один кадр (анимация идёт)
bool TakeFrameRequest();    // сбросить и вернуть флаг
void SetAnimationsEnabled(bool on);
bool AnimationsEnabled();

enum class Ease
{
    Linear,
    OutCubic,
    InCubic,
    InOutCubic,
    OutQuint,
    OutBack,
};
float ApplyEase(Ease e, float t);

// Значение, плавно переходящее к цели за заданное время.
class Tween
{
public:
    Tween() = default;
    explicit Tween(float v) : m_from(v), m_to(v) {}

    float Value() const;              // текущее значение (просит кадр, пока анимируется)
    float Target() const { return m_to; }
    bool Animating() const;
    void Set(float target, float duration = 0.2f, Ease ease = Ease::OutCubic);
    void Start(float from, float to, float duration, Ease ease = Ease::OutCubic);
    void Snap(float v);

private:
    float m_from = 0.0f;
    float m_to = 0.0f;
    double m_start = 0.0;
    float m_duration = 0.0f;
    Ease m_ease = Ease::OutCubic;
};

// Отложенные действия: выполняются после обработки текущего события ввода
// (безопасно перестраивать интерфейс из обработчиков щелчков).
void PostAction(std::function<void()> fn);
bool RunPostedActions();

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
enum class MouseButton
{
    Left,
    Right,
    Middle,
};

struct KeyMods
{
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};
KeyMods CurrentMods();

struct MouseEvent
{
    Point pos;
    MouseButton button = MouseButton::Left;
    int clicks = 1;     // 2 — двойной щелчок
    KeyMods mods;
    float wheel = 0.0f; // в «щелчках» колеса, вверх > 0
};

enum class CursorType
{
    Arrow,
    Hand,
    IBeam,
    SizeWE,
    SizeNS,
    SizeNWSE,
    SizeNESW,
    SizeAll,
    Wait,
    Cross,
};

// ---------------------------------------------------------------------------
// Строки, форматирование, буфер обмена
// ---------------------------------------------------------------------------
std::wstring ToLower(const std::wstring& s);
bool ContainsNoCase(const std::wstring& hay, const std::wstring& needle);
bool StartsWithNoCase(const std::wstring& s, const std::wstring& prefix);
int CompareNoCase(const std::wstring& a, const std::wstring& b);
std::wstring Trim(const std::wstring& s);
std::wstring IntToStr(long long v);
std::wstring Pad2(int v);
std::wstring GroupDigits(long long v);              // 1 234 567
std::wstring FormatKB(unsigned long long bytes);     // «12 КБ»
std::wstring FormatSize(unsigned long long bytes);   // «1,5 ГБ»
std::wstring FormatDateTime(const SYSTEMTIME& st);   // 10.10.2026 14:05
std::wstring FormatTime(const SYSTEMTIME& st);       // 14:05
std::wstring FormatDate(const SYSTEMTIME& st);       // 10.10.2026
std::wstring FormatLongDate(const SYSTEMTIME& st);   // суббота, 10 октября
const wchar_t* MonthName(int month);                 // январь
const wchar_t* MonthNameGenitive(int month);         // января
const wchar_t* WeekdayName(int dow);                 // 0 = воскресенье
const wchar_t* WeekdayShort(int dowMondayFirst);     // 0 = Пн
const wchar_t* PluralRu(long long n, const wchar_t* one, const wchar_t* few, const wchar_t* many);
SYSTEMTIME LocalNow();
SYSTEMTIME AddDays(const SYSTEMTIME& st, int days);
SYSTEMTIME AddMinutes(const SYSTEMTIME& st, int minutes);
int DaysInMonth(int year, int month);
int DayOfWeek(int year, int month, int day); // 0 = воскресенье

bool SetClipboardText(HWND hwnd, const std::wstring& text);
std::wstring GetClipboardText(HWND hwnd);

float RandomFloat(float lo, float hi);
int RandomInt(int lo, int hi);
