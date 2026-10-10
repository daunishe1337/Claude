#include "StartMenu.h"

#include "Taskbar.h"

namespace
{
constexpr int kPerPage = 18;

struct SettingEntry
{
    const wchar_t* name;
    const wchar_t* page;
    Icon icon;
};

const std::vector<SettingEntry>& SettingEntries()
{
    static const std::vector<SettingEntry> list = {
        {L"Персонализация", L"personalization", Icon::Personalize},
        {L"Цвета", L"colors", Icon::Palette},
        {L"Тёмная тема", L"colors", Icon::NightLight},
        {L"Светлая тема", L"colors", Icon::Brightness},
        {L"Контрастный цвет", L"colors", Icon::Palette},
        {L"Фон рабочего стола", L"background", Icon::Pictures},
        {L"Обои", L"background", Icon::Pictures},
        {L"Панель задач", L"taskbar", Icon::Apps},
        {L"Параметры меню «Пуск»", L"start", Icon::Apps},
        {L"Дисплей", L"display", Icon::Display},
        {L"Яркость", L"display", Icon::Brightness},
        {L"Ночной свет", L"display", Icon::NightLight},
        {L"Звук", L"sound", Icon::Volume3},
        {L"Уведомления", L"notifications", Icon::Bell},
        {L"Питание и батарея", L"power", Icon::Battery2},
        {L"Хранилище", L"storage", Icon::Storage},
        {L"Многозадачность", L"multitasking", Icon::Multitask},
        {L"О системе", L"about", Icon::Info},
        {L"Bluetooth и устройства", L"bluetooth", Icon::Bluetooth},
        {L"Мышь", L"bluetooth", Icon::Mouse},
        {L"Сеть и Интернет", L"network", Icon::Globe},
        {L"Wi-Fi", L"network", Icon::Wifi},
        {L"Режим «в самолёте»", L"network", Icon::Airplane},
        {L"Установленные приложения", L"apps", Icon::Apps},
        {L"Учётные записи", L"accounts", Icon::User},
        {L"Дата и время", L"time", Icon::Clock},
        {L"Язык и регион", L"time", Icon::Language},
        {L"Игры", L"gaming", Icon::Game},
        {L"Специальные возможности", L"accessibility", Icon::Accessibility},
        {L"Эффекты анимации", L"accessibility", Icon::Eye},
        {L"Конфиденциальность и защита", L"privacy", Icon::Shield},
        {L"Центр обновления Windows", L"update", Icon::Update},
    };
    return list;
}

std::wstring RelativeTime(const SYSTEMTIME& t)
{
    const SYSTEMTIME now = LocalNow();
    FILETIME a{}, b{};
    SystemTimeToFileTime(&t, &a);
    SystemTimeToFileTime(&now, &b);
    const long long ta = static_cast<long long>((static_cast<unsigned long long>(a.dwHighDateTime) << 32) | a.dwLowDateTime);
    const long long tb = static_cast<long long>((static_cast<unsigned long long>(b.dwHighDateTime) << 32) | b.dwLowDateTime);
    const long long minutes = (tb - ta) / 600000000LL;
    if (minutes < 1)
        return L"Только что";
    if (minutes < 60)
        return std::to_wstring(minutes) + L" мин назад";
    if (t.wYear == now.wYear && t.wMonth == now.wMonth && t.wDay == now.wDay)
        return std::to_wstring(minutes / 60) + L" ч назад";
    const SYSTEMTIME y = AddDays(now, -1);
    if (t.wYear == y.wYear && t.wMonth == y.wMonth && t.wDay == y.wDay)
        return L"Вчера в " + FormatTime(t);
    static const wchar_t* months[] = {L"янв.", L"февр.", L"мар.", L"апр.", L"мая", L"июн.",
                                      L"июл.", L"авг.", L"сент.", L"окт.", L"нояб.", L"дек."};
    return std::to_wstring(t.wDay) + L" " + months[Clamp(static_cast<int>(t.wMonth), 1, 12) - 1];
}
} // namespace

StartMenu::StartMenu() : m_pinned(StartPinnedApps())
{
    m_search = m_ui.Add<TextBox>(L"Поиск приложений, параметров и документов");
    m_search->pill = true;
    m_search->leadingIcon = Icon::Search;
    m_search->onChange = [this](const std::wstring& s) { UpdateSearch(s); };
    m_search->onEnter = [this]() { OpenResult(m_selResult); };
    m_search->onKey = [this](UINT vk, const KeyMods&) {
        if (m_view != View::Search)
            return false;
        if (vk == VK_DOWN)
        {
            m_selResult = std::min(m_selResult + 1, static_cast<int>(m_results.size()) - 1);
            return true;
        }
        if (vk == VK_UP)
        {
            m_selResult = std::max(m_selResult - 1, 0);
            return true;
        }
        return false;
    };
}

Rect StartMenu::Bounds() const
{
    Shell& sh = GetShell();
    const float w = 640.0f;
    const float top = sh.TB().Top();
    const float h = std::min(726.0f, top - 24.0f);
    const float x = sh.Settings().taskbarCenter ? std::floor((sh.ScreenW() - w) * 0.5f) : 12.0f;
    return {x, top - 12.0f - h, w, h};
}

bool StartMenu::IsPinned(AppId app) const { return std::find(m_pinned.begin(), m_pinned.end(), app) != m_pinned.end(); }

void StartMenu::Pin(AppId app)
{
    if (!IsPinned(app))
        m_pinned.push_back(app);
}

void StartMenu::Unpin(AppId app) { m_pinned.erase(std::remove(m_pinned.begin(), m_pinned.end(), app), m_pinned.end()); }

int StartMenu::PageCount() const { return std::max(1, (static_cast<int>(m_pinned.size()) + kPerPage - 1) / kPerPage); }

void StartMenu::OnOpen()
{
    m_view = View::Home;
    m_viewAnim.Snap(1.0f);
    m_page = 0;
    m_pageAnim.Snap(0.0f);
    m_search->SetText(L"");
    m_ui.SetFocus(m_search);
    m_hover = Hit{};
    m_press = Hit{};
    const Rect b = Bounds();
    m_search->SetBounds({b.x + 32.0f, b.y + 24.0f, b.w - 64.0f, 36.0f});
}

void StartMenu::OnClose() { m_hover = Hit{}; }

void StartMenu::OpenSearch()
{
    if (!m_open)
        Open();
    m_ui.SetFocus(m_search);
}

void StartMenu::SetView(View v)
{
    if (m_view == v)
        return;
    m_view = v;
    m_viewAnim.Start(0.0f, 1.0f, 0.22f, Ease::OutCubic);
    m_hover = Hit{};
    if (v == View::AllApps)
    {
        BuildList();
        m_listScroll.Snap(0.0f);
    }
}

void StartMenu::BuildList()
{
    std::vector<AppId> apps = AllApps();
    std::sort(apps.begin(), apps.end(),
              [](AppId a, AppId b) { return CompareNoCase(GetAppInfo(a).name, GetAppInfo(b).name) < 0; });
    m_rows.clear();
    wchar_t last = 0;
    for (AppId a : apps)
    {
        const std::wstring name = GetAppInfo(a).name;
        const wchar_t letter = static_cast<wchar_t>(towupper(name.empty() ? L'#' : name[0]));
        if (letter != last)
        {
            ListRow row;
            row.letter = true;
            row.text = std::wstring(1, letter);
            m_rows.push_back(row);
            last = letter;
        }
        ListRow row;
        row.text = name;
        row.app = a;
        m_rows.push_back(row);
    }
}

void StartMenu::UpdateSearch(const std::wstring& q)
{
    const std::wstring query = Trim(q);
    if (query.empty())
    {
        SetView(View::Home);
        m_results.clear();
        return;
    }
    SetView(View::Search);
    m_results.clear();
    // Приложения: сначала начинающиеся с запроса.
    std::vector<AppId> starts, contains;
    for (AppId a : AllApps())
    {
        const std::wstring name = GetAppInfo(a).name;
        if (StartsWithNoCase(name, query))
            starts.push_back(a);
        else if (ContainsNoCase(name, query))
            contains.push_back(a);
    }
    for (const auto* list : {&starts, &contains})
    {
        for (AppId a : *list)
        {
            Result res;
            res.type = Result::Type::App;
            res.app = a;
            res.name = GetAppInfo(a).name;
            res.detail = GetAppInfo(a).kind;
            res.icon = GetAppInfo(a).icon;
            m_results.push_back(res);
        }
    }
    for (const SettingEntry& s : SettingEntries())
    {
        if (!ContainsNoCase(s.name, query))
            continue;
        Result res;
        res.type = Result::Type::Setting;
        res.name = s.name;
        res.detail = L"Системные параметры";
        res.arg = s.page;
        res.glyph = s.icon;
        m_results.push_back(res);
    }
    VirtualFS& fs = GetShell().FS();
    std::vector<FsNode*> files;
    fs.Search(fs.Home(), query, files);
    for (FsNode* n : files)
    {
        if (m_results.size() > 12)
            break;
        Result res;
        res.type = Result::Type::File;
        res.node = n;
        res.name = n->name;
        res.detail = fs.TypeName(n) + L" • " + fs.PathOf(n->parent);
        res.icon = fs.IconFor(n);
        m_results.push_back(res);
    }
    if (m_results.size() > 12)
        m_results.resize(12);
    m_selResult = 0;
}

void StartMenu::Launch(AppId app) { GetShell().Launch(app); }

void StartMenu::OpenResult(int index)
{
    if (index < 0 || index >= static_cast<int>(m_results.size()))
        return;
    const Result res = m_results[static_cast<size_t>(index)];
    Shell& sh = GetShell();
    switch (res.type)
    {
    case Result::Type::App:
        sh.Launch(res.app);
        break;
    case Result::Type::Setting:
        sh.Launch(AppId::Settings, res.arg);
        break;
    case Result::Type::File:
        sh.CloseFlyouts();
        if (sh.FS().IsAlive(res.node))
            sh.Open(res.node);
        break;
    }
}

std::vector<FsNode*> StartMenu::RecommendedItems() const { return GetShell().FS().Recent(6); }

std::vector<std::wstring> StartMenu::ActionsFor(const Result& res) const
{
    switch (res.type)
    {
    case Result::Type::App:
        return {L"Открыть", IsPinned(res.app) ? L"Открепить от начального экрана" : L"Закрепить на начальном экране",
                GetShell().TB().IsPinned(res.app) ? L"Открепить от панели задач" : L"Закрепить на панели задач"};
    case Result::Type::Setting:
        return {L"Открыть"};
    case Result::Type::File:
        return {L"Открыть", L"Открыть расположение файла", L"Копировать путь"};
    }
    return {};
}

// ---------------------------------------------------------------------------
// Геометрия
// ---------------------------------------------------------------------------
Rect StartMenu::PinnedCell(int index) const
{
    const Rect b = Bounds();
    const int local = index % kPerPage;
    const float slide = (static_cast<float>(index / kPerPage) - m_pageAnim.Value()) * 260.0f;
    return {b.x + 32.0f + static_cast<float>(local % 6) * 96.0f, b.y + 118.0f + static_cast<float>(local / 6) * 84.0f + slide,
            96.0f, 84.0f};
}

Rect StartMenu::RecommendedCell(int index) const
{
    const Rect b = Bounds();
    return {b.x + 32.0f + static_cast<float>(index % 2) * 292.0f, b.y + 426.0f + static_cast<float>(index / 2) * 60.0f,
            284.0f, 56.0f};
}

Rect StartMenu::ButtonRect(HitKind kind) const
{
    const Rect b = Bounds();
    switch (kind)
    {
    case HitKind::AllAppsButton:
        return {b.Right() - 52.0f - 132.0f, b.y + 82.0f, 132.0f, 26.0f};
    case HitKind::MoreButton:
        return {b.Right() - 52.0f - 120.0f, b.y + 388.0f, 120.0f, 26.0f};
    case HitKind::BackButton:
        return {b.Right() - 52.0f - 92.0f, b.y + 82.0f, 92.0f, 26.0f};
    case HitKind::User:
        return {b.x + 36.0f, b.Bottom() - 52.0f, 220.0f, 40.0f};
    case HitKind::Power:
        return {b.Right() - 36.0f - 40.0f, b.Bottom() - 52.0f, 40.0f, 40.0f};
    default:
        return {};
    }
}

Rect StartMenu::ListRect() const
{
    const Rect b = Bounds();
    return {b.x + 24.0f, b.y + 116.0f, b.w - 48.0f, b.h - 116.0f - 72.0f};
}

Rect StartMenu::ResultRect(int index) const
{
    const Rect b = Bounds();
    if (index == 0)
        return {b.x + 24.0f, b.y + 112.0f, 300.0f, 64.0f};
    return {b.x + 24.0f, b.y + 112.0f + 64.0f + 34.0f + static_cast<float>(index - 1) * 40.0f, 300.0f, 40.0f};
}

Rect StartMenu::ActionRect(int index) const
{
    const Rect b = Bounds();
    const Rect pane{b.x + 340.0f, b.y + 84.0f, b.w - 340.0f - 24.0f, b.h - 84.0f - 72.0f - 12.0f};
    return {pane.x + 8.0f, pane.y + 210.0f + static_cast<float>(index) * 40.0f, pane.w - 16.0f, 36.0f};
}

StartMenu::Hit StartMenu::HitAt(Point p) const
{
    Hit h;
    const Rect b = Bounds();
    if (!b.Contains(p))
        return h;
    if (ButtonRect(HitKind::User).Contains(p))
        return {HitKind::User, 0};
    if (ButtonRect(HitKind::Power).Contains(p))
        return {HitKind::Power, 0};
    switch (m_view)
    {
    case View::Home:
    {
        if (ButtonRect(HitKind::AllAppsButton).Contains(p))
            return {HitKind::AllAppsButton, 0};
        if (ButtonRect(HitKind::MoreButton).Contains(p))
            return {HitKind::MoreButton, 0};
        for (int i = m_page * kPerPage; i < std::min(static_cast<int>(m_pinned.size()), (m_page + 1) * kPerPage); ++i)
        {
            if (PinnedCell(i).Contains(p))
                return {HitKind::Pinned, i};
        }
        const int pages = PageCount();
        if (pages > 1)
        {
            for (int i = 0; i < pages; ++i)
            {
                const Rect dot{b.Right() - 26.0f, b.y + 230.0f + static_cast<float>(i) * 16.0f - 8.0f, 16.0f, 16.0f};
                if (dot.Contains(p))
                    return {HitKind::PageDot, i};
            }
        }
        const std::vector<FsNode*> rec = RecommendedItems();
        for (int i = 0; i < static_cast<int>(rec.size()); ++i)
        {
            if (RecommendedCell(i).Contains(p))
                return {HitKind::Recommended, i};
        }
        break;
    }
    case View::AllApps:
    {
        if (ButtonRect(HitKind::BackButton).Contains(p))
            return {HitKind::BackButton, 0};
        const Rect lr = ListRect();
        if (!lr.Contains(p))
            break;
        float y = lr.y - m_listScroll.Value();
        for (int i = 0; i < static_cast<int>(m_rows.size()); ++i)
        {
            const float rh = m_rows[static_cast<size_t>(i)].letter ? 36.0f : 40.0f;
            if (p.y >= y && p.y < y + rh)
                return {m_rows[static_cast<size_t>(i)].letter ? HitKind::ListLetter : HitKind::ListApp, i};
            y += rh;
        }
        break;
    }
    case View::Search:
    {
        for (int i = 0; i < static_cast<int>(std::min<size_t>(m_results.size(), 9)); ++i)
        {
            if (ResultRect(i).Contains(p))
                return {HitKind::Result, i};
        }
        if (m_selResult >= 0 && m_selResult < static_cast<int>(m_results.size()))
        {
            const auto actions = ActionsFor(m_results[static_cast<size_t>(m_selResult)]);
            for (int i = 0; i < static_cast<int>(actions.size()); ++i)
            {
                if (ActionRect(i).Contains(p))
                    return {HitKind::ResultAction, i};
            }
        }
        break;
    }
    }
    return h;
}

// ---------------------------------------------------------------------------
// Отрисовка
// ---------------------------------------------------------------------------
void StartMenu::Draw(Renderer& r)
{
    if (!BeginAnim(r, 0.0f, 70.0f))
        return;
    Shell& sh = GetShell();
    const Palette& p = Theme::P();
    const Rect b = Bounds();
    m_search->SetBounds({b.x + 32.0f, b.y + 24.0f, b.w - 64.0f, 36.0f});
    DrawPanel(r, b);

    auto smallButton = [&](HitKind kind, const std::wstring& text, bool chevronLeft) {
        const Rect br = ButtonRect(kind);
        const bool hot = m_hover.kind == kind;
        r.FillRoundRect(br, 4.0f, hot ? p.controlHover : p.control);
        r.StrokeRoundRect(br, 4.0f, p.controlStroke);
        if (chevronLeft)
        {
            r.Glyph(Icon::ChevronLeft, {br.x + 10.0f, br.CenterY() - 5.0f, 10.0f, 10.0f}, p.text);
            r.Text(text, {br.x + 26.0f, br.y, br.w - 32.0f, br.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        else
        {
            r.Text(text, {br.x + 10.0f, br.y, br.w - 28.0f, br.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Glyph(Icon::ChevronRight, {br.Right() - 20.0f, br.CenterY() - 5.0f, 10.0f, 10.0f}, p.text);
        }
    };

    const float vt = m_viewAnim.Value();
    r.PushOpacity(vt);
    r.PushTransform(0.0f, (1.0f - vt) * 18.0f);
    if (m_view == View::Home)
    {
        r.Text(L"Закреплено", {b.x + 52.0f, b.y + 82.0f, 200.0f, 26.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        smallButton(HitKind::AllAppsButton, L"Все приложения", false);
        r.PushClip({b.x + 8.0f, b.y + 114.0f, b.w - 16.0f, 84.0f * 3.0f + 8.0f});
        for (int i = 0; i < static_cast<int>(m_pinned.size()); ++i)
        {
            const Rect cell = PinnedCell(i);
            if (cell.y > b.y + 400.0f || cell.Bottom() < b.y + 100.0f)
                continue;
            const AppId app = m_pinned[static_cast<size_t>(i)];
            const bool hot = m_hover.kind == HitKind::Pinned && m_hover.index == i;
            const bool down = hot && m_press.kind == HitKind::Pinned && m_press.index == i;
            if (hot)
                r.FillRoundRect(cell.Inflated(-2.0f), 4.0f, down ? p.subtlePressed : p.subtleHover);
            const float isz = down ? 28.0f : 32.0f;
            r.DrawAppIcon(GetAppInfo(app).icon, {cell.CenterX() - isz * 0.5f, cell.y + 14.0f + (32.0f - isz) * 0.5f, isz, isz});
            r.Text(GetAppInfo(app).name, {cell.x + 4.0f, cell.y + 52.0f, cell.w - 8.0f, 20.0f}, p.text, 12.0f,
                   FontWeight::Regular, TextFlags::Center);
        }
        r.PopClip();
        const int pages = PageCount();
        if (pages > 1)
        {
            for (int i = 0; i < pages; ++i)
            {
                const float cy = b.y + 230.0f + static_cast<float>(i) * 16.0f;
                const bool cur = i == m_page;
                r.FillCircle(b.Right() - 18.0f, cy, cur ? 3.0f : 2.0f, cur ? p.text : p.textTertiary);
            }
        }
        if (sh.Settings().startRecommended)
        {
            r.Text(L"Рекомендуем", {b.x + 52.0f, b.y + 388.0f, 200.0f, 26.0f}, p.text, 14.0f, FontWeight::Semibold,
                   TextFlags::VCenter);
            smallButton(HitKind::MoreButton, L"Дополнительно", false);
            const std::vector<FsNode*> rec = RecommendedItems();
            for (int i = 0; i < static_cast<int>(rec.size()); ++i)
            {
                const Rect cell = RecommendedCell(i);
                if (cell.Bottom() > b.Bottom() - 70.0f)
                    break;
                const bool hot = m_hover.kind == HitKind::Recommended && m_hover.index == i;
                if (hot)
                    r.FillRoundRect(cell, 4.0f, p.subtleHover);
                FsNode* n = rec[static_cast<size_t>(i)];
                r.DrawAppIcon(sh.FS().IconFor(n), {cell.x + 12.0f, cell.y + 12.0f, 32.0f, 32.0f});
                r.Text(VirtualFS::DisplayName(n, sh.Settings().showExtensions), {cell.x + 56.0f, cell.y + 9.0f, cell.w - 64.0f, 20.0f},
                       p.text, 12.0f);
                r.Text(RelativeTime(n->modified), {cell.x + 56.0f, cell.y + 28.0f, cell.w - 64.0f, 18.0f}, p.textSecondary, 12.0f);
            }
        }
    }
    else if (m_view == View::AllApps)
    {
        r.Text(L"Все приложения", {b.x + 52.0f, b.y + 82.0f, 240.0f, 26.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        smallButton(HitKind::BackButton, L"Назад", true);
        const Rect lr = ListRect();
        r.PushClip(lr);
        float y = lr.y - m_listScroll.Value();
        for (int i = 0; i < static_cast<int>(m_rows.size()); ++i)
        {
            const ListRow& row = m_rows[static_cast<size_t>(i)];
            const float rh = row.letter ? 36.0f : 40.0f;
            if (y + rh >= lr.y && y <= lr.Bottom())
            {
                const bool hot = m_hover.index == i && (m_hover.kind == HitKind::ListApp || m_hover.kind == HitKind::ListLetter);
                const Rect rr{lr.x + 8.0f, y, lr.w - 16.0f, rh};
                if (row.letter)
                {
                    if (hot)
                        r.FillRoundRect({rr.x + 12.0f, rr.y + 4.0f, 28.0f, 28.0f}, 4.0f, p.subtleHover);
                    r.Text(row.text, {rr.x + 12.0f, rr.y + 4.0f, 28.0f, 28.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::Middle);
                }
                else
                {
                    if (hot)
                        r.FillRoundRect(rr, 4.0f, p.subtleHover);
                    r.DrawAppIcon(GetAppInfo(row.app).icon, {rr.x + 20.0f, rr.y + 8.0f, 24.0f, 24.0f});
                    r.Text(row.text, {rr.x + 60.0f, rr.y, rr.w - 70.0f, rh}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
                }
            }
            y += rh;
        }
        r.PopClip();
    }
    else
    {
        if (m_results.empty())
        {
            r.Text(L"Ничего не найдено по запросу «" + Trim(m_search->Text()) + L"»", {b.x + 32.0f, b.y + 120.0f, b.w - 64.0f, 24.0f},
                   p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Center);
        }
        else
        {
            r.Text(L"Лучшее соответствие", {b.x + 36.0f, b.y + 82.0f, 280.0f, 24.0f}, p.text, 14.0f, FontWeight::Semibold,
                   TextFlags::VCenter);
            if (m_results.size() > 1)
                r.Text(L"Другие результаты", {b.x + 36.0f, b.y + 182.0f, 280.0f, 24.0f}, p.text, 14.0f, FontWeight::Semibold,
                       TextFlags::VCenter);
            for (int i = 0; i < static_cast<int>(std::min<size_t>(m_results.size(), 9)); ++i)
            {
                const Result& res = m_results[static_cast<size_t>(i)];
                const Rect rr = ResultRect(i);
                if (rr.Bottom() > b.Bottom() - 72.0f)
                    break;
                const bool sel = i == m_selResult;
                const bool hot = m_hover.kind == HitKind::Result && m_hover.index == i;
                if (sel || hot)
                    r.FillRoundRect(rr, 4.0f, sel ? p.subtleHover : p.subtlePressed);
                if (sel)
                    r.FillRoundRect({rr.x, rr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
                const float isz = i == 0 ? 32.0f : 20.0f;
                const Rect ir{rr.x + 14.0f, rr.CenterY() - isz * 0.5f, isz, isz};
                if (res.glyph != Icon::None)
                    r.Glyph(res.glyph, ir, p.accentText);
                else
                    r.DrawAppIcon(res.icon, ir);
                if (i == 0)
                {
                    r.Text(res.name, {rr.x + 58.0f, rr.y + 12.0f, rr.w - 66.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
                    r.Text(res.detail, {rr.x + 58.0f, rr.y + 34.0f, rr.w - 66.0f, 18.0f}, p.textSecondary, 12.0f);
                }
                else
                    r.Text(res.name, {rr.x + 46.0f, rr.y, rr.w - 54.0f, rr.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            }
            // Панель подробностей.
            if (m_selResult >= 0 && m_selResult < static_cast<int>(m_results.size()))
            {
                const Result& res = m_results[static_cast<size_t>(m_selResult)];
                const Rect pane{b.x + 340.0f, b.y + 84.0f, b.w - 340.0f - 24.0f, b.h - 84.0f - 72.0f - 12.0f};
                r.FillRoundRect(pane, 8.0f, p.card);
                r.StrokeRoundRect(pane, 8.0f, p.cardStroke);
                const Rect big{pane.CenterX() - 32.0f, pane.y + 32.0f, 64.0f, 64.0f};
                if (res.glyph != Icon::None)
                    r.Glyph(res.glyph, big, p.accentText, 0.8f);
                else
                    r.DrawAppIcon(res.icon, big);
                r.Text(res.name, {pane.x + 12.0f, pane.y + 108.0f, pane.w - 24.0f, 28.0f}, p.text, 20.0f, FontWeight::Semibold,
                       TextFlags::Center);
                r.Text(res.type == Result::Type::App ? std::wstring(L"Приложение")
                                                     : (res.type == Result::Type::Setting ? std::wstring(L"Системные параметры") : res.detail),
                       {pane.x + 12.0f, pane.y + 140.0f, pane.w - 24.0f, 40.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                       TextFlags::Center | TextFlags::Wrap);
                r.FillRect({pane.x + 16.0f, pane.y + 196.0f, pane.w - 32.0f, 1.0f}, p.divider);
                const auto actions = ActionsFor(res);
                static const Icon actIcons[] = {Icon::Open, Icon::Pin, Icon::Pin};
                for (int i = 0; i < static_cast<int>(actions.size()); ++i)
                {
                    const Rect ar = ActionRect(i);
                    const bool hot = m_hover.kind == HitKind::ResultAction && m_hover.index == i;
                    if (hot)
                        r.FillRoundRect(ar, 4.0f, p.subtleHover);
                    Icon ic = actIcons[std::min(i, 2)];
                    if (res.type == Result::Type::File && i > 0)
                        ic = i == 1 ? Icon::Folder : Icon::CopyPath;
                    r.Glyph(ic, {ar.x + 12.0f, ar.CenterY() - 8.0f, 16.0f, 16.0f}, p.accentText);
                    r.Text(actions[static_cast<size_t>(i)], {ar.x + 40.0f, ar.y, ar.w - 48.0f, ar.h}, p.text, 14.0f,
                           FontWeight::Regular, TextFlags::VCenter);
                }
            }
        }
    }
    r.PopTransform();
    r.PopOpacity();

    // Нижняя полоса: пользователь и питание.
    const Rect bar{b.x, b.Bottom() - 64.0f, b.w, 64.0f};
    r.FillRoundRect(bar, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color(0, 0, 0, 0x2A) : Color(0, 0, 0, 0x07));
    r.FillRect({bar.x, bar.y, bar.w, 1.0f}, p.divider);
    const Rect ur = ButtonRect(HitKind::User);
    if (m_hover.kind == HitKind::User)
        r.FillRoundRect(ur, 4.0f, p.subtleHover);
    r.DrawAppIcon(AppIcon::User, {ur.x + 12.0f, ur.CenterY() - 16.0f, 32.0f, 32.0f});
    r.Text(sh.FS().UserName(), {ur.x + 56.0f, ur.y, ur.w - 64.0f, ur.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
    const Rect pr = ButtonRect(HitKind::Power);
    if (m_hover.kind == HitKind::Power)
        r.FillRoundRect(pr, 4.0f, p.subtleHover);
    r.Glyph(Icon::Power, {pr.CenterX() - 8.0f, pr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);

    m_ui.Draw(r);
    EndAnim(r);
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
void StartMenu::MouseMove(const MouseEvent& e)
{
    Flyout::MouseMove(e);
    m_hover = HitAt(e.pos);
    if (m_hover.kind == HitKind::Power)
        GetShell().Tooltip(this, L"Питание", ButtonRect(HitKind::Power), true);
    else if (m_hover.kind == HitKind::Pinned)
        GetShell().Tooltip(&m_pinned[static_cast<size_t>(m_hover.index)], GetAppInfo(m_pinned[static_cast<size_t>(m_hover.index)]).name,
                           PinnedCell(m_hover.index), false);
}

void StartMenu::MouseDown(const MouseEvent& e)
{
    if (m_search->Bounds().Contains(e.pos))
    {
        Flyout::MouseDown(e);
        return;
    }
    m_press = HitAt(e.pos);
}

void StartMenu::MouseUp(const MouseEvent& e)
{
    Flyout::MouseUp(e);
    const Hit h = HitAt(e.pos);
    const Hit pressed = m_press;
    m_press = Hit{};
    if (h.kind == HitKind::None || h.kind != pressed.kind || h.index != pressed.index)
        return;
    Shell& sh = GetShell();
    if (e.button == MouseButton::Right)
    {
        ShowItemMenu(h, e.pos);
        return;
    }
    switch (h.kind)
    {
    case HitKind::Pinned:
        Launch(m_pinned[static_cast<size_t>(h.index)]);
        break;
    case HitKind::Recommended:
    {
        const std::vector<FsNode*> rec = RecommendedItems();
        if (h.index < static_cast<int>(rec.size()))
        {
            FsNode* n = rec[static_cast<size_t>(h.index)];
            sh.CloseFlyouts();
            sh.Open(n);
        }
        break;
    }
    case HitKind::AllAppsButton:
        SetView(View::AllApps);
        break;
    case HitKind::MoreButton:
        sh.Launch(AppId::Explorer, L"::home");
        break;
    case HitKind::BackButton:
        SetView(View::Home);
        break;
    case HitKind::ListApp:
        Launch(m_rows[static_cast<size_t>(h.index)].app);
        break;
    case HitKind::ListLetter:
        break;
    case HitKind::Result:
        if (m_selResult == h.index || e.clicks >= 2)
            OpenResult(h.index);
        else
            m_selResult = h.index;
        break;
    case HitKind::ResultAction:
    {
        if (m_selResult < 0 || m_selResult >= static_cast<int>(m_results.size()))
            break;
        const Result res = m_results[static_cast<size_t>(m_selResult)];
        if (h.index == 0)
            OpenResult(m_selResult);
        else if (res.type == Result::Type::App)
        {
            if (h.index == 1)
                IsPinned(res.app) ? Unpin(res.app) : Pin(res.app);
            else
                sh.TB().IsPinned(res.app) ? sh.TB().Unpin(res.app) : sh.TB().Pin(res.app);
        }
        else if (res.type == Result::Type::File && res.node)
        {
            if (h.index == 1)
                sh.Launch(AppId::Explorer, sh.FS().PathOf(res.node->parent));
            else
                SetClipboardText(sh.Hwnd(), sh.FS().PathOf(res.node));
        }
        break;
    }
    case HitKind::User:
    {
        std::vector<MenuItem> items;
        items.push_back(MenuItem(L"Изменить параметры учётной записи", Icon::User, []() { GetShell().Launch(AppId::Settings, L"accounts"); }));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Заблокировать", Icon::Lock, []() { GetShell().Lock(); }));
        items.push_back(MenuItem(L"Выход", Icon::SignOut, []() { GetShell().Lock(); }));
        sh.ShowMenuBelow(ButtonRect(HitKind::User), std::move(items), 240.0f, true);
        break;
    }
    case HitKind::Power:
    {
        std::vector<MenuItem> items;
        items.push_back(MenuItem(L"Параметры входа", Icon::Settings, []() { GetShell().Launch(AppId::Settings, L"accounts"); }));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Спящий режим", Icon::Sleep, []() { GetShell().Sleep(); }));
        items.push_back(MenuItem(L"Завершение работы", Icon::Power, []() { GetShell().Shutdown(false); }));
        items.push_back(MenuItem(L"Перезагрузка", Icon::Restart, []() { GetShell().Shutdown(true); }));
        const Rect pr = ButtonRect(HitKind::Power);
        sh.ShowMenuBelow({pr.x - 140.0f, pr.y, pr.w, pr.h}, std::move(items), 200.0f, true);
        break;
    }
    case HitKind::PageDot:
        m_page = h.index;
        m_pageAnim.Set(static_cast<float>(m_page), 0.3f);
        break;
    case HitKind::None:
        break;
    }
}

void StartMenu::ShowItemMenu(const Hit& h, Point p)
{
    Shell& sh = GetShell();
    AppId app = AppId::None;
    if (h.kind == HitKind::Pinned)
        app = m_pinned[static_cast<size_t>(h.index)];
    else if (h.kind == HitKind::ListApp)
        app = m_rows[static_cast<size_t>(h.index)].app;
    else if (h.kind == HitKind::Result && m_results[static_cast<size_t>(h.index)].type == Result::Type::App)
        app = m_results[static_cast<size_t>(h.index)].app;
    if (app == AppId::None)
        return;
    std::vector<MenuItem> items;
    if (IsPinned(app))
    {
        items.push_back(MenuItem(L"Открепить от начального экрана", Icon::Unpin, [this, app]() { Unpin(app); }));
        if (h.kind == HitKind::Pinned && h.index > 0)
            items.push_back(MenuItem(L"Переместить в начало", Icon::ArrowUp, [this, app]() {
                Unpin(app);
                m_pinned.insert(m_pinned.begin(), app);
            }));
    }
    else
        items.push_back(MenuItem(L"Закрепить на начальном экране", Icon::Pin, [this, app]() { Pin(app); }));
    if (sh.TB().IsPinned(app))
        items.push_back(MenuItem(L"Открепить от панели задач", Icon::Unpin, [app]() { GetShell().TB().Unpin(app); }));
    else
        items.push_back(MenuItem(L"Закрепить на панели задач", Icon::Pin, [app]() { GetShell().TB().Pin(app); }));
    sh.ShowMenu(p, std::move(items));
}

bool StartMenu::MouseWheel(const MouseEvent& e)
{
    if (m_view == View::AllApps)
    {
        float content = 0.0f;
        for (const ListRow& row : m_rows)
            content += row.letter ? 36.0f : 40.0f;
        const float maxS = std::max(0.0f, content - ListRect().h);
        m_listScroll.Set(Clamp(m_listScroll.Target() - e.wheel * 80.0f, 0.0f, maxS), 0.25f);
        return true;
    }
    if (m_view == View::Home && PageCount() > 1)
    {
        const int np = Clamp(m_page + (e.wheel < 0.0f ? 1 : -1), 0, PageCount() - 1);
        if (np != m_page)
        {
            m_page = np;
            m_pageAnim.Set(static_cast<float>(m_page), 0.3f);
        }
    }
    return true;
}

void StartMenu::MouseLeave()
{
    Flyout::MouseLeave();
    m_hover = Hit{};
}

bool StartMenu::KeyDown(UINT vk, const KeyMods& mods)
{
    if (vk == VK_ESCAPE)
    {
        if (m_view == View::Search)
        {
            m_search->SetText(L"");
            SetView(View::Home);
            return true;
        }
        if (m_view == View::AllApps)
        {
            SetView(View::Home);
            return true;
        }
        Close();
        return true;
    }
    if (m_ui.FocusedChild() != m_search)
        m_ui.SetFocus(m_search);
    if (vk == VK_RETURN && m_view != View::Search)
        return true;
    return m_ui.OnKeyDown(vk, mods);
}

bool StartMenu::Char(wchar_t ch)
{
    if (ch < 32)
        return true;
    if (m_ui.FocusedChild() != m_search)
        m_ui.SetFocus(m_search);
    return m_ui.OnChar(ch);
}

CursorType StartMenu::Cursor(Point p) const
{
    if (m_search->Bounds().Contains(p))
        return CursorType::IBeam;
    return CursorType::Arrow;
}
