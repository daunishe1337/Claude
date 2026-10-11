#pragma once
// Модель погоды без сети: детерминированный «прогноз» по климату города (сезонный ход
// температуры, суточный цикл, псевдослучайные аномалии и явления), восход и закат Солнца,
// фаза Луны. Используется приложением «Погода», виджетами и панелью задач.

#include "../Renderer.h"
#include "Cities.h"

namespace WeatherData
{
enum class Cond
{
    Clear,
    PartlyCloudy,
    Cloudy,
    Showers,
    Rain,
    Storm,
    Snow,
    Fog,
};

struct DayWx
{
    int day = 0; // номер дня от 01.01.2000
    float hi = 0.0f;
    float lo = 0.0f;
    Cond cond = Cond::Clear;
    int precip = 0; // вероятность осадков, %
    float wind = 0.0f; // м/с
    int windDir = 0;   // направление, откуда дует (градусы)
    int humidity = 50;
    int uv = 0;
};

struct HourWx
{
    int hour = 0;
    float temp = 0.0f;
    Cond cond = Cond::Clear;
    int precip = 0;
    float wind = 0.0f;
    int windDir = 0;
    int humidity = 50;
    bool night = false;
};

struct NowWx
{
    float temp = 0.0f;
    float feels = 0.0f;
    Cond cond = Cond::Clear;
    bool night = false;
    float wind = 0.0f;
    float gust = 0.0f;
    int windDir = 0;
    int humidity = 50;
    int pressure = 1013; // гПа
    float visibility = 10.0f;
    float dew = 0.0f;
    int uv = 0;
    int aqi = 30;
    int sunrise = 360; // минуты от полуночи (местное время города)
    int sunset = 1080;
    SYSTEMTIME local{};
    int today = 0;
};

// ---- служебное ---------------------------------------------------------------
inline uint32_t Hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

inline float Rand01(int a, int b)
{
    const uint32_t h = Hash(static_cast<uint32_t>(a) * 0x9E3779B9u ^ Hash(static_cast<uint32_t>(b) + 0x632BE5ABu));
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

inline int DayNumber(const SYSTEMTIME& t)
{
    // Дни от 01.01.2000 (алгоритм для григорианского календаря).
    int y = t.wYear, m = t.wMonth;
    const int d = t.wDay;
    if (m <= 2)
    {
        y -= 1;
        m += 12;
    }
    const int jdn = 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d;
    static const int base = 365 * 1999 + 1999 / 4 - 1999 / 100 + 1999 / 400 + (153 * (13 - 3) + 2) / 5 + 1;
    return jdn - base;
}

inline SYSTEMTIME DateOf(int day)
{
    // 01.01.2000 + day
    FILETIME ft{};
    SYSTEMTIME base{};
    base.wYear = 2000;
    base.wMonth = 1;
    base.wDay = 1;
    SystemTimeToFileTime(&base, &ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += static_cast<ULONGLONG>(static_cast<LONGLONG>(day) * 864000000000LL);
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    return st;
}

inline int DayOfYear(const SYSTEMTIME& t)
{
    SYSTEMTIME jan1{};
    jan1.wYear = t.wYear;
    jan1.wMonth = 1;
    jan1.wDay = 1;
    return DayNumber(t) - DayNumber(jan1);
}

inline float SeasonTemp(const Cities::City& c, int doy)
{
    return c.tMean + c.tAmp * std::cos(2.0f * kPi * static_cast<float>(doy - 200) / 365.0f);
}

// ---- суточные данные -------------------------------------------------------------
inline DayWx Day(int city, int day)
{
    const Cities::City& c = Cities::kCities[Clamp(city, 0, Cities::kCityCount - 1)];
    DayWx d;
    d.day = day;
    const float season = SeasonTemp(c, DayOfYear(DateOf(day)));
    auto noise = [&](int dd) { return Rand01(city * 7 + 1, dd) * 2.0f - 1.0f; };
    const float anomaly = (noise(day - 1) + 2.0f * noise(day) + noise(day + 1)) * 0.25f * 6.0f;
    const float range = 5.0f + 5.0f * Rand01(city * 7 + 2, day) - c.wet * 2.0f;
    d.hi = season + anomaly + range * 0.5f;
    d.lo = season + anomaly - range * 0.5f;
    const float w = Rand01(city * 7 + 3, day);
    const bool cold = d.hi < 1.5f;
    if (w < c.wet * 0.32f)
        d.cond = cold ? Cond::Snow : (w < c.wet * 0.06f && d.hi > 12.0f ? Cond::Storm : Cond::Rain);
    else if (w < c.wet * 0.58f)
        d.cond = cold ? Cond::Snow : Cond::Showers;
    else if (w < c.wet * 0.62f && d.lo > -5.0f && d.hi < 15.0f)
        d.cond = Cond::Fog;
    else if (w < 0.22f + c.wet * 0.62f)
        d.cond = Cond::Cloudy;
    else if (w < 0.5f + c.wet * 0.45f)
        d.cond = Cond::PartlyCloudy;
    else
        d.cond = Cond::Clear;
    const float pr = Rand01(city * 7 + 4, day);
    switch (d.cond)
    {
    case Cond::Clear:
        d.precip = static_cast<int>(pr * 5.0f);
        break;
    case Cond::PartlyCloudy:
        d.precip = 5 + static_cast<int>(pr * 12.0f);
        break;
    case Cond::Cloudy:
    case Cond::Fog:
        d.precip = 12 + static_cast<int>(pr * 20.0f);
        break;
    case Cond::Showers:
        d.precip = 40 + static_cast<int>(pr * 30.0f);
        break;
    case Cond::Rain:
    case Cond::Snow:
        d.precip = 60 + static_cast<int>(pr * 30.0f);
        break;
    case Cond::Storm:
        d.precip = 70 + static_cast<int>(pr * 25.0f);
        break;
    }
    d.wind = 1.5f + 6.5f * Rand01(city * 7 + 5, day) + (d.cond == Cond::Storm ? 5.0f : 0.0f);
    d.windDir = static_cast<int>(Rand01(city * 7 + 6, day) * 360.0f);
    d.humidity = Clamp(static_cast<int>(45.0f + 35.0f * c.wet + static_cast<float>(d.precip) * 0.2f + (Rand01(city, day) - 0.5f) * 10.0f), 15, 100);
    const float sunny = d.cond == Cond::Clear ? 1.0f : (d.cond == Cond::PartlyCloudy ? 0.75f : 0.4f);
    d.uv = Clamp(static_cast<int>((8.0f - std::fabs(c.lat) / 9.0f + (season - c.tMean) / 4.0f) * sunny + 0.5f), 0, 11);
    return d;
}

inline float SmoothStep01(float t) { return 0.5f - 0.5f * std::cos(kPi * Saturate(t)); }

// Температура в момент времени (часы от полуночи могут быть дробными).
inline float TempAt(int city, int day, float hour)
{
    const DayWx d = Day(city, day);
    if (hour >= 5.0f && hour <= 15.0f)
        return d.lo + (d.hi - d.lo) * SmoothStep01((hour - 5.0f) / 10.0f);
    if (hour > 15.0f)
    {
        const DayWx next = Day(city, day + 1);
        return d.hi + (next.lo - d.hi) * SmoothStep01((hour - 15.0f) / 14.0f);
    }
    const DayWx prev = Day(city, day - 1);
    return prev.hi + (d.lo - prev.hi) * SmoothStep01((hour + 9.0f) / 14.0f);
}

// ---- Солнце и Луна ---------------------------------------------------------------
// Восход и закат (минуты местного времени); polar = 1 — полярный день, -1 — полярная ночь.
inline void SunTimes(int city, int day, int offsetMinutes, int& sunrise, int& sunset, int* polar = nullptr)
{
    const Cities::City& c = Cities::kCities[Clamp(city, 0, Cities::kCityCount - 1)];
    const float doy = static_cast<float>(DayOfYear(DateOf(day)));
    const float decl = -23.44f * std::cos(2.0f * kPi / 365.0f * (doy + 10.0f)) * kPi / 180.0f;
    const float lat = c.lat * kPi / 180.0f;
    // −0,83° — рефракция и радиус диска.
    const float cosw = (std::sin(-0.0145f) - std::sin(lat) * std::sin(decl)) / (std::cos(lat) * std::cos(decl));
    const float noon = 720.0f - 4.0f * c.lon + static_cast<float>(offsetMinutes);
    if (polar)
        *polar = cosw < -1.0f ? 1 : (cosw > 1.0f ? -1 : 0);
    const float w = std::acos(Clamp(cosw, -1.0f, 1.0f)) * 180.0f / kPi;
    sunrise = static_cast<int>(noon - 4.0f * w);
    sunset = static_cast<int>(noon + 4.0f * w);
}

// Возраст Луны в днях (0 — новолуние, ~14,8 — полнолуние).
inline float MoonAge(int day)
{
    const float synodic = 29.530588f;
    float age = std::fmod(static_cast<float>(day) - 5.24f, synodic);
    if (age < 0.0f)
        age += synodic;
    return age;
}

inline const wchar_t* MoonPhaseName(float age)
{
    static const wchar_t* names[] = {L"Новолуние",      L"Растущий серп",      L"Первая четверть", L"Растущая луна",
                                     L"Полнолуние",     L"Убывающая луна",     L"Последняя четверть", L"Убывающий серп"};
    const int idx = static_cast<int>(age / 29.530588f * 8.0f + 0.5f) % 8;
    return names[idx];
}

// ---- почасовые данные и текущая погода ------------------------------------------------
inline HourWx Hour(int city, int day, int hour, int offsetMinutes)
{
    const DayWx d = Day(city, day);
    HourWx h;
    h.hour = hour;
    h.temp = TempAt(city, day, static_cast<float>(hour));
    const float v = Rand01(city * 13 + hour, day);
    h.cond = d.cond;
    // Облачность и осадки меняются в течение дня.
    if (d.cond == Cond::Showers)
        h.cond = v < 0.45f ? Cond::Showers : (v < 0.75f ? Cond::Cloudy : Cond::PartlyCloudy);
    else if (d.cond == Cond::Rain && v > 0.7f)
        h.cond = Cond::Cloudy;
    else if (d.cond == Cond::Storm)
        h.cond = v < 0.35f ? Cond::Storm : (v < 0.75f ? Cond::Rain : Cond::Cloudy);
    else if (d.cond == Cond::PartlyCloudy && v > 0.75f)
        h.cond = v > 0.9f ? Cond::Cloudy : Cond::Clear;
    else if (d.cond == Cond::Clear && v > 0.88f)
        h.cond = Cond::PartlyCloudy;
    else if (d.cond == Cond::Fog && (hour > 11 && hour < 20))
        h.cond = Cond::Cloudy;
    const bool wetHour = h.cond == Cond::Rain || h.cond == Cond::Showers || h.cond == Cond::Storm || h.cond == Cond::Snow;
    h.precip = Clamp(d.precip + static_cast<int>((v - 0.5f) * 30.0f) + (wetHour ? 15 : -20), 0, 100);
    h.wind = std::max(0.5f, d.wind * (0.7f + 0.6f * Rand01(city * 17 + hour, day)) * (hour >= 11 && hour <= 18 ? 1.15f : 0.85f));
    h.windDir = (d.windDir + static_cast<int>((Rand01(city * 19 + hour, day) - 0.5f) * 50.0f) + 360) % 360;
    h.humidity = Clamp(d.humidity + static_cast<int>((15.0f - h.temp + d.hi) * 0.6f) - 6, 10, 100);
    int rise = 0, set = 0, polar = 0;
    SunTimes(city, day, offsetMinutes, rise, set, &polar);
    const int m = hour * 60 + 30;
    h.night = polar == -1 || (polar == 0 && (m < rise || m > set));
    return h;
}

inline float FeelsLike(float t, float windMs, int humidity)
{
    const float v = windMs * 3.6f;
    if (t <= 10.0f && v > 4.8f)
    {
        const float p = std::pow(v, 0.16f);
        return 13.12f + 0.6215f * t - 11.37f * p + 0.3965f * t * p;
    }
    if (t >= 27.0f)
        return t + (static_cast<float>(humidity) - 40.0f) * 0.1f;
    return t - windMs * 0.2f;
}

inline float DewPoint(float t, int humidity)
{
    const float g = std::log(static_cast<float>(std::max(1, humidity)) / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

inline NowWx Now(int city)
{
    const Cities::City& c = Cities::kCities[Clamp(city, 0, Cities::kCityCount - 1)];
    NowWx n;
    int offset = 0;
    n.local = Cities::CityTime(c, offset);
    n.today = DayNumber(n.local);
    const float hour = static_cast<float>(n.local.wHour) + static_cast<float>(n.local.wMinute) / 60.0f;
    const HourWx h = Hour(city, n.today, n.local.wHour, offset);
    const DayWx d = Day(city, n.today);
    n.temp = TempAt(city, n.today, hour);
    n.cond = h.cond;
    n.wind = h.wind;
    n.gust = h.wind * 1.6f + 1.0f;
    n.windDir = h.windDir;
    n.humidity = h.humidity;
    n.feels = FeelsLike(n.temp, n.wind, n.humidity);
    n.dew = DewPoint(n.temp, n.humidity);
    const bool wet = n.cond == Cond::Rain || n.cond == Cond::Showers || n.cond == Cond::Storm;
    n.pressure = static_cast<int>(1016.0f + (Rand01(city * 23, n.today) - 0.5f) * 18.0f - (wet ? 8.0f : 0.0f));
    switch (n.cond)
    {
    case Cond::Clear:
        n.visibility = 30.0f;
        break;
    case Cond::PartlyCloudy:
        n.visibility = 25.0f;
        break;
    case Cond::Cloudy:
        n.visibility = 16.0f;
        break;
    case Cond::Fog:
        n.visibility = 0.8f;
        break;
    case Cond::Snow:
        n.visibility = 3.0f;
        break;
    default:
        n.visibility = 7.0f;
        break;
    }
    int polar = 0;
    SunTimes(city, n.today, offset, n.sunrise, n.sunset, &polar);
    const int m = n.local.wHour * 60 + n.local.wMinute;
    n.night = polar == -1 || (polar == 0 && (m < n.sunrise || m > n.sunset));
    n.uv = n.night ? 0 : static_cast<int>(static_cast<float>(d.uv) * std::sin(kPi * Saturate(static_cast<float>(m - n.sunrise) / std::max(1.0f, static_cast<float>(n.sunset - n.sunrise)))) + 0.5f);
    n.aqi = 15 + static_cast<int>(Rand01(city * 29, n.today) * 45.0f) + (c.wet < 0.2f ? 25 : 0);
    return n;
}

// ---- тексты и значки ----------------------------------------------------------------
inline const wchar_t* CondText(Cond c, bool night = false)
{
    switch (c)
    {
    case Cond::Clear:
        return night ? L"Ясно" : L"Солнечно";
    case Cond::PartlyCloudy:
        return L"Переменная облачность";
    case Cond::Cloudy:
        return L"Облачно";
    case Cond::Showers:
        return L"Ливни";
    case Cond::Rain:
        return L"Дождь";
    case Cond::Storm:
        return L"Гроза";
    case Cond::Snow:
        return L"Снег";
    case Cond::Fog:
        return L"Туман";
    }
    return L"";
}

inline const wchar_t* WindDirText(int deg)
{
    static const wchar_t* dirs[] = {L"С", L"СВ", L"В", L"ЮВ", L"Ю", L"ЮЗ", L"З", L"СЗ"};
    return dirs[((deg + 22) % 360) / 45];
}

inline const wchar_t* AqiText(int aqi)
{
    if (aqi <= 50)
        return L"Хорошее";
    if (aqi <= 100)
        return L"Удовлетворительное";
    return L"Вредно для чувствительных групп";
}

inline const wchar_t* UvText(int uv)
{
    if (uv <= 2)
        return L"Низкий";
    if (uv <= 5)
        return L"Умеренный";
    if (uv <= 7)
        return L"Высокий";
    if (uv <= 10)
        return L"Очень высокий";
    return L"Экстремальный";
}

// Цветной значок погоды (солнце, луна, облака, осадки), как в приложении «Погода».
// outline.a > 0 — контур (для светлого фона), рисуется силуэтом чуть большего размера.
inline void DrawCloud(Renderer& r, float cx, float cy, float s, Color c, Color outline = Color(0, 0, 0, 0))
{
    for (int pass = outline.a ? 0 : 1; pass < 2; ++pass)
    {
        const float g = pass == 0 ? std::max(1.0f, s * 0.035f) : 0.0f;
        const Color col = pass == 0 ? outline : c;
        r.FillCircle(cx - s * 0.18f, cy - s * 0.08f, s * 0.22f + g, col);
        r.FillCircle(cx + s * 0.1f, cy - s * 0.16f, s * 0.27f + g, col);
        r.FillCircle(cx + s * 0.32f, cy + 0.02f * s, s * 0.17f + g, col);
        r.FillRoundRect({cx - s * 0.4f - g, cy - s * 0.02f - g, s * 0.89f + 2.0f * g, s * 0.24f + 2.0f * g}, s * 0.12f + g, col);
    }
}

inline void DrawSun(Renderer& r, float cx, float cy, float s)
{
    const Color ray = Color::Hex(0xFFC83D);
    for (int i = 0; i < 8; ++i)
    {
        const float a = static_cast<float>(i) * kPi / 4.0f;
        r.Line(cx + std::cos(a) * s * 0.3f, cy + std::sin(a) * s * 0.3f, cx + std::cos(a) * s * 0.42f, cy + std::sin(a) * s * 0.42f, ray,
               std::max(1.0f, s * 0.06f));
    }
    r.FillCircle(cx, cy, s * 0.22f, Color::Hex(0xFFB900));
}

// Полумесяц: внешний круг без смещённого внутреннего (многоугольник из двух дуг).
inline void Crescent(Renderer& r, float cx, float cy, float rad, float dx, float dy, float innerRad, Color c)
{
    const float phi = std::atan2(dy, dx);
    std::vector<Point> pts;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i)
    {
        const float a = phi + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(steps);
        const Point q{cx + std::cos(a) * rad, cy + std::sin(a) * rad};
        if (Distance(q, {cx + dx, cy + dy}) >= innerRad)
            pts.push_back(q);
    }
    for (int i = 0; i <= steps; ++i)
    {
        const float a = phi + 2.0f * kPi - 2.0f * kPi * static_cast<float>(i) / static_cast<float>(steps);
        const Point q{cx + dx + std::cos(a) * innerRad, cy + dy + std::sin(a) * innerRad};
        if (Distance(q, {cx, cy}) < rad)
            pts.push_back(q);
    }
    if (pts.size() >= 3)
        r.FillPolygon(pts, c);
}

inline void DrawMoon(Renderer& r, float cx, float cy, float s, bool onLight = false)
{
    Crescent(r, cx, cy, s * 0.26f, s * 0.13f, -s * 0.1f, s * 0.22f, onLight ? Color::Hex(0xE3B53B) : Color::Hex(0xE8EDF5));
}

// onLight — значок на светлом фоне (карточки, светлая панель задач): облака с контуром.
inline void DrawIcon(Renderer& r, const Rect& rc, Cond c, bool night, bool onLight = false)
{
    const float s = std::min(rc.w, rc.h);
    const float cx = rc.CenterX(), cy = rc.CenterY();
    const Color cloudLight = onLight ? Color::Hex(0xEEF2F6) : Color::Hex(0xF2F5F9), cloudGray = onLight ? Color::Hex(0xC9D2DC) : Color::Hex(0xB7C2CF),
                cloudDark = Color::Hex(0x8C99A8);
    const Color edge = onLight ? Color::Hex(0x9AA8B8) : Color(0, 0, 0, 0);
    auto cloud = [&](float x, float y, float size, Color col) { DrawCloud(r, x, y, size, col, edge); };
    auto drops = [&](Color col, int n) {
        for (int i = 0; i < n; ++i)
        {
            const float x = cx - s * 0.2f + static_cast<float>(i) * s * 0.2f;
            r.Line(x, cy + s * 0.26f, x - s * 0.06f, cy + s * 0.42f, col, std::max(1.2f, s * 0.055f));
        }
    };
    switch (c)
    {
    case Cond::Clear:
        if (night)
            DrawMoon(r, cx, cy, s, onLight);
        else
            DrawSun(r, cx, cy, s);
        break;
    case Cond::PartlyCloudy:
        if (night)
            DrawMoon(r, cx - s * 0.14f, cy - s * 0.12f, s * 0.9f, onLight);
        else
            DrawSun(r, cx - s * 0.14f, cy - s * 0.14f, s * 0.85f);
        cloud(cx + s * 0.06f, cy + s * 0.1f, s * 0.78f, cloudLight);
        break;
    case Cond::Cloudy:
        cloud(cx - s * 0.1f, cy - s * 0.08f, s * 0.7f, cloudGray);
        cloud(cx + s * 0.06f, cy + s * 0.06f, s * 0.82f, cloudLight);
        break;
    case Cond::Showers:
        if (!night)
            DrawSun(r, cx - s * 0.16f, cy - s * 0.18f, s * 0.75f);
        cloud(cx + s * 0.04f, cy, s * 0.82f, cloudLight);
        drops(Color::Hex(0x3A96DD), 3);
        break;
    case Cond::Rain:
        cloud(cx, cy - s * 0.04f, s * 0.86f, cloudGray);
        drops(Color::Hex(0x3A96DD), 3);
        break;
    case Cond::Storm:
        cloud(cx, cy - s * 0.06f, s * 0.86f, cloudDark);
        r.FillPolygon({{cx + s * 0.02f, cy + s * 0.12f}, {cx - s * 0.12f, cy + s * 0.3f}, {cx - s * 0.01f, cy + s * 0.3f},
                       {cx - s * 0.08f, cy + s * 0.46f}, {cx + s * 0.14f, cy + s * 0.24f}, {cx + s * 0.02f, cy + s * 0.24f}},
                      Color::Hex(0xFFC83D));
        break;
    case Cond::Snow:
        cloud(cx, cy - s * 0.06f, s * 0.86f, cloudGray);
        for (int i = 0; i < 3; ++i)
        {
            const float x = cx - s * 0.2f + static_cast<float>(i) * s * 0.2f, y = cy + s * 0.32f + (i == 1 ? s * 0.06f : 0.0f);
            r.FillCircle(x, y, std::max(1.2f, s * 0.05f), Color::Hex(0xFFFFFF));
            r.StrokeCircle(x, y, std::max(1.2f, s * 0.05f), Color::Hex(0x9FB4CC), 1.0f);
        }
        break;
    case Cond::Fog:
        cloud(cx, cy - s * 0.12f, s * 0.8f, cloudGray);
        for (int i = 0; i < 3; ++i)
            r.Line(cx - s * 0.36f + static_cast<float>(i % 2) * s * 0.08f, cy + s * 0.16f + static_cast<float>(i) * s * 0.11f,
                   cx + s * 0.36f - static_cast<float>(i % 2) * s * 0.08f, cy + s * 0.16f + static_cast<float>(i) * s * 0.11f, cloudDark,
                   std::max(1.2f, s * 0.05f));
        break;
    }
}

// ---- основное местоположение (общее для «Погоды», виджетов и панели задач) ----------------
inline int& HomeCity()
{
    static int city = Cities::CityIndex(L"moscow");
    return city;
}

inline bool& Fahrenheit()
{
    static bool f = false;
    return f;
}

inline int ShowTemp(float c) { return static_cast<int>(std::lround(Fahrenheit() ? c * 9.0f / 5.0f + 32.0f : c)); }
inline std::wstring Deg(float c) { return std::to_wstring(ShowTemp(c)) + L"°"; }
inline std::wstring DegUnit(float c) { return std::to_wstring(ShowTemp(c)) + (Fahrenheit() ? L"°F" : L"°C"); }
} // namespace WeatherData
