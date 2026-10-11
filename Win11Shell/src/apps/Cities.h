#pragma once
// Города для «Часов» и «Погоды»: координаты, часовые пояса с переходом на летнее время и
// климатические параметры (для модели прогноза погоды).

#include "../Common.h"

namespace Cities
{
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
    float tMean; // среднегодовая температура, °C
    float tAmp;  // полуамплитуда годового хода (отрицательная — южное полушарие)
    float wet;   // склонность к осадкам 0..1
};

inline const City kCities[] = {
    {L"Москва", L"Россия", L"moscow", 55.75f, 37.62f, 180, Dst::None, 6.0f, 13.0f, 0.5f},
    {L"Санкт-Петербург", L"Россия", L"saint petersburg", 59.94f, 30.31f, 180, Dst::None, 6.0f, 12.0f, 0.55f},
    {L"Калининград", L"Россия", L"kaliningrad", 54.71f, 20.51f, 120, Dst::None, 8.0f, 9.0f, 0.55f},
    {L"Екатеринбург", L"Россия", L"yekaterinburg", 56.84f, 60.6f, 300, Dst::None, 3.0f, 16.0f, 0.45f},
    {L"Новосибирск", L"Россия", L"novosibirsk", 55.03f, 82.92f, 420, Dst::None, 2.0f, 19.0f, 0.4f},
    {L"Владивосток", L"Россия", L"vladivostok", 43.12f, 131.89f, 600, Dst::None, 5.0f, 16.0f, 0.4f},
    {L"Лондон", L"Великобритания", L"london", 51.51f, -0.13f, 0, Dst::Eu, 11.5f, 7.0f, 0.6f},
    {L"Париж", L"Франция", L"paris", 48.86f, 2.35f, 60, Dst::Eu, 12.5f, 8.0f, 0.5f},
    {L"Берлин", L"Германия", L"berlin", 52.52f, 13.4f, 60, Dst::Eu, 10.0f, 9.5f, 0.5f},
    {L"Рим", L"Италия", L"rome", 41.9f, 12.5f, 60, Dst::Eu, 16.0f, 8.5f, 0.35f},
    {L"Мадрид", L"Испания", L"madrid", 40.42f, -3.7f, 60, Dst::Eu, 15.0f, 10.0f, 0.25f},
    {L"Стамбул", L"Турция", L"istanbul", 41.01f, 28.98f, 180, Dst::None, 15.0f, 9.0f, 0.4f},
    {L"Рейкьявик", L"Исландия", L"reykjavik", 64.15f, -21.94f, 0, Dst::None, 5.0f, 5.5f, 0.65f},
    {L"Нью-Йорк", L"США", L"new york", 40.71f, -74.0f, -300, Dst::Us, 13.0f, 12.0f, 0.45f},
    {L"Чикаго", L"США", L"chicago", 41.88f, -87.63f, -360, Dst::Us, 10.5f, 14.0f, 0.45f},
    {L"Лос-Анджелес", L"США", L"los angeles", 34.05f, -118.24f, -480, Dst::Us, 18.5f, 4.0f, 0.12f},
    {L"Торонто", L"Канада", L"toronto", 43.65f, -79.38f, -300, Dst::Us, 9.0f, 13.0f, 0.45f},
    {L"Гонолулу", L"США", L"honolulu", 21.31f, -157.86f, -600, Dst::None, 25.5f, 2.0f, 0.3f},
    {L"Мехико", L"Мексика", L"mexico city", 19.43f, -99.13f, -360, Dst::None, 17.0f, 3.0f, 0.35f},
    {L"Сан-Паулу", L"Бразилия", L"sao paulo", -23.55f, -46.63f, -180, Dst::None, 20.0f, -3.5f, 0.55f},
    {L"Буэнос-Айрес", L"Аргентина", L"buenos aires", -34.6f, -58.38f, -180, Dst::None, 18.0f, -6.5f, 0.4f},
    {L"Кейптаун", L"ЮАР", L"cape town", -33.92f, 18.42f, 120, Dst::None, 17.0f, -4.0f, 0.3f},
    {L"Найроби", L"Кения", L"nairobi", -1.29f, 36.82f, 180, Dst::None, 19.0f, 1.5f, 0.4f},
    {L"Дубай", L"ОАЭ", L"dubai", 25.2f, 55.27f, 240, Dst::None, 28.0f, 8.5f, 0.04f},
    {L"Нью-Дели", L"Индия", L"new delhi", 28.61f, 77.21f, 330, Dst::None, 25.0f, 8.5f, 0.3f},
    {L"Бангкок", L"Таиланд", L"bangkok", 13.76f, 100.5f, 420, Dst::None, 28.5f, 1.5f, 0.55f},
    {L"Сингапур", L"Сингапур", L"singapore", 1.35f, 103.82f, 480, Dst::None, 27.5f, 0.5f, 0.75f},
    {L"Пекин", L"Китай", L"beijing", 39.9f, 116.4f, 480, Dst::None, 13.0f, 15.0f, 0.25f},
    {L"Сеул", L"Южная Корея", L"seoul", 37.57f, 126.98f, 540, Dst::None, 12.5f, 14.0f, 0.4f},
    {L"Токио", L"Япония", L"tokyo", 35.68f, 139.69f, 540, Dst::None, 16.0f, 10.0f, 0.5f},
    {L"Сидней", L"Австралия", L"sydney", -33.87f, 151.21f, 600, Dst::Au, 18.0f, -4.5f, 0.4f},
    {L"Окленд", L"Новая Зеландия", L"auckland", -36.85f, 174.76f, 720, Dst::Nz, 15.5f, -4.0f, 0.6f},
};

inline constexpr int kCityCount = static_cast<int>(sizeof(kCities) / sizeof(kCities[0]));

inline int CityIndex(const wchar_t* alias)
{
    for (int i = 0; i < kCityCount; ++i)
    {
        if (std::wcscmp(kCities[i].alias, alias) == 0)
            return i;
    }
    return 0;
}

// День недели (0 — воскресенье).
inline int DayOfWeek(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

inline int DaysInMonth(int y, int m)
{
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : d[m - 1];
}

inline int NthSunday(int y, int m, int n)
{
    const int first = DayOfWeek(y, m, 1);
    return 1 + (7 - first) % 7 + 7 * (n - 1);
}

inline int LastSunday(int y, int m)
{
    const int last = DaysInMonth(y, m);
    return last - DayOfWeek(y, m, last);
}

inline SYSTEMTIME UtcShifted(int minutes)
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
inline bool InDst(Dst rule, const SYSTEMTIME& t, int offset)
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

inline SYSTEMTIME CityTime(const City& c, int& offset)
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

inline int LocalOffset()
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
} // namespace Cities
