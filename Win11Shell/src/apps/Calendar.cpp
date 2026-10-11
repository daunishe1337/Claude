// Календарь Windows 11: представления «День», «Неделя» и «Месяц», мини-календарь, список
// календарей (личный, праздники России, дни рождения), создание и изменение событий в диалоге.

#include "../Shell.h"
#include "AppFactories.h"
#include "ImmUi.h"
#include "WeatherData.h"

namespace
{
using WeatherData::DateOf;
using WeatherData::DayNumber;

constexpr float kTitleH = 48.0f;
constexpr float kPaneW = 268.0f;
constexpr float kToolH = 60.0f;

const wchar_t* const kMonthNom[12] = {L"Январь", L"Февраль", L"Март",     L"Апрель",  L"Май",    L"Июнь",
                                      L"Июль",   L"Август",  L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"};
const wchar_t* const kMonthGen[12] = {L"января", L"февраля", L"марта",    L"апреля",  L"мая",    L"июня",
                                      L"июля",   L"августа", L"сентября", L"октября", L"ноября", L"декабря"};
const wchar_t* const kWeekShort[7] = {L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс"};
const wchar_t* const kWeekLong[7] = {L"Понедельник", L"Вторник", L"Среда", L"Четверг", L"Пятница", L"Суббота", L"Воскресенье"};
const uint32_t kColors[6] = {0x0F6CBD, 0x13A10E, 0xC239B3, 0xCA5010, 0x8764B8, 0xD13438};

struct Event
{
    std::wstring title;
    std::wstring location;
    int day = 0;
    int start = 9 * 60;
    int end = 10 * 60;
    bool allDay = false;
    int color = 0;
    int calendar = 0; // 0 — личный, 1 — дни рождения
};

// Понедельник = 0.
int Weekday(int day) { return (DateOf(day).wDayOfWeek + 6) % 7; }

std::vector<Event>& Events()
{
    static std::vector<Event> v;
    static bool init = false;
    if (!init)
    {
        init = true;
        const int t = DayNumber(LocalNow());
        auto add = [&](const wchar_t* title, const wchar_t* loc, int d, int s, int e, int color, bool allDay = false, int cal = 0) {
            Event ev;
            ev.title = title;
            ev.location = loc;
            ev.day = t + d;
            ev.start = s;
            ev.end = e;
            ev.color = color;
            ev.allDay = allDay;
            ev.calendar = cal;
            v.push_back(ev);
        };
        add(L"Планёрка команды", L"Переговорная «Север»", 0, 10 * 60, 10 * 60 + 30, 0);
        add(L"Обед с Марией", L"Кафе у реки", 0, 13 * 60, 14 * 60, 3);
        add(L"Ревью интерфейса", L"Онлайн", 1, 11 * 60, 12 * 60 + 30, 4);
        add(L"Спортзал", L"", 1, 19 * 60, 20 * 60 + 30, 1);
        add(L"Отчёт за квартал", L"", 3, 15 * 60, 17 * 60, 5);
        add(L"День рождения Анны", L"", 5, 0, 0, 2, true, 1);
        add(L"Поездка за город", L"", 6, 0, 0, 1, true);
        add(L"Стоматолог", L"Клиника «Улыбка»", -2, 9 * 60, 10 * 60, 3);
        add(L"Встреча выпускников", L"Ресторан «Сад»", 9, 18 * 60 + 30, 22 * 60, 4);
    }
    return v;
}

// Государственные праздники России (фиксированные даты).
const wchar_t* Holiday(const SYSTEMTIME& d)
{
    struct H
    {
        int m, d;
        const wchar_t* name;
    };
    static const H hs[] = {{1, 1, L"Новый год"},
                           {1, 7, L"Рождество Христово"},
                           {2, 23, L"День защитника Отечества"},
                           {3, 8, L"Международный женский день"},
                           {5, 1, L"Праздник Весны и Труда"},
                           {5, 9, L"День Победы"},
                           {6, 12, L"День России"},
                           {11, 4, L"День народного единства"},
                           {12, 31, L"Новогодний вечер"}};
    for (const H& h : hs)
    {
        if (d.wMonth == h.m && d.wDay == h.d)
            return h.name;
    }
    if (d.wMonth == 1 && d.wDay >= 2 && d.wDay <= 8)
        return L"Новогодние каникулы";
    return nullptr;
}

std::wstring TimeText(int minutes) { return Pad2(minutes / 60 % 24) + L":" + Pad2(minutes % 60); }

enum class View
{
    Day,
    Week,
    Month,
};

enum Hk : int
{
    kHkNone = 0,
    kHkNew,
    kHkToday,
    kHkPrev,
    kHkNext,
    kHkView,
    kHkMiniPrev,
    kHkMiniNext,
    kHkMiniDay,
    kHkCalToggle,
    kHkCell,   // день в сетке месяца (index — номер дня)
    kHkEvent,  // событие (index — в Events())
    kHkSlot,   // ячейка часа в днях/неделе (index = day*24 + hour)
    kHkMore,
    kHkDlgDayPrev,
    kHkDlgDayNext,
    kHkDlgUp,
    kHkDlgDown,
    kHkDlgAllDay,
    kHkDlgColor,
    kHkDlgDelete,
};

class CalendarWindow : public Window
{
public:
    explicit CalendarWindow(const std::wstring& /*arg*/) : Window(AppId::Calendar, L"Календарь")
    {
        SetInitialSize(1200.0f, 780.0f);
        m_minSize = {640.0f, 520.0f};
        m_today = DayNumber(LocalNow());
        m_sel = m_today;
        m_miniMonth = MonthKey(m_today);
        m_titleBox = m_ui.Add<TextBox>(L"Добавить название");
        m_titleBox->SetVisible(false);
        m_titleBox->onEnter = [this]() { SaveDialog(); };
        m_titleBox->onEscape = [this]() { CloseDialog(); };
        m_locBox = m_ui.Add<TextBox>(L"Добавить расположение");
        m_locBox->leadingIcon = Icon::Location;
        m_locBox->SetVisible(false);
        m_locBox->onEnter = [this]() { SaveDialog(); };
        m_locBox->onEscape = [this]() { CloseDialog(); };
        m_saveBtn = m_ui.Add<Button>(L"Сохранить", Icon::None, ButtonStyle::Accent);
        m_saveBtn->SetVisible(false);
        m_saveBtn->onClick = [this]() { SaveDialog(); };
        m_cancelBtn = m_ui.Add<Button>(L"Отмена", Icon::None, ButtonStyle::Standard);
        m_cancelBtn->SetVisible(false);
        m_cancelBtn->onClick = [this]() { CloseDialog(); };
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnResize() override
    {
        if (m_dlg)
            LayoutDialog();
    }

    void OnTimer1s() override
    {
        const int t = DayNumber(LocalNow());
        if (t != m_today)
            m_today = t;
    }

private:
    static int MonthKey(int day)
    {
        const SYSTEMTIME d = DateOf(day);
        return d.wYear * 12 + d.wMonth - 1;
    }
    static int FirstOfMonth(int key)
    {
        SYSTEMTIME s{};
        s.wYear = static_cast<WORD>(key / 12);
        s.wMonth = static_cast<WORD>(key % 12 + 1);
        s.wDay = 1;
        return DayNumber(s);
    }

    bool PaneVisible() const { return Width() >= 900.0f; }
    Rect MainRect() const
    {
        const float x = PaneVisible() ? kPaneW : 0.0f;
        return {x, kTitleH, Width() - x, Height() - kTitleH};
    }

    bool Visible(const Event& e) const { return e.calendar == 0 ? m_showPersonal : m_showBirthdays; }

    std::vector<int> EventsOn(int day, bool allDay) const
    {
        std::vector<int> out;
        const std::vector<Event>& ev = Events();
        for (int i = 0; i < static_cast<int>(ev.size()); ++i)
        {
            if (ev[static_cast<size_t>(i)].day == day && ev[static_cast<size_t>(i)].allDay == allDay && Visible(ev[static_cast<size_t>(i)]))
                out.push_back(i);
        }
        std::sort(out.begin(), out.end(), [&](int a, int b) { return ev[static_cast<size_t>(a)].start < ev[static_cast<size_t>(b)].start; });
        return out;
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        r.DrawAppIcon(AppIcon::Calendar, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(L"Календарь", {48.0f, 0.0f, 200.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        if (PaneVisible())
            DrawPane(r);
        const Rect m = MainRect();
        Imm::ContentLayer(r, m);
        DrawToolbar(r, m);
        const Rect body{m.x, m.y + kToolH, m.w, m.h - kToolH};
        r.PushClip(body);
        m_imm.SetClip(body);
        if (m_view == View::Month)
            DrawMonth(r, body);
        else
            DrawDays(r, body, m_view == View::Day ? 1 : 7);
        r.PopClip();
        m_imm.ResetClip();
        if (m_dlg)
        {
            m_imm.Clear();
            DrawDialog(r);
        }
        m_ui.Draw(r);
    }

    void DrawPane(Renderer& r)
    {
        const Palette& p = Theme::P();
        Imm::TextButton(r, m_imm, {16.0f, kTitleH + 8.0f, kPaneW - 32.0f, 36.0f}, L"Новое событие", Icon::Add, true, kHkNew);
        // Мини-календарь
        const float x0 = 16.0f, y0 = kTitleH + 64.0f, cell = (kPaneW - 32.0f) / 7.0f;
        r.Text(std::wstring(kMonthNom[m_miniMonth % 12]) + L" " + std::to_wstring(m_miniMonth / 12), {x0 + 4.0f, y0, 180.0f, 28.0f}, p.text, 14.0f,
               FontWeight::Semibold, TextFlags::VCenter);
        Imm::SubtleButton(r, m_imm, {kPaneW - 16.0f - 64.0f, y0, 30.0f, 28.0f}, Icon::ChevronUp, kHkMiniPrev, 0, L"Предыдущий месяц");
        Imm::SubtleButton(r, m_imm, {kPaneW - 16.0f - 32.0f, y0, 30.0f, 28.0f}, Icon::ChevronDown, kHkMiniNext, 0, L"Следующий месяц");
        for (int i = 0; i < 7; ++i)
            r.Text(kWeekShort[i], {x0 + static_cast<float>(i) * cell, y0 + 34.0f, cell, 20.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Center);
        const int first = FirstOfMonth(m_miniMonth);
        const int start = first - Weekday(first);
        for (int k = 0; k < 42; ++k)
        {
            const int day = start + k;
            const SYSTEMTIME d = DateOf(day);
            const float cx = x0 + static_cast<float>(k % 7) * cell + cell * 0.5f, cy = y0 + 70.0f + static_cast<float>(k / 7) * 32.0f;
            const bool inMonth = MonthKey(day) == m_miniMonth;
            const bool today = day == m_today, sel = day == m_sel;
            if (today)
                r.FillCircle(cx, cy, 14.0f, m_imm.Hot(kHkMiniDay, k) ? p.accentHover : p.accent);
            else if (sel)
                r.FillCircle(cx, cy, 14.0f, p.subtlePressed);
            else if (m_imm.Hot(kHkMiniDay, k))
                r.FillCircle(cx, cy, 14.0f, p.subtleHover);
            r.Text(std::to_wstring(d.wDay), {cx - 16.0f, cy - 12.0f, 32.0f, 24.0f}, today ? p.textOnAccent : (inMonth ? p.text : p.textTertiary), 12.0f,
                   FontWeight::Regular, TextFlags::Middle);
            if (!today && !EventsOn(day, false).empty())
                r.FillCircle(cx, cy + 10.0f, 1.8f, p.accent);
            m_imm.Add({cx - 15.0f, cy - 15.0f, 30.0f, 30.0f}, kHkMiniDay, k);
        }
        // Календари
        float y = y0 + 70.0f + 6.0f * 32.0f + 8.0f;
        r.FillRect({16.0f, y, kPaneW - 32.0f, 1.0f}, p.divider);
        y += 14.0f;
        r.Text(L"Календари", {20.0f, y, kPaneW - 40.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 32.0f;
        const struct
        {
            const wchar_t* name;
            bool on;
            uint32_t color;
        } cals[] = {{L"Календарь", m_showPersonal, kColors[0]}, {L"Праздники России", m_showHolidays, kColors[5]}, {L"Дни рождения", m_showBirthdays, kColors[2]}};
        for (int i = 0; i < 3; ++i)
        {
            const Rect row{12.0f, y, kPaneW - 24.0f, 32.0f};
            if (m_imm.Hot(kHkCalToggle, i))
                r.FillRoundRect(row, 4.0f, p.subtleHover);
            const Rect box{row.x + 8.0f, row.CenterY() - 9.0f, 18.0f, 18.0f};
            const Color col = Color::Hex(cals[i].color);
            if (cals[i].on)
            {
                r.FillRoundRect(box, 4.0f, col);
                r.Glyph(Icon::Check, box.Inset(3.0f, 3.0f, 3.0f, 3.0f), Color(255, 255, 255));
            }
            else
                r.StrokeRoundRect(box, 4.0f, col, 1.5f);
            r.Text(cals[i].name, {row.x + 36.0f, row.y, row.w - 40.0f, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            m_imm.Add(row, kHkCalToggle, i);
            y += 34.0f;
        }
    }

    std::wstring RangeTitle() const
    {
        const SYSTEMTIME s = DateOf(m_sel);
        if (m_view == View::Month)
            return std::wstring(kMonthNom[s.wMonth - 1]) + L" " + std::to_wstring(s.wYear);
        if (m_view == View::Day)
            return std::to_wstring(s.wDay) + L" " + kMonthGen[s.wMonth - 1] + L" " + std::to_wstring(s.wYear);
        const int start = m_sel - Weekday(m_sel);
        const SYSTEMTIME a = DateOf(start), b = DateOf(start + 6);
        if (a.wMonth == b.wMonth)
            return std::to_wstring(a.wDay) + L"–" + std::to_wstring(b.wDay) + L" " + kMonthGen[a.wMonth - 1] + L" " + std::to_wstring(a.wYear);
        return std::to_wstring(a.wDay) + L" " + kMonthGen[a.wMonth - 1] + L" – " + std::to_wstring(b.wDay) + L" " + kMonthGen[b.wMonth - 1] + L" " +
               std::to_wstring(b.wYear);
    }

    void DrawToolbar(Renderer& r, const Rect& m)
    {
        const Palette& p = Theme::P();
        const float y = m.y + 12.0f;
        float x = m.x + 20.0f;
        if (!PaneVisible())
        {
            Imm::SubtleButton(r, m_imm, {x, y, 36.0f, 36.0f}, Icon::Add, kHkNew, 0, L"Новое событие");
            x += 44.0f;
        }
        Imm::TextButton(r, m_imm, {x, y + 2.0f, 96.0f, 32.0f}, L"Сегодня", Icon::None, false, kHkToday);
        x += 108.0f;
        Imm::SubtleButton(r, m_imm, {x, y, 36.0f, 36.0f}, Icon::ChevronLeft, kHkPrev, 0, L"Назад");
        Imm::SubtleButton(r, m_imm, {x + 40.0f, y, 36.0f, 36.0f}, Icon::ChevronRight, kHkNext, 0, L"Вперёд");
        x += 88.0f;
        r.Text(RangeTitle(), {x, y, m.Right() - x - 300.0f, 36.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        // Переключатель представлений
        static const wchar_t* names[] = {L"День", L"Неделя", L"Месяц"};
        float vx = m.Right() - 20.0f;
        for (int i = 2; i >= 0; --i)
        {
            const float w = r.TextWidth(names[i], 14.0f) + 28.0f;
            vx -= w;
            const Rect rc{vx, y + 2.0f, w, 32.0f};
            const bool sel = static_cast<int>(m_view) == i;
            if (sel)
                r.FillRoundRect(rc, 4.0f, p.dark ? Color(255, 255, 255, 0x18) : Color(0, 0, 0, 0x0A));
            else if (m_imm.Hot(kHkView, i))
                r.FillRoundRect(rc, 4.0f, p.subtleHover);
            r.Text(names[i], rc, sel ? p.text : p.textSecondary, 14.0f, sel ? FontWeight::Semibold : FontWeight::Regular, TextFlags::Middle);
            if (sel)
                r.FillRoundRect({rc.CenterX() - 8.0f, rc.Bottom() - 3.0f, 16.0f, 3.0f}, 1.5f, p.accent);
            m_imm.Add(rc, kHkView, i);
            vx -= 4.0f;
        }
        r.FillRect({m.x, m.y + kToolH - 1.0f, m.w, 1.0f}, p.divider);
    }

    void EventChip(Renderer& r, const Rect& rc, int idx, bool compact)
    {
        const Palette& p = Theme::P();
        const Event& e = Events()[static_cast<size_t>(idx)];
        const Color col = Color::Hex(kColors[Clamp(e.color, 0, 5)]);
        const bool hot = m_imm.Hot(kHkEvent, idx);
        if (e.allDay || !compact)
        {
            r.FillRoundRect(rc, 4.0f, hot ? col : col.MulAlpha(p.dark ? 0.55f : 0.22f));
            r.FillRoundRect({rc.x, rc.y, 3.0f, rc.h}, 1.5f, col);
            r.Text(compact ? e.title : e.title, {rc.x + 8.0f, rc.y, rc.w - 10.0f, std::min(rc.h, 20.0f)}, hot ? Color(255, 255, 255) : p.text, 12.0f,
                   FontWeight::Semibold, TextFlags::VCenter);
            if (!compact && rc.h > 36.0f)
            {
                std::wstring sub = TimeText(e.start) + L" – " + TimeText(e.end);
                if (!e.location.empty())
                    sub += L" · " + e.location;
                r.Text(sub, {rc.x + 8.0f, rc.y + 18.0f, rc.w - 10.0f, 18.0f}, hot ? Color(255, 255, 255, 0xD0) : p.textSecondary, 11.0f, FontWeight::Regular,
                       TextFlags::VCenter);
            }
        }
        else
        {
            if (hot)
                r.FillRoundRect(rc, 4.0f, p.subtleHover);
            r.FillCircle(rc.x + 8.0f, rc.CenterY(), 4.0f, col);
            r.Text(TimeText(e.start) + L" " + e.title, {rc.x + 18.0f, rc.y, rc.w - 20.0f, rc.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        m_imm.Add(rc, kHkEvent, idx, e.title);
    }

    void DrawMonth(Renderer& r, const Rect& body)
    {
        const Palette& p = Theme::P();
        const int key = MonthKey(m_sel);
        const int first = FirstOfMonth(key);
        const int start = first - Weekday(first);
        const float hdr = 28.0f;
        const float cw = body.w / 7.0f, ch = (body.h - hdr) / 6.0f;
        for (int i = 0; i < 7; ++i)
            r.Text(kWeekLong[i], {body.x + static_cast<float>(i) * cw + 10.0f, body.y + 4.0f, cw - 12.0f, 20.0f}, p.textSecondary, 12.0f);
        for (int k = 0; k < 42; ++k)
        {
            const int day = start + k;
            const SYSTEMTIME d = DateOf(day);
            const Rect cell{body.x + static_cast<float>(k % 7) * cw, body.y + hdr + static_cast<float>(k / 7) * ch, cw, ch};
            const bool inMonth = MonthKey(day) == key;
            if (day == m_sel)
                r.FillRect(cell, p.dark ? Color(255, 255, 255, 0x0C) : Color(0, 0, 0, 0x06));
            else if (m_imm.Hot(kHkCell, k))
                r.FillRect(cell, p.dark ? Color(255, 255, 255, 0x06) : Color(0, 0, 0, 0x03));
            r.FillRect({cell.x, cell.y, cell.w, 1.0f}, p.divider);
            if (k % 7)
                r.FillRect({cell.x, cell.y, 1.0f, cell.h}, p.divider);
            m_imm.Add(cell, kHkCell, day);
            const std::wstring num = d.wDay == 1 ? std::to_wstring(d.wDay) + L" " + kMonthGen[d.wMonth - 1] : std::to_wstring(d.wDay);
            if (day == m_today)
            {
                const float tw = r.TextWidth(num, 12.0f, FontWeight::Semibold) + 16.0f;
                r.FillRoundRect({cell.x + 6.0f, cell.y + 6.0f, std::max(24.0f, tw), 24.0f}, 12.0f, p.accent);
                r.Text(num, {cell.x + 6.0f, cell.y + 6.0f, std::max(24.0f, tw), 24.0f}, p.textOnAccent, 12.0f, FontWeight::Semibold, TextFlags::Middle);
            }
            else
                r.Text(num, {cell.x + 12.0f, cell.y + 6.0f, cell.w - 20.0f, 24.0f}, inMonth ? p.text : p.textTertiary, 12.0f,
                       d.wDay == 1 ? FontWeight::Semibold : FontWeight::Regular, TextFlags::VCenter);
            float ey = cell.y + 34.0f;
            int shown = 0, total = 0;
            const float rowH = 22.0f;
            const int capacity = std::max(0, static_cast<int>((cell.Bottom() - ey - 4.0f) / rowH));
            const wchar_t* hol = m_showHolidays ? Holiday(d) : nullptr;
            if (hol)
            {
                ++total;
                if (shown < capacity)
                {
                    const Rect hr{cell.x + 4.0f, ey, cell.w - 8.0f, rowH - 2.0f};
                    r.FillRoundRect(hr, 4.0f, Color::Hex(kColors[5]).MulAlpha(p.dark ? 0.5f : 0.18f));
                    r.Text(hol, {hr.x + 8.0f, hr.y, hr.w - 10.0f, hr.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
                    ey += rowH;
                    ++shown;
                }
            }
            std::vector<int> evs = EventsOn(day, true);
            const std::vector<int> timed = EventsOn(day, false);
            evs.insert(evs.end(), timed.begin(), timed.end());
            for (int idx : evs)
            {
                ++total;
                if (shown >= capacity - (static_cast<int>(evs.size()) + (hol ? 1 : 0) > capacity ? 1 : 0))
                    continue;
                EventChip(r, {cell.x + 4.0f, ey, cell.w - 8.0f, rowH - 2.0f}, idx, true);
                ey += rowH;
                ++shown;
            }
            if (total > shown && capacity > 0)
                r.Text(L"Ещё " + std::to_wstring(total - shown), {cell.x + 12.0f, ey, cell.w - 20.0f, rowH - 2.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                       TextFlags::VCenter);
        }
    }

    void DrawDays(Renderer& r, const Rect& body, int count)
    {
        const Palette& p = Theme::P();
        const int start = count == 1 ? m_sel : m_sel - Weekday(m_sel);
        const float gutter = 64.0f;
        const float colW = (body.w - gutter) / static_cast<float>(count);
        // Заголовки дней и строка событий на весь день
        float allDayH = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const int n = static_cast<int>(EventsOn(start + i, true).size()) + (m_showHolidays && Holiday(DateOf(start + i)) ? 1 : 0);
            allDayH = std::max(allDayH, static_cast<float>(n) * 24.0f);
        }
        const float headH = 56.0f + (allDayH > 0.0f ? allDayH + 6.0f : 0.0f);
        for (int i = 0; i < count; ++i)
        {
            const int day = start + i;
            const SYSTEMTIME d = DateOf(day);
            const float x = body.x + gutter + static_cast<float>(i) * colW;
            const bool today = day == m_today;
            r.Text(kWeekShort[Weekday(day)], {x + 10.0f, body.y + 6.0f, colW - 16.0f, 18.0f}, today ? p.accentText : p.textSecondary, 12.0f);
            if (today)
            {
                r.FillCircle(x + 26.0f, body.y + 38.0f, 15.0f, p.accent);
                r.Text(std::to_wstring(d.wDay), {x + 10.0f, body.y + 22.0f, 32.0f, 32.0f}, p.textOnAccent, 16.0f, FontWeight::Semibold, TextFlags::Middle);
            }
            else
                r.Text(std::to_wstring(d.wDay), {x + 10.0f, body.y + 22.0f, 60.0f, 32.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
            float ay = body.y + 58.0f;
            if (const wchar_t* hol = m_showHolidays ? Holiday(d) : nullptr)
            {
                const Rect hr{x + 2.0f, ay, colW - 4.0f, 22.0f};
                r.FillRoundRect(hr, 4.0f, Color::Hex(kColors[5]).MulAlpha(p.dark ? 0.5f : 0.18f));
                r.Text(hol, hr.Inset(8.0f, 0.0f, 2.0f, 0.0f), p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
                ay += 24.0f;
            }
            for (int idx : EventsOn(day, true))
            {
                EventChip(r, {x + 2.0f, ay, colW - 4.0f, 22.0f}, idx, true);
                ay += 24.0f;
            }
        }
        r.FillRect({body.x, body.y + headH - 1.0f, body.w, 1.0f}, p.divider);
        // Сетка часов (прокручиваемая)
        const Rect grid{body.x, body.y + headH, body.w, body.h - headH};
        const float hourH = 52.0f;
        m_gridView = grid;
        m_gridH = hourH * 24.0f;
        if (!m_scrolledInit)
        {
            m_scroll = Clamp(hourH * 7.5f, 0.0f, std::max(0.0f, m_gridH - grid.h));
            m_scrolledInit = true;
        }
        r.PushClip(grid);
        const Rect saved = m_imm.Clip();
        m_imm.SetClip(grid.Intersect(saved));
        const float gy = grid.y - m_scroll;
        for (int h = 0; h < 24; ++h)
        {
            const float y = gy + static_cast<float>(h) * hourH;
            if (h > 0)
                r.Text(Pad2(h) + L":00", {body.x, y - 9.0f, gutter - 10.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Right);
            r.FillRect({body.x + gutter, y, body.w - gutter, 1.0f}, p.divider);
            for (int i = 0; i < count; ++i)
            {
                const Rect slot{body.x + gutter + static_cast<float>(i) * colW, y, colW, hourH};
                if (m_imm.Hot(kHkSlot, (start + i - m_today + 1000) * 24 + h))
                    r.FillRect(slot.Inset(1.0f, 1.0f, 0.0f, 0.0f), p.dark ? Color(255, 255, 255, 0x08) : Color(0, 0, 0, 0x04));
                m_imm.Add(slot, kHkSlot, (start + i - m_today + 1000) * 24 + h);
            }
        }
        for (int i = 0; i <= count; ++i)
            r.FillRect({body.x + gutter + static_cast<float>(i) * colW, gy, 1.0f, m_gridH}, p.divider);
        // События
        for (int i = 0; i < count; ++i)
        {
            const int day = start + i;
            const std::vector<int> evs = EventsOn(day, false);
            // Простое размещение пересекающихся событий в колонки.
            std::vector<int> lane(evs.size(), 0);
            int lanes = 1;
            for (size_t a = 0; a < evs.size(); ++a)
            {
                for (size_t b = 0; b < a; ++b)
                {
                    const Event& ea = Events()[static_cast<size_t>(evs[a])];
                    const Event& eb = Events()[static_cast<size_t>(evs[b])];
                    if (eb.end > ea.start && lane[b] == lane[a])
                        lane[a] = lane[b] + 1;
                }
                lanes = std::max(lanes, lane[a] + 1);
            }
            const float x = body.x + gutter + static_cast<float>(i) * colW;
            for (size_t a = 0; a < evs.size(); ++a)
            {
                const Event& e = Events()[static_cast<size_t>(evs[a])];
                const float lw = (colW - 8.0f) / static_cast<float>(lanes);
                const float y0 = gy + static_cast<float>(e.start) / 60.0f * hourH, y1 = gy + static_cast<float>(std::max(e.end, e.start + 20)) / 60.0f * hourH;
                EventChip(r, {x + 4.0f + lw * static_cast<float>(lane[a]), y0 + 1.0f, lw - 2.0f, y1 - y0 - 2.0f}, evs[a], false);
            }
            // Линия текущего времени
            if (day == m_today)
            {
                const SYSTEMTIME now = LocalNow();
                const float ny = gy + (static_cast<float>(now.wHour) + static_cast<float>(now.wMinute) / 60.0f) * hourH;
                r.FillRect({x, ny - 1.0f, colW, 2.0f}, p.critical);
                r.FillCircle(x, ny, 5.0f, p.critical);
            }
        }
        m_imm.SetClip(saved);
        r.PopClip();
        Imm::Scrollbar(r, grid, m_scroll, m_gridH);
    }

    // ================================================================== диалог события
    Rect DialogRect() const
    {
        const float w = 460.0f, h = 452.0f;
        return {std::floor((Width() - w) * 0.5f), std::floor(std::max(kTitleH, (Height() - h) * 0.5f)), w, h};
    }

    void OpenDialog(int index, int day, int startMin)
    {
        m_dlg = true;
        m_dlgIndex = index;
        Event e;
        if (index >= 0)
            e = Events()[static_cast<size_t>(index)];
        else
        {
            e.day = day;
            e.start = Clamp(startMin, 0, 23 * 60);
            e.end = std::min(24 * 60 - 1, e.start + 60);
        }
        m_dlgEvent = e;
        m_titleBox->SetText(e.title);
        m_locBox->SetText(e.location);
        m_titleBox->SetVisible(true);
        m_locBox->SetVisible(true);
        m_saveBtn->SetVisible(true);
        m_cancelBtn->SetVisible(true);
        m_ui.SetFocus(m_titleBox);
        m_titleBox->SelectAll();
        m_dlgAnim.Start(0.0f, 1.0f, 0.2f, Ease::OutCubic);
        LayoutDialog();
    }

    void LayoutDialog()
    {
        const Rect d = DialogRect();
        m_titleBox->SetBounds({d.x + 24.0f, d.y + 64.0f, d.w - 48.0f, 32.0f});
        m_locBox->SetBounds({d.x + 24.0f, d.y + 284.0f, d.w - 48.0f, 32.0f});
        const float bw = (d.w - 56.0f) * 0.5f;
        m_saveBtn->SetBounds({d.x + 24.0f, d.Bottom() - 56.0f, bw, 32.0f});
        m_cancelBtn->SetBounds({d.x + 32.0f + bw, d.Bottom() - 56.0f, bw, 32.0f});
    }

    void CloseDialog()
    {
        m_dlg = false;
        m_titleBox->SetVisible(false);
        m_locBox->SetVisible(false);
        m_saveBtn->SetVisible(false);
        m_cancelBtn->SetVisible(false);
        m_ui.SetFocus(nullptr);
    }

    void SaveDialog()
    {
        Event e = m_dlgEvent;
        e.title = Trim(m_titleBox->Text());
        if (e.title.empty())
            e.title = L"Без названия";
        e.location = Trim(m_locBox->Text());
        if (e.end <= e.start)
            e.end = std::min(24 * 60 - 1, e.start + 30);
        if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(Events().size()))
            Events()[static_cast<size_t>(m_dlgIndex)] = e;
        else
            Events().push_back(e);
        m_sel = e.day;
        CloseDialog();
    }

    void DrawDialog(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect d = DialogRect();
        Imm::DialogFrame(r, {0.0f, 0.0f, Width(), Height()}, d, m_dlgAnim.Value(), m_dlgIndex >= 0 ? L"Изменить событие" : L"Новое событие");
        if (m_dlgIndex >= 0)
            Imm::SubtleButton(r, m_imm, {d.Right() - 56.0f, d.y + 16.0f, 36.0f, 34.0f}, Icon::Delete, kHkDlgDelete, 0, L"Удалить событие");
        const Event& e = m_dlgEvent;
        // Дата
        float y = d.y + 112.0f;
        const SYSTEMTIME sd = DateOf(e.day);
        r.Glyph(Icon::Calendar, {d.x + 26.0f, y + 8.0f, 16.0f, 16.0f}, p.textSecondary);
        Imm::SubtleButton(r, m_imm, {d.x + 52.0f, y, 32.0f, 32.0f}, Icon::ChevronLeft, kHkDlgDayPrev, 0, L"Предыдущий день");
        r.Text(std::wstring(kWeekLong[Weekday(e.day)]) + L", " + std::to_wstring(sd.wDay) + L" " + kMonthGen[sd.wMonth - 1] + L" " + std::to_wstring(sd.wYear),
               {d.x + 90.0f, y, d.w - 170.0f, 32.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Middle);
        Imm::SubtleButton(r, m_imm, {d.Right() - 24.0f - 32.0f, y, 32.0f, 32.0f}, Icon::ChevronRight, kHkDlgDayNext, 0, L"Следующий день");
        y += 48.0f;
        Imm::Check(r, m_imm, d.x + 26.0f, y + 6.0f, e.allDay, L"Весь день", kHkDlgAllDay);
        y += 44.0f;
        // Время начала и окончания
        const bool timed = !e.allDay;
        auto timeBox = [&](float x, int value, int which, const wchar_t* label) {
            r.Text(label, {x, y - 2.0f, 60.0f, 32.0f}, timed ? p.textSecondary : p.textDisabled, 13.0f, FontWeight::Regular, TextFlags::VCenter);
            const Rect box{x + 56.0f, y, 132.0f, 32.0f};
            r.FillRoundRect(box, 4.0f, timed ? p.control : p.controlDisabled);
            r.StrokeRoundRect(box, 4.0f, p.controlStroke);
            r.Text(TimeText(value), {box.x + 12.0f, box.y, 60.0f, box.h}, timed ? p.text : p.textDisabled, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            if (timed)
            {
                Imm::SubtleButton(r, m_imm, {box.Right() - 60.0f, box.y + 2.0f, 28.0f, 28.0f}, Icon::ChevronUp, kHkDlgUp, which, L"Позже на 15 минут");
                Imm::SubtleButton(r, m_imm, {box.Right() - 30.0f, box.y + 2.0f, 28.0f, 28.0f}, Icon::ChevronDown, kHkDlgDown, which, L"Раньше на 15 минут");
            }
        };
        timeBox(d.x + 24.0f, e.start, 0, L"Начало");
        timeBox(d.x + 236.0f, e.end, 1, L"Конец");
        y += 72.0f; // поле расположения (виджет) — на y = d.y + 284
        y += 4.0f;
        // Цвет
        r.Text(L"Цвет", {d.x + 24.0f, d.y + 330.0f, 60.0f, 28.0f}, p.textSecondary, 13.0f, FontWeight::Regular, TextFlags::VCenter);
        for (int i = 0; i < 6; ++i)
        {
            const float cx = d.x + 104.0f + static_cast<float>(i) * 36.0f, cy = d.y + 344.0f;
            r.FillCircle(cx, cy, 12.0f, Color::Hex(kColors[i]));
            if (i == e.color)
                r.StrokeCircle(cx, cy, 15.5f, p.text, 2.0f);
            else if (m_imm.Hot(kHkDlgColor, i))
                r.StrokeCircle(cx, cy, 15.5f, p.controlStrongStroke, 1.5f);
            m_imm.Add({cx - 16.0f, cy - 16.0f, 32.0f, 32.0f}, kHkDlgColor, i);
        }
    }

    // ================================================================== действия
    void Shift(int dir)
    {
        if (m_view == View::Day)
            m_sel += dir;
        else if (m_view == View::Week)
            m_sel += 7 * dir;
        else
        {
            const SYSTEMTIME s = DateOf(m_sel);
            const int key = MonthKey(m_sel) + dir;
            m_sel = FirstOfMonth(key) + std::min(static_cast<int>(s.wDay) - 1, 27);
        }
        m_miniMonth = MonthKey(m_sel);
    }

    void Activate(int kind, int index, int clicks)
    {
        switch (kind)
        {
        case kHkNew:
            OpenDialog(-1, m_sel, (LocalNow().wHour + 1) * 60);
            break;
        case kHkToday:
            m_sel = m_today;
            m_miniMonth = MonthKey(m_today);
            m_scrolledInit = false;
            break;
        case kHkPrev:
        case kHkNext:
            Shift(kind == kHkNext ? 1 : -1);
            break;
        case kHkView:
            m_view = static_cast<View>(Clamp(index, 0, 2));
            m_scrolledInit = false;
            break;
        case kHkMiniPrev:
            --m_miniMonth;
            break;
        case kHkMiniNext:
            ++m_miniMonth;
            break;
        case kHkMiniDay:
        {
            const int first = FirstOfMonth(m_miniMonth);
            m_sel = first - Weekday(first) + index;
            m_miniMonth = MonthKey(m_sel);
            break;
        }
        case kHkCalToggle:
            if (index == 0)
                m_showPersonal = !m_showPersonal;
            else if (index == 1)
                m_showHolidays = !m_showHolidays;
            else
                m_showBirthdays = !m_showBirthdays;
            break;
        case kHkCell:
            m_sel = index;
            if (clicks >= 2)
                OpenDialog(-1, index, 9 * 60);
            break;
        case kHkEvent:
            OpenDialog(index, 0, 0);
            break;
        case kHkSlot:
        {
            const int day = index / 24 - 1000 + m_today, hour = index % 24;
            m_sel = day;
            if (clicks >= 2)
                OpenDialog(-1, day, hour * 60);
            break;
        }
        case kHkDlgDayPrev:
            --m_dlgEvent.day;
            break;
        case kHkDlgDayNext:
            ++m_dlgEvent.day;
            break;
        case kHkDlgAllDay:
            m_dlgEvent.allDay = !m_dlgEvent.allDay;
            break;
        case kHkDlgUp:
        case kHkDlgDown:
        {
            int& v = index == 0 ? m_dlgEvent.start : m_dlgEvent.end;
            const int len = m_dlgEvent.end - m_dlgEvent.start;
            v = Clamp(v + (kind == kHkDlgUp ? 15 : -15), 0, 24 * 60 - 15);
            if (index == 0)
                m_dlgEvent.end = std::min(24 * 60 - 1, m_dlgEvent.start + std::max(15, len));
            else if (m_dlgEvent.end <= m_dlgEvent.start)
                m_dlgEvent.start = std::max(0, m_dlgEvent.end - 15);
            break;
        }
        case kHkDlgColor:
            m_dlgEvent.color = index;
            break;
        case kHkDlgDelete:
            if (m_dlgIndex >= 0 && m_dlgIndex < static_cast<int>(Events().size()))
                Events().erase(Events().begin() + m_dlgIndex);
            CloseDialog();
            break;
        default:
            break;
        }
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (!HasDialog())
            m_imm.Move(e.pos, {Frame().x, Frame().y});
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_imm.Leave();
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        Window::OnMouseDown(e);
        if (HasDialog() || m_ui.HasCapture())
            return;
        m_imm.Down(e.pos, e.clicks);
        // Двойной щелчок приходит вторым нажатием: обрабатываем сразу.
        if (e.clicks >= 2 && e.button == MouseButton::Left && !m_dlg)
        {
            const ImmHot* h = m_imm.Hit(e.pos);
            if (h && (h->kind == kHkCell || h->kind == kHkSlot))
            {
                Activate(h->kind, h->index, 2);
                m_imm.Leave();
            }
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog())
            return;
        int kind = 0, index = 0;
        if (m_imm.Up(e.pos, kind, index) && e.button == MouseButton::Left)
            Activate(kind, index, 1);
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog() || m_dlg)
            return true;
        if (m_view == View::Month)
            Shift(e.wheel > 0.0f ? -1 : 1);
        else if (m_gridView.Contains(e.pos))
            m_scroll = Clamp(m_scroll - e.wheel * 80.0f, 0.0f, std::max(0.0f, m_gridH - m_gridView.h));
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return Window::OnKeyDown(vk, mods);
        if (m_dlg)
        {
            if (m_ui.FocusedChild())
                return Window::OnKeyDown(vk, mods);
            if (vk == VK_ESCAPE)
                CloseDialog();
            else if (vk == VK_RETURN)
                SaveDialog();
            return true;
        }
        if (mods.ctrl && vk == 'N')
        {
            Activate(kHkNew, 0, 1);
            return true;
        }
        if (mods.ctrl && (vk == '1' || vk == '2' || vk == '3'))
        {
            m_view = static_cast<View>(vk - '1');
            m_scrolledInit = false;
            return true;
        }
        if (vk == VK_LEFT || vk == VK_RIGHT)
        {
            Shift(vk == VK_RIGHT ? 1 : -1);
            return true;
        }
        if (vk == 'T' && !mods.ctrl)
        {
            Activate(kHkToday, 0, 1);
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

    // ================================================================== поля
    View m_view = View::Month;
    int m_today = 0;
    int m_sel = 0;
    int m_miniMonth = 0;
    bool m_showPersonal = true;
    bool m_showHolidays = true;
    bool m_showBirthdays = true;
    float m_scroll = 0.0f;
    bool m_scrolledInit = false;
    Rect m_gridView;
    float m_gridH = 0.0f;
    ImmUi m_imm;
    bool m_dlg = false;
    int m_dlgIndex = -1;
    Event m_dlgEvent;
    Tween m_dlgAnim;
    TextBox* m_titleBox = nullptr;
    TextBox* m_locBox = nullptr;
    Button* m_saveBtn = nullptr;
    Button* m_cancelBtn = nullptr;
};
} // namespace

std::unique_ptr<Window> CreateCalendar(const std::wstring& arg) { return std::make_unique<CalendarWindow>(arg); }
