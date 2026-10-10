#include "Common.h"

#include <cwctype>
#include <random>

namespace
{
LARGE_INTEGER g_qpcFreq{};
LARGE_INTEGER g_qpcStart{};
bool g_qpcReady = false;
bool g_frameRequested = false;
bool g_animations = true;

std::mt19937& Rng()
{
    static std::mt19937 rng(static_cast<unsigned>(GetTickCount64() & 0xFFFFFFFFu));
    return rng;
}
} // namespace

double NowSeconds()
{
    if (!g_qpcReady)
    {
        QueryPerformanceFrequency(&g_qpcFreq);
        QueryPerformanceCounter(&g_qpcStart);
        g_qpcReady = true;
    }
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - g_qpcStart.QuadPart) / static_cast<double>(g_qpcFreq.QuadPart);
}

void RequestFrame() { g_frameRequested = true; }

bool TakeFrameRequest()
{
    const bool r = g_frameRequested;
    g_frameRequested = false;
    return r;
}

void SetAnimationsEnabled(bool on) { g_animations = on; }
bool AnimationsEnabled() { return g_animations; }

float ApplyEase(Ease e, float t)
{
    t = Saturate(t);
    switch (e)
    {
    case Ease::Linear:
        return t;
    case Ease::OutCubic:
    {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }
    case Ease::InCubic:
        return t * t * t;
    case Ease::InOutCubic:
        if (t < 0.5f)
            return 4.0f * t * t * t;
        else
        {
            const float u = -2.0f * t + 2.0f;
            return 1.0f - u * u * u * 0.5f;
        }
    case Ease::OutQuint:
    {
        const float u = 1.0f - t;
        return 1.0f - u * u * u * u * u;
    }
    case Ease::OutBack:
    {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        const float u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }
    }
    return t;
}

// ---------------------------------------------------------------------------
// Tween
// ---------------------------------------------------------------------------
float Tween::Value() const
{
    if (m_duration <= 0.0f)
        return m_to;
    const double t = (NowSeconds() - m_start) / static_cast<double>(m_duration);
    if (t >= 1.0)
        return m_to;
    RequestFrame();
    const float k = ApplyEase(m_ease, static_cast<float>(t < 0.0 ? 0.0 : t));
    return m_from + (m_to - m_from) * k;
}

bool Tween::Animating() const
{
    if (m_duration <= 0.0f)
        return false;
    return (NowSeconds() - m_start) < static_cast<double>(m_duration);
}

void Tween::Set(float target, float duration, Ease ease)
{
    if (target == m_to)
        return;
    Start(Value(), target, duration, ease);
}

void Tween::Start(float from, float to, float duration, Ease ease)
{
    m_from = from;
    m_to = to;
    m_start = NowSeconds();
    m_duration = g_animations ? duration : 0.0f;
    m_ease = ease;
    RequestFrame();
}

void Tween::Snap(float v)
{
    m_from = v;
    m_to = v;
    m_duration = 0.0f;
}

// ---------------------------------------------------------------------------
// Отложенные действия
// ---------------------------------------------------------------------------
namespace
{
std::vector<std::function<void()>>& PostedQueue()
{
    static std::vector<std::function<void()>> q;
    return q;
}
} // namespace

void PostAction(std::function<void()> fn)
{
    if (fn)
        PostedQueue().push_back(std::move(fn));
}

bool RunPostedActions()
{
    bool any = false;
    // Действия могут добавлять новые — обрабатываем, пока очередь не опустеет (с защитой от зацикливания).
    for (int guard = 0; guard < 16 && !PostedQueue().empty(); ++guard)
    {
        std::vector<std::function<void()>> batch;
        batch.swap(PostedQueue());
        for (auto& fn : batch)
            fn();
        any = true;
    }
    return any;
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
KeyMods CurrentMods()
{
    KeyMods m;
    m.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    m.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    m.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    return m;
}

// ---------------------------------------------------------------------------
// Строки
// ---------------------------------------------------------------------------
std::wstring ToLower(const std::wstring& s)
{
    std::wstring r = s;
    if (!r.empty())
        CharLowerBuffW(&r[0], static_cast<DWORD>(r.size()));
    return r;
}

bool ContainsNoCase(const std::wstring& hay, const std::wstring& needle)
{
    if (needle.empty())
        return true;
    return ToLower(hay).find(ToLower(needle)) != std::wstring::npos;
}

bool StartsWithNoCase(const std::wstring& s, const std::wstring& prefix)
{
    if (prefix.size() > s.size())
        return false;
    return ToLower(s.substr(0, prefix.size())) == ToLower(prefix);
}

int CompareNoCase(const std::wstring& a, const std::wstring& b)
{
    const int r = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                 static_cast<int>(b.size()));
    return r - 2; // CSTR_LESS_THAN = 1, CSTR_EQUAL = 2, CSTR_GREATER_THAN = 3
}

std::wstring Trim(const std::wstring& s)
{
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b]))
        ++b;
    while (e > b && iswspace(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

std::wstring IntToStr(long long v) { return std::to_wstring(v); }

std::wstring Pad2(int v)
{
    std::wstring s = std::to_wstring(v);
    if (s.size() < 2)
        s.insert(s.begin(), L'0');
    return s;
}

std::wstring GroupDigits(long long v)
{
    const bool neg = v < 0;
    unsigned long long u = neg ? static_cast<unsigned long long>(-(v + 1)) + 1ULL : static_cast<unsigned long long>(v);
    std::wstring digits = std::to_wstring(u);
    std::wstring out;
    int count = 0;
    for (size_t i = digits.size(); i > 0; --i)
    {
        out.insert(out.begin(), digits[i - 1]);
        if (++count % 3 == 0 && i > 1)
            out.insert(out.begin(), L' ');
    }
    if (neg)
        out.insert(out.begin(), L'-');
    return out;
}

std::wstring FormatKB(unsigned long long bytes)
{
    const unsigned long long kb = (bytes + 1023ULL) / 1024ULL;
    return GroupDigits(static_cast<long long>(kb == 0 && bytes > 0 ? 1 : kb)) + L" КБ";
}

std::wstring FormatSize(unsigned long long bytes)
{
    const wchar_t* units[] = {L"байт", L"КБ", L"МБ", L"ГБ", L"ТБ"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4)
    {
        v /= 1024.0;
        ++u;
    }
    std::wstring num;
    if (u == 0 || v >= 100.0)
        num = GroupDigits(static_cast<long long>(v + 0.5));
    else
    {
        const long long tenths = static_cast<long long>(v * 10.0 + 0.5);
        num = GroupDigits(tenths / 10);
        if (tenths % 10 != 0)
            num += L"," + std::to_wstring(tenths % 10);
    }
    return num + L" " + units[u];
}

std::wstring FormatTime(const SYSTEMTIME& st) { return Pad2(st.wHour) + L":" + Pad2(st.wMinute); }

std::wstring FormatDate(const SYSTEMTIME& st)
{
    return Pad2(st.wDay) + L"." + Pad2(st.wMonth) + L"." + std::to_wstring(st.wYear);
}

std::wstring FormatDateTime(const SYSTEMTIME& st) { return FormatDate(st) + L" " + FormatTime(st); }

const wchar_t* MonthName(int month)
{
    static const wchar_t* names[] = {L"январь", L"февраль", L"март",     L"апрель",  L"май",    L"июнь",
                                     L"июль",   L"август",  L"сентябрь", L"октябрь", L"ноябрь", L"декабрь"};
    return names[Clamp(month, 1, 12) - 1];
}

const wchar_t* MonthNameGenitive(int month)
{
    static const wchar_t* names[] = {L"января", L"февраля", L"марта",    L"апреля",  L"мая",    L"июня",
                                     L"июля",   L"августа", L"сентября", L"октября", L"ноября", L"декабря"};
    return names[Clamp(month, 1, 12) - 1];
}

const wchar_t* WeekdayName(int dow)
{
    static const wchar_t* names[] = {L"воскресенье", L"понедельник", L"вторник", L"среда",
                                     L"четверг",     L"пятница",     L"суббота"};
    return names[Clamp(dow, 0, 6)];
}

const wchar_t* WeekdayShort(int dowMondayFirst)
{
    static const wchar_t* names[] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};
    return names[Clamp(dowMondayFirst, 0, 6)];
}

std::wstring FormatLongDate(const SYSTEMTIME& st)
{
    return std::wstring(WeekdayName(st.wDayOfWeek)) + L", " + std::to_wstring(st.wDay) + L" " +
           MonthNameGenitive(st.wMonth);
}

const wchar_t* PluralRu(long long n, const wchar_t* one, const wchar_t* few, const wchar_t* many)
{
    if (n < 0)
        n = -n;
    const long long m10 = n % 10, m100 = n % 100;
    if (m10 == 1 && m100 != 11)
        return one;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return few;
    return many;
}

SYSTEMTIME LocalNow()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return st;
}

static SYSTEMTIME AddTicks(const SYSTEMTIME& st, long long ticks)
{
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart = static_cast<ULONGLONG>(static_cast<long long>(u.QuadPart) + ticks);
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME out{};
    FileTimeToSystemTime(&ft, &out);
    return out;
}

SYSTEMTIME AddDays(const SYSTEMTIME& st, int days) { return AddTicks(st, static_cast<long long>(days) * 864000000000LL); }

SYSTEMTIME AddMinutes(const SYSTEMTIME& st, int minutes)
{
    return AddTicks(st, static_cast<long long>(minutes) * 600000000LL);
}

int DaysInMonth(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2)
    {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return days[Clamp(month, 1, 12) - 1];
}

int DayOfWeek(int year, int month, int day)
{
    // Алгоритм Сакамото
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3)
        year -= 1;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

// ---------------------------------------------------------------------------
// Буфер обмена
// ---------------------------------------------------------------------------
bool SetClipboardText(HWND hwnd, const std::wstring& text)
{
    if (!OpenClipboard(hwnd))
        return false;
    EmptyClipboard();
    std::wstring crlf;
    crlf.reserve(text.size() + 16);
    for (wchar_t c : text)
    {
        if (c == L'\n')
            crlf += L"\r\n";
        else
            crlf += c;
    }
    const size_t bytes = (crlf.size() + 1) * sizeof(wchar_t);
    bool ok = false;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem)
    {
        void* p = GlobalLock(mem);
        if (p)
        {
            std::memcpy(p, crlf.c_str(), bytes);
            GlobalUnlock(mem);
            ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        }
        if (!ok)
            GlobalFree(mem);
    }
    CloseClipboard();
    return ok;
}

std::wstring GetClipboardText(HWND hwnd)
{
    std::wstring result;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(hwnd))
        return result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h)
    {
        const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p)
        {
            for (; *p; ++p)
            {
                if (*p != L'\r')
                    result += *p;
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}

float RandomFloat(float lo, float hi)
{
    std::uniform_real_distribution<float> d(lo, hi);
    return d(Rng());
}

int RandomInt(int lo, int hi)
{
    std::uniform_int_distribution<int> d(lo, hi);
    return d(Rng());
}
