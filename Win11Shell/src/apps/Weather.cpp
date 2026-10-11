// Погода Windows 11: прогноз с баннером текущей погоды, ежедневный прогноз на 10 дней, почасовой
// график (температура, осадки, ветер, влажность), плитки подробностей (восход и закат, Луна,
// УФ-индекс, ветер, давление...), карта температуры, климат по месяцам, избранные места и поиск.
// Данные моделируются WeatherData по климату города — без сети.

#include "../Shell.h"
#include "AppFactories.h"
#include "ImmUi.h"
#include "WeatherData.h"
#include "WorldMap.h"

namespace
{
using namespace WeatherData;
using Cities::kCities;
using Cities::kCityCount;

constexpr float kTitleH = 48.0f;
constexpr float kRailW = 72.0f;
constexpr float kPad = 28.0f;

const wchar_t* const kWeekShort[7] = {L"Вс", L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб"};
const wchar_t* const kWeekLong[7] = {L"воскресенье", L"понедельник", L"вторник", L"среда", L"четверг", L"пятница", L"суббота"};
const wchar_t* const kMonthGen[12] = {L"января", L"февраля", L"марта",    L"апреля",  L"мая",    L"июня",
                                      L"июля",   L"августа", L"сентября", L"октября", L"ноября", L"декабря"};
const wchar_t* const kMonthShort[12] = {L"Янв", L"Фев", L"Мар", L"Апр", L"Май", L"Июн", L"Июл", L"Авг", L"Сен", L"Окт", L"Ноя", L"Дек"};

enum class Page
{
    Forecast,
    Maps,
    History,
    Favorites,
    Settings,
};

enum Hk : int
{
    kHkNone = 0,
    kHkNav,
    kHkSuggest,
    kHkFav,
    kHkDay,
    kHkDayPrev,
    kHkDayNext,
    kHkTab,
    kHkFavCard,
    kHkFavRemove,
    kHkFavAdd,
    kHkUnit,
    kHkHome,
};

std::vector<int>& Favorites()
{
    static std::vector<int> v = {Cities::CityIndex(L"moscow"), Cities::CityIndex(L"saint petersburg"), Cities::CityIndex(L"london"),
                                 Cities::CityIndex(L"new york"), Cities::CityIndex(L"tokyo")};
    return v;
}

std::wstring HhMm(int minutes)
{
    const int m = ((minutes % 1440) + 1440) % 1440;
    return Pad2(m / 60) + L":" + Pad2(m % 60);
}

std::wstring CityTitle(int city) { return std::wstring(kCities[city].name) + L", " + kCities[city].country; }

void SkyColors(Cond c, bool night, Color& top, Color& bottom)
{
    if (night)
    {
        top = Color::Hex(0x0B1630);
        bottom = c == Cond::Clear ? Color::Hex(0x26457A) : Color::Hex(0x2B3A52);
        return;
    }
    switch (c)
    {
    case Cond::Clear:
        top = Color::Hex(0x2E7FE0);
        bottom = Color::Hex(0x86C3F6);
        break;
    case Cond::PartlyCloudy:
        top = Color::Hex(0x3F82CF);
        bottom = Color::Hex(0x9BC4EA);
        break;
    case Cond::Cloudy:
    case Cond::Fog:
        top = Color::Hex(0x5F7891);
        bottom = Color::Hex(0xA3B5C7);
        break;
    case Cond::Snow:
        top = Color::Hex(0x7891AD);
        bottom = Color::Hex(0xC9D7E6);
        break;
    default:
        top = Color::Hex(0x45607C);
        bottom = Color::Hex(0x7D93AA);
        break;
    }
}

// Цвет шкалы температуры (карта, климат).
Color TempColor(float t, uint8_t alpha = 255)
{
    struct Stop
    {
        float t;
        uint32_t c;
    };
    static const Stop stops[] = {{-30.0f, 0x5E3C99}, {-15.0f, 0x3B6FD8}, {0.0f, 0x74B9F0}, {10.0f, 0x7CCB8A},
                                 {20.0f, 0xF2D45C}, {30.0f, 0xF28E3C}, {40.0f, 0xD7303A}};
    if (t <= stops[0].t)
        return Color::Hex(stops[0].c, alpha);
    for (int i = 0; i < 6; ++i)
    {
        if (t <= stops[i + 1].t)
        {
            const float k = (t - stops[i].t) / (stops[i + 1].t - stops[i].t);
            return LerpColor(Color::Hex(stops[i].c, alpha), Color::Hex(stops[i + 1].c, alpha), k);
        }
    }
    return Color::Hex(stops[6].c, alpha);
}

// Модельная температура для карты (по широте, сезону и местному солнечному времени).
float FieldTemp(float lat, float lon, int doy, float utcHour)
{
    const float alat = std::fabs(lat);
    const float base = 27.0f - 0.0058f * alat * alat;
    const float season = -(lat >= 0.0f ? 1.0f : -1.0f) * alat * 0.3f * std::cos(2.0f * kPi * static_cast<float>(doy - 15) / 365.0f);
    float solar = utcHour + lon / 15.0f;
    solar = std::fmod(solar + 48.0f, 24.0f);
    const float diurnal = 4.0f * std::cos(2.0f * kPi * (solar - 15.0f) / 24.0f);
    const float noise = 3.0f * std::sin(lon * 0.07f + lat * 0.11f) + 2.0f * std::cos(lon * 0.13f - lat * 0.05f);
    return base + season + diurnal + noise;
}

class WeatherWindow : public Window
{
public:
    explicit WeatherWindow(const std::wstring& arg) : Window(AppId::Weather, L"Погода")
    {
        SetInitialSize(1180.0f, 780.0f);
        m_minSize = {600.0f, 520.0f};
        m_city = HomeCity();
        m_search = m_ui.Add<TextBox>(L"Поиск местоположений");
        m_search->leadingIcon = Icon::Search;
        m_search->onChange = [this](const std::wstring&) { m_suggestSel = 0; };
        m_search->onEnter = [this]() { PickSuggestion(m_suggestSel); };
        m_search->onEscape = [this]() {
            m_search->SetText(L"");
            m_ui.SetFocus(nullptr);
        };
        m_search->onKey = [this](UINT vk, const KeyMods&) {
            if (vk != VK_DOWN && vk != VK_UP)
                return false;
            const int n = static_cast<int>(Suggestions().size());
            if (n > 0)
                m_suggestSel = Clamp(m_suggestSel + (vk == VK_DOWN ? 1 : -1), 0, n - 1);
            return true;
        };
        OnLaunchArgs(arg);
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnLaunchArgs(const std::wstring& arg) override
    {
        const std::wstring a = ToLower(Trim(arg));
        if (a.empty())
            return;
        for (int i = 0; i < kCityCount; ++i)
        {
            if (a == kCities[i].alias || a == ToLower(kCities[i].name))
            {
                OpenCity(i);
                return;
            }
        }
    }

    void OnResize() override
    {
        const float w = std::min(440.0f, Width() - 46.0f * 3.0f - 200.0f);
        m_search->SetBounds({std::floor((Width() - w) * 0.5f), 8.0f, w, 32.0f});
        m_search->SetVisible(w > 160.0f);
    }

private:
    // ================================================================== общее
    Rect ContentRect() const { return {kRailW, kTitleH, Width() - kRailW, Height() - kTitleH}; }
    float MaxScroll() const { return std::max(0.0f, m_contentH - ContentRect().h); }

    void SetPage(Page pg)
    {
        if (pg != m_page)
            m_scroll = 0.0f;
        m_page = pg;
    }

    void OpenCity(int city)
    {
        m_city = Clamp(city, 0, kCityCount - 1);
        m_selDay = 0;
        m_dayScroll = 0;
        m_scroll = 0.0f;
        m_page = Page::Forecast;
    }

    std::vector<int> Suggestions() const
    {
        std::vector<int> out;
        const std::wstring q = Trim(m_search->Text());
        if (q.empty())
            return out;
        for (int i = 0; i < kCityCount && out.size() < 8; ++i)
        {
            if (ContainsNoCase(kCities[i].name, q) || ContainsNoCase(kCities[i].alias, q) || ContainsNoCase(kCities[i].country, q))
                out.push_back(i);
        }
        return out;
    }

    void PickSuggestion(int i)
    {
        const std::vector<int> s = Suggestions();
        if (i < 0 || i >= static_cast<int>(s.size()))
            return;
        OpenCity(s[static_cast<size_t>(i)]);
        m_search->SetText(L"");
        m_ui.SetFocus(nullptr);
    }

    int CityOffset(int city) const
    {
        int off = 0;
        (void)Cities::CityTime(kCities[city], off);
        return off;
    }

    // Почасовые данные для графика: от текущего часа (сегодня) или весь выбранный день.
    std::vector<HourWx> ChartHours(const NowWx& now) const
    {
        std::vector<HourWx> v;
        const int off = CityOffset(m_city);
        int day = now.today + m_selDay, hour = m_selDay == 0 ? now.local.wHour : 0;
        for (int i = 0; i < 24; ++i)
        {
            v.push_back(Hour(m_city, day, hour, off));
            if (++hour >= 24)
            {
                hour = 0;
                ++day;
            }
        }
        return v;
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        const Rect full{0.0f, 0.0f, Width(), Height()};
        m_imm.Begin(full);
        r.DrawAppIcon(AppIcon::Weather, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(L"Погода", {48.0f, 0.0f, 120.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        DrawRail(r);
        const Rect c = ContentRect();
        Imm::ContentLayer(r, c);
        r.PushClip(c);
        m_imm.SetClip(c);
        m_contentH = 0.0f;
        switch (m_page)
        {
        case Page::Forecast:
            DrawForecast(r, c);
            break;
        case Page::Maps:
            DrawMaps(r, c);
            break;
        case Page::History:
            DrawHistory(r, c);
            break;
        case Page::Favorites:
            DrawFavorites(r, c);
            break;
        case Page::Settings:
            DrawSettings(r, c);
            break;
        }
        Imm::Scrollbar(r, c, m_scroll, m_contentH);
        r.PopClip();
        m_imm.ResetClip();
        m_ui.Draw(r);
        DrawSuggestions(r);
    }

    void DrawRail(Renderer& r)
    {
        const Palette& p = Theme::P();
        struct Item
        {
            Page page;
            Icon icon;
            const wchar_t* text;
        };
        static const Item items[] = {{Page::Forecast, Icon::PartlyCloudy, L"Прогноз"},
                                     {Page::Maps, Icon::Location, L"Карты"},
                                     {Page::History, Icon::Chart, L"История"},
                                     {Page::Favorites, Icon::Star, L"Избранное"},
                                     {Page::Settings, Icon::Settings, L"Параметры"}};
        for (int i = 0; i < 5; ++i)
        {
            const bool settings = i == 4;
            const Rect rr = settings ? Rect{4.0f, Height() - 64.0f, kRailW - 8.0f, 58.0f}
                                     : Rect{4.0f, kTitleH + 4.0f + static_cast<float>(i) * 62.0f, kRailW - 8.0f, 58.0f};
            const bool sel = m_page == items[i].page;
            if (m_imm.Pressed(kHkNav, i))
                r.FillRoundRect(rr, 6.0f, p.subtlePressed);
            else if (sel || m_imm.Hot(kHkNav, i))
                r.FillRoundRect(rr, 6.0f, p.subtleHover);
            if (sel)
                r.FillRoundRect({rr.x, rr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Glyph(items[i].icon, {rr.CenterX() - 10.0f, rr.y + 10.0f, 20.0f, 20.0f}, sel ? p.accentText : p.text);
            r.Text(items[i].text, {rr.x, rr.y + 34.0f, rr.w, 16.0f}, sel ? p.text : p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Center);
            m_imm.Add(rr, kHkNav, i);
        }
    }

    void DrawSuggestions(Renderer& r)
    {
        if (!m_search->Visible() || !m_search->Focused())
            return;
        const std::vector<int> s = Suggestions();
        if (s.empty() && Trim(m_search->Text()).empty())
            return;
        const Palette& p = Theme::P();
        const Rect sb = m_search->Bounds();
        const float rows = static_cast<float>(std::max<size_t>(1, s.size()));
        const Rect box{sb.x, sb.Bottom() + 4.0f, sb.w, rows * 40.0f + 8.0f};
        r.Shadow(box, 8.0f, 16.0f, 6.0f, p.dark ? 0.5f : 0.22f);
        r.Acrylic(box, 8.0f, p.menuTint, p.menuAmount);
        r.StrokeRoundRect(box, 8.0f, p.flyoutStroke);
        if (s.empty())
        {
            r.Text(L"Ничего не найдено", {box.x, box.y + 4.0f, box.w, 40.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
            return;
        }
        for (int i = 0; i < static_cast<int>(s.size()); ++i)
        {
            const Rect row{box.x + 4.0f, box.y + 4.0f + static_cast<float>(i) * 40.0f, box.w - 8.0f, 40.0f};
            if (i == m_suggestSel || m_imm.Hot(kHkSuggest, i))
                r.FillRoundRect(row, 4.0f, p.subtleHover);
            if (i == m_suggestSel)
                r.FillRoundRect({row.x, row.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Glyph(Icon::Location, {row.x + 12.0f, row.CenterY() - 8.0f, 16.0f, 16.0f}, p.textSecondary);
            const int city = s[static_cast<size_t>(i)];
            r.Text(CityTitle(city), {row.x + 40.0f, row.y, row.w - 120.0f, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            const NowWx n = Now(city);
            DrawIcon(r, {row.Right() - 92.0f, row.CenterY() - 12.0f, 24.0f, 24.0f}, n.cond, n.night, !p.dark);
            r.Text(Deg(n.temp), {row.Right() - 60.0f, row.y, 48.0f, row.h}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter | TextFlags::Right);
            m_imm.Add(row, kHkSuggest, i);
        }
    }

    // ---- прогноз -----------------------------------------------------------------
    void DrawForecast(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const NowWx now = Now(m_city);
        const float x0 = c.x + kPad, w = c.w - 2.0f * kPad;
        float y = c.y + 18.0f - m_scroll;
        // Заголовок места
        const std::wstring title = CityTitle(m_city);
        r.Text(title, {x0, y, w - 120.0f, 34.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        const float tw = r.TextWidth(title, 20.0f, FontWeight::Semibold);
        const bool fav = std::find(Favorites().begin(), Favorites().end(), m_city) != Favorites().end();
        Imm::SubtleButton(r, m_imm, {x0 + tw + 10.0f, y + 1.0f, 32.0f, 32.0f}, Icon::Star, kHkFav, 0,
                          fav ? L"Удалить из избранного" : L"Добавить в избранное", false, fav ? Color::Hex(0xF2B200) : Color(0, 0, 0, 0));
        if (m_city == HomeCity())
        {
            r.Glyph(Icon::Home, {x0 + tw + 52.0f, y + 9.0f, 16.0f, 16.0f}, p.textSecondary);
            r.Text(L"Основное местоположение", {x0 + tw + 74.0f, y, 220.0f, 34.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        y += 46.0f;
        DrawHero(r, {x0, y, w, 300.0f}, now);
        y += 324.0f;
        // Ежедневный прогноз
        r.Text(L"Ежедневный прогноз", {x0, y, w, 24.0f}, p.text, 16.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 36.0f;
        y = DrawDays(r, x0, y, w, now) + 28.0f;
        // Почасовой график
        DrawChartCard(r, {x0, y, w, 340.0f}, now);
        y += 364.0f;
        // Подробности
        r.Text(L"Подробности о погоде", {x0, y, w, 24.0f}, p.text, 16.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 36.0f;
        y = DrawTiles(r, x0, y, w, now);
        m_contentH = y + m_scroll - c.y + 32.0f;
    }

    void DrawHero(Renderer& r, const Rect& h, const NowWx& now)
    {
        Color top, bottom;
        SkyColors(now.cond, now.night, top, bottom);
        r.BeginRounded(h, 8.0f);
        r.FillGradientV(h, 8.0f, top, bottom);
        r.PushClip(h);
        // Декор: солнечный ореол, звёзды, облака, осадки.
        if (now.cond == Cond::Clear && !now.night)
        {
            for (int k = 0; k < 6; ++k)
                r.FillCircle(h.Right() - 120.0f, h.y + 40.0f, 60.0f + static_cast<float>(k) * 34.0f, Color(255, 236, 170, static_cast<uint8_t>(26 - k * 4)));
        }
        if (now.night)
        {
            for (int k = 0; k < 60; ++k)
            {
                const float sx = h.x + Rand01(k, 7) * h.w, sy = h.y + Rand01(k, 9) * h.h * 0.7f;
                r.FillCircle(sx, sy, 0.6f + Rand01(k, 11) * 1.0f, Color(255, 255, 255, static_cast<uint8_t>(80 + Rand01(k, 13) * 140.0f)));
            }
        }
        if (now.cond != Cond::Clear)
        {
            // Мягкие облака: несколько вложенных полупрозрачных эллипсов дают размытый край.
            const uint8_t a = now.cond == Cond::PartlyCloudy ? 0x08 : 0x0C;
            for (int k = 0; k < 8; ++k)
            {
                const float cx = h.x + h.w * (0.08f + 0.13f * static_cast<float>(k)) + Rand01(k, 3) * 40.0f;
                const float cy = h.y + 10.0f + Rand01(k, 5) * 70.0f;
                const float s = 110.0f + Rand01(k, 6) * 90.0f;
                for (int layer = 0; layer < 5; ++layer)
                {
                    const float f = 1.0f - static_cast<float>(layer) * 0.16f;
                    r.FillEllipse({cx - s * f, cy - s * 0.42f * f, s * 2.0f * f, s * 0.84f * f}, Color(255, 255, 255, a));
                }
            }
        }
        if (now.cond == Cond::Rain || now.cond == Cond::Showers || now.cond == Cond::Storm)
        {
            for (int k = 0; k < 90; ++k)
            {
                const float sx = h.x + Rand01(k, 21) * h.w, sy = h.y + Rand01(k, 23) * h.h;
                r.Line(sx, sy, sx - 6.0f, sy + 16.0f, Color(255, 255, 255, 0x30), 1.2f);
            }
        }
        if (now.cond == Cond::Snow)
        {
            for (int k = 0; k < 80; ++k)
                r.FillCircle(h.x + Rand01(k, 31) * h.w, h.y + Rand01(k, 33) * h.h, 1.0f + Rand01(k, 35) * 2.0f, Color(255, 255, 255, 0x90));
        }
        r.PopClip();
        r.EndRounded();
        const Color white(255, 255, 255), soft(255, 255, 255, 0xD0);
        DrawIcon(r, {h.x + 28.0f, h.y + 36.0f, 104.0f, 104.0f}, now.cond, now.night);
        const std::wstring t = std::to_wstring(ShowTemp(now.temp));
        r.Text(t, {h.x + 146.0f, h.y + 30.0f, 200.0f, 110.0f}, white, 88.0f, FontWeight::Light, TextFlags::VCenter);
        const float tw = r.TextWidth(t, 88.0f, FontWeight::Light);
        r.Text(Fahrenheit() ? L"°F" : L"°C", {h.x + 150.0f + tw, h.y + 48.0f, 60.0f, 30.0f}, white, 22.0f, FontWeight::Regular);
        r.Text(CondText(now.cond, now.night), {h.x + 32.0f, h.y + 156.0f, h.w * 0.5f, 30.0f}, white, 22.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(L"Ощущается как " + Deg(now.feels), {h.x + 32.0f, h.y + 188.0f, h.w * 0.5f, 22.0f}, white, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        const DayWx d = Day(m_city, now.today);
        std::wstring summary = std::wstring(L"Сегодня: ") + ToLower(CondText(d.cond)) + L". Максимум " + Deg(d.hi) + L", минимум " + Deg(d.lo) + L".";
        if (d.precip >= 40)
            summary += L" Вероятность осадков " + std::to_wstring(d.precip) + L"%.";
        const float sumW = h.w >= 900.0f ? h.w - 540.0f : h.w - 64.0f;
        r.Text(summary, {h.x + 32.0f, h.y + 216.0f, sumW, 44.0f}, soft, 14.0f, FontWeight::Regular, TextFlags::Wrap);
        r.Text(L"Обновлено в " + Pad2(now.local.wHour) + L":" + Pad2(now.local.wMinute) + L" (местное время)", {h.x + 32.0f, h.Bottom() - 32.0f, 400.0f, 20.0f},
               Color(255, 255, 255, 0xB0), 12.0f, FontWeight::Regular, TextFlags::VCenter);
        if (h.w < 900.0f)
            return;
        // Сводка справа
        struct Item
        {
            const wchar_t* label;
            std::wstring value;
        };
        const Item items[] = {
            {L"Качество воздуха", std::to_wstring(now.aqi) + L" · " + AqiText(now.aqi)},
            {L"Ветер", std::wstring(WindDirText(now.windDir)) + L" " + std::to_wstring(static_cast<int>(std::lround(now.wind))) + L" м/с"},
            {L"Влажность", std::to_wstring(now.humidity) + L"%"},
            {L"Видимость", std::to_wstring(static_cast<int>(std::lround(std::max(1.0f, now.visibility)))) + L" км"},
            {L"Давление", std::to_wstring(now.pressure) + L" гПа"},
            {L"Точка росы", Deg(now.dew)},
        };
        const float gx = h.Right() - 492.0f;
        r.FillRoundRect({gx - 16.0f, h.y + 28.0f, 480.0f, 244.0f}, 8.0f, Color(255, 255, 255, 0x1C));
        for (int i = 0; i < 6; ++i)
        {
            const float cx = gx + static_cast<float>(i % 2) * 232.0f, cy = h.y + 44.0f + static_cast<float>(i / 2) * 76.0f;
            r.Text(items[i].label, {cx, cy, 220.0f, 20.0f}, soft, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(items[i].value, {cx, cy + 22.0f, 220.0f, 28.0f}, white, 17.0f, FontWeight::Semibold, TextFlags::VCenter);
        }
    }

    float DrawDays(Renderer& r, float x0, float y, float w, const NowWx& now)
    {
        const Palette& p = Theme::P();
        const float cw = 112.0f, ch = 160.0f, gap = 8.0f;
        const int fit = std::max(1, static_cast<int>((w - 80.0f + gap) / (cw + gap)));
        const bool arrows = fit < 10;
        const int visible = std::min(10, arrows ? fit : 10);
        m_dayScroll = Clamp(m_dayScroll, 0, 10 - visible);
        float x = x0 + (arrows ? 40.0f : 0.0f);
        if (arrows)
        {
            if (m_dayScroll > 0)
                Imm::SubtleButton(r, m_imm, {x0, y + ch * 0.5f - 18.0f, 32.0f, 36.0f}, Icon::ChevronLeft, kHkDayPrev, 0, L"Назад");
            if (m_dayScroll + visible < 10)
                Imm::SubtleButton(r, m_imm, {x + static_cast<float>(visible) * (cw + gap), y + ch * 0.5f - 18.0f, 32.0f, 36.0f}, Icon::ChevronRight,
                                  kHkDayNext, 0, L"Вперёд");
        }
        for (int k = 0; k < visible; ++k)
        {
            const int i = k + m_dayScroll;
            const DayWx d = Day(m_city, now.today + i);
            const SYSTEMTIME date = DateOf(now.today + i);
            const Rect rc{x + static_cast<float>(k) * (cw + gap), y, cw, ch};
            const bool sel = i == m_selDay, hot = m_imm.Hot(kHkDay, i);
            r.FillRoundRect(rc, 8.0f, sel ? (p.dark ? Color(255, 255, 255, 0x18) : Color(255, 255, 255)) : (hot ? p.controlHover : p.card));
            r.StrokeRoundRect(rc, 8.0f, sel ? p.accent : p.cardStroke, sel ? 2.0f : 1.0f);
            const std::wstring dayName = i == 0 ? L"Сегодня" : (i == 1 ? L"Завтра" : kWeekShort[date.wDayOfWeek]);
            r.Text(dayName, {rc.x, rc.y + 12.0f, rc.w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::Center);
            r.Text(std::to_wstring(date.wDay) + L" " + kMonthGen[date.wMonth - 1], {rc.x, rc.y + 32.0f, rc.w, 18.0f}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::Center);
            DrawIcon(r, {rc.CenterX() - 22.0f, rc.y + 54.0f, 44.0f, 44.0f}, d.cond, false, !p.dark);
            const std::wstring hi = Deg(d.hi), lo = Deg(d.lo);
            const float hw = r.TextWidth(hi, 16.0f, FontWeight::Semibold), lw = r.TextWidth(lo, 14.0f);
            const float tx = rc.CenterX() - (hw + lw + 8.0f) * 0.5f;
            r.Text(hi, {tx, rc.y + 104.0f, hw + 2.0f, 22.0f}, p.text, 16.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(lo, {tx + hw + 8.0f, rc.y + 105.0f, lw + 2.0f, 22.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Glyph(Icon::Drop, {rc.CenterX() - 22.0f, rc.y + 133.0f, 12.0f, 12.0f}, p.accentText);
            r.Text(std::to_wstring(d.precip) + L"%", {rc.CenterX() - 6.0f, rc.y + 128.0f, 40.0f, 22.0f}, p.accentText, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter);
            m_imm.Add(rc, kHkDay, i);
        }
        return y + ch;
    }

    void DrawChartCard(Renderer& r, const Rect& card, const NowWx& now)
    {
        const Palette& p = Theme::P();
        Imm::Card(r, card);
        static const wchar_t* tabs[] = {L"Обзор", L"Осадки", L"Ветер", L"Влажность"};
        float tx = card.x + 20.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float w = r.TextWidth(tabs[i], 14.0f, i == m_tab ? FontWeight::Semibold : FontWeight::Regular) + 24.0f;
            const Rect tr{tx, card.y + 14.0f, w, 34.0f};
            if (m_imm.Hot(kHkTab, i) && i != m_tab)
                r.FillRoundRect(tr, 4.0f, p.subtleHover);
            r.Text(tabs[i], tr, i == m_tab ? p.text : p.textSecondary, 14.0f, i == m_tab ? FontWeight::Semibold : FontWeight::Regular, TextFlags::Middle);
            if (i == m_tab)
                r.FillRoundRect({tr.CenterX() - 10.0f, tr.Bottom() - 3.0f, 20.0f, 3.0f}, 1.5f, p.accent);
            m_imm.Add(tr, kHkTab, i);
            tx += w + 4.0f;
        }
        const SYSTEMTIME date = DateOf(now.today + m_selDay);
        const std::wstring when = (m_selDay == 0 ? std::wstring(L"Ближайшие 24 часа") : std::wstring(kWeekLong[date.wDayOfWeek]) + L", " +
                                                                                           std::to_wstring(date.wDay) + L" " + kMonthGen[date.wMonth - 1]);
        r.Text(when, {card.Right() - 320.0f, card.y + 14.0f, 300.0f, 34.0f}, p.textSecondary, 13.0f, FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
        const Rect area{card.x + 24.0f, card.y + 64.0f, card.w - 48.0f, card.h - 84.0f};
        m_chartArea = area;
        DrawChart(r, area, ChartHours(now));
    }

    void DrawChart(Renderer& r, const Rect& area, const std::vector<HourWx>& hours)
    {
        const Palette& p = Theme::P();
        const int n = static_cast<int>(hours.size());
        if (n == 0)
            return;
        const float step = area.w / static_cast<float>(n);
        const int stride = step < 30.0f ? 3 : (step < 44.0f ? 2 : 1);
        auto px = [&](int i) { return area.x + step * (static_cast<float>(i) + 0.5f); };
        const float plotTop = area.y + 64.0f, plotBottom = area.Bottom() - 30.0f;
        // Подписи часов
        for (int i = 0; i < n; i += stride)
            r.Text(i == 0 && m_selDay == 0 ? std::wstring(L"Сейчас") : Pad2(hours[static_cast<size_t>(i)].hour) + L":00",
                   {px(i) - 30.0f, area.Bottom() - 22.0f, 60.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center);
        r.FillRect({area.x, plotBottom + 2.0f, area.w, 1.0f}, p.divider);
        std::vector<float> values;
        for (const HourWx& h : hours)
        {
            switch (m_tab)
            {
            case 0:
                values.push_back(h.temp);
                break;
            case 1:
                values.push_back(static_cast<float>(h.precip));
                break;
            case 2:
                values.push_back(h.wind);
                break;
            default:
                values.push_back(static_cast<float>(h.humidity));
                break;
            }
        }
        float mn = *std::min_element(values.begin(), values.end()), mx = *std::max_element(values.begin(), values.end());
        if (m_tab == 1 || m_tab == 3)
        {
            mn = 0.0f;
            mx = 100.0f;
        }
        else
        {
            mn -= m_tab == 2 ? std::min(mn, 1.0f) : 2.0f;
            mx += 2.0f;
        }
        auto py = [&](float v) { return plotBottom - (v - mn) / std::max(0.01f, mx - mn) * (plotBottom - plotTop); };
        if (m_tab == 1 || m_tab == 3)
        {
            // Столбцы (осадки, влажность)
            for (int i = 0; i < n; ++i)
            {
                const float v = values[static_cast<size_t>(i)];
                const float bw = std::max(4.0f, step * 0.46f);
                const Rect bar{px(i) - bw * 0.5f, py(v), bw, plotBottom - py(v)};
                r.FillRoundRect(bar, std::min(3.0f, bw * 0.5f), (i == m_chartHover ? p.accent : p.accent.MulAlpha(m_tab == 1 ? 0.55f : 0.35f)));
                if (i % stride == 0)
                {
                    r.Text(std::to_wstring(static_cast<int>(v)) + L"%", {px(i) - 24.0f, py(v) - 22.0f, 48.0f, 18.0f}, p.text, 12.0f, FontWeight::Regular,
                           TextFlags::Center);
                    if (m_tab == 1)
                        DrawIcon(r, {px(i) - 11.0f, area.y + 4.0f, 22.0f, 22.0f}, hours[static_cast<size_t>(i)].cond, hours[static_cast<size_t>(i)].night, !p.dark);
                }
            }
        }
        else
        {
            std::vector<Gdiplus::PointF> pts;
            for (int i = 0; i < n; ++i)
                pts.push_back({px(i), py(values[static_cast<size_t>(i)])});
            const Color lineC = m_tab == 0 ? p.accent : (p.dark ? Color::Hex(0x9FB4CC) : Color::Hex(0x5B7A99));
            Gdiplus::GraphicsPath fill;
            fill.AddCurve(pts.data(), static_cast<INT>(pts.size()), 0.5f);
            fill.AddLine(pts.back().X, pts.back().Y, pts.back().X, plotBottom);
            fill.AddLine(pts.back().X, plotBottom, pts.front().X, plotBottom);
            fill.CloseFigure();
            Gdiplus::LinearGradientBrush grad(Gdiplus::PointF(0.0f, plotTop - 10.0f), Gdiplus::PointF(0.0f, plotBottom),
                                              r.Gp(lineC.WithAlpha(0x55)), r.Gp(lineC.WithAlpha(0x00)));
            r.FillPath(fill, grad);
            Gdiplus::GraphicsPath line;
            line.AddCurve(pts.data(), static_cast<INT>(pts.size()), 0.5f);
            r.StrokePath(line, lineC, 2.0f);
            for (int i = 0; i < n; i += stride)
            {
                const Point q{pts[static_cast<size_t>(i)].X, pts[static_cast<size_t>(i)].Y};
                r.FillCircle(q.x, q.y, 3.0f, lineC);
                const HourWx& h = hours[static_cast<size_t>(i)];
                if (m_tab == 0)
                {
                    r.Text(Deg(h.temp), {q.x - 24.0f, q.y - 26.0f, 48.0f, 20.0f}, p.text, 13.0f, FontWeight::Semibold, TextFlags::Center);
                    DrawIcon(r, {q.x - 12.0f, q.y - 54.0f, 24.0f, 24.0f}, h.cond, h.night, !p.dark);
                }
                else
                {
                    r.Text(std::to_wstring(static_cast<int>(std::lround(h.wind))), {q.x - 24.0f, q.y - 24.0f, 48.0f, 18.0f}, p.text, 12.0f,
                           FontWeight::Semibold, TextFlags::Center);
                    DrawWindArrow(r, q.x, area.y + 14.0f, 8.0f, h.windDir, p.textSecondary);
                }
            }
            if (m_tab == 2)
                r.Text(L"м/с", {area.x, area.y - 8.0f, 60.0f, 16.0f}, p.textSecondary, 11.0f);
        }
        // Подсказка при наведении
        if (m_chartHover >= 0 && m_chartHover < n)
        {
            const HourWx& h = hours[static_cast<size_t>(m_chartHover)];
            const float hx = px(m_chartHover);
            r.FillRect({std::floor(hx), plotTop - 8.0f, 1.0f, plotBottom - plotTop + 10.0f}, p.textTertiary);
            const std::wstring l1 = Pad2(h.hour) + L":00 · " + CondText(h.cond, h.night);
            const std::wstring l2 = Deg(h.temp) + L" · осадки " + std::to_wstring(h.precip) + L"% · " + std::to_wstring(static_cast<int>(std::lround(h.wind))) +
                                    L" м/с · " + std::to_wstring(h.humidity) + L"%";
            const float bw = std::max(r.TextWidth(l1, 12.0f, FontWeight::Semibold), r.TextWidth(l2, 12.0f)) + 24.0f;
            Rect tip{hx + 10.0f, plotTop, bw, 48.0f};
            if (tip.Right() > area.Right())
                tip.x = hx - 10.0f - bw;
            r.Shadow(tip, 6.0f, 12.0f, 4.0f, 0.2f);
            r.FillRoundRect(tip, 6.0f, p.dark ? Color::Hex(0x2C2C2C) : Color::Hex(0xFFFFFF));
            r.StrokeRoundRect(tip, 6.0f, p.surfaceStroke);
            r.Text(l1, {tip.x + 12.0f, tip.y + 5.0f, bw, 20.0f}, p.text, 12.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(l2, {tip.x + 12.0f, tip.y + 24.0f, bw, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
    }

    static void DrawWindArrow(Renderer& r, float cx, float cy, float s, int fromDeg, Color c)
    {
        // Стрелка показывает, куда дует ветер.
        const float a = (static_cast<float>(fromDeg) + 180.0f) * kPi / 180.0f;
        const float dx = std::sin(a), dy = -std::cos(a);
        const Point tip{cx + dx * s, cy + dy * s}, tail{cx - dx * s, cy - dy * s};
        const Point l{tail.x - dy * s * 0.55f, tail.y + dx * s * 0.55f}, rr{tail.x + dy * s * 0.55f, tail.y - dx * s * 0.55f};
        r.FillPolygon({tip, l, {cx - dx * s * 0.4f, cy - dy * s * 0.4f}, rr}, c);
    }

    // ---- плитки подробностей ------------------------------------------------------------
    float DrawTiles(Renderer& r, float x0, float y, float w, const NowWx& now)
    {
        const int cols = w >= 900.0f ? 4 : (w >= 640.0f ? 3 : 2);
        const float gap = 16.0f, tw = (w - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols), th = 200.0f;
        for (int i = 0; i < 8; ++i)
        {
            const Rect t{x0 + static_cast<float>(i % cols) * (tw + gap), y + static_cast<float>(i / cols) * (th + gap), tw, th};
            DrawTile(r, t, i, now);
        }
        return y + static_cast<float>((8 + cols - 1) / cols) * (th + gap) - gap;
    }

    void TileHeader(Renderer& r, const Rect& t, Icon icon, const wchar_t* title)
    {
        const Palette& p = Theme::P();
        Imm::Card(r, t);
        r.Glyph(icon, {t.x + 16.0f, t.y + 16.0f, 16.0f, 16.0f}, p.textSecondary);
        r.Text(title, {t.x + 40.0f, t.y + 12.0f, t.w - 56.0f, 24.0f}, p.textSecondary, 13.0f, FontWeight::Semibold, TextFlags::VCenter);
    }

    void GradientBar(Renderer& r, const Rect& bar, const std::vector<Color>& colors, float marker)
    {
        const Palette& p = Theme::P();
        const int n = static_cast<int>(colors.size()) - 1;
        r.FillRoundRect(bar, bar.h * 0.5f, colors.front());
        for (int i = 0; i < n; ++i)
        {
            const float sx = bar.x + bar.w * static_cast<float>(i) / static_cast<float>(n);
            r.FillGradientH({sx, bar.y, bar.w / static_cast<float>(n) + 0.5f, bar.h}, i == 0 || i == n - 1 ? 0.0f : 0.0f, colors[static_cast<size_t>(i)],
                            colors[static_cast<size_t>(i) + 1]);
        }
        const float mx = bar.x + bar.w * Saturate(marker);
        r.FillCircle(mx, bar.CenterY(), bar.h * 0.5f + 3.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.StrokeCircle(mx, bar.CenterY(), bar.h * 0.5f + 3.0f, p.controlStrongStroke, 1.5f);
    }

    void DrawTile(Renderer& r, const Rect& t, int kind, const NowWx& now)
    {
        const Palette& p = Theme::P();
        const DayWx today = Day(m_city, now.today);
        switch (kind)
        {
        case 0: // Восход и закат
        {
            TileHeader(r, t, Icon::Sun, L"Восход и закат");
            const float cx = t.CenterX(), cy = t.y + 140.0f, rx = std::min(t.w * 0.5f - 28.0f, 110.0f), ry = 70.0f;
            Gdiplus::Pen dash(r.Gp(p.textTertiary), 1.5f);
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            r.G().DrawArc(&dash, cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, 180.0f, 180.0f);
            r.FillRect({cx - rx - 12.0f, cy, rx * 2.0f + 24.0f, 1.0f}, p.divider);
            const int m = now.local.wHour * 60 + now.local.wMinute;
            const float frac = static_cast<float>(m - now.sunrise) / std::max(1.0f, static_cast<float>(now.sunset - now.sunrise));
            if (frac >= 0.0f && frac <= 1.0f)
            {
                const float a = kPi * (1.0f - frac);
                const float sx = cx + rx * std::cos(a), sy = cy - ry * std::sin(a);
                Gdiplus::Pen solid(r.Gp(Color::Hex(0xFFB900)), 2.0f);
                r.G().DrawArc(&solid, cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, 180.0f, 180.0f * frac);
                r.FillCircle(sx, sy, 9.0f, Color(255, 185, 0, 0x40));
                r.FillCircle(sx, sy, 6.0f, Color::Hex(0xFFB900));
            }
            r.Text(L"Восход", {t.x + 16.0f, cy + 8.0f, 100.0f, 18.0f}, p.textSecondary, 12.0f);
            r.Text(HhMm(now.sunrise), {t.x + 16.0f, cy + 24.0f, 100.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold);
            r.Text(L"Закат", {t.Right() - 116.0f, cy + 8.0f, 100.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Right);
            r.Text(HhMm(now.sunset), {t.Right() - 116.0f, cy + 24.0f, 100.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold, TextFlags::Right);
            const int len = std::max(0, now.sunset - now.sunrise);
            r.Text(L"Световой день: " + std::to_wstring(len / 60) + L" ч " + std::to_wstring(len % 60) + L" мин", {t.x, t.y + 40.0f, t.w, 18.0f},
                   p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center);
            break;
        }
        case 1: // Луна
        {
            TileHeader(r, t, Icon::Moon, L"Луна");
            const float age = MoonAge(now.today);
            const float k = (1.0f - std::cos(2.0f * kPi * age / 29.530588f)) * 0.5f;
            const float cx = t.x + 70.0f, cy = t.y + 112.0f, rad = 40.0f;
            DrawMoonPhase(r, cx, cy, rad, age);
            r.Text(MoonPhaseName(age), {cx + 60.0f, cy - 34.0f, t.Right() - cx - 70.0f, 44.0f}, p.text, 16.0f, FontWeight::Semibold, TextFlags::Wrap);
            r.Text(L"Освещённость " + std::to_wstring(static_cast<int>(std::lround(k * 100.0f))) + L"%", {cx + 60.0f, cy + 14.0f, t.Right() - cx - 70.0f, 20.0f},
                   p.textSecondary, 12.0f);
            r.Text(L"Возраст: " + std::to_wstring(static_cast<int>(age)) + L" дн.", {cx + 60.0f, cy + 34.0f, t.Right() - cx - 70.0f, 20.0f}, p.textSecondary, 12.0f);
            break;
        }
        case 2: // УФ-индекс
        {
            TileHeader(r, t, Icon::Sun, L"УФ-индекс");
            r.Text(std::to_wstring(now.uv), {t.x + 16.0f, t.y + 48.0f, 80.0f, 56.0f}, p.text, 40.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(UvText(now.uv), {t.x + 16.0f, t.y + 104.0f, t.w - 32.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold);
            r.Text(L"Максимум сегодня: " + std::to_wstring(today.uv), {t.x + 16.0f, t.y + 128.0f, t.w - 32.0f, 20.0f}, p.textSecondary, 12.0f);
            GradientBar(r, {t.x + 16.0f, t.Bottom() - 32.0f, t.w - 32.0f, 6.0f},
                        {Color::Hex(0x3CB44B), Color::Hex(0xF2D45C), Color::Hex(0xF28E3C), Color::Hex(0xD7303A), Color::Hex(0x8E44AD)},
                        static_cast<float>(now.uv) / 11.0f);
            break;
        }
        case 3: // Ветер
        {
            TileHeader(r, t, Icon::Wind, L"Ветер");
            const float cx = t.x + 68.0f, cy = t.y + 118.0f, rad = 46.0f;
            r.StrokeCircle(cx, cy, rad, p.controlStrongStroke, 1.0f);
            for (int i = 0; i < 36; ++i)
            {
                const float a = static_cast<float>(i) * kPi / 18.0f;
                const float r0 = rad - (i % 9 == 0 ? 8.0f : 4.0f);
                r.Line(cx + std::cos(a) * r0, cy + std::sin(a) * r0, cx + std::cos(a) * rad, cy + std::sin(a) * rad, p.textTertiary, 1.0f, false);
            }
            const wchar_t* dirs[] = {L"С", L"В", L"Ю", L"З"};
            for (int i = 0; i < 4; ++i)
            {
                const float a = static_cast<float>(i) * kPi * 0.5f - kPi * 0.5f;
                r.Text(dirs[i], {cx + std::cos(a) * (rad + 12.0f) - 10.0f, cy + std::sin(a) * (rad + 12.0f) - 9.0f, 20.0f, 18.0f}, p.textSecondary, 11.0f,
                       FontWeight::Regular, TextFlags::Middle);
            }
            const float a = (static_cast<float>(now.windDir) + 180.0f) * kPi / 180.0f;
            const float dx = std::sin(a), dy = -std::cos(a);
            r.Line(cx - dx * (rad - 14.0f), cy - dy * (rad - 14.0f), cx + dx * (rad - 18.0f), cy + dy * (rad - 18.0f), p.accent, 2.5f);
            DrawWindArrow(r, cx + dx * (rad - 18.0f), cy + dy * (rad - 18.0f), 8.0f, now.windDir, p.accent);
            const float tx = cx + rad + 26.0f;
            r.Text(std::to_wstring(static_cast<int>(std::lround(now.wind))) + L" м/с", {tx, t.y + 66.0f, t.Right() - tx - 8.0f, 36.0f}, p.text, 24.0f,
                   FontWeight::Semibold, TextFlags::VCenter);
            r.Text(L"Порывы: " + std::to_wstring(static_cast<int>(std::lround(now.gust))) + L" м/с", {tx, t.y + 106.0f, t.Right() - tx - 8.0f, 20.0f},
                   p.textSecondary, 12.0f);
            r.Text(std::wstring(L"Откуда: ") + WindDirText(now.windDir), {tx, t.y + 126.0f, t.Right() - tx - 8.0f, 20.0f}, p.textSecondary, 12.0f);
            break;
        }
        case 4: // Влажность
        {
            TileHeader(r, t, Icon::Drop, L"Влажность");
            r.Text(std::to_wstring(now.humidity) + L"%", {t.x + 16.0f, t.y + 48.0f, t.w - 32.0f, 56.0f}, p.text, 40.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(L"Точка росы: " + Deg(now.dew), {t.x + 16.0f, t.y + 108.0f, t.w - 32.0f, 20.0f}, p.textSecondary, 12.0f);
            const Rect bar{t.x + 16.0f, t.Bottom() - 34.0f, t.w - 32.0f, 8.0f};
            r.FillRoundRect(bar, 4.0f, p.dark ? Color(255, 255, 255, 0x1A) : Color(0, 0, 0, 0x10));
            r.FillRoundRect({bar.x, bar.y, bar.w * static_cast<float>(now.humidity) / 100.0f, bar.h}, 4.0f, p.accent);
            break;
        }
        case 5: // Видимость
        {
            TileHeader(r, t, Icon::Eye, L"Видимость");
            const int vis = static_cast<int>(std::lround(std::max(1.0f, now.visibility)));
            r.Text(std::to_wstring(vis) + L" км", {t.x + 16.0f, t.y + 48.0f, t.w - 32.0f, 56.0f}, p.text, 40.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(vis >= 10 ? L"Отличная видимость" : (vis >= 4 ? L"Умеренная видимость" : L"Плохая видимость"), {t.x + 16.0f, t.y + 108.0f, t.w - 32.0f, 22.0f},
                   p.text, 14.0f);
            r.Text(now.cond == Cond::Fog ? L"Из-за тумана видимость снижена." : L"Ничто не мешает обзору.", {t.x + 16.0f, t.y + 132.0f, t.w - 32.0f, 36.0f},
                   p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Wrap);
            break;
        }
        case 6: // Давление
        {
            TileHeader(r, t, Icon::Performance, L"Давление");
            const float cx = t.x + 74.0f, cy = t.y + 122.0f, rad = 46.0f;
            r.Arc(cx, cy, rad, 135.0f, 270.0f, p.dark ? Color(255, 255, 255, 0x26) : Color(0, 0, 0, 0x16), 6.0f);
            const float f = Saturate((static_cast<float>(now.pressure) - 970.0f) / 80.0f);
            r.Arc(cx, cy, rad, 135.0f, 270.0f * f, p.accent, 6.0f);
            const float a = (135.0f + 270.0f * f) * kPi / 180.0f;
            r.Line(cx, cy, cx + std::cos(a) * (rad - 12.0f), cy + std::sin(a) * (rad - 12.0f), p.text, 2.0f);
            r.FillCircle(cx, cy, 4.0f, p.text);
            const float tx = cx + rad + 24.0f;
            r.Text(std::to_wstring(now.pressure), {tx, t.y + 70.0f, t.Right() - tx - 8.0f, 36.0f}, p.text, 26.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(L"гПа", {tx, t.y + 106.0f, t.Right() - tx - 8.0f, 20.0f}, p.textSecondary, 12.0f);
            r.Text(L"≈ " + std::to_wstring(static_cast<int>(std::lround(static_cast<float>(now.pressure) * 0.750062f))) + L" мм рт. ст.",
                   {tx, t.y + 126.0f, t.Right() - tx - 8.0f, 20.0f}, p.textSecondary, 12.0f);
            break;
        }
        default: // Качество воздуха
        {
            TileHeader(r, t, Icon::Wind, L"Качество воздуха");
            r.Text(std::to_wstring(now.aqi), {t.x + 16.0f, t.y + 48.0f, t.w - 32.0f, 56.0f}, p.text, 40.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(AqiText(now.aqi), {t.x + 16.0f, t.y + 104.0f, t.w - 32.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold);
            r.Text(L"Основной загрязнитель: PM2.5", {t.x + 16.0f, t.y + 128.0f, t.w - 32.0f, 20.0f}, p.textSecondary, 12.0f);
            GradientBar(r, {t.x + 16.0f, t.Bottom() - 32.0f, t.w - 32.0f, 6.0f},
                        {Color::Hex(0x3CB44B), Color::Hex(0xF2D45C), Color::Hex(0xF28E3C), Color::Hex(0xD7303A), Color::Hex(0x8E44AD), Color::Hex(0x7E0023)},
                        static_cast<float>(now.aqi) / 300.0f);
            break;
        }
        }
    }

    // Фаза Луны: освещённая часть диска (растущая — справа).
    static void DrawMoonPhase(Renderer& r, float cx, float cy, float rad, float age)
    {
        const Color dark = Theme::P().dark ? Color::Hex(0x3A3F48) : Color::Hex(0x5B6270);
        const Color light = Color::Hex(0xF1EFE6);
        r.FillCircle(cx, cy, rad, dark);
        const float phase = age / 29.530588f; // 0..1
        const bool waxing = phase < 0.5f;
        const float k = std::cos(2.0f * kPi * phase); // 1 — новолуние, −1 — полнолуние
        // Освещённая половина
        Gdiplus::GraphicsPath half;
        half.AddArc(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, waxing ? -90.0f : 90.0f, 180.0f);
        half.CloseFigure();
        r.FillPath(half, light);
        // Терминатор — эллипс с полушириной |k|·R: тёмный (серп) или светлый (горбатая луна).
        const float ex = std::fabs(k) * rad;
        r.FillEllipse({cx - ex, cy - rad, ex * 2.0f, rad * 2.0f}, k > 0.0f ? dark : light);
        r.StrokeCircle(cx, cy, rad, Color(0, 0, 0, 0x20));
    }

    // ---- карты -----------------------------------------------------------------
    Gdiplus::Bitmap* TempField()
    {
        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        const int key = DayNumber(utc) * 1000 + utc.wHour * 10 + utc.wMinute / 10;
        if (m_field && m_fieldKey == key)
            return m_field.get();
        const int w = 360, h = static_cast<int>(WorldMap::kTopLat - WorldMap::kBottomLat);
        m_field = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
        m_fieldKey = key;
        const int doy = DayOfYear(utc);
        const float hour = static_cast<float>(utc.wHour) + static_cast<float>(utc.wMinute) / 60.0f;
        Gdiplus::BitmapData data{};
        Gdiplus::Rect rc(0, 0, w, h);
        if (m_field->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
        {
            for (int y = 0; y < h; ++y)
            {
                auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride);
                const float lat = WorldMap::kTopLat - static_cast<float>(y) - 0.5f;
                for (int x = 0; x < w; ++x)
                {
                    const Color c = TempColor(FieldTemp(lat, static_cast<float>(x) - 180.0f + 0.5f, doy, hour));
                    row[x] = 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
                }
            }
            m_field->UnlockBits(&data);
        }
        return m_field.get();
    }

    void DrawMaps(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const float x0 = c.x + kPad, w = c.w - 2.0f * kPad;
        float y = c.y + 18.0f - m_scroll;
        r.Text(L"Карта температуры", {x0, y, w, 34.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        r.Text(L"Приземная температура воздуха · " + Pad2(utc.wHour) + L":" + Pad2(utc.wMinute) + L" UTC", {x0, y + 34.0f, w, 20.0f}, p.textSecondary, 12.0f);
        y += 68.0f;
        float mw = w, mh = mw / WorldMap::kAspect;
        const float maxH = std::max(220.0f, c.h - 200.0f);
        if (mh > maxH)
        {
            mh = maxH;
            mw = mh * WorldMap::kAspect;
        }
        const Rect map{std::floor(x0 + (w - mw) * 0.5f), y, std::floor(mw), std::floor(mh)};
        const Color ocean = p.dark ? Color(0x26, 0x2B, 0x33) : Color(0xE3, 0xEA, 0xF2);
        r.BeginRounded(map, 8.0f);
        r.FillRect(map, ocean);
        r.PushClip(map);
        // Температурное поле только над сушей.
        Gdiplus::GraphicsPath land;
        for (const WorldMap::Shape& s : WorldMap::Land())
        {
            std::vector<Gdiplus::PointF> pts;
            for (size_t i = 0; i + 1 < s.count; i += 2)
            {
                const Point q = WorldMap::Project(map, s.pts[i], s.pts[i + 1]);
                pts.push_back({q.x, q.y});
            }
            land.AddPolygon(pts.data(), static_cast<INT>(pts.size()));
        }
        Gdiplus::Graphics& g = r.G();
        const Gdiplus::GraphicsState st = g.Save();
        g.SetClip(&land, Gdiplus::CombineModeIntersect);
        g.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        r.Image(TempField(), map);
        g.Restore(st);
        WorldMap::FillShapes(r, map, WorldMap::Seas(), ocean);
        r.StrokePath(land, p.dark ? Color(0, 0, 0, 0x50) : Color(255, 255, 255, 0x90), 0.8f);
        r.PopClip();
        r.EndRounded();
        r.StrokeRoundRect(map, 8.0f, p.cardStroke);
        // Города
        std::vector<int> shown = Favorites();
        if (std::find(shown.begin(), shown.end(), m_city) == shown.end())
            shown.push_back(m_city);
        r.PushClip(map);
        for (int city : shown)
        {
            const NowWx n = Now(city);
            const Point q = WorldMap::Project(map, kCities[city].lon, kCities[city].lat);
            const std::wstring label = std::wstring(kCities[city].name) + L"  " + Deg(n.temp);
            const float lw = r.TextWidth(label, 12.0f) + 40.0f;
            Rect pill{q.x + 8.0f, q.y - 28.0f, lw, 24.0f};
            if (pill.Right() > map.Right() - 4.0f)
                pill.x = q.x - 8.0f - lw;
            r.FillRoundRect(pill, 12.0f, p.dark ? Color(0x2B, 0x2B, 0x2B, 0xF0) : Color(255, 255, 255, 0xF2));
            r.StrokeRoundRect(pill, 12.0f, p.surfaceStroke);
            DrawIcon(r, {pill.x + 6.0f, pill.y + 2.0f, 20.0f, 20.0f}, n.cond, n.night, !p.dark);
            r.Text(label, {pill.x + 30.0f, pill.y, lw - 34.0f, pill.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.FillCircle(q.x, q.y, 4.5f, city == m_city ? p.accent : p.text);
            r.StrokeCircle(q.x, q.y, 4.5f, Color(255, 255, 255), 1.5f);
        }
        r.PopClip();
        y = map.Bottom() + 18.0f;
        // Легенда
        const Rect lg{map.x, y, std::min(480.0f, map.w), 10.0f};
        for (int i = 0; i < 70; ++i)
            r.FillRect({lg.x + lg.w * static_cast<float>(i) / 70.0f, lg.y, lg.w / 70.0f + 0.6f, lg.h}, TempColor(-30.0f + static_cast<float>(i)));
        for (int t = -30; t <= 40; t += 10)
        {
            const float lx = lg.x + lg.w * static_cast<float>(t + 30) / 70.0f;
            r.Text(Deg(static_cast<float>(t)), {lx - 20.0f, lg.Bottom() + 4.0f, 40.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Center);
        }
        y += 48.0f;
        m_contentH = y + m_scroll - c.y + 16.0f;
    }

    // ---- исторические данные ---------------------------------------------------------
    void DrawHistory(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const Cities::City& city = kCities[m_city];
        const float x0 = c.x + kPad, w = c.w - 2.0f * kPad;
        float y = c.y + 18.0f - m_scroll;
        r.Text(L"Исторические данные: " + std::wstring(city.name), {x0, y, w, 34.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(L"Средние значения по месяцам за последние 30 лет (модель климата)", {x0, y + 34.0f, w, 20.0f}, p.textSecondary, 12.0f);
        y += 72.0f;
        const NowWx now = Now(m_city);
        // Сравнение с нормой
        {
            const Rect card{x0, y, w, 96.0f};
            Imm::Card(r, card);
            const DayWx d = Day(m_city, now.today);
            const float normHi = SeasonTemp(city, DayOfYear(now.local)) + 3.5f - city.wet;
            const int diff = ShowTemp(d.hi) - ShowTemp(normHi);
            r.Text(L"Сегодня", {card.x + 20.0f, card.y + 14.0f, 200.0f, 22.0f}, p.textSecondary, 13.0f, FontWeight::Semibold);
            r.Text(L"Максимум " + Deg(d.hi) + L", минимум " + Deg(d.lo), {card.x + 20.0f, card.y + 38.0f, card.w - 40.0f, 26.0f}, p.text, 18.0f,
                   FontWeight::Semibold);
            const std::wstring cmp = diff == 0 ? std::wstring(L"Как обычно для этого времени года")
                                               : (L"На " + std::to_wstring(std::abs(diff)) + L"° " + (diff > 0 ? L"теплее" : L"холоднее") + L" нормы (" +
                                                  Deg(normHi) + L")");
            r.Text(cmp, {card.x + 20.0f, card.y + 66.0f, card.w - 40.0f, 20.0f}, p.textSecondary, 13.0f);
            y += 112.0f;
        }
        // Температура по месяцам
        const Rect card{x0, y, w, 320.0f};
        Imm::Card(r, card);
        r.Text(L"Температура", {card.x + 20.0f, card.y + 14.0f, 300.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold);
        r.Text(L"Средний максимум и минимум", {card.Right() - 320.0f, card.y + 14.0f, 300.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::Right | TextFlags::VCenter);
        const Rect area{card.x + 56.0f, card.y + 56.0f, card.w - 80.0f, card.h - 100.0f};
        float hi[12], lo[12];
        float mn = 100.0f, mx = -100.0f;
        static const int midDoy[12] = {15, 45, 74, 105, 135, 166, 196, 227, 258, 288, 319, 349};
        for (int m = 0; m < 12; ++m)
        {
            const float s = SeasonTemp(city, midDoy[m]);
            const float range = 8.0f - city.wet * 3.0f;
            hi[m] = s + range * 0.5f;
            lo[m] = s - range * 0.5f;
            mn = std::min(mn, lo[m]);
            mx = std::max(mx, hi[m]);
        }
        const float vmin = std::floor((mn - 2.0f) / 5.0f) * 5.0f, vmax = std::ceil((mx + 2.0f) / 5.0f) * 5.0f;
        auto py = [&](float v) { return area.Bottom() - (v - vmin) / std::max(1.0f, vmax - vmin) * area.h; };
        for (float v = vmin; v <= vmax + 0.1f; v += 5.0f)
        {
            r.FillRect({area.x, std::floor(py(v)), area.w, 1.0f}, p.divider);
            r.Text(Deg(v), {card.x + 8.0f, py(v) - 9.0f, 44.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Right);
        }
        const int curMonth = now.local.wMonth - 1;
        const float bw = area.w / 12.0f;
        for (int m = 0; m < 12; ++m)
        {
            const float bx = area.x + bw * static_cast<float>(m) + bw * 0.3f;
            const Rect bar{bx, py(hi[m]), bw * 0.4f, py(lo[m]) - py(hi[m])};
            const Color top = TempColor(hi[m]), bottom = TempColor(lo[m]);
            r.FillGradientV(bar, std::min(4.0f, bar.w * 0.5f), top, bottom);
            if (m == curMonth)
                r.StrokeRoundRect(bar.Inflated(2.0f), 5.0f, p.text, 1.5f);
            r.Text(Deg(hi[m]), {bx - 16.0f, bar.y - 20.0f, bar.w + 32.0f, 18.0f}, p.text, 11.0f, FontWeight::Semibold, TextFlags::Center);
            r.Text(Deg(lo[m]), {bx - 16.0f, bar.Bottom() + 2.0f, bar.w + 32.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Center);
            r.Text(kMonthShort[m], {area.x + bw * static_cast<float>(m), card.Bottom() - 30.0f, bw, 20.0f}, m == curMonth ? p.text : p.textSecondary, 12.0f,
                   m == curMonth ? FontWeight::Semibold : FontWeight::Regular, TextFlags::Center);
        }
        y += 336.0f;
        // Дни с осадками
        const Rect card2{x0, y, w, 240.0f};
        Imm::Card(r, card2);
        r.Text(L"Дни с осадками", {card2.x + 20.0f, card2.y + 14.0f, 300.0f, 22.0f}, p.text, 15.0f, FontWeight::Semibold);
        const Rect area2{card2.x + 56.0f, card2.y + 56.0f, card2.w - 80.0f, card2.h - 100.0f};
        for (int m = 0; m < 12; ++m)
        {
            const float season = std::cos(2.0f * kPi * static_cast<float>(midDoy[m] - 200) / 365.0f) * (city.tAmp >= 0.0f ? 1.0f : -1.0f);
            const int days = Clamp(static_cast<int>(4.0f + city.wet * 16.0f + season * 2.5f * city.wet), 0, 28);
            const float bh = area2.h * static_cast<float>(days) / 28.0f;
            const Rect bar{area2.x + bw * static_cast<float>(m) + bw * 0.25f, area2.Bottom() - bh, bw * 0.5f, bh};
            r.FillRoundRect(bar, 3.0f, m == curMonth ? p.accent : p.accent.MulAlpha(0.45f));
            r.Text(std::to_wstring(days), {bar.x - 10.0f, bar.y - 20.0f, bar.w + 20.0f, 18.0f}, p.text, 11.0f, FontWeight::Regular, TextFlags::Center);
            r.Text(kMonthShort[m], {area2.x + bw * static_cast<float>(m), card2.Bottom() - 30.0f, bw, 20.0f}, m == curMonth ? p.text : p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::Center);
        }
        y += 256.0f;
        m_contentH = y + m_scroll - c.y + 16.0f;
    }

    // ---- избранное -----------------------------------------------------------------
    void DrawFavorites(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const float x0 = c.x + kPad, w = c.w - 2.0f * kPad;
        float y = c.y + 18.0f - m_scroll;
        r.Text(L"Избранные места", {x0, y, w, 34.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 52.0f;
        const float cw = 264.0f, chh = 168.0f, gap = 16.0f;
        const int cols = std::max(1, static_cast<int>((w + gap) / (cw + gap)));
        std::vector<int>& favs = Favorites();
        const int count = static_cast<int>(favs.size());
        for (int i = 0; i <= count; ++i)
        {
            const Rect rc{x0 + static_cast<float>(i % cols) * (cw + gap), y + static_cast<float>(i / cols) * (chh + gap), cw, chh};
            if (i == count)
            {
                const bool hot = m_imm.Hot(kHkFavAdd);
                r.FillRoundRect(rc, 8.0f, hot ? p.controlHover : Color(0, 0, 0, 0));
                Gdiplus::Pen dash(r.Gp(p.controlStrongStroke), 1.0f);
                dash.SetDashStyle(Gdiplus::DashStyleDash);
                Gdiplus::GraphicsPath path;
                Renderer::RoundRectPath(path, rc.Inset(0.5f, 0.5f, 0.5f, 0.5f), 8.0f);
                r.G().DrawPath(&dash, &path);
                r.Glyph(Icon::Add, {rc.CenterX() - 10.0f, rc.CenterY() - 26.0f, 20.0f, 20.0f}, p.text);
                r.Text(L"Добавить место", {rc.x, rc.CenterY() + 4.0f, rc.w, 22.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Center);
                m_imm.Add(rc, kHkFavAdd);
                break;
            }
            const int city = favs[static_cast<size_t>(i)];
            const NowWx n = Now(city);
            const DayWx d = Day(city, n.today);
            const bool hot = m_imm.Hot(kHkFavCard, i) || m_imm.Hot(kHkFavRemove, i);
            Color top, bottom;
            SkyColors(n.cond, n.night, top, bottom);
            r.FillGradientV(rc, 8.0f, top, bottom);
            if (hot)
                r.FillRoundRect(rc, 8.0f, Color(255, 255, 255, 0x18));
            m_imm.Add(rc, kHkFavCard, i);
            const Color white(255, 255, 255), soft(255, 255, 255, 0xD0);
            r.Text(kCities[city].name, {rc.x + 16.0f, rc.y + 12.0f, rc.w - 60.0f, 24.0f}, white, 15.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(Pad2(n.local.wHour) + L":" + Pad2(n.local.wMinute) + (city == HomeCity() ? L" · основное" : L""), {rc.x + 16.0f, rc.y + 36.0f, rc.w - 32.0f, 18.0f},
                   soft, 12.0f);
            DrawIcon(r, {rc.x + 12.0f, rc.y + 62.0f, 56.0f, 56.0f}, n.cond, n.night);
            r.Text(Deg(n.temp), {rc.x + 76.0f, rc.y + 60.0f, 120.0f, 60.0f}, white, 40.0f, FontWeight::Light, TextFlags::VCenter);
            r.Text(CondText(n.cond, n.night), {rc.x + 16.0f, rc.y + 120.0f, rc.w - 32.0f, 20.0f}, white, 13.0f);
            r.Text(Deg(d.hi) + L" / " + Deg(d.lo), {rc.x + 16.0f, rc.y + 140.0f, rc.w - 32.0f, 18.0f}, soft, 12.0f);
            if (hot)
                Imm::SubtleButton(r, m_imm, {rc.Right() - 40.0f, rc.y + 8.0f, 32.0f, 32.0f}, Icon::Close, kHkFavRemove, i, L"Удалить из избранного", false, white);
        }
        m_contentH = y + static_cast<float>(count / cols + 1) * (chh + gap) + m_scroll - c.y + 16.0f;
    }

    // ---- параметры ------------------------------------------------------------------
    void DrawSettings(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const float x0 = c.x + kPad + 12.0f, w = std::min(1000.0f, c.w - 2.0f * kPad - 24.0f);
        float y = c.y + 24.0f - m_scroll;
        r.Text(L"Параметры", {x0, y, w, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 64.0f;
        r.Text(L"Единицы измерения", {x0, y, w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 30.0f;
        {
            const Rect rc{x0, y, w, 96.0f};
            Imm::Card(r, rc);
            r.Glyph(Icon::Thermometer, {rc.x + 20.0f, rc.y + 18.0f, 20.0f, 20.0f}, p.text);
            r.Text(L"Температура", {rc.x + 60.0f, rc.y + 16.0f, 300.0f, 22.0f}, p.text, 14.0f);
            Imm::Check(r, m_imm, rc.x + 60.0f, rc.y + 54.0f, !Fahrenheit(), L"По Цельсию (°C)", kHkUnit, 0, true, true);
            Imm::Check(r, m_imm, rc.x + 260.0f, rc.y + 54.0f, Fahrenheit(), L"По Фаренгейту (°F)", kHkUnit, 1, true, true);
            y += 112.0f;
        }
        r.Text(L"Основное местоположение", {x0, y, w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 30.0f;
        {
            const Rect rc{x0, y, w, 72.0f};
            Imm::Card(r, rc);
            r.Glyph(Icon::Home, {rc.x + 20.0f, rc.CenterY() - 10.0f, 20.0f, 20.0f}, p.text);
            r.Text(CityTitle(HomeCity()), {rc.x + 60.0f, rc.y + 14.0f, rc.w - 360.0f, 22.0f}, p.text, 14.0f);
            r.Text(L"Используется на панели задач и в виджетах", {rc.x + 60.0f, rc.y + 36.0f, rc.w - 360.0f, 20.0f}, p.textSecondary, 12.0f);
            if (m_city != HomeCity())
            {
                const std::wstring t = L"Сделать основным: " + std::wstring(kCities[m_city].name);
                const float bw = r.TextWidth(t, 14.0f) + 32.0f;
                Imm::TextButton(r, m_imm, {rc.Right() - 20.0f - bw, rc.CenterY() - 16.0f, bw, 32.0f}, t, Icon::None, false, kHkHome);
            }
            y += 88.0f;
        }
        r.Text(L"Сведения", {x0, y, w, 20.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 30.0f;
        const Rect about{x0, y, w, 96.0f};
        Imm::Card(r, about);
        r.DrawAppIcon(AppIcon::Weather, {about.x + 20.0f, about.y + 20.0f, 32.0f, 32.0f});
        r.Text(L"Погода", {about.x + 68.0f, about.y + 16.0f, about.w - 90.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(L"Версия 4.53.62", {about.x + 68.0f, about.y + 38.0f, about.w - 90.0f, 18.0f}, p.textSecondary, 12.0f);
        r.Text(L"Данные смоделированы программно по климату каждого города и не являются реальным прогнозом.", {about.x + 68.0f, about.y + 58.0f, about.w - 90.0f, 34.0f},
               p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Wrap);
        y += 112.0f;
        m_contentH = y + m_scroll - c.y;
    }

    // ================================================================== ввод
    void Activate(int kind, int index)
    {
        switch (kind)
        {
        case kHkNav:
        {
            static const Page pages[] = {Page::Forecast, Page::Maps, Page::History, Page::Favorites, Page::Settings};
            SetPage(pages[Clamp(index, 0, 4)]);
            break;
        }
        case kHkSuggest:
            PickSuggestion(index);
            break;
        case kHkFav:
        {
            std::vector<int>& f = Favorites();
            auto it = std::find(f.begin(), f.end(), m_city);
            if (it != f.end())
                f.erase(it);
            else
                f.push_back(m_city);
            break;
        }
        case kHkDay:
            m_selDay = index;
            break;
        case kHkDayPrev:
            m_dayScroll = std::max(0, m_dayScroll - 3);
            break;
        case kHkDayNext:
            m_dayScroll += 3;
            break;
        case kHkTab:
            m_tab = index;
            break;
        case kHkFavCard:
            OpenCity(Favorites()[static_cast<size_t>(index)]);
            break;
        case kHkFavRemove:
            Favorites().erase(Favorites().begin() + index);
            break;
        case kHkFavAdd:
            if (m_search->Visible())
                m_ui.SetFocus(m_search);
            break;
        case kHkUnit:
            Fahrenheit() = index == 1;
            break;
        case kHkHome:
            HomeCity() = m_city;
            break;
        default:
            break;
        }
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        m_imm.Move(e.pos, {Frame().x, Frame().y});
        const int prev = m_chartHover;
        m_chartHover = -1;
        if (m_page == Page::Forecast && m_chartArea.Contains(e.pos) && ContentRect().Contains(e.pos))
            m_chartHover = Clamp(static_cast<int>((e.pos.x - m_chartArea.x) / (m_chartArea.w / 24.0f)), 0, 23);
        if (prev != m_chartHover)
            RequestFrame();
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_imm.Leave();
        m_chartHover = -1;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        // Подсказки поиска перекрывают поле содержимого: проверяем их до виджетов.
        const ImmHot* h = m_imm.Hit(e.pos);
        if (h && h->kind == kHkSuggest)
        {
            PickSuggestion(h->index);
            return;
        }
        Window::OnMouseDown(e);
        if (HasDialog() || m_ui.HasCapture())
            return;
        if (m_search->Focused() && !m_search->Bounds().Contains(e.pos))
            m_ui.SetFocus(nullptr);
        m_imm.Down(e.pos, e.clicks);
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog())
            return;
        int kind = 0, index = 0;
        if (m_imm.Up(e.pos, kind, index) && e.button == MouseButton::Left)
            Activate(kind, index);
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        if (ContentRect().Contains(e.pos))
            m_scroll = Clamp(m_scroll - e.wheel * 90.0f, 0.0f, MaxScroll());
        return true;
    }

    // ================================================================== поля
    Page m_page = Page::Forecast;
    int m_city = 0;
    int m_selDay = 0;
    int m_dayScroll = 0;
    int m_tab = 0;
    int m_chartHover = -1;
    Rect m_chartArea;
    float m_scroll = 0.0f;
    float m_contentH = 0.0f;
    int m_suggestSel = 0;
    ImmUi m_imm;
    TextBox* m_search = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> m_field;
    int m_fieldKey = -1;
};
} // namespace

std::unique_ptr<Window> CreateWeather(const std::wstring& arg) { return std::make_unique<WeatherWindow>(arg); }
