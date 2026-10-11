// Часы Windows 11: сеансы фокусировки, таймеры, будильники, секундомер с кругами и мировое время
// с картой смены дня и ночи. Состояние хранится вне окна, поэтому таймеры, будильники и сеансы
// фокусировки срабатывают (уведомлением) и при закрытом приложении.

#include "../Shell.h"
#include "AppFactories.h"
#include "WorldMap.h"

namespace
{
constexpr float kTitleH = 48.0f;
constexpr float kNavFull = 280.0f;
constexpr float kNavCompact = 48.0f;
constexpr float kCardW = 316.0f;
constexpr float kGap = 16.0f;
constexpr float kPad = 28.0f;

const wchar_t* const kDayShort[7] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};
const wchar_t* const kSounds[] = {L"Звонок", L"Колокольчики", L"Ксилофон", L"Перезвон", L"Капли", L"Восход"};
constexpr int kSoundCount = 6;
const int kSnoozes[] = {0, 5, 10, 20, 30, 60};
constexpr int kSnoozeCount = 6;
const wchar_t* const kMonthsGen[12] = {L"января", L"февраля", L"марта",    L"апреля",  L"мая",    L"июня",
                                       L"июля",   L"августа", L"сентября", L"октября", L"ноября", L"декабря"};

std::wstring Plural(int n, const wchar_t* one, const wchar_t* few, const wchar_t* many)
{
    const int m10 = n % 10, m100 = n % 100;
    if (m10 == 1 && m100 != 11)
        return std::to_wstring(n) + L" " + one;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return std::to_wstring(n) + L" " + few;
    return std::to_wstring(n) + L" " + many;
}

// «1 ч 30 мин»
std::wstring DurationText(int minutes)
{
    const int h = minutes / 60, m = minutes % 60;
    if (h > 0 && m > 0)
        return std::to_wstring(h) + L" ч " + std::to_wstring(m) + L" мин";
    if (h > 0)
        return std::to_wstring(h) + L" ч";
    return std::to_wstring(m) + L" мин";
}

// ============================================================================ мировое время
enum class Dst
{
    None,
    Eu,
    Us,
    Au,
    Nz,
};

struct City
{
    const wchar_t* name;
    const wchar_t* country;
    const wchar_t* alias;
    float lat;
    float lon;
    int offset; // минуты относительно UTC (зимнее время)
    Dst dst;
};

const City kCities[] = {
    {L"Москва", L"Россия", L"moscow", 55.75f, 37.62f, 180, Dst::None},
    {L"Санкт-Петербург", L"Россия", L"saint petersburg", 59.94f, 30.31f, 180, Dst::None},
    {L"Калининград", L"Россия", L"kaliningrad", 54.71f, 20.51f, 120, Dst::None},
    {L"Екатеринбург", L"Россия", L"yekaterinburg", 56.84f, 60.6f, 300, Dst::None},
    {L"Новосибирск", L"Россия", L"novosibirsk", 55.03f, 82.92f, 420, Dst::None},
    {L"Владивосток", L"Россия", L"vladivostok", 43.12f, 131.89f, 600, Dst::None},
    {L"Лондон", L"Великобритания", L"london", 51.51f, -0.13f, 0, Dst::Eu},
    {L"Париж", L"Франция", L"paris", 48.86f, 2.35f, 60, Dst::Eu},
    {L"Берлин", L"Германия", L"berlin", 52.52f, 13.4f, 60, Dst::Eu},
    {L"Рим", L"Италия", L"rome", 41.9f, 12.5f, 60, Dst::Eu},
    {L"Мадрид", L"Испания", L"madrid", 40.42f, -3.7f, 60, Dst::Eu},
    {L"Стамбул", L"Турция", L"istanbul", 41.01f, 28.98f, 180, Dst::None},
    {L"Рейкьявик", L"Исландия", L"reykjavik", 64.15f, -21.94f, 0, Dst::None},
    {L"Нью-Йорк", L"США", L"new york", 40.71f, -74.0f, -300, Dst::Us},
    {L"Чикаго", L"США", L"chicago", 41.88f, -87.63f, -360, Dst::Us},
    {L"Лос-Анджелес", L"США", L"los angeles", 34.05f, -118.24f, -480, Dst::Us},
    {L"Торонто", L"Канада", L"toronto", 43.65f, -79.38f, -300, Dst::Us},
    {L"Гонолулу", L"США", L"honolulu", 21.31f, -157.86f, -600, Dst::None},
    {L"Мехико", L"Мексика", L"mexico city", 19.43f, -99.13f, -360, Dst::None},
    {L"Сан-Паулу", L"Бразилия", L"sao paulo", -23.55f, -46.63f, -180, Dst::None},
    {L"Буэнос-Айрес", L"Аргентина", L"buenos aires", -34.6f, -58.38f, -180, Dst::None},
    {L"Кейптаун", L"ЮАР", L"cape town", -33.92f, 18.42f, 120, Dst::None},
    {L"Найроби", L"Кения", L"nairobi", -1.29f, 36.82f, 180, Dst::None},
    {L"Дубай", L"ОАЭ", L"dubai", 25.2f, 55.27f, 240, Dst::None},
    {L"Нью-Дели", L"Индия", L"new delhi", 28.61f, 77.21f, 330, Dst::None},
    {L"Бангкок", L"Таиланд", L"bangkok", 13.76f, 100.5f, 420, Dst::None},
    {L"Сингапур", L"Сингапур", L"singapore", 1.35f, 103.82f, 480, Dst::None},
    {L"Пекин", L"Китай", L"beijing", 39.9f, 116.4f, 480, Dst::None},
    {L"Сеул", L"Южная Корея", L"seoul", 37.57f, 126.98f, 540, Dst::None},
    {L"Токио", L"Япония", L"tokyo", 35.68f, 139.69f, 540, Dst::None},
    {L"Сидней", L"Австралия", L"sydney", -33.87f, 151.21f, 600, Dst::Au},
    {L"Окленд", L"Новая Зеландия", L"auckland", -36.85f, 174.76f, 720, Dst::Nz},
};
constexpr int kCityCount = static_cast<int>(sizeof(kCities) / sizeof(kCities[0]));

int CityIndex(const wchar_t* alias)
{
    for (int i = 0; i < kCityCount; ++i)
    {
        if (std::wcscmp(kCities[i].alias, alias) == 0)
            return i;
    }
    return 0;
}

// День недели (0 — воскресенье).
int DayOfWeek(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

int DaysInMonth(int y, int m)
{
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : d[m - 1];
}

int NthSunday(int y, int m, int n)
{
    const int first = DayOfWeek(y, m, 1);
    return 1 + (7 - first) % 7 + 7 * (n - 1);
}

int LastSunday(int y, int m)
{
    const int last = DaysInMonth(y, m);
    return last - DayOfWeek(y, m, last);
}

SYSTEMTIME UtcShifted(int minutes)
{
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += static_cast<ULONGLONG>(static_cast<LONGLONG>(minutes) * 600000000LL);
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    return st;
}

// t — местное зимнее время города.
bool InDst(Dst rule, const SYSTEMTIME& t, int offset)
{
    const int y = t.wYear;
    auto at = [](int m, int d, int h) { return ((static_cast<long long>(m) * 32 + d) * 24 + h) * 60; };
    const long long now = ((static_cast<long long>(t.wMonth) * 32 + t.wDay) * 24 + t.wHour) * 60 + t.wMinute;
    switch (rule)
    {
    case Dst::Eu:
    {
        const int h = 1 + offset / 60; // переход в 01:00 UTC
        return now >= at(3, LastSunday(y, 3), h) && now < at(10, LastSunday(y, 10), h);
    }
    case Dst::Us:
        return now >= at(3, NthSunday(y, 3, 2), 2) && now < at(11, NthSunday(y, 11, 1), 1);
    case Dst::Au:
        return now >= at(10, NthSunday(y, 10, 1), 2) || now < at(4, NthSunday(y, 4, 1), 2);
    case Dst::Nz:
        return now >= at(9, LastSunday(y, 9), 2) || now < at(4, NthSunday(y, 4, 1), 2);
    case Dst::None:
        break;
    }
    return false;
}

SYSTEMTIME CityTime(const City& c, int& offset)
{
    offset = c.offset;
    SYSTEMTIME st = UtcShifted(c.offset);
    if (InDst(c.dst, st, c.offset))
    {
        offset += 60;
        st = UtcShifted(offset);
    }
    return st;
}

int LocalOffset()
{
    TIME_ZONE_INFORMATION tzi{};
    const DWORD r = GetTimeZoneInformation(&tzi);
    LONG bias = tzi.Bias;
    if (r == TIME_ZONE_ID_DAYLIGHT)
        bias += tzi.DaylightBias;
    else if (r == TIME_ZONE_ID_STANDARD)
        bias += tzi.StandardBias;
    return static_cast<int>(-bias);
}

std::wstring OffsetText(int minutes)
{
    const int a = minutes < 0 ? -minutes : minutes;
    return std::wstring(L"UTC") + (minutes < 0 ? L"−" : L"+") + Pad2(a / 60) + L":" + Pad2(a % 60);
}

std::wstring HhMm(const SYSTEMTIME& t) { return Pad2(t.wHour) + L":" + Pad2(t.wMinute); }

long long DateKey(const SYSTEMTIME& t) { return (static_cast<long long>(t.wYear) * 13 + t.wMonth) * 32 + t.wDay; }

// «Сегодня, на 3 ч позже»
std::wstring RelativeText(const SYSTEMTIME& city, int cityOff, const SYSTEMTIME& local, int localOff)
{
    const long long a = DateKey(city), b = DateKey(local);
    std::wstring day = a == b ? L"Сегодня" : (a > b ? L"Завтра" : L"Вчера");
    const int diff = cityOff - localOff;
    if (diff == 0)
        return day + L", то же время";
    const int ad = diff < 0 ? -diff : diff;
    std::wstring amount = std::to_wstring(ad / 60) + L" ч";
    if (ad % 60)
        amount += L" " + std::to_wstring(ad % 60) + L" мин";
    if (ad < 60)
        amount = std::to_wstring(ad) + L" мин";
    return day + L", на " + amount + (diff > 0 ? L" позже" : L" раньше");
}

// ============================================================================ состояние
struct TimerItem
{
    std::wstring name;
    int total = 60;
    double left = 60.0;
    double endsAt = 0.0;
    bool running = false;
};

struct AlarmItem
{
    std::wstring name = L"Будильник";
    int hour = 7;
    int minute = 0;
    unsigned days = 0; // бит 0 — понедельник … бит 6 — воскресенье; 0 — однократно
    bool on = true;
    int sound = 0;
    int snooze = 2;
    long long lastKey = -1;
};

struct ClockState
{
    bool init = false;
    int lastPage = 0;
    std::vector<TimerItem> timers;
    std::vector<AlarmItem> alarms;
    int timerSerial = 0;
    int alarmSerial = 0;
    // Секундомер
    bool swRunning = false;
    double swStart = 0.0;
    double swAccum = 0.0;
    std::vector<double> laps; // время с начала на момент каждого круга
    // Мировое время
    std::vector<int> cities;
    // Фокусировка
    int focusMinutes = 30;
    bool skipBreaks = false;
    bool focusOn = false;
    bool focusPaused = false;
    double focusEnds = 0.0;
    double focusLeft = 0.0;
    int focusTotal = 30;
    bool focusSetDnd = false;
    int goalMinutes = 60;
    int doneToday = 0;
    int doneYesterday = 0;
    int streak = 0;
    long long doneDay = -1;
    int defaultSound = 0;
};

ClockState& S()
{
    static ClockState s;
    if (!s.init)
    {
        s.init = true;
        for (int m : {1, 3, 5, 10})
        {
            TimerItem t;
            t.name = std::to_wstring(m) + L" мин";
            t.total = m * 60;
            t.left = t.total;
            s.timers.push_back(t);
        }
        AlarmItem a;
        a.name = L"Доброе утро";
        a.days = 0x1F;
        a.on = false;
        s.alarms.push_back(a);
        s.cities = {CityIndex(L"london"), CityIndex(L"new york"), CityIndex(L"tokyo")};
        s.doneDay = DateKey(LocalNow());
    }
    return s;
}

double TimerLeft(const TimerItem& t) { return t.running ? std::max(0.0, t.endsAt - NowSeconds()) : t.left; }
double StopwatchTime(const ClockState& s) { return s.swAccum + (s.swRunning ? NowSeconds() - s.swStart : 0.0); }
double FocusLeft(const ClockState& s) { return s.focusPaused ? s.focusLeft : std::max(0.0, s.focusEnds - NowSeconds()); }

int BreakCount(int minutes, bool skip) { return skip || minutes <= 30 ? 0 : (minutes + 29) / 30 - 1; }

std::wstring Hms(double secs)
{
    const long long t = static_cast<long long>(std::ceil(std::max(0.0, secs) - 1e-6));
    return Pad2(static_cast<int>(t / 3600)) + L":" + Pad2(static_cast<int>(t / 60 % 60)) + L":" + Pad2(static_cast<int>(t % 60));
}

std::wstring StopwatchText(double secs)
{
    const long long cs = static_cast<long long>(std::max(0.0, secs) * 100.0);
    return Pad2(static_cast<int>(cs / 360000)) + L":" + Pad2(static_cast<int>(cs / 6000 % 60)) + L":" + Pad2(static_cast<int>(cs / 100 % 60)) +
           L"," + Pad2(static_cast<int>(cs % 100));
}

std::wstring LapText(double secs)
{
    const long long cs = static_cast<long long>(std::max(0.0, secs) * 100.0);
    std::wstring out;
    if (cs >= 360000)
        out = Pad2(static_cast<int>(cs / 360000)) + L":";
    return out + Pad2(static_cast<int>(cs / 6000 % 60)) + L":" + Pad2(static_cast<int>(cs / 100 % 60)) + L"," + Pad2(static_cast<int>(cs % 100));
}

// Минут до следующего срабатывания будильника.
int MinutesUntil(const AlarmItem& a)
{
    const SYSTEMTIME now = LocalNow();
    const int nowMin = now.wHour * 60 + now.wMinute;
    const int dow = (now.wDayOfWeek + 6) % 7; // 0 — понедельник
    for (int d = 0; d < 8; ++d)
    {
        const int day = (dow + d) % 7;
        if (a.days != 0 && !((a.days >> day) & 1u))
            continue;
        const int m = d * 1440 + a.hour * 60 + a.minute - nowMin;
        if (m > 0)
            return m;
    }
    return 7 * 1440;
}

std::wstring UntilText(int minutes)
{
    const int d = minutes / 1440, h = minutes % 1440 / 60, m = minutes % 60;
    std::wstring s = L"через";
    if (d > 0)
        s += L" " + Plural(d, L"день", L"дня", L"дней");
    if (h > 0)
        s += L" " + std::to_wstring(h) + L" ч";
    if (m > 0 && d == 0)
        s += L" " + std::to_wstring(m) + L" мин";
    return s;
}

void Ring()
{
    MessageBeep(MB_ICONASTERISK);
}

void UpdateDay(ClockState& s)
{
    const long long today = DateKey(LocalNow());
    if (s.doneDay == today)
        return;
    s.streak = s.doneToday >= s.goalMinutes ? s.streak + 1 : 0;
    s.doneYesterday = s.doneToday;
    s.doneToday = 0;
    s.doneDay = today;
}

void FinishFocus(ClockState& s, bool completed)
{
    const int done = completed ? s.focusTotal : static_cast<int>((s.focusTotal * 60 - FocusLeft(s)) / 60.0);
    s.focusOn = false;
    s.focusPaused = false;
    UpdateDay(s);
    s.doneToday += std::max(0, done);
    if (s.focusSetDnd)
        GetShell().Settings().focus = false;
    s.focusSetDnd = false;
    if (completed)
    {
        GetShell().Notify(AppIcon::Clock, L"Часы", L"Сеанс фокусировки завершён",
                          L"Отличная работа! Вы были сосредоточены " + DurationText(s.focusTotal) + L".");
        Ring();
    }
}

std::wstring NextName(const wchar_t* base, int serial) { return std::wstring(base) + L" (" + std::to_wstring(serial) + L")"; }

// ============================================================================ окно
enum class Page
{
    Focus,
    Timer,
    Alarm,
    Stopwatch,
    World,
    Settings,
};

enum Hk : int
{
    kHkNone = 0,
    kHkHamburger,
    kHkNav,
    kHkCmdEdit,
    kHkCmdAdd,
    kHkTimerPlay,
    kHkTimerReset,
    kHkTimerExpand,
    kHkTimerDelete,
    kHkTimerCard,
    kHkTimerCollapse,
    kHkAlarmToggle,
    kHkAlarmDelete,
    kHkAlarmCard,
    kHkSwPlay,
    kHkSwLap,
    kHkSwReset,
    kHkCityDelete,
    kHkFocusUp,
    kHkFocusDown,
    kHkFocusSkip,
    kHkFocusStart,
    kHkFocusPause,
    kHkFocusStop,
    kHkFocusGoal,
    kHkSetNotify,
    kHkSetSound,
    kHkDlgUp,
    kHkDlgDown,
    kHkDlgCol,
    kHkDlgRepeat,
    kHkDlgDay,
    kHkDlgSound,
    kHkDlgSoundPlay,
    kHkDlgSnooze,
    kHkDlgDelete,
    kHkDlgCity,
};

struct Hot
{
    Rect r;
    Rect clip;
    int kind = 0;
    int index = 0;
    std::wstring tip;
};

enum class Dlg
{
    None,
    Timer,
    Alarm,
    City,
};

struct NavDef
{
    Page page;
    Icon icon;
    const wchar_t* text;
};
const NavDef kNav[] = {{Page::Focus, Icon::Focus, L"Фокусировка"},
                       {Page::Timer, Icon::Timer, L"Таймер"},
                       {Page::Alarm, Icon::Alarm, L"Будильник"},
                       {Page::Stopwatch, Icon::Stopwatch, L"Секундомер"},
                       {Page::World, Icon::WorldClock, L"Мировое время"}};

char g_tipIds[512];

class ClockWindow : public Window
{
public:
    explicit ClockWindow(const std::wstring& arg) : Window(AppId::Clock, L"Часы")
    {
        SetInitialSize(1060.0f, 720.0f);
        m_minSize = {520.0f, 520.0f};
        m_page = static_cast<Page>(S().lastPage);
        OnLaunchArgs(arg);
        m_nameBox = m_ui.Add<TextBox>();
        m_nameBox->trailingIcon = Icon::Edit;
        m_nameBox->SetVisible(false);
        m_nameBox->onEnter = [this]() { SaveDialog(); };
        m_nameBox->onEscape = [this]() { CloseDialog(); };
        m_searchBox = m_ui.Add<TextBox>(L"Введите название города");
        m_searchBox->leadingIcon = Icon::Search;
        m_searchBox->SetVisible(false);
        m_searchBox->onChange = [this](const std::wstring&) { FilterCities(); };
        m_searchBox->onEnter = [this]() { SaveDialog(); };
        m_searchBox->onEscape = [this]() { CloseDialog(); };
        m_saveBtn = m_ui.Add<Button>(L"Сохранить", Icon::None, ButtonStyle::Accent);
        m_saveBtn->SetVisible(false);
        m_saveBtn->onClick = [this]() { SaveDialog(); };
        m_cancelBtn = m_ui.Add<Button>(L"Отмена", Icon::None, ButtonStyle::Standard);
        m_cancelBtn->SetVisible(false);
        m_cancelBtn->onClick = [this]() { CloseDialog(); };
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnLaunchArgs(const std::wstring& arg) override
    {
        const std::wstring a = ToLower(arg);
        if (a == L"focus")
            SetPage(Page::Focus);
        else if (a == L"timer")
            SetPage(Page::Timer);
        else if (a == L"alarm")
            SetPage(Page::Alarm);
        else if (a == L"stopwatch")
            SetPage(Page::Stopwatch);
        else if (a == L"world")
            SetPage(Page::World);
    }

    void OnResize() override
    {
        if (m_dlg != Dlg::None)
            LayoutDialog();
    }

private:
    // ================================================================== общее
    bool Wide() const { return Width() >= 860.0f; }
    float NavW() const { return Wide() && m_navPref ? kNavFull : kNavCompact; }
    Rect ContentRect() const { return {NavW(), kTitleH, Width() - NavW(), Height() - kTitleH}; }

    void SetPage(Page pg)
    {
        if (m_page != pg)
        {
            m_scroll = 0.0f;
            m_edit = false;
            m_expanded = -1;
        }
        m_page = pg;
        m_navOverlay = false;
        S().lastPage = static_cast<int>(pg);
    }

    void AddHot(const Rect& rc, int kind, int index = 0, std::wstring tip = {})
    {
        m_hots.push_back({rc, m_hotClip, kind, index, std::move(tip)});
    }
    const Hot* HitHot(Point p) const
    {
        for (auto it = m_hots.rbegin(); it != m_hots.rend(); ++it)
        {
            if (it->r.Contains(p) && it->clip.Contains(p))
                return &*it;
        }
        return nullptr;
    }
    bool IsHot(int kind, int index = 0) const { return m_hoverKind == kind && m_hoverIndex == index; }
    bool IsPressed(int kind, int index = 0) const { return IsHot(kind, index) && m_pressKind == kind && m_pressIndex == index; }

    // ================================================================== элементы
    void RoundButton(Renderer& r, const Rect& rc, Icon icon, bool accent, bool enabled, int kind, int index, const wchar_t* tip)
    {
        const Palette& p = Theme::P();
        const bool hot = enabled && IsHot(kind, index), pressed = enabled && IsPressed(kind, index);
        const float cx = rc.CenterX(), cy = rc.CenterY(), rad = rc.w * 0.5f;
        const float gs = rc.w >= 48.0f ? 18.0f : 16.0f;
        const Rect gr{cx - gs * 0.5f, cy - gs * 0.5f, gs, gs};
        if (accent)
        {
            r.FillCircle(cx, cy, rad, !enabled ? p.accentDisabled : (pressed ? p.accentPressed : (hot ? p.accentHover : p.accent)));
            r.Glyph(icon, gr, p.textOnAccent);
        }
        else
        {
            r.FillCircle(cx, cy, rad, !enabled ? p.controlDisabled : (pressed ? p.controlPressed : (hot ? p.controlHover : p.control)));
            r.StrokeCircle(cx, cy, rad - 0.5f, p.controlStroke);
            r.Glyph(icon, gr, enabled ? (pressed ? p.textSecondary : p.text) : p.textDisabled);
        }
        if (enabled)
            AddHot(rc, kind, index, tip);
    }

    void SubtleButton(Renderer& r, const Rect& rc, Icon icon, int kind, int index, const wchar_t* tip, bool active = false)
    {
        const Palette& p = Theme::P();
        if (IsPressed(kind, index))
            r.FillRoundRect(rc, 4.0f, p.subtlePressed);
        else if (IsHot(kind, index) || active)
            r.FillRoundRect(rc, 4.0f, p.subtleHover);
        r.Glyph(icon, {rc.CenterX() - 8.0f, rc.CenterY() - 8.0f, 16.0f, 16.0f}, IsPressed(kind, index) ? p.textSecondary : p.text);
        AddHot(rc, kind, index, tip);
    }

    void TextButton(Renderer& r, const Rect& rc, const std::wstring& text, Icon icon, bool accent, int kind, int index = 0)
    {
        const Palette& p = Theme::P();
        const bool hot = IsHot(kind, index), pressed = IsPressed(kind, index);
        if (accent)
            r.FillRoundRect(rc, 4.0f, pressed ? p.accentPressed : (hot ? p.accentHover : p.accent));
        else
        {
            r.FillRoundRect(rc, 4.0f, pressed ? p.controlPressed : (hot ? p.controlHover : p.control));
            r.StrokeRoundRect(rc, 4.0f, p.controlStroke);
        }
        const Color fg = accent ? p.textOnAccent : p.text;
        const float tw = r.TextWidth(text, 14.0f);
        float x = rc.CenterX() - tw * 0.5f;
        if (icon != Icon::None)
        {
            x += 12.0f;
            r.Glyph(icon, {x - 26.0f, rc.CenterY() - 7.0f, 14.0f, 14.0f}, fg);
        }
        r.Text(text, {x, rc.y, tw + 4.0f, rc.h}, fg, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        AddHot(rc, kind, index);
    }

    void ToggleDraw(Renderer& r, const Rect& rc, bool on, bool hot)
    {
        const Palette& p = Theme::P();
        const float rad = rc.h * 0.5f, knob = hot ? 7.0f : 6.0f;
        if (on)
        {
            r.FillRoundRect(rc, rad, hot ? p.accentHover : p.accent);
            r.FillCircle(rc.Right() - rad, rc.CenterY(), knob, p.textOnAccent);
        }
        else
        {
            r.FillRoundRect(rc, rad, hot ? p.controlHover : p.controlAltFill);
            r.StrokeRoundRect(rc, rad, p.controlStrongStroke);
            r.FillCircle(rc.x + rad, rc.CenterY(), knob, p.textSecondary);
        }
    }

    void CheckDraw(Renderer& r, float x, float y, bool checked, const std::wstring& text, int kind, bool enabled = true)
    {
        const Palette& p = Theme::P();
        const Rect box{x, y, 20.0f, 20.0f};
        const bool hot = enabled && IsHot(kind);
        if (checked)
        {
            r.FillRoundRect(box, 4.0f, !enabled ? p.accentDisabled : (hot ? p.accentHover : p.accent));
            r.Glyph(Icon::Check, {x + 4.0f, y + 4.0f, 12.0f, 12.0f}, p.textOnAccent);
        }
        else
        {
            r.FillRoundRect(box, 4.0f, hot ? p.controlHover : p.controlAltFill);
            r.StrokeRoundRect(box, 4.0f, enabled ? p.controlStrongStroke : p.controlStroke);
        }
        const float tw = r.TextWidth(text, 14.0f);
        r.Text(text, {x + 30.0f, y - 2.0f, tw + 4.0f, 24.0f}, enabled ? p.text : p.textDisabled, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        if (enabled)
            AddHot({x - 2.0f, y - 4.0f, tw + 36.0f, 28.0f}, kind);
    }

    void ProgressRing(Renderer& r, float cx, float cy, float rad, float frac, float width)
    {
        const Palette& p = Theme::P();
        r.Arc(cx, cy, rad, 0.0f, 360.0f, p.dark ? Color(255, 255, 255, 0x26) : Color(0, 0, 0, 0x16), width);
        if (frac > 0.0005f)
            r.Arc(cx, cy, rad, -90.0f, 360.0f * Saturate(frac), p.accent, width);
    }

    void Card(Renderer& r, const Rect& rc, bool hot = false)
    {
        const Palette& p = Theme::P();
        r.FillRoundRect(rc, 8.0f, hot ? (p.dark ? Color(255, 255, 255, 0x12) : Color(0xF9, 0xF9, 0xF9)) : p.card);
        r.StrokeRoundRect(rc, 8.0f, p.cardStroke);
    }

    struct Grid
    {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float w = kCardW;
        float h = 0.0f;
        int cols = 1;
        Rect Cell(int i) const
        {
            return {x0 + static_cast<float>(i % cols) * (w + kGap), y0 + static_cast<float>(i / cols) * (h + kGap), w, h};
        }
        float Bottom(int count) const
        {
            const int rows = std::max(1, (count + cols - 1) / cols);
            return y0 + static_cast<float>(rows) * (h + kGap) - kGap;
        }
    };

    Grid MakeGrid(const Rect& c, float top, float cardH) const
    {
        Grid g;
        const float avail = c.w - 2.0f * kPad;
        g.cols = std::max(1, static_cast<int>((avail + kGap) / (kCardW + kGap)));
        g.w = std::min(kCardW, avail);
        g.h = cardH;
        g.x0 = c.x + kPad;
        g.y0 = top;
        return g;
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        UpdateDay(S());
        m_hots.clear();
        const Rect full{0.0f, 0.0f, Width(), Height()};
        m_hotClip = full;
        r.DrawAppIcon(AppIcon::Clock, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(L"Часы", {48.0f, 0.0f, 200.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);

        const Rect c = ContentRect();
        {
            Gdiplus::GraphicsPath path;
            Renderer::RoundRectPath(path, {c.x, c.y, c.w + 12.0f, c.h + 12.0f}, 8.0f, 0.0f, 0.0f, 0.0f);
            r.FillPath(path, p.layer);
            r.StrokePath(path, p.cardStroke);
        }
        r.PushClip(c);
        m_hotClip = c;
        m_contentH = 0.0f;
        switch (m_page)
        {
        case Page::Focus:
            DrawFocus(r, c);
            break;
        case Page::Timer:
            DrawTimers(r, c);
            break;
        case Page::Alarm:
            DrawAlarms(r, c);
            break;
        case Page::Stopwatch:
            DrawStopwatch(r, c);
            break;
        case Page::World:
            DrawWorld(r, c);
            break;
        case Page::Settings:
            DrawSettings(r, c);
            break;
        }
        DrawScrollbar(r, c);
        DrawCommandBar(r, c);
        r.PopClip();
        m_hotClip = full;
        DrawNav(r);
        if (m_dlg != Dlg::None)
        {
            m_hots.clear();
            DrawDialog(r);
        }
        m_ui.Draw(r);
        if (NeedsFrames())
            RequestFrame();
    }

    bool NeedsFrames() const
    {
        const ClockState& s = S();
        if (m_page == Page::Stopwatch && s.swRunning)
            return true;
        if (m_page == Page::Focus && s.focusOn && !s.focusPaused)
            return true;
        if (m_page == Page::Timer)
        {
            for (const TimerItem& t : s.timers)
            {
                if (t.running)
                    return true;
            }
        }
        return false;
    }

    float MaxScroll() const { return std::max(0.0f, m_contentH - ContentRect().h); }

    void DrawScrollbar(Renderer& r, const Rect& c)
    {
        const float maxS = MaxScroll();
        if (maxS <= 0.0f)
            return;
        const float th = std::max(40.0f, c.h * c.h / m_contentH);
        const float ty = c.y + 4.0f + (c.h - 8.0f - th) * (m_scroll / maxS);
        r.FillRoundRect({c.Right() - 6.0f, ty, 3.0f, th}, 1.5f, Theme::P().scrollThumb);
    }

    // ---- навигация -------------------------------------------------------------
    void DrawNav(Renderer& r)
    {
        const Palette& p = Theme::P();
        const bool overlay = m_navOverlay && !Wide();
        const bool expanded = overlay || (Wide() && m_navPref);
        const float w = expanded ? kNavFull : kNavCompact;
        if (overlay)
        {
            m_hots.clear(); // пока панель открыта поверх, страница недоступна
            const Rect pane{0.0f, kTitleH, kNavFull, Height() - kTitleH};
            r.Shadow(pane, 0.0f, 24.0f, 0.0f, p.dark ? 0.6f : 0.25f);
            r.Acrylic(pane, 0.0f, p.acrylicTint, p.acrylicAmount);
            r.FillRect({pane.Right() - 1.0f, pane.y, 1.0f, pane.h}, p.flyoutStroke);
        }
        SubtleButton(r, {4.0f, kTitleH, 40.0f, 36.0f}, Icon::Hamburger, kHkHamburger, 0, expanded ? L"Закрыть навигацию" : L"Открыть навигацию");
        const int count = static_cast<int>(sizeof(kNav) / sizeof(kNav[0]));
        for (int i = 0; i <= count; ++i)
        {
            const bool settings = i == count;
            const Page pg = settings ? Page::Settings : kNav[i].page;
            const Icon icon = settings ? Icon::Settings : kNav[i].icon;
            const wchar_t* text = settings ? L"Параметры" : kNav[i].text;
            const Rect rr = settings ? Rect{4.0f, Height() - 44.0f, w - 8.0f, 36.0f} : Rect{4.0f, kTitleH + 44.0f + static_cast<float>(i) * 40.0f, w - 8.0f, 36.0f};
            const bool sel = m_page == pg;
            if (IsPressed(kHkNav, i))
                r.FillRoundRect(rr, 4.0f, p.subtlePressed);
            else if (sel || IsHot(kHkNav, i))
                r.FillRoundRect(rr, 4.0f, sel && IsHot(kHkNav, i) ? p.subtlePressed : p.subtleHover);
            if (sel)
                r.FillRoundRect({rr.x, rr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Glyph(icon, {rr.x + 12.0f, rr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            if (expanded)
                r.Text(text, {rr.x + 44.0f, rr.y, rr.w - 50.0f, rr.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            AddHot(rr, kHkNav, i, expanded ? L"" : text);
        }
    }

    // ---- нижняя панель команд ------------------------------------------------------
    void DrawCommandBar(Renderer& r, const Rect& c)
    {
        const bool timers = m_page == Page::Timer && m_expanded < 0;
        if (!timers && m_page != Page::Alarm && m_page != Page::World)
            return;
        const Palette& p = Theme::P();
        const Rect bar{c.Right() - 24.0f - 92.0f, c.Bottom() - 24.0f - 48.0f, 92.0f, 48.0f};
        r.Shadow(bar, 8.0f, 16.0f, 4.0f, p.dark ? 0.4f : 0.18f);
        r.FillRoundRect(bar, 8.0f, p.dark ? Color::Hex(0x2C2C2C) : Color::Hex(0xFBFBFB));
        r.StrokeRoundRect(bar, 8.0f, p.surfaceStroke);
        const wchar_t* editTip = m_page == Page::Timer ? L"Изменить таймеры" : (m_page == Page::Alarm ? L"Изменить будильники" : L"Изменить города");
        const wchar_t* addTip = m_page == Page::Timer ? L"Добавить новый таймер" : (m_page == Page::Alarm ? L"Добавить новый будильник" : L"Добавить новый город");
        SubtleButton(r, {bar.x + 6.0f, bar.y + 6.0f, 36.0f, 36.0f}, m_edit ? Icon::Check : Icon::Edit, kHkCmdEdit, 0, m_edit ? L"Готово" : editTip, m_edit);
        SubtleButton(r, {bar.x + 50.0f, bar.y + 6.0f, 36.0f, 36.0f}, Icon::Add, kHkCmdAdd, 0, addTip);
    }

    // ---- фокусировка -------------------------------------------------------------
    void DrawFocus(Renderer& r, const Rect& c)
    {
        const float top = c.y + 24.0f - m_scroll;
        const float avail = c.w - 2.0f * kPad;
        Rect a, b;
        if (c.w >= 900.0f)
        {
            a = {c.x + kPad, top, std::floor(avail * 0.56f), 390.0f};
            b = {a.Right() + kGap, top, avail - a.w - kGap, 390.0f};
        }
        else
        {
            a = {c.x + kPad, top, avail, 390.0f};
            b = {c.x + kPad, a.Bottom() + kGap, avail, 330.0f};
        }
        DrawFocusCard(r, a);
        DrawProgressCard(r, b);
        m_contentH = b.Bottom() + m_scroll - c.y + 24.0f;
    }

    void DrawFocusCard(Renderer& r, const Rect& a)
    {
        const Palette& p = Theme::P();
        ClockState& s = S();
        Card(r, a);
        const float cx = a.CenterX();
        if (!s.focusOn)
        {
            r.Text(L"Начало работы", {a.x + 24.0f, a.y + 18.0f, a.w - 48.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(L"На время сеанса включится режим «Не беспокоить», а уведомления будут скрыты.", {a.x + 24.0f, a.y + 52.0f, a.w - 48.0f, 40.0f},
                   p.textSecondary, 13.0f, FontWeight::Regular, TextFlags::Wrap);
            const Rect box{cx - 84.0f, a.y + 106.0f, 168.0f, 112.0f};
            r.FillRoundRect(box, 8.0f, p.control);
            r.StrokeRoundRect(box, 8.0f, p.controlStroke);
            r.Text(std::to_wstring(s.focusMinutes), {box.x + 12.0f, box.y + 10.0f, box.w - 56.0f, 64.0f}, p.text, 44.0f, FontWeight::Semibold,
                   TextFlags::Middle);
            r.Text(L"мин", {box.x + 12.0f, box.y + 72.0f, box.w - 56.0f, 24.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
            SubtleButton(r, {box.Right() - 42.0f, box.y + 10.0f, 32.0f, 42.0f}, Icon::ChevronUp, kHkFocusUp, 0, L"Увеличить время");
            SubtleButton(r, {box.Right() - 42.0f, box.Bottom() - 52.0f, 32.0f, 42.0f}, Icon::ChevronDown, kHkFocusDown, 0, L"Уменьшить время");
            const int breaks = BreakCount(s.focusMinutes, s.skipBreaks);
            const std::wstring info = breaks == 0 ? std::wstring(L"Перерывов не будет")
                                                  : L"Вы получите " + Plural(breaks, L"перерыв", L"перерыва", L"перерывов");
            r.Text(info, {a.x, a.y + 232.0f, a.w, 22.0f}, p.textSecondary, 13.0f, FontWeight::Regular, TextFlags::Middle);
            const std::wstring skip = L"Пропустить перерывы";
            const float sw = r.TextWidth(skip, 14.0f) + 30.0f;
            CheckDraw(r, cx - sw * 0.5f, a.y + 268.0f, s.skipBreaks, skip, kHkFocusSkip, s.focusMinutes > 30);
            TextButton(r, {cx - 130.0f, a.y + 318.0f, 260.0f, 36.0f}, L"Начать сеанс фокусировки", Icon::Play, true, kHkFocusStart);
            return;
        }
        const double left = FocusLeft(s);
        r.Text(s.focusPaused ? L"Сеанс приостановлен" : L"Сеанс фокусировки", {a.x + 24.0f, a.y + 18.0f, a.w - 48.0f, 30.0f}, p.text, 20.0f,
               FontWeight::Semibold, TextFlags::VCenter);
        const float rad = 92.0f, cy = a.y + 178.0f;
        ProgressRing(r, cx, cy, rad, static_cast<float>(left / (s.focusTotal * 60.0)), 8.0f);
        const long long secs = static_cast<long long>(std::ceil(left - 1e-6));
        const std::wstring t = secs >= 3600 ? Hms(left) : Pad2(static_cast<int>(secs / 60)) + L":" + Pad2(static_cast<int>(secs % 60));
        r.Text(t, {cx - rad, cy - 30.0f, rad * 2.0f, 48.0f}, p.text, 36.0f, FontWeight::Semibold, TextFlags::Middle);
        r.Text(L"осталось", {cx - rad, cy + 20.0f, rad * 2.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Middle);
        RoundButton(r, {cx - 52.0f, a.y + 312.0f, 44.0f, 44.0f}, s.focusPaused ? Icon::Play : Icon::Pause, true, true, kHkFocusPause, 0,
                    s.focusPaused ? L"Продолжить" : L"Пауза");
        RoundButton(r, {cx + 8.0f, a.y + 312.0f, 44.0f, 44.0f}, Icon::Stop, false, true, kHkFocusStop, 0, L"Завершить сеанс");
    }

    void DrawProgressCard(Renderer& r, const Rect& b)
    {
        const Palette& p = Theme::P();
        const ClockState& s = S();
        Card(r, b);
        r.Text(L"Ежедневный прогресс", {b.x + 24.0f, b.y + 16.0f, b.w - 80.0f, 28.0f}, p.text, 16.0f, FontWeight::Semibold, TextFlags::VCenter);
        m_goalAnchor = {b.Right() - 48.0f, b.y + 12.0f, 36.0f, 36.0f};
        SubtleButton(r, m_goalAnchor, Icon::Edit, kHkFocusGoal, 0, L"Изменить ежедневную цель");
        const float cx = b.CenterX(), cy = b.y + 160.0f, rad = 66.0f;
        ProgressRing(r, cx, cy, rad, static_cast<float>(s.doneToday) / static_cast<float>(std::max(1, s.goalMinutes)), 8.0f);
        r.Text(L"Ежедневная цель", {cx - rad, cy - 30.0f, rad * 2.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Middle);
        r.Text(DurationText(s.goalMinutes), {cx - rad, cy - 12.0f, rad * 2.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::Middle);
        r.Text(L"Выполнено: " + Plural(s.doneToday, L"минута", L"минуты", L"минут"), {b.x, cy + rad + 14.0f, b.w, 20.0f}, p.textSecondary, 12.0f,
               FontWeight::Regular, TextFlags::Middle);
        auto stat = [&](float x, float y, float w, const wchar_t* label, const std::wstring& value) {
            r.Text(label, {x, y, w, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Middle);
            r.Text(value, {x, y + 20.0f, w, 28.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::Middle);
        };
        const std::wstring yesterday = Plural(s.doneYesterday, L"минута", L"минуты", L"минут");
        const std::wstring streak = Plural(s.streak, L"день", L"дня", L"дней");
        if (b.w >= 520.0f)
        {
            stat(b.x + 16.0f, cy - 24.0f, cx - rad - b.x - 32.0f, L"Вчера", yesterday);
            stat(cx + rad + 16.0f, cy - 24.0f, b.Right() - cx - rad - 32.0f, L"Серия", streak);
        }
        else
        {
            const float y = cy + rad + 44.0f;
            stat(b.x, y, b.w * 0.5f, L"Вчера", yesterday);
            stat(b.x + b.w * 0.5f, y, b.w * 0.5f, L"Серия", streak);
        }
    }

    // ---- таймеры ---------------------------------------------------------------
    void DrawTimers(Renderer& r, const Rect& c)
    {
        ClockState& s = S();
        if (m_expanded >= static_cast<int>(s.timers.size()))
            m_expanded = -1;
        if (m_expanded >= 0)
        {
            DrawExpandedTimer(r, c, m_expanded);
            return;
        }
        const Grid g = MakeGrid(c, c.y + 24.0f - m_scroll, 330.0f);
        for (int i = 0; i < static_cast<int>(s.timers.size()); ++i)
            DrawTimerCard(r, g.Cell(i), i);
        if (s.timers.empty())
            r.Text(L"Таймеров пока нет. Нажмите «+», чтобы добавить таймер.", {c.x, c.y + 80.0f, c.w, 24.0f}, Theme::P().textSecondary, 14.0f,
                   FontWeight::Regular, TextFlags::Middle);
        m_contentH = g.Bottom(static_cast<int>(s.timers.size())) + m_scroll - c.y + 96.0f;
    }

    void DrawTimerCard(Renderer& r, const Rect& rc, int i)
    {
        const Palette& p = Theme::P();
        TimerItem& t = S().timers[static_cast<size_t>(i)];
        const double left = TimerLeft(t);
        Card(r, rc, m_edit && IsHot(kHkTimerCard, i));
        if (m_edit)
            AddHot(rc, kHkTimerCard, i);
        r.Text(t.name, {rc.x + 20.0f, rc.y + 12.0f, rc.w - 76.0f, 28.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        const Rect tb{rc.Right() - 46.0f, rc.y + 10.0f, 34.0f, 32.0f};
        if (m_edit)
            SubtleButton(r, tb, Icon::Delete, kHkTimerDelete, i, L"Удалить таймер");
        else
            SubtleButton(r, tb, Icon::Maximize, kHkTimerExpand, i, L"Развернуть");
        const float cx = rc.CenterX(), cy = rc.y + 152.0f, rad = std::min(96.0f, rc.w * 0.5f - 40.0f);
        ProgressRing(r, cx, cy, rad, t.total > 0 ? static_cast<float>(left / t.total) : 0.0f, 6.0f);
        r.Text(Hms(left), {cx - rad, cy - 26.0f, rad * 2.0f, 52.0f}, m_edit ? p.textSecondary : p.text, 32.0f, FontWeight::Semibold, TextFlags::Middle);
        const float by = rc.Bottom() - 62.0f;
        RoundButton(r, {cx - 50.0f, by, 40.0f, 40.0f}, t.running ? Icon::Pause : Icon::Play, true, !m_edit, kHkTimerPlay, i,
                    t.running ? L"Пауза" : L"Запустить");
        RoundButton(r, {cx + 10.0f, by, 40.0f, 40.0f}, Icon::Refresh, false, !m_edit && (t.running || left < t.total - 0.01), kHkTimerReset, i,
                    L"Сбросить");
    }

    void DrawExpandedTimer(Renderer& r, const Rect& c, int i)
    {
        const Palette& p = Theme::P();
        TimerItem& t = S().timers[static_cast<size_t>(i)];
        const double left = TimerLeft(t);
        r.Text(t.name, {c.x, c.y + 24.0f, c.w, 36.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::Middle);
        SubtleButton(r, {c.Right() - 60.0f, c.y + 20.0f, 40.0f, 40.0f}, Icon::Restore, kHkTimerCollapse, i, L"Свернуть");
        const float rad = std::max(80.0f, std::min(c.w * 0.36f, (c.h - 220.0f) * 0.5f));
        const float cx = c.CenterX(), cy = c.y + 80.0f + rad;
        ProgressRing(r, cx, cy, rad, t.total > 0 ? static_cast<float>(left / t.total) : 0.0f, 10.0f);
        const float fs = Clamp(rad * 0.3f, 28.0f, 96.0f);
        r.Text(Hms(left), {cx - rad, cy - fs, rad * 2.0f, fs * 2.0f}, p.text, fs, FontWeight::Semibold, TextFlags::Middle);
        const float by = cy + rad + 32.0f;
        RoundButton(r, {cx - 60.0f, by, 48.0f, 48.0f}, t.running ? Icon::Pause : Icon::Play, true, true, kHkTimerPlay, i, t.running ? L"Пауза" : L"Запустить");
        RoundButton(r, {cx + 12.0f, by, 48.0f, 48.0f}, Icon::Refresh, false, t.running || left < t.total - 0.01, kHkTimerReset, i, L"Сбросить");
    }

    // ---- будильники --------------------------------------------------------------
    void DrawAlarms(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        ClockState& s = S();
        const Rect info{c.x + kPad, c.y + 20.0f - m_scroll, c.w - 2.0f * kPad, 48.0f};
        r.FillRoundRect(info, 6.0f, p.dark ? Color(0x27, 0x27, 0x27) : Color(0xF6, 0xF6, 0xF6));
        r.StrokeRoundRect(info, 6.0f, p.cardStroke);
        r.FillCircle(info.x + 26.0f, info.CenterY(), 8.0f, p.accent);
        r.Text(L"i", {info.x + 18.0f, info.y, 16.0f, info.h}, p.textOnAccent, 12.0f, FontWeight::Bold, TextFlags::Middle);
        r.Text(L"Уведомления будут отображаться, только если компьютер находится в режиме бодрствования.",
               {info.x + 48.0f, info.y, info.w - 60.0f, info.h}, p.text, 13.0f, FontWeight::Regular, TextFlags::VCenter);
        const Grid g = MakeGrid(c, info.Bottom() + 20.0f, 172.0f);
        for (int i = 0; i < static_cast<int>(s.alarms.size()); ++i)
            DrawAlarmCard(r, g.Cell(i), i);
        if (s.alarms.empty())
            r.Text(L"Будильников пока нет. Нажмите «+», чтобы добавить будильник.", {c.x, info.Bottom() + 60.0f, c.w, 24.0f}, p.textSecondary, 14.0f,
                   FontWeight::Regular, TextFlags::Middle);
        m_contentH = g.Bottom(static_cast<int>(s.alarms.size())) + m_scroll - c.y + 96.0f;
    }

    void DrawAlarmCard(Renderer& r, const Rect& rc, int i)
    {
        const Palette& p = Theme::P();
        AlarmItem& a = S().alarms[static_cast<size_t>(i)];
        Card(r, rc, IsHot(kHkAlarmCard, i));
        AddHot(rc, kHkAlarmCard, i);
        const Color main = a.on ? p.text : p.textSecondary;
        r.Text(Pad2(a.hour) + L":" + Pad2(a.minute), {rc.x + 20.0f, rc.y + 12.0f, 200.0f, 58.0f}, main, 40.0f, FontWeight::Semibold, TextFlags::VCenter);
        if (m_edit)
            SubtleButton(r, {rc.Right() - 48.0f, rc.y + 20.0f, 34.0f, 32.0f}, Icon::Delete, kHkAlarmDelete, i, L"Удалить будильник");
        else
        {
            const Rect tg{rc.Right() - 64.0f, rc.y + 26.0f, 40.0f, 20.0f};
            ToggleDraw(r, tg, a.on, IsHot(kHkAlarmToggle, i));
            AddHot(tg.Inflated(6.0f), kHkAlarmToggle, i, a.on ? L"Выключить будильник" : L"Включить будильник");
        }
        if (a.on)
        {
            r.Glyph(Icon::Alarm, {rc.x + 20.0f, rc.y + 76.0f, 14.0f, 14.0f}, p.textSecondary);
            r.Text(UntilText(MinutesUntil(a)), {rc.x + 40.0f, rc.y + 72.0f, rc.w - 60.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter);
        }
        r.Text(a.name, {rc.x + 20.0f, rc.y + 98.0f, rc.w - 40.0f, 24.0f}, main, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        if (a.days == 0)
            r.Text(L"Только один раз", {rc.x + 20.0f, rc.y + 130.0f, rc.w - 40.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter);
        else
        {
            for (int d = 0; d < 7; ++d)
            {
                const bool on = ((a.days >> d) & 1u) != 0;
                r.Text(kDayShort[d], {rc.x + 20.0f + static_cast<float>(d) * 34.0f, rc.y + 130.0f, 32.0f, 22.0f},
                       on ? (a.on ? p.accentText : p.text) : p.textTertiary, 12.0f, on ? FontWeight::Semibold : FontWeight::Regular,
                       TextFlags::VCenter);
            }
        }
    }

    // ---- секундомер -------------------------------------------------------------
    void DrawStopwatch(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        ClockState& s = S();
        const double t = StopwatchTime(s);
        const float size = Clamp(c.w / 8.0f, 40.0f, 88.0f);
        const std::wstring text = StopwatchText(t);
        const bool laps = !s.laps.empty();
        const float lineH = size * 1.35f;
        const float timeY = (laps ? c.y + 36.0f : c.y + std::max(36.0f, (c.h - lineH - 150.0f) * 0.42f)) - m_scroll;
        const float tw = r.TextWidth(text, size, FontWeight::Semibold);
        const float tx = std::floor(c.CenterX() - tw * 0.5f);
        r.Text(text, {tx - 4.0f, timeY, tw + 12.0f, lineH}, p.text, size, FontWeight::Semibold, TextFlags::VCenter | TextFlags::NoEllipsis);
        const float w2 = r.TextWidth(L"00", size, FontWeight::Semibold), wc = r.TextWidth(L"00:", size, FontWeight::Semibold);
        const wchar_t* labels[] = {L"ч", L"мин", L"с"};
        for (int k = 0; k < 3; ++k)
        {
            const float gx = tx + static_cast<float>(k) * wc + w2 * 0.5f;
            r.Text(labels[k], {gx - 40.0f, timeY + lineH - 4.0f, 80.0f, 20.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
        }
        const float by = timeY + lineH + 40.0f;
        const float cx = c.CenterX();
        RoundButton(r, {cx - 108.0f, by, 48.0f, 48.0f}, s.swRunning ? Icon::Pause : Icon::Play, true, true, kHkSwPlay, 0,
                    s.swRunning ? L"Пауза (пробел)" : L"Запустить (пробел)");
        RoundButton(r, {cx - 24.0f, by, 48.0f, 48.0f}, Icon::Flag, false, s.swRunning, kHkSwLap, 0, L"Круг (L)");
        RoundButton(r, {cx + 60.0f, by, 48.0f, 48.0f}, Icon::Refresh, false, t > 0.0, kHkSwReset, 0, L"Сбросить (R)");
        float y = by + 88.0f;
        if (laps)
        {
            const float lw = std::min(600.0f, c.w - 2.0f * kPad), lx = std::floor(cx - lw * 0.5f);
            const float c1 = lx + 16.0f, c2 = lx + lw * 0.36f, c3 = lx + lw * 0.68f;
            r.Text(L"Круги", {c1, y, 120.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(L"Время", {c2, y, 120.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(L"Всего", {c3, y, 120.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 28.0f;
            r.FillRect({lx, y, lw, 1.0f}, p.divider);
            // Самый быстрый и самый медленный круги, как в «Часах».
            int fast = -1, slow = -1;
            if (s.laps.size() >= 2)
            {
                double fv = 1e18, sv = -1.0;
                for (size_t i = 0; i < s.laps.size(); ++i)
                {
                    const double d = s.laps[i] - (i ? s.laps[i - 1] : 0.0);
                    if (d < fv)
                    {
                        fv = d;
                        fast = static_cast<int>(i);
                    }
                    if (d > sv)
                    {
                        sv = d;
                        slow = static_cast<int>(i);
                    }
                }
            }
            for (int i = static_cast<int>(s.laps.size()) - 1; i >= 0; --i)
            {
                const size_t ui = static_cast<size_t>(i);
                const double d = s.laps[ui] - (ui ? s.laps[ui - 1] : 0.0);
                const Rect row{lx, y + 1.0f, lw, 40.0f};
                std::wstring num = std::to_wstring(i + 1);
                r.Text(num, {c1, row.y, 60.0f, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
                if (i == fast || i == slow)
                    r.Text(i == fast ? L"Самый быстрый" : L"Самый медленный", {c1 + 32.0f, row.y, c2 - c1 - 36.0f, row.h},
                           i == fast ? p.success : p.critical, 12.0f, FontWeight::Regular, TextFlags::VCenter);
                r.Text(LapText(d), {c2, row.y, c3 - c2, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
                r.Text(LapText(s.laps[ui]), {c3, row.y, lx + lw - c3, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
                y += 41.0f;
                r.FillRect({lx, y, lw, 1.0f}, p.divider);
            }
        }
        m_contentH = y + m_scroll - c.y + 32.0f;
    }

    // ---- мировое время -----------------------------------------------------------
    void DrawWorld(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        ClockState& s = S();
        const float avail = c.w - 2.0f * kPad;
        float mw = avail, mh = mw / WorldMap::kAspect;
        const float maxH = std::max(150.0f, c.h * 0.46f);
        if (mh > maxH)
        {
            mh = maxH;
            mw = mh * WorldMap::kAspect;
        }
        const Rect map{std::floor(c.x + kPad + (avail - mw) * 0.5f), c.y + 20.0f - m_scroll, std::floor(mw), std::floor(mh)};
        const Color ocean = p.dark ? Color(0x2A, 0x2E, 0x35) : Color(0xE6, 0xEC, 0xF3);
        const Color land = p.dark ? Color(0x4B, 0x53, 0x60) : Color(0xC6, 0xD0, 0xDC);
        r.BeginRounded(map, 8.0f);
        r.FillRect(map, ocean);
        r.PushClip(map);
        WorldMap::FillShapes(r, map, WorldMap::Land(), land);
        WorldMap::FillShapes(r, map, WorldMap::Seas(), ocean);
        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        // Ночная сторона с мягкой границей (несколько слоёв со сдвигом).
        const uint8_t na = p.dark ? 0x22 : 0x0E;
        for (float shift : {-6.0f, -3.0f, 0.0f, 3.0f})
            r.FillPolygon(WorldMap::NightPolygon(map, utc, shift), Color(0, 0, 0, na));
        r.PopClip();
        r.EndRounded();
        r.StrokeRoundRect(map, 8.0f, p.cardStroke);
        // Отметки городов
        r.PushClip(map);
        for (int idx : s.cities)
        {
            const City& city = kCities[idx];
            int off = 0;
            const SYSTEMTIME ct = CityTime(city, off);
            const Point pt = WorldMap::Project(map, city.lon, city.lat);
            const std::wstring label = std::wstring(city.name) + L"  " + HhMm(ct);
            const float lw = r.TextWidth(label, 12.0f) + 16.0f;
            Rect pill{pt.x + 8.0f, pt.y - 28.0f, lw, 22.0f};
            if (pill.Right() > map.Right() - 4.0f)
                pill.x = pt.x - 8.0f - lw;
            if (pill.y < map.y + 4.0f)
                pill.y = pt.y + 6.0f;
            r.FillRoundRect(pill, 4.0f, p.dark ? Color(0x2B, 0x2B, 0x2B, 0xF0) : Color(255, 255, 255, 0xF2));
            r.StrokeRoundRect(pill, 4.0f, p.surfaceStroke);
            r.Text(label, pill.Inset(8.0f, 0.0f, 4.0f, 0.0f), p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.FillCircle(pt.x, pt.y, 5.0f, p.accent);
            r.StrokeCircle(pt.x, pt.y, 5.0f, Color(255, 255, 255), 1.5f);
        }
        r.PopClip();
        // Карточки
        const Grid g = MakeGrid(c, map.Bottom() + 20.0f, 96.0f);
        const SYSTEMTIME local = LocalNow();
        const int localOff = LocalOffset();
        {
            const Rect rc = g.Cell(0);
            Card(r, rc);
            r.Text(L"Местное время", {rc.x + 20.0f, rc.y + 18.0f, rc.w - 120.0f, 24.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(OffsetText(localOff) + L" · " + std::to_wstring(local.wDay) + L" " + kMonthsGen[Clamp(static_cast<int>(local.wMonth), 1, 12) - 1],
                   {rc.x + 20.0f, rc.y + 46.0f, rc.w - 120.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(HhMm(local), {rc.Right() - 120.0f, rc.y, 100.0f, rc.h}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter | TextFlags::Right);
        }
        for (int k = 0; k < static_cast<int>(s.cities.size()); ++k)
        {
            const City& city = kCities[s.cities[static_cast<size_t>(k)]];
            const Rect rc = g.Cell(k + 1);
            Card(r, rc);
            int off = 0;
            const SYSTEMTIME ct = CityTime(city, off);
            r.Text(city.name, {rc.x + 20.0f, rc.y + 18.0f, rc.w - 130.0f, 24.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(RelativeText(ct, off, local, localOff), {rc.x + 20.0f, rc.y + 46.0f, rc.w - 130.0f, 22.0f}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter);
            if (m_edit)
                SubtleButton(r, {rc.Right() - 50.0f, rc.CenterY() - 16.0f, 34.0f, 32.0f}, Icon::Delete, kHkCityDelete, k, L"Удалить город");
            else
                r.Text(HhMm(ct), {rc.Right() - 120.0f, rc.y, 100.0f, rc.h}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter | TextFlags::Right);
        }
        m_contentH = g.Bottom(static_cast<int>(s.cities.size()) + 1) + m_scroll - c.y + 96.0f;
    }

    // ---- параметры ---------------------------------------------------------------
    void DrawSettings(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const ClockState& s = S();
        const float x = c.x + kPad + 12.0f, w = std::min(1000.0f, c.w - 2.0f * kPad - 24.0f);
        float y = c.y + 24.0f - m_scroll;
        r.Text(L"Параметры", {x, y, w, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 64.0f;
        r.Text(L"Общие", {x, y, w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 30.0f;
        auto row = [&](Icon icon, const wchar_t* title, const wchar_t* sub, int kind) {
            const Rect rc{x, y, w, 68.0f};
            Card(r, rc, IsHot(kind));
            AddHot(rc, kind);
            r.Glyph(icon, {rc.x + 20.0f, rc.CenterY() - 10.0f, 20.0f, 20.0f}, p.text);
            r.Text(title, {rc.x + 60.0f, rc.y + 14.0f, rc.w - 300.0f, 22.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(sub, {rc.x + 60.0f, rc.y + 36.0f, rc.w - 300.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 72.0f;
            return rc;
        };
        const Rect n = row(Icon::Bell, L"Уведомления", L"Изменить параметры уведомлений в системе", kHkSetNotify);
        r.Glyph(Icon::ChevronRight, {n.Right() - 32.0f, n.CenterY() - 6.0f, 12.0f, 12.0f}, p.textSecondary);
        const Rect snd = row(Icon::Alarm, L"Звук будильника по умолчанию", L"Используется для новых будильников и таймеров", kHkSetSound);
        const Rect dd{snd.Right() - 220.0f, snd.CenterY() - 16.0f, 200.0f, 32.0f};
        r.FillRoundRect(dd, 4.0f, IsHot(kHkSetSound) ? p.controlHover : p.control);
        r.StrokeRoundRect(dd, 4.0f, p.controlStroke);
        r.Text(kSounds[s.defaultSound], dd.Inset(12.0f, 0.0f, 30.0f, 0.0f), p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        r.Glyph(Icon::ChevronDown, {dd.Right() - 24.0f, dd.CenterY() - 5.0f, 10.0f, 10.0f}, p.textSecondary);
        m_soundAnchor = dd;
        y += 24.0f;
        r.Text(L"Сведения", {x, y, w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 30.0f;
        const Rect about{x, y, w, 104.0f};
        Card(r, about);
        r.DrawAppIcon(AppIcon::Clock, {about.x + 20.0f, about.y + 20.0f, 32.0f, 32.0f});
        r.Text(L"Часы", {about.x + 68.0f, about.y + 16.0f, about.w - 90.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(L"Версия 11.2409.29.0", {about.x + 68.0f, about.y + 38.0f, about.w - 90.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        r.Text(L"Демонстрационная копия интерфейса: значки, карта и звуки созданы программно.", {about.x + 68.0f, about.y + 62.0f, about.w - 90.0f, 36.0f},
               p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Wrap);
        y += 120.0f;
        m_contentH = y + m_scroll - c.y;
    }

    // ================================================================== диалоги
    Rect DialogRect() const
    {
        const float w = 440.0f;
        const float h = m_dlg == Dlg::Alarm ? 560.0f : (m_dlg == Dlg::City ? 440.0f : 400.0f);
        return {std::floor((Width() - w) * 0.5f), std::floor(std::max(kTitleH, (Height() - h) * 0.5f)), w, h};
    }

    int SpinCount() const { return m_dlg == Dlg::Timer ? 3 : 2; }
    int SpinMax(int i) const { return m_dlg == Dlg::Timer ? (i == 0 ? 99 : 59) : (i == 0 ? 23 : 59); }

    Rect SpinRect(int i) const
    {
        const Rect d = DialogRect();
        const int n = SpinCount();
        const float cw = 96.0f, gap = 28.0f;
        const float total = cw * static_cast<float>(n) + gap * static_cast<float>(n - 1);
        return {std::floor(d.CenterX() - total * 0.5f) + static_cast<float>(i) * (cw + gap), d.y + 70.0f, cw, 124.0f};
    }

    void OpenTimerDialog(int index)
    {
        ClockState& s = S();
        m_dlg = Dlg::Timer;
        m_dlgIndex = index;
        const int total = index >= 0 ? s.timers[static_cast<size_t>(index)].total : 5 * 60;
        m_spin[0] = total / 3600;
        m_spin[1] = total / 60 % 60;
        m_spin[2] = total % 60;
        m_nameBox->SetText(index >= 0 ? s.timers[static_cast<size_t>(index)].name : NextName(L"Таймер", s.timerSerial + 1));
        OpenDialogCommon(L"Сохранить");
    }

    void OpenAlarmDialog(int index)
    {
        ClockState& s = S();
        m_dlg = Dlg::Alarm;
        m_dlgIndex = index;
        AlarmItem a;
        a.sound = s.defaultSound;
        if (index >= 0)
            a = s.alarms[static_cast<size_t>(index)];
        else
            a.name = NextName(L"Будильник", s.alarmSerial + 1);
        m_spin[0] = a.hour;
        m_spin[1] = a.minute;
        m_dlgDays = a.days;
        m_dlgRepeat = a.days != 0;
        m_dlgSound = a.sound;
        m_dlgSnooze = a.snooze;
        m_nameBox->SetText(a.name);
        OpenDialogCommon(L"Сохранить");
    }

    void OpenCityDialog()
    {
        m_dlg = Dlg::City;
        m_dlgIndex = -1;
        m_searchBox->SetText(L"");
        m_citySel = -1;
        m_cityScroll = 0;
        FilterCities();
        OpenDialogCommon(L"Добавить");
    }

    void OpenDialogCommon(const wchar_t* saveText)
    {
        m_edit = false;
        m_saveBtn->text = saveText;
        m_saveBtn->SetVisible(true);
        m_cancelBtn->SetVisible(true);
        TextBox* box = m_dlg == Dlg::City ? m_searchBox : m_nameBox;
        box->SetVisible(true);
        (m_dlg == Dlg::City ? m_nameBox : m_searchBox)->SetVisible(false);
        m_ui.SetFocus(box);
        if (m_dlg != Dlg::City)
            box->SelectAll();
        m_dlgAnim.Start(0.0f, 1.0f, 0.2f, Ease::OutCubic);
        LayoutDialog();
    }

    void LayoutDialog()
    {
        const Rect d = DialogRect();
        if (m_dlg == Dlg::Timer)
            m_nameBox->SetBounds({d.x + 24.0f, d.y + 236.0f, d.w - 48.0f, 32.0f});
        else if (m_dlg == Dlg::Alarm)
            m_nameBox->SetBounds({d.x + 24.0f, d.y + 208.0f, d.w - 48.0f, 32.0f});
        m_searchBox->SetBounds({d.x + 24.0f, d.y + 66.0f, d.w - 48.0f, 32.0f});
        const float bw = (d.w - 48.0f - 8.0f) * 0.5f;
        m_saveBtn->SetBounds({d.x + 24.0f, d.Bottom() - 56.0f, bw, 32.0f});
        m_cancelBtn->SetBounds({d.x + 32.0f + bw, d.Bottom() - 56.0f, bw, 32.0f});
    }

    void CloseDialog()
    {
        m_dlg = Dlg::None;
        m_nameBox->SetVisible(false);
        m_searchBox->SetVisible(false);
        m_saveBtn->SetVisible(false);
        m_cancelBtn->SetVisible(false);
        m_ui.SetFocus(nullptr);
    }

    void SaveDialog()
    {
        ClockState& s = S();
        if (m_dlg == Dlg::Timer)
        {
            const int total = m_spin[0] * 3600 + m_spin[1] * 60 + m_spin[2];
            if (total <= 0)
                return;
            std::wstring name = Trim(m_nameBox->Text());
            if (name.empty())
                name = NextName(L"Таймер", s.timerSerial + 1);
            if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(s.timers.size()))
            {
                TimerItem& t = s.timers[static_cast<size_t>(m_dlgIndex)];
                t.name = name;
                t.total = total;
                t.left = total;
                t.running = false;
            }
            else
            {
                TimerItem t;
                t.name = name;
                t.total = total;
                t.left = total;
                s.timers.push_back(t);
                ++s.timerSerial;
            }
        }
        else if (m_dlg == Dlg::Alarm)
        {
            AlarmItem a;
            if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(s.alarms.size()))
                a = s.alarms[static_cast<size_t>(m_dlgIndex)];
            a.name = Trim(m_nameBox->Text());
            if (a.name.empty())
                a.name = L"Будильник";
            a.hour = m_spin[0];
            a.minute = m_spin[1];
            a.days = m_dlgRepeat ? m_dlgDays : 0u;
            if (m_dlgRepeat && a.days == 0)
                a.days = 0x7F;
            a.sound = m_dlgSound;
            a.snooze = m_dlgSnooze;
            a.on = true;
            a.lastKey = -1;
            if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(s.alarms.size()))
                s.alarms[static_cast<size_t>(m_dlgIndex)] = a;
            else
            {
                s.alarms.push_back(a);
                ++s.alarmSerial;
            }
            GetShell().Notify(AppIcon::Clock, L"Часы", L"Будильник установлен",
                              a.name + L" — " + Pad2(a.hour) + L":" + Pad2(a.minute) + L", " + UntilText(MinutesUntil(a)) + L".");
        }
        else if (m_dlg == Dlg::City)
        {
            if (m_citySel < 0 && m_cityMatches.size() == 1)
                m_citySel = 0;
            if (m_citySel < 0 || m_citySel >= static_cast<int>(m_cityMatches.size()))
                return;
            s.cities.push_back(m_cityMatches[static_cast<size_t>(m_citySel)]);
        }
        CloseDialog();
    }

    void FilterCities()
    {
        const ClockState& s = S();
        const std::wstring q = Trim(m_searchBox->Text());
        m_cityMatches.clear();
        for (int i = 0; i < kCityCount; ++i)
        {
            if (std::find(s.cities.begin(), s.cities.end(), i) != s.cities.end())
                continue;
            if (q.empty() || ContainsNoCase(kCities[i].name, q) || ContainsNoCase(kCities[i].country, q) || ContainsNoCase(kCities[i].alias, q))
                m_cityMatches.push_back(i);
        }
        m_citySel = q.empty() || m_cityMatches.empty() ? -1 : 0;
        m_cityScroll = 0;
    }

    Rect CityListRect() const
    {
        const Rect d = DialogRect();
        return {d.x + 24.0f, d.y + 108.0f, d.w - 48.0f, d.h - 108.0f - 96.0f};
    }

    void DrawDialog(Renderer& r)
    {
        const Palette& p = Theme::P();
        const float t = m_dlgAnim.Value();
        r.FillRect({0.0f, 0.0f, Width(), Height()}, p.smoke.MulAlpha(t));
        const Rect d = DialogRect();
        r.Shadow(d, 8.0f, 32.0f, 12.0f, p.dark ? 0.55f : 0.3f);
        r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.FillRoundRect({d.x, d.Bottom() - 80.0f, d.w, 80.0f}, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
        r.FillRect({d.x, d.Bottom() - 80.0f, d.w, 1.0f}, p.divider);
        r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
        const wchar_t* title = L"";
        if (m_dlg == Dlg::Timer)
            title = m_dlgIndex >= 0 ? L"Изменить таймер" : L"Добавить новый таймер";
        else if (m_dlg == Dlg::Alarm)
            title = m_dlgIndex >= 0 ? L"Изменить будильник" : L"Добавить новый будильник";
        else
            title = L"Добавить новый город";
        r.Text(title, {d.x + 24.0f, d.y + 18.0f, d.w - 90.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        if (m_dlg == Dlg::Alarm && m_dlgIndex >= 0)
            SubtleButton(r, {d.Right() - 52.0f, d.y + 16.0f, 36.0f, 34.0f}, Icon::Delete, kHkDlgDelete, 0, L"Удалить будильник");
        if (m_dlg == Dlg::City)
        {
            DrawCityList(r);
            m_saveBtn->SetEnabled(m_citySel >= 0 || m_cityMatches.size() == 1);
            return;
        }
        m_saveBtn->SetEnabled(m_dlg != Dlg::Timer || m_spin[0] + m_spin[1] + m_spin[2] > 0);
        // Барабаны выбора времени
        for (int i = 0; i < SpinCount(); ++i)
        {
            const Rect sr = SpinRect(i);
            const bool hot = IsHot(kHkDlgCol, i) || IsHot(kHkDlgUp, i) || IsHot(kHkDlgDown, i);
            r.FillRoundRect(sr, 6.0f, hot ? p.controlHover : p.control);
            r.StrokeRoundRect(sr, 6.0f, p.controlStroke);
            AddHot(sr, kHkDlgCol, i);
            const Rect up{sr.x + 4.0f, sr.y + 4.0f, sr.w - 8.0f, 28.0f}, dn{sr.x + 4.0f, sr.Bottom() - 32.0f, sr.w - 8.0f, 28.0f};
            if (hot)
            {
                SubtleButton(r, up, Icon::ChevronUp, kHkDlgUp, i, L"");
                SubtleButton(r, dn, Icon::ChevronDown, kHkDlgDown, i, L"");
            }
            else
            {
                AddHot(up, kHkDlgUp, i);
                AddHot(dn, kHkDlgDown, i);
            }
            r.Text(Pad2(m_spin[i]), {sr.x, sr.y + 30.0f, sr.w, sr.h - 60.0f}, p.text, 40.0f, FontWeight::Semibold, TextFlags::Middle);
            if (i + 1 < SpinCount())
                r.Text(L":", {sr.Right(), sr.y, 28.0f, sr.h}, p.text, 36.0f, FontWeight::Semibold, TextFlags::Middle);
            if (m_dlg == Dlg::Timer)
            {
                const wchar_t* lbl[] = {L"ч", L"мин", L"с"};
                r.Text(lbl[i], {sr.x, sr.Bottom() + 6.0f, sr.w, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Middle);
            }
        }
        if (m_dlg != Dlg::Alarm)
            return;
        float y = d.y + 256.0f;
        CheckDraw(r, d.x + 24.0f, y, m_dlgRepeat, L"Повторить будильник", kHkDlgRepeat);
        y += 36.0f;
        for (int k = 0; k < 7; ++k)
        {
            const float cx = d.x + 24.0f + 18.0f + static_cast<float>(k) * 56.0f;
            const bool on = m_dlgRepeat && ((m_dlgDays >> k) & 1u) != 0;
            const bool hot = m_dlgRepeat && IsHot(kHkDlgDay, k);
            if (on)
                r.FillCircle(cx, y + 18.0f, 18.0f, hot ? p.accentHover : p.accent);
            else
            {
                r.FillCircle(cx, y + 18.0f, 18.0f, hot ? p.controlHover : p.control);
                r.StrokeCircle(cx, y + 18.0f, 17.5f, p.controlStroke);
            }
            r.Text(kDayShort[k], {cx - 18.0f, y, 36.0f, 36.0f}, on ? p.textOnAccent : (m_dlgRepeat ? p.text : p.textDisabled), 12.0f,
                   FontWeight::Regular, TextFlags::Middle);
            if (m_dlgRepeat)
                AddHot({cx - 18.0f, y, 36.0f, 36.0f}, kHkDlgDay, k);
        }
        y += 56.0f;
        auto dropdown = [&](Icon icon, const std::wstring& text, int kind, float width) {
            r.Glyph(icon, {d.x + 26.0f, y + 8.0f, 16.0f, 16.0f}, p.text);
            const Rect dd{d.x + 56.0f, y, width, 32.0f};
            r.FillRoundRect(dd, 4.0f, IsHot(kind) ? p.controlHover : p.control);
            r.StrokeRoundRect(dd, 4.0f, p.controlStroke);
            r.Text(text, dd.Inset(12.0f, 0.0f, 30.0f, 0.0f), p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Glyph(Icon::ChevronDown, {dd.Right() - 24.0f, dd.CenterY() - 5.0f, 10.0f, 10.0f}, p.textSecondary);
            AddHot(dd, kind);
            return dd;
        };
        m_soundAnchor = dropdown(Icon::Bell, kSounds[m_dlgSound], kHkDlgSound, d.w - 56.0f - 24.0f - 44.0f);
        SubtleButton(r, {d.Right() - 24.0f - 36.0f, y, 36.0f, 32.0f}, Icon::Play, kHkDlgSoundPlay, 0, L"Прослушать");
        y += 44.0f;
        const int sn = kSnoozes[Clamp(m_dlgSnooze, 0, kSnoozeCount - 1)];
        m_snoozeAnchor = dropdown(Icon::Sleep, sn == 0 ? std::wstring(L"Отключено") : L"Повтор через " + std::to_wstring(sn) + L" мин", kHkDlgSnooze,
                                  d.w - 56.0f - 24.0f);
    }

    void DrawCityList(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect lr = CityListRect();
        r.PushClip(lr);
        const Rect saveClip = m_hotClip;
        m_hotClip = lr;
        if (m_cityMatches.empty())
            r.Text(L"Ничего не найдено", {lr.x, lr.y + 20.0f, lr.w, 24.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
        for (int i = 0; i < static_cast<int>(m_cityMatches.size()); ++i)
        {
            const Rect row{lr.x, lr.y + static_cast<float>(i) * 40.0f - m_cityScroll, lr.w, 38.0f};
            if (row.Bottom() < lr.y || row.y > lr.Bottom())
                continue;
            const City& city = kCities[m_cityMatches[static_cast<size_t>(i)]];
            const bool sel = i == m_citySel;
            if (sel || IsHot(kHkDlgCity, i))
                r.FillRoundRect(row, 4.0f, sel ? p.subtlePressed : p.subtleHover);
            if (sel)
                r.FillRoundRect({row.x, row.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Text(std::wstring(city.name) + L", " + city.country, row.Inset(14.0f, 0.0f, 80.0f, 0.0f), p.text, 14.0f, FontWeight::Regular,
                   TextFlags::VCenter);
            int off = 0;
            const SYSTEMTIME ct = CityTime(city, off);
            r.Text(HhMm(ct), row.Inset(0.0f, 0.0f, 12.0f, 0.0f), p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
            AddHot(row, kHkDlgCity, i);
        }
        m_hotClip = saveClip;
        r.PopClip();
    }

    // ================================================================== действия
    void Activate(int kind, int index, int clicks)
    {
        ClockState& s = S();
        switch (kind)
        {
        case kHkHamburger:
            if (Wide())
                m_navPref = !m_navPref;
            else
                m_navOverlay = !m_navOverlay;
            break;
        case kHkNav:
        {
            const int count = static_cast<int>(sizeof(kNav) / sizeof(kNav[0]));
            SetPage(index == count ? Page::Settings : kNav[index].page);
            break;
        }
        case kHkCmdEdit:
            m_edit = !m_edit;
            break;
        case kHkCmdAdd:
            if (m_page == Page::Timer)
                OpenTimerDialog(-1);
            else if (m_page == Page::Alarm)
                OpenAlarmDialog(-1);
            else if (m_page == Page::World)
                OpenCityDialog();
            break;
        case kHkTimerPlay:
        {
            TimerItem& t = s.timers[static_cast<size_t>(index)];
            if (t.running)
            {
                t.left = TimerLeft(t);
                t.running = false;
            }
            else
            {
                if (t.left <= 0.01)
                    t.left = t.total;
                t.endsAt = NowSeconds() + t.left;
                t.running = true;
            }
            break;
        }
        case kHkTimerReset:
        {
            TimerItem& t = s.timers[static_cast<size_t>(index)];
            t.running = false;
            t.left = t.total;
            break;
        }
        case kHkTimerExpand:
            m_expanded = index;
            break;
        case kHkTimerCollapse:
            m_expanded = -1;
            break;
        case kHkTimerDelete:
            s.timers.erase(s.timers.begin() + index);
            if (s.timers.empty())
                m_edit = false;
            break;
        case kHkTimerCard:
            OpenTimerDialog(index);
            break;
        case kHkAlarmToggle:
        {
            AlarmItem& a = s.alarms[static_cast<size_t>(index)];
            a.on = !a.on;
            a.lastKey = -1;
            break;
        }
        case kHkAlarmDelete:
            s.alarms.erase(s.alarms.begin() + index);
            if (s.alarms.empty())
                m_edit = false;
            break;
        case kHkAlarmCard:
            if (!m_edit)
                OpenAlarmDialog(index);
            break;
        case kHkSwPlay:
            if (s.swRunning)
            {
                s.swAccum = StopwatchTime(s);
                s.swRunning = false;
            }
            else
            {
                s.swStart = NowSeconds();
                s.swRunning = true;
            }
            break;
        case kHkSwLap:
            if (s.swRunning)
            {
                s.laps.push_back(StopwatchTime(s));
                if (s.laps.size() > 999)
                    s.laps.erase(s.laps.begin());
            }
            break;
        case kHkSwReset:
            s.swRunning = false;
            s.swAccum = 0.0;
            s.laps.clear();
            m_scroll = 0.0f;
            break;
        case kHkCityDelete:
            s.cities.erase(s.cities.begin() + index);
            if (s.cities.empty())
                m_edit = false;
            break;
        case kHkFocusUp:
            s.focusMinutes = s.focusMinutes < 30 ? s.focusMinutes + 5 : std::min(240, s.focusMinutes + 15);
            break;
        case kHkFocusDown:
            s.focusMinutes = s.focusMinutes <= 30 ? std::max(5, s.focusMinutes - 5) : s.focusMinutes - 15;
            break;
        case kHkFocusSkip:
            s.skipBreaks = !s.skipBreaks;
            break;
        case kHkFocusStart:
            ClockStartFocus(s.focusMinutes);
            break;
        case kHkFocusPause:
            if (s.focusPaused)
            {
                s.focusEnds = NowSeconds() + s.focusLeft;
                s.focusPaused = false;
            }
            else
            {
                s.focusLeft = FocusLeft(s);
                s.focusPaused = true;
            }
            break;
        case kHkFocusStop:
            ClockStopFocus();
            break;
        case kHkFocusGoal:
            ShowGoalMenu();
            break;
        case kHkSetNotify:
            GetShell().Launch(AppId::Settings, L"notifications");
            break;
        case kHkSetSound:
        {
            std::vector<MenuItem> items;
            for (int i = 0; i < kSoundCount; ++i)
                items.push_back(MenuItem::Check(kSounds[i], s.defaultSound == i, [i]() { S().defaultSound = i; }, true));
            GetShell().ShowMenuBelow(LocalToScreen(m_soundAnchor), std::move(items), m_soundAnchor.w);
            break;
        }
        case kHkDlgUp:
        case kHkDlgDown:
            Spin(index, kind == kHkDlgUp ? 1 : -1);
            break;
        case kHkDlgRepeat:
            m_dlgRepeat = !m_dlgRepeat;
            if (m_dlgRepeat && m_dlgDays == 0)
                m_dlgDays = 0x1F;
            break;
        case kHkDlgDay:
            m_dlgDays ^= 1u << index;
            break;
        case kHkDlgSound:
        {
            std::vector<MenuItem> items;
            for (int i = 0; i < kSoundCount; ++i)
                items.push_back(MenuItem::Check(kSounds[i], m_dlgSound == i, [this, i]() { m_dlgSound = i; }, true));
            GetShell().ShowMenuBelow(LocalToScreen(m_soundAnchor), std::move(items), m_soundAnchor.w);
            break;
        }
        case kHkDlgSoundPlay:
            Ring();
            break;
        case kHkDlgSnooze:
        {
            std::vector<MenuItem> items;
            for (int i = 0; i < kSnoozeCount; ++i)
            {
                const std::wstring text = kSnoozes[i] == 0 ? std::wstring(L"Отключено") : std::to_wstring(kSnoozes[i]) + L" мин";
                items.push_back(MenuItem::Check(text, m_dlgSnooze == i, [this, i]() { m_dlgSnooze = i; }, true));
            }
            GetShell().ShowMenuBelow(LocalToScreen(m_snoozeAnchor), std::move(items), m_snoozeAnchor.w);
            break;
        }
        case kHkDlgDelete:
            if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(s.alarms.size()))
                s.alarms.erase(s.alarms.begin() + m_dlgIndex);
            CloseDialog();
            break;
        case kHkDlgCity:
            m_citySel = index;
            if (clicks >= 2)
                SaveDialog();
            break;
        default:
            break;
        }
    }

    void Spin(int i, int delta)
    {
        const int mx = SpinMax(i);
        m_spin[i] = (m_spin[i] + delta + mx + 1) % (mx + 1);
    }

    void ShowGoalMenu()
    {
        std::vector<MenuItem> items;
        for (int m : {30, 60, 90, 120, 180, 240, 300})
            items.push_back(MenuItem::Check(DurationText(m), S().goalMinutes == m, [m]() { S().goalMinutes = m; }, true));
        GetShell().ShowMenuBelow(LocalToScreen(m_goalAnchor), std::move(items), 200.0f);
    }

    void ShowContextMenu(const Hot& h, Point local)
    {
        std::vector<MenuItem> items;
        const int i = h.index;
        if (h.kind == kHkTimerCard || h.kind == kHkTimerPlay || h.kind == kHkTimerExpand)
        {
            items = {MenuItem(L"Изменить", Icon::Edit, [this, i]() { OpenTimerDialog(i); }),
                     MenuItem(L"Удалить", Icon::Delete, [i]() {
                         if (i < static_cast<int>(S().timers.size()))
                             S().timers.erase(S().timers.begin() + i);
                     })};
        }
        else if (h.kind == kHkAlarmCard || h.kind == kHkAlarmToggle)
        {
            items = {MenuItem(L"Изменить", Icon::Edit, [this, i]() { OpenAlarmDialog(i); }),
                     MenuItem(L"Удалить", Icon::Delete, [i]() {
                         if (i < static_cast<int>(S().alarms.size()))
                             S().alarms.erase(S().alarms.begin() + i);
                     })};
        }
        if (!items.empty())
            GetShell().ShowMenu(LocalToScreen(local), std::move(items));
    }

    // ================================================================== ввод
    void UpdateHover(Point pt)
    {
        const Hot* h = HitHot(pt);
        m_hoverKind = h ? h->kind : 0;
        m_hoverIndex = h ? h->index : 0;
        if (h && !h->tip.empty())
            GetShell().Tooltip(&g_tipIds[(h->kind * 16 + h->index) & 511], h->tip, LocalToScreen(h->r), false);
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        UpdateHover(e.pos);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_hoverKind = 0;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        Window::OnMouseDown(e);
        if (HasDialog() || m_ui.HasCapture())
            return;
        if (m_navOverlay && !Wide() && e.pos.x > kNavFull && e.pos.y > kTitleH && m_dlg == Dlg::None)
        {
            m_navOverlay = false;
            return;
        }
        const Hot* h = HitHot(e.pos);
        m_pressKind = h ? h->kind : 0;
        m_pressIndex = h ? h->index : 0;
        m_pressClicks = e.clicks;
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog())
            return;
        const Hot* h = HitHot(e.pos);
        const int kind = m_pressKind, index = m_pressIndex;
        m_pressKind = 0;
        if (!h || h->kind != kind || h->index != index || kind == 0)
            return;
        if (e.button == MouseButton::Right)
        {
            if (m_dlg == Dlg::None)
                ShowContextMenu(*h, e.pos);
            return;
        }
        if (e.button == MouseButton::Left)
            Activate(kind, index, m_pressClicks);
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        if (m_dlg != Dlg::None)
        {
            const Hot* h = HitHot(e.pos);
            if (h && (h->kind == kHkDlgCol || h->kind == kHkDlgUp || h->kind == kHkDlgDown))
                Spin(h->index, e.wheel > 0.0f ? 1 : -1);
            else if (m_dlg == Dlg::City && CityListRect().Contains(e.pos))
            {
                const float maxS = std::max(0.0f, static_cast<float>(m_cityMatches.size()) * 40.0f - CityListRect().h);
                m_cityScroll = Clamp(m_cityScroll - e.wheel * 80.0f, 0.0f, maxS);
            }
            return true;
        }
        if (ContentRect().Contains(e.pos))
            m_scroll = Clamp(m_scroll - e.wheel * 90.0f, 0.0f, MaxScroll());
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return Window::OnKeyDown(vk, mods);
        if (m_dlg != Dlg::None)
        {
            if (m_dlg == Dlg::City && m_ui.FocusedChild() == m_searchBox && (vk == VK_DOWN || vk == VK_UP))
            {
                const int n = static_cast<int>(m_cityMatches.size());
                if (n > 0)
                {
                    m_citySel = Clamp(m_citySel + (vk == VK_DOWN ? 1 : -1), 0, n - 1);
                    const float rowY = static_cast<float>(m_citySel) * 40.0f, listH = CityListRect().h;
                    if (rowY < m_cityScroll)
                        m_cityScroll = rowY;
                    else if (rowY + 38.0f > m_cityScroll + listH)
                        m_cityScroll = rowY + 38.0f - listH;
                }
                return true;
            }
            if (m_ui.FocusedChild())
                return Window::OnKeyDown(vk, mods);
            if (vk == VK_ESCAPE)
                CloseDialog();
            else if (vk == VK_RETURN)
                SaveDialog();
            return true;
        }
        if (vk == VK_ESCAPE)
        {
            if (m_navOverlay)
                m_navOverlay = false;
            else if (m_expanded >= 0)
                m_expanded = -1;
            else if (m_edit)
                m_edit = false;
            return true;
        }
        if (m_page == Page::Stopwatch && !mods.ctrl && !mods.alt)
        {
            if (vk == VK_SPACE)
            {
                Activate(kHkSwPlay, 0, 1);
                return true;
            }
            if (vk == 'L')
            {
                Activate(kHkSwLap, 0, 1);
                return true;
            }
            if (vk == 'R' && StopwatchTime(S()) > 0.0)
            {
                Activate(kHkSwReset, 0, 1);
                return true;
            }
        }
        return Window::OnKeyDown(vk, mods);
    }

    CursorType Cursor(Point local) const override
    {
        if (!HasDialog() && m_dlg == Dlg::None)
        {
            const Hot* h = HitHot(local);
            if (h && (h->kind == kHkNav || h->kind == kHkSetNotify))
                return CursorType::Arrow;
        }
        return Window::Cursor(local);
    }

    // ================================================================== поля
    Page m_page = Page::Focus;
    bool m_navPref = true;
    bool m_navOverlay = false;
    bool m_edit = false;
    int m_expanded = -1;
    float m_scroll = 0.0f;
    float m_contentH = 0.0f;
    std::vector<Hot> m_hots;
    Rect m_hotClip;
    int m_hoverKind = 0;
    int m_hoverIndex = 0;
    int m_pressKind = 0;
    int m_pressIndex = 0;
    int m_pressClicks = 1;
    Rect m_soundAnchor;
    Rect m_snoozeAnchor;
    Rect m_goalAnchor;
    // Диалоги
    Dlg m_dlg = Dlg::None;
    int m_dlgIndex = -1;
    int m_spin[3] = {0, 0, 0};
    unsigned m_dlgDays = 0;
    bool m_dlgRepeat = false;
    int m_dlgSound = 0;
    int m_dlgSnooze = 2;
    std::vector<int> m_cityMatches;
    int m_citySel = -1;
    float m_cityScroll = 0.0f;
    Tween m_dlgAnim;
    TextBox* m_nameBox = nullptr;
    TextBox* m_searchBox = nullptr;
    Button* m_saveBtn = nullptr;
    Button* m_cancelBtn = nullptr;
};
} // namespace

// ============================================================================ внешний интерфейс
void ClockBackgroundTick()
{
    ClockState& s = S();
    const double now = NowSeconds();
    for (TimerItem& t : s.timers)
    {
        if (t.running && now >= t.endsAt)
        {
            t.running = false;
            t.left = t.total;
            GetShell().Notify(AppIcon::Clock, L"Часы", L"Время таймера истекло", t.name + L" — " + Hms(t.total));
            Ring();
        }
    }
    const SYSTEMTIME lt = LocalNow();
    const int dow = (lt.wDayOfWeek + 6) % 7;
    const long long key = DateKey(lt) * 1440 + lt.wHour * 60 + lt.wMinute;
    for (AlarmItem& a : s.alarms)
    {
        if (!a.on || a.hour != lt.wHour || a.minute != lt.wMinute || a.lastKey == key)
            continue;
        if (a.days != 0 && !((a.days >> dow) & 1u))
            continue;
        a.lastKey = key;
        if (a.days == 0)
            a.on = false;
        GetShell().Notify(AppIcon::Clock, L"Будильник", a.name, Pad2(a.hour) + L":" + Pad2(a.minute) + L" · " + kSounds[Clamp(a.sound, 0, kSoundCount - 1)]);
        Ring();
    }
    if (s.focusOn && !s.focusPaused && now >= s.focusEnds)
        FinishFocus(s, true);
}

void ClockStartFocus(int minutes)
{
    ClockState& s = S();
    s.focusMinutes = Clamp(minutes, 5, 240);
    s.focusTotal = s.focusMinutes;
    s.focusOn = true;
    s.focusPaused = false;
    s.focusEnds = NowSeconds() + s.focusTotal * 60.0;
    ShellSettings& st = GetShell().Settings();
    s.focusSetDnd = !st.focus;
    st.focus = true;
    GetShell().Notify(AppIcon::Clock, L"Часы", L"Сеанс фокусировки начат",
                      L"Продолжительность: " + DurationText(s.focusTotal) + L". Уведомления будут скрыты.");
}

void ClockStopFocus()
{
    ClockState& s = S();
    if (s.focusOn)
        FinishFocus(s, false);
}

bool ClockFocusActive() { return S().focusOn; }

std::unique_ptr<Window> CreateClock(const std::wstring& arg) { return std::make_unique<ClockWindow>(arg); }
