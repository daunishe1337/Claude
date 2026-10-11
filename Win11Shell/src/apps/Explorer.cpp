// Проводник Windows 11: вкладки, адресная строка, командная панель, область навигации,
// таблица/значки, «Главная», «Этот компьютер», «Галерея», корзина, поиск и переименование.

#include "../Shell.h"
#include "AppFactories.h"
#include "Photos.h"

namespace
{
enum class Special
{
    None,
    Home,
    Gallery,
    Network,
};
enum class ViewMode
{
    Details,
    Icons,
};
enum class SortKey
{
    Name,
    Date,
    Type,
    Size,
};

struct Location
{
    FsNode* node = nullptr;
    Special special = Special::Home;
    bool operator==(const Location& o) const { return node == o.node && special == o.special; }
};

struct Tab
{
    Location loc;
    std::vector<Location> back;
    std::vector<Location> forward;
    std::vector<FsNode*> sel;
    float scroll = 0.0f;
    std::wstring query;
};

struct NavEntry
{
    std::wstring name;
    AppIcon icon = AppIcon::Folder;
    Location loc;
    int indent = 0;
    bool expandable = false;
    bool expanded = false;
    bool separatorBefore = false;
};

constexpr float kTabBarH = 40.0f;
constexpr float kAddrY = 40.0f;
constexpr float kCmdY = 88.0f;
constexpr float kMainY = 137.0f;
constexpr float kStatusH = 26.0f;
constexpr float kRowH = 30.0f;
constexpr float kHeaderH = 34.0f;

class ExplorerWindow : public Window
{
public:
    explicit ExplorerWindow(const std::wstring& arg) : Window(AppId::Explorer, L"Проводник")
    {
        SetInitialSize(1180.0f, 720.0f);
        m_minSize = {560.0f, 400.0f};
        Tab t;
        t.loc = LocationFromArg(arg);
        m_tabs.push_back(t);

        m_search = m_ui.Add<TextBox>();
        m_search->trailingIcon = Icon::Search;
        m_search->fontSize = 12.0f;
        m_search->onChange = [this](const std::wstring& q) {
            Cur().query = Trim(q);
            Cur().scroll = 0.0f;
            Cur().sel.clear();
            m_dirty = true;
        };
        m_search->onEscape = [this]() {
            m_search->SetText(L"", true);
            m_ui.SetFocus(nullptr);
        };
        m_address = m_ui.Add<TextBox>();
        m_address->fontSize = 12.0f;
        m_address->SetVisible(false);
        m_address->onEnter = [this]() { CommitAddress(); };
        m_address->onEscape = [this]() { CancelAddress(); };
        m_rename = m_ui.Add<TextBox>();
        m_rename->fontSize = 12.0f;
        m_rename->SetVisible(false);
        m_rename->onEnter = [this]() { CommitRename(); };
        m_rename->onEscape = [this]() { CancelRename(); };
        BuildToolbar();
        UpdateTitle();
    }

    float TitleBarHeight() const override { return kTabBarH; }
    bool ShowTitle() const override { return false; }
    AppIcon WindowIcon() const override { return AppIcon::Explorer; }

    void OnLaunchArgs(const std::wstring& arg) override { Navigate(LocationFromArg(arg)); }

private:
    // ------------------------------------------------------------------ данные
    Tab& Cur() { return m_tabs[static_cast<size_t>(m_cur)]; }
    const Tab& Cur() const { return m_tabs[static_cast<size_t>(m_cur)]; }
    VirtualFS& FS() const { return GetShell().FS(); }
    bool InBin() const { return Cur().loc.node == FS().Bin(); }
    bool Searching() const { return !Cur().query.empty(); }

    Location LocationFromArg(const std::wstring& arg) const
    {
        Location l;
        if (arg == L"::home" || arg.empty())
            return l;
        if (arg == L"::gallery")
        {
            l.special = Special::Gallery;
            return l;
        }
        FsNode* n = FS().FromArg(arg);
        if (n && n->IsFolder())
        {
            l.node = n;
            l.special = Special::None;
        }
        else if (n && n->parent)
        {
            l.node = n->parent;
            l.special = Special::None;
        }
        return l;
    }

    std::wstring LocName(const Location& l) const
    {
        if (l.node)
            return l.node->name;
        switch (l.special)
        {
        case Special::Home:
            return L"Главная";
        case Special::Gallery:
            return L"Галерея";
        case Special::Network:
            return L"Сеть";
        case Special::None:
            break;
        }
        return L"";
    }

    AppIcon LocIcon(const Location& l) const
    {
        if (l.node)
            return FS().IconFor(l.node);
        switch (l.special)
        {
        case Special::Home:
            return AppIcon::Home;
        case Special::Gallery:
            return AppIcon::Gallery;
        case Special::Network:
            return AppIcon::Network;
        case Special::None:
            break;
        }
        return AppIcon::Folder;
    }

    void UpdateTitle() { SetTitle(LocName(Cur().loc) + L" — Проводник"); }

    void Navigate(const Location& l, bool record = true)
    {
        CancelRename();
        CancelAddress();
        Tab& t = Cur();
        if (record && !(t.loc == l))
        {
            t.back.push_back(t.loc);
            t.forward.clear();
        }
        t.loc = l;
        t.sel.clear();
        t.scroll = 0.0f;
        t.query.clear();
        m_search->SetText(L"");
        m_focusIndex = -1;
        m_dirty = true;
        BuildToolbar();
        UpdateTitle();
    }

    void GoBack()
    {
        Tab& t = Cur();
        if (t.back.empty())
            return;
        t.forward.push_back(t.loc);
        const Location l = t.back.back();
        t.back.pop_back();
        Navigate(l, false);
    }

    void GoForward()
    {
        Tab& t = Cur();
        if (t.forward.empty())
            return;
        t.back.push_back(t.loc);
        const Location l = t.forward.back();
        t.forward.pop_back();
        Navigate(l, false);
    }

    void GoUp()
    {
        const Location& l = Cur().loc;
        if (!l.node)
            return;
        if (l.node == FS().Root() || l.node == FS().Bin())
        {
            Navigate({});
            return;
        }
        Location up;
        up.node = l.node->parent;
        up.special = Special::None;
        if (up.node)
            Navigate(up);
    }

    ViewMode CurrentView() const
    {
        const Location& l = Cur().loc;
        auto it = m_views.find(l.node);
        if (it != m_views.end())
            return it->second;
        if (l.node == FS().Pictures() || (l.node && l.node->parent == FS().Pictures()))
            return ViewMode::Icons;
        return ViewMode::Details;
    }

    void SetView(ViewMode v)
    {
        m_views[Cur().loc.node] = v;
        Cur().scroll = 0.0f;
    }

    void Recompute()
    {
        VirtualFS& fs = FS();
        // Вкладки, чьи папки удалены, возвращаются на «Главную».
        for (Tab& t : m_tabs)
        {
            if (t.loc.node && !fs.IsAlive(t.loc.node))
            {
                t.loc = {};
                t.sel.clear();
            }
            t.sel.erase(std::remove_if(t.sel.begin(), t.sel.end(), [&fs](FsNode* n) { return !fs.IsAlive(n); }), t.sel.end());
        }
        m_items.clear();
        const Tab& t = Cur();
        if (Searching())
        {
            FsNode* base = t.loc.node ? t.loc.node : fs.Home();
            fs.Search(base, t.query, m_items);
        }
        else if (t.loc.node)
        {
            for (auto& c : t.loc.node->children)
                m_items.push_back(c.get());
        }
        else if (t.loc.special == Special::Gallery)
        {
            std::function<void(FsNode*)> collect = [&](FsNode* d) {
                for (auto& c : d->children)
                {
                    if (c->kind == FileKind::Image)
                        m_items.push_back(c.get());
                    else if (c->IsFolder())
                        collect(c.get());
                }
            };
            collect(fs.Pictures());
        }
        else if (t.loc.special == Special::Home)
        {
            m_items = fs.Recent(12);
        }
        const SortKey key = m_sortKey;
        const bool asc = m_sortAsc;
        auto stamp = [](const SYSTEMTIME& st) {
            FILETIME ft{};
            SystemTimeToFileTime(&st, &ft);
            return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        // Диски в «Этом компьютере» — в порядке букв, «Главная» — по давности.
        if ((t.loc.special != Special::Home || t.loc.node) && (t.loc.node != fs.Root() || Searching()))
        {
            std::stable_sort(m_items.begin(), m_items.end(), [&](FsNode* a, FsNode* b) {
                if (a->IsFolder() != b->IsFolder())
                    return a->IsFolder();
                int c = 0;
                switch (key)
                {
                case SortKey::Name:
                    c = CompareNoCase(a->name, b->name);
                    break;
                case SortKey::Date:
                {
                    const auto sa = stamp(a->modified), sb = stamp(b->modified);
                    c = sa < sb ? -1 : (sa > sb ? 1 : 0);
                    break;
                }
                case SortKey::Type:
                    c = CompareNoCase(fs.TypeName(a), fs.TypeName(b));
                    break;
                case SortKey::Size:
                {
                    const auto sa = fs.SizeOf(a), sb = fs.SizeOf(b);
                    c = sa < sb ? -1 : (sa > sb ? 1 : 0);
                    break;
                }
                }
                if (c == 0)
                    c = CompareNoCase(a->name, b->name);
                return asc ? c < 0 : c > 0;
            });
        }
        m_fsVersion = fs.Version();
        m_dirty = false;
    }

    bool IsSelected(FsNode* n) const
    {
        const auto& s = Cur().sel;
        return std::find(s.begin(), s.end(), n) != s.end();
    }

    // ------------------------------------------------------------------ геометрия
    float NavW() const { return m_showNav ? std::min(240.0f, std::max(170.0f, Width() * 0.2f)) : 0.0f; }
    float DetailsW() const { return m_showDetails ? 300.0f : 0.0f; }
    Rect ContentRect() const
    {
        const float nav = NavW();
        return {nav + (m_showNav ? 1.0f : 0.0f), kMainY, Width() - nav - DetailsW() - (m_showNav ? 1.0f : 0.0f), Height() - kMainY - kStatusH};
    }
    Rect NavRect() const { return {0.0f, kMainY, NavW(), Height() - kMainY - kStatusH}; }
    float TabWidth() const
    {
        const float avail = Width() - 8.0f - 46.0f * 3.0f - 48.0f;
        return Clamp(avail / static_cast<float>(m_tabs.size()), 90.0f, 240.0f);
    }
    Rect TabRect(int i) const { return {8.0f + static_cast<float>(i) * TabWidth(), 8.0f, TabWidth(), 32.0f}; }
    Rect NewTabRect() const { return {8.0f + static_cast<float>(m_tabs.size()) * TabWidth() + 6.0f, 10.0f, 30.0f, 28.0f}; }
    Rect AddressRect() const
    {
        const float sw = std::min(280.0f, Width() * 0.25f);
        return {170.0f, kAddrY + 8.0f, Width() - 170.0f - 16.0f - sw - 10.0f, 32.0f};
    }
    Rect SearchRect() const
    {
        const float sw = std::min(280.0f, Width() * 0.25f);
        return {Width() - 16.0f - sw, kAddrY + 8.0f, sw, 32.0f};
    }

    // Колонки таблицы.
    struct Columns
    {
        float name, date, type, size;
    };
    Columns Cols() const
    {
        const Rect c = ContentRect();
        Columns col{};
        col.size = 100.0f;
        col.type = std::min(170.0f, c.w * 0.18f);
        col.date = std::min(170.0f, c.w * 0.19f);
        col.name = std::max(140.0f, c.w - 24.0f - col.date - col.type - col.size);
        return col;
    }

    Rect RowRect(int index) const
    {
        const Rect c = ContentRect();
        return {c.x + 8.0f, c.y + kHeaderH + 4.0f + static_cast<float>(index) * kRowH - Cur().scroll, c.w - 16.0f, kRowH};
    }

    float IconCellW() const { return 112.0f; }
    float IconCellH() const { return 128.0f; }
    int IconCols() const
    {
        const Rect c = ContentRect();
        return std::max(1, static_cast<int>((c.w - 24.0f) / IconCellW()));
    }
    Rect IconCell(int index) const
    {
        const Rect c = ContentRect();
        const int cols = IconCols();
        return {c.x + 12.0f + static_cast<float>(index % cols) * IconCellW(),
                c.y + 12.0f + static_cast<float>(index / cols) * IconCellH() - Cur().scroll, IconCellW() - 4.0f, IconCellH() - 4.0f};
    }

    bool UsesList() const
    {
        const Location& l = Cur().loc;
        if (Searching())
            return true;
        return l.node != nullptr && l.node != FS().Root() ? true : l.special == Special::Gallery;
    }

    Rect ItemRect(int index) const { return CurrentView() == ViewMode::Icons || Cur().loc.special == Special::Gallery ? IconCell(index) : RowRect(index); }

    bool IconLayout() const
    {
        if (Searching())
            return false;
        return Cur().loc.special == Special::Gallery || (Cur().loc.node && CurrentView() == ViewMode::Icons);
    }

    float ContentHeight() const
    {
        const int n = static_cast<int>(m_items.size());
        if (IconLayout())
        {
            const int cols = IconCols();
            return 24.0f + static_cast<float>((n + cols - 1) / cols) * IconCellH();
        }
        return kHeaderH + 12.0f + static_cast<float>(n) * kRowH;
    }

    // Элементы «Главной»: плитки быстрого доступа и список последних.
    std::vector<FsNode*> QuickAccess() const
    {
        VirtualFS& fs = FS();
        std::vector<FsNode*> q = {fs.Desktop(), fs.Downloads(), fs.Documents(), fs.Pictures(), fs.Music(), fs.Videos()};
        for (FsNode* p : m_pinned)
        {
            if (fs.IsAlive(p))
                q.push_back(p);
        }
        return q;
    }
    Rect QuickTile(int i) const
    {
        const Rect c = ContentRect();
        const float tw = 244.0f, th = 68.0f;
        const int cols = std::max(1, static_cast<int>((c.w - 32.0f) / (tw + 8.0f)));
        return {c.x + 20.0f + static_cast<float>(i % cols) * (tw + 8.0f), c.y + 52.0f + static_cast<float>(i / cols) * (th + 8.0f) - Cur().scroll,
                tw, th};
    }
    float HomeRecentTop() const
    {
        const int n = static_cast<int>(QuickAccess().size());
        const Rect last = QuickTile(std::max(0, n - 1));
        return last.Bottom() + Cur().scroll + 40.0f;
    }
    Rect HomeRow(int i) const
    {
        const Rect c = ContentRect();
        return {c.x + 8.0f, HomeRecentTop() + 112.0f + kHeaderH + static_cast<float>(i) * kRowH - Cur().scroll, c.w - 16.0f, kRowH};
    }

    // «Этот компьютер»: плитки дисков.
    Rect DriveTile(int i) const
    {
        const Rect c = ContentRect();
        const float tw = 280.0f, th = 76.0f;
        const int cols = std::max(1, static_cast<int>((c.w - 32.0f) / (tw + 12.0f)));
        return {c.x + 20.0f + static_cast<float>(i % cols) * (tw + 12.0f), c.y + 52.0f + static_cast<float>(i / cols) * (th + 12.0f), tw, th};
    }

    std::vector<NavEntry> NavEntries() const
    {
        VirtualFS& fs = FS();
        std::vector<NavEntry> v;
        auto add = [&](const std::wstring& name, AppIcon icon, FsNode* node, Special sp, int indent = 0, bool sepBefore = false,
                       bool expandable = false, bool expanded = false) {
            NavEntry e;
            e.name = name;
            e.icon = icon;
            e.loc.node = node;
            e.loc.special = node ? Special::None : sp;
            e.indent = indent;
            e.separatorBefore = sepBefore;
            e.expandable = expandable;
            e.expanded = expanded;
            v.push_back(e);
        };
        add(L"Главная", AppIcon::Home, nullptr, Special::Home);
        add(L"Галерея", AppIcon::Gallery, nullptr, Special::Gallery);
        add(L"Рабочий стол", AppIcon::FolderDesktop, fs.Desktop(), Special::None, 0, true);
        add(L"Загрузки", AppIcon::FolderDownloads, fs.Downloads(), Special::None);
        add(L"Документы", AppIcon::FolderDocuments, fs.Documents(), Special::None);
        add(L"Изображения", AppIcon::FolderPictures, fs.Pictures(), Special::None);
        add(L"Музыка", AppIcon::FolderMusic, fs.Music(), Special::None);
        add(L"Видео", AppIcon::FolderVideos, fs.Videos(), Special::None);
        for (FsNode* p : m_pinned)
        {
            if (fs.IsAlive(p))
                add(p->name, AppIcon::Folder, p, Special::None);
        }
        add(L"Этот компьютер", AppIcon::ThisPC, fs.Root(), Special::None, 0, true, true, m_pcExpanded);
        if (m_pcExpanded)
        {
            for (auto& c : fs.Root()->children)
                add(c->name, fs.IconFor(c.get()), c.get(), Special::None, 1);
        }
        add(L"Сеть", AppIcon::Network, nullptr, Special::Network);
        add(L"Корзина", fs.IconFor(fs.Bin()), fs.Bin(), Special::None, 0, true);
        return v;
    }

    Rect NavRow(int i, const std::vector<NavEntry>& entries) const
    {
        float y = kMainY + 8.0f - m_navScroll;
        for (int k = 0; k <= i && k < static_cast<int>(entries.size()); ++k)
        {
            if (entries[static_cast<size_t>(k)].separatorBefore)
                y += 13.0f;
            if (k == i)
                break;
            y += 32.0f;
        }
        return {6.0f, y, NavW() - 12.0f, 30.0f};
    }

    // ------------------------------------------------------------------ панель инструментов
    Button* ToolButton(Icon icon, const wchar_t* tip, float x, std::function<void()> fn)
    {
        auto* b = m_ui.Add<Button>(L"", icon, ButtonStyle::Subtle);
        b->SetBounds({x, kCmdY + 6.0f, 40.0f, 36.0f});
        b->SetTooltip(tip);
        b->onClick = std::move(fn);
        return b;
    }

    void BuildToolbar()
    {
        // Удалить старые кнопки (кроме полей ввода).
        std::vector<Widget*> keep = {m_search, m_address, m_rename};
        std::vector<Widget*> remove;
        for (auto& c : m_ui.Children())
        {
            if (std::find(keep.begin(), keep.end(), c.get()) == keep.end())
                remove.push_back(c.get());
        }
        for (Widget* w : remove)
            m_ui.Remove(w);
        m_selButtons.clear();
        m_pasteButton = nullptr;

        auto nav = [&](Icon icon, const wchar_t* tip, float x, std::function<void()> fn) {
            auto* b = m_ui.Add<Button>(L"", icon, ButtonStyle::Subtle);
            b->SetBounds({x, kAddrY + 8.0f, 36.0f, 32.0f});
            b->SetTooltip(tip);
            b->onClick = std::move(fn);
            return b;
        };
        m_backBtn = nav(Icon::ArrowLeft, L"Назад (Alt + стрелка влево)", 8.0f, [this]() { GoBack(); });
        m_fwdBtn = nav(Icon::ArrowRight, L"Вперёд (Alt + стрелка вправо)", 48.0f, [this]() { GoForward(); });
        m_upBtn = nav(Icon::ArrowUp, L"Вверх (Alt + стрелка вверх)", 88.0f, [this]() { GoUp(); });
        nav(Icon::Refresh, L"Обновить (F5)", 128.0f, [this]() { m_dirty = true; });

        float x = 12.0f;
        if (InBin())
        {
            auto* empty = m_ui.Add<Button>(L"Очистить корзину", Icon::Delete, ButtonStyle::Subtle);
            empty->SetBounds({x, kCmdY + 6.0f, 160.0f, 36.0f});
            empty->onClick = [this]() { ConfirmEmptyBin(); };
            x += 164.0f;
            auto* restore = m_ui.Add<Button>(L"Восстановить", Icon::Undo, ButtonStyle::Subtle);
            restore->SetBounds({x, kCmdY + 6.0f, 136.0f, 36.0f});
            restore->onClick = [this]() { RestoreSelected(true); };
            x += 140.0f;
        }
        else
        {
            auto* create = m_ui.Add<Button>(L"Создать", Icon::Add, ButtonStyle::Subtle);
            create->chevron = true;
            create->SetBounds({x, kCmdY + 6.0f, 112.0f, 36.0f});
            create->onClick = [this, create]() { ShowCreateMenu(LocalToScreen(create->Bounds())); };
            create->SetEnabled(Cur().loc.node && Cur().loc.node != FS().Root());
            x += 116.0f;
        }
        m_sep1 = x + 2.0f;
        x += 8.0f;
        m_selButtons.push_back(ToolButton(Icon::Cut, L"Вырезать (Ctrl+X)", x, [this]() { ClipSelection(true); }));
        x += 42.0f;
        m_selButtons.push_back(ToolButton(Icon::Copy, L"Копировать (Ctrl+C)", x, [this]() { ClipSelection(false); }));
        x += 42.0f;
        m_pasteButton = ToolButton(Icon::Paste, L"Вставить (Ctrl+V)", x, [this]() { Paste(); });
        x += 42.0f;
        m_selButtons.push_back(ToolButton(Icon::Rename, L"Переименовать (F2)", x, [this]() { StartRename(); }));
        x += 42.0f;
        m_selButtons.push_back(ToolButton(Icon::Share, L"Поделиться", x, []() {
            GetShell().ShowMessage(L"Поделиться", L"Функция «Поделиться» недоступна в этой демонстрации.");
        }));
        x += 42.0f;
        m_selButtons.push_back(ToolButton(Icon::Delete, L"Удалить (Delete)", x, [this]() { DeleteSelection(false); }));
        x += 42.0f;
        m_sep2 = x + 2.0f;
        x += 8.0f;
        auto* sort = m_ui.Add<Button>(L"Сортировать", Icon::Sort, ButtonStyle::Subtle);
        sort->chevron = true;
        sort->SetBounds({x, kCmdY + 6.0f, 148.0f, 36.0f});
        sort->onClick = [this, sort]() { ShowSortMenu(LocalToScreen(sort->Bounds())); };
        x += 152.0f;
        auto* view = m_ui.Add<Button>(L"Просмотреть", Icon::View, ButtonStyle::Subtle);
        view->chevron = true;
        view->SetBounds({x, kCmdY + 6.0f, 150.0f, 36.0f});
        view->onClick = [this, view]() { ShowViewMenu(LocalToScreen(view->Bounds())); };
        x += 154.0f;
        m_sep3 = x + 2.0f;
        x += 8.0f;
        auto* more = m_ui.Add<Button>(L"", Icon::More, ButtonStyle::Subtle);
        more->SetBounds({x, kCmdY + 6.0f, 40.0f, 36.0f});
        more->SetTooltip(L"Подробнее");
        more->onClick = [this, more]() { ShowMoreMenu(LocalToScreen(more->Bounds())); };
        m_detailsBtn = m_ui.Add<Button>(L"Сведения", Icon::Info, ButtonStyle::Subtle);
        m_detailsBtn->onClick = [this]() { m_showDetails = !m_showDetails; };
        OnResize();
    }

    void OnResize() override
    {
        if (m_detailsBtn)
            m_detailsBtn->SetBounds({Width() - 12.0f - 112.0f, kCmdY + 6.0f, 112.0f, 36.0f});
        m_search->SetBounds(SearchRect());
        m_address->SetBounds(AddressRect());
    }

    void ShowCreateMenu(const Rect& anchor)
    {
        FsNode* dir = Cur().loc.node;
        if (!dir)
            return;
        std::vector<MenuItem> items = {
            MenuItem(L"Папку", Icon::Folder, [this, dir]() { CreateItem(dir, true, L"Новая папка", FileKind::Folder); }, L"Ctrl+Shift+N"),
            MenuItem::Sep(),
            MenuItem(L"Текстовый документ", Icon::Documents,
                     [this, dir]() { CreateItem(dir, false, L"Новый текстовый документ.txt", FileKind::Text); }),
            MenuItem(L"Точечный рисунок", Icon::Pictures,
                     [this, dir]() { CreateItem(dir, false, L"Новый точечный рисунок.bmp", FileKind::Image); }),
            MenuItem(L"Сжатая ZIP-папка", Icon::Folder,
                     [this, dir]() { CreateItem(dir, false, L"Новая сжатая ZIP-папка.zip", FileKind::Archive); }),
        };
        GetShell().ShowMenuBelow(anchor, std::move(items), 220.0f);
    }

    void ShowSortMenu(const Rect& anchor)
    {
        auto set = [this](SortKey k) {
            m_sortKey = k;
            m_dirty = true;
        };
        std::vector<MenuItem> items = {
            MenuItem::Check(L"Имя", m_sortKey == SortKey::Name, [set]() { set(SortKey::Name); }, true),
            MenuItem::Check(L"Дата изменения", m_sortKey == SortKey::Date, [set]() { set(SortKey::Date); }, true),
            MenuItem::Check(L"Тип", m_sortKey == SortKey::Type, [set]() { set(SortKey::Type); }, true),
            MenuItem::Check(L"Размер", m_sortKey == SortKey::Size, [set]() { set(SortKey::Size); }, true),
            MenuItem::Sep(),
            MenuItem::Check(L"По возрастанию", m_sortAsc, [this]() {
                m_sortAsc = true;
                m_dirty = true;
            }, true),
            MenuItem::Check(L"По убыванию", !m_sortAsc, [this]() {
                m_sortAsc = false;
                m_dirty = true;
            }, true),
        };
        GetShell().ShowMenuBelow(anchor, std::move(items), 200.0f);
    }

    void ShowViewMenu(const Rect& anchor)
    {
        const ViewMode v = CurrentView();
        ShellSettings& st = GetShell().Settings();
        std::vector<MenuItem> show = {
            MenuItem::Check(L"Область навигации", m_showNav, [this]() { m_showNav = !m_showNav; }),
            MenuItem::Check(L"Область сведений", m_showDetails, [this]() { m_showDetails = !m_showDetails; }),
            MenuItem::Check(L"Расширения имён файлов", st.showExtensions, [this]() {
                ShellSettings& s = GetShell().Settings();
                s.showExtensions = !s.showExtensions;
                m_dirty = true;
            }),
            MenuItem::Check(L"Скрытые элементы", false, nullptr).Disabled(),
        };
        std::vector<MenuItem> items = {
            MenuItem::Check(L"Крупные значки", v == ViewMode::Icons, [this]() { SetView(ViewMode::Icons); }, true, L"Ctrl+Shift+2"),
            MenuItem::Check(L"Таблица", v == ViewMode::Details, [this]() { SetView(ViewMode::Details); }, true, L"Ctrl+Shift+6"),
            MenuItem::Sep(),
            MenuItem::Sub(L"Показать", Icon::Eye, show),
        };
        GetShell().ShowMenuBelow(anchor, std::move(items), 240.0f);
    }

    void ShowMoreMenu(const Rect& anchor)
    {
        FsNode* dir = Cur().loc.node;
        std::vector<MenuItem> items;
        items.push_back(MenuItem(L"Отменить", Icon::Undo, nullptr, L"Ctrl+Z").Disabled());
        items.push_back(MenuItem::Sep());
        if (dir && dir->IsFolder() && dir != FS().Root())
            items.push_back(MenuItem(L"Закрепить на панели быстрого доступа", Icon::Pin, [this, dir]() { PinFolder(dir); }));
        items.push_back(MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"));
        items.push_back(MenuItem(L"Снять выделение", Icon::None, [this]() { Cur().sel.clear(); }));
        items.push_back(MenuItem(L"Обратить выделение", Icon::None, [this]() {
            std::vector<FsNode*> inv;
            for (FsNode* n : m_items)
            {
                if (!IsSelected(n))
                    inv.push_back(n);
            }
            Cur().sel = inv;
        }));
        items.push_back(MenuItem::Sep());
        if (dir)
            items.push_back(MenuItem(L"Свойства", Icon::Properties, [dir]() { GetShell().ShowProperties(dir); }, L"Alt+Enter"));
        items.push_back(MenuItem(L"Открыть в Терминале", Icon::Terminal, [this]() {
            FsNode* d = Cur().loc.node && Cur().loc.node != FS().Root() ? Cur().loc.node : FS().Home();
            GetShell().Launch(AppId::Terminal, FS().PathOf(d));
        }));
        GetShell().ShowMenuBelow(anchor, std::move(items), 260.0f);
    }

    // ------------------------------------------------------------------ действия
    void PinFolder(FsNode* dir)
    {
        if (std::find(m_pinned.begin(), m_pinned.end(), dir) == m_pinned.end())
            m_pinned.push_back(dir);
    }

    void CreateItem(FsNode* dir, bool folder, const std::wstring& name, FileKind kind)
    {
        FsNode* n = folder ? FS().NewFolder(dir, name) : FS().NewFile(dir, name, kind);
        Recompute();
        Cur().sel = {n};
        StartRename();
    }

    void SelectAll() { Cur().sel = m_items; }

    void ClipSelection(bool cut)
    {
        if (Cur().sel.empty())
            return;
        FS().clip = Cur().sel;
        FS().clipCut = cut;
    }

    void Paste()
    {
        FsNode* dir = Cur().loc.node;
        if (!dir || dir == FS().Root() || InBin())
            return;
        FS().Paste(dir);
        m_dirty = true;
    }

    void DeleteSelection(bool permanent)
    {
        std::vector<FsNode*> sel = Cur().sel;
        if (sel.empty())
            return;
        if (permanent || InBin())
        {
            const std::wstring msg = sel.size() == 1 ? L"Вы действительно хотите безвозвратно удалить «" + sel.front()->name + L"»?"
                                                     : L"Вы действительно хотите безвозвратно удалить эти элементы (" +
                                                           std::to_wstring(sel.size()) + L")?";
            ShowDialog(L"Удаление", msg, {{L"Да", true}, {L"Нет", false}}, [this, sel](int r) {
                if (r != 0)
                    return;
                for (FsNode* n : sel)
                    FS().DeleteForever(n);
                Cur().sel.clear();
                m_dirty = true;
            });
            return;
        }
        for (FsNode* n : sel)
            FS().Recycle(n);
        Cur().sel.clear();
        m_dirty = true;
    }

    void ConfirmEmptyBin()
    {
        const size_t n = FS().Bin()->children.size();
        if (n == 0)
            return;
        ShowDialog(L"Очистка корзины",
                   L"Вы действительно хотите безвозвратно удалить эти элементы (" + std::to_wstring(n) + L")?",
                   {{L"Да", true}, {L"Нет", false}}, [this](int r) {
                       if (r == 0)
                       {
                           FS().EmptyBin();
                           Cur().sel.clear();
                           m_dirty = true;
                       }
                   });
    }

    void RestoreSelected(bool allIfNone)
    {
        std::vector<FsNode*> sel = Cur().sel;
        if (sel.empty() && allIfNone)
            for (auto& c : FS().Bin()->children)
                sel.push_back(c.get());
        for (FsNode* n : sel)
            FS().Restore(n);
        Cur().sel.clear();
        m_dirty = true;
    }

    void OpenItem(FsNode* n)
    {
        if (!n)
            return;
        if (InBin())
        {
            GetShell().ShowProperties(n);
            return;
        }
        if (n->IsFolder())
        {
            Location l;
            l.node = n;
            l.special = Special::None;
            Navigate(l);
        }
        else
            GetShell().Open(n);
    }

    void NewTab(const Location& l)
    {
        Tab t;
        t.loc = l;
        m_tabs.push_back(t);
        SwitchTab(static_cast<int>(m_tabs.size()) - 1);
    }

    void SwitchTab(int i)
    {
        CancelRename();
        CancelAddress();
        m_cur = Clamp(i, 0, static_cast<int>(m_tabs.size()) - 1);
        m_search->SetText(Cur().query);
        m_dirty = true;
        BuildToolbar();
        UpdateTitle();
    }

    void CloseTab(int i)
    {
        if (m_tabs.size() <= 1)
        {
            Close();
            return;
        }
        m_tabs.erase(m_tabs.begin() + i);
        if (m_cur >= static_cast<int>(m_tabs.size()))
            m_cur = static_cast<int>(m_tabs.size()) - 1;
        else if (m_cur > i)
            --m_cur;
        SwitchTab(m_cur);
    }

    // Переименование
    void StartRename()
    {
        if (Cur().sel.size() != 1)
            return;
        FsNode* n = Cur().sel.front();
        if (n->system)
            return;
        const auto it = std::find(m_items.begin(), m_items.end(), n);
        if (it == m_items.end())
            return;
        const int idx = static_cast<int>(it - m_items.begin());
        m_renameNode = n;
        Rect r = ItemRect(idx);
        if (IconLayout())
            r = {r.x - 4.0f, r.y + 82.0f, r.w + 8.0f, 26.0f};
        else
            r = {r.x + 32.0f, r.y + 2.0f, Cols().name - 36.0f, r.h - 4.0f};
        m_rename->SetBounds(r);
        m_rename->SetVisible(true);
        const bool showExt = GetShell().Settings().showExtensions;
        const std::wstring shown = VirtualFS::DisplayName(n, showExt);
        m_rename->SetText(shown);
        // Как в Windows: при видимых расширениях выделяется только имя без расширения.
        std::wstring ext;
        const std::wstring base = VirtualFS::SplitExtension(shown, ext);
        if (n->IsFolder() || ext.empty())
            m_rename->SelectAll();
        else
            m_rename->Select(0, static_cast<int>(base.size()));
        m_ui.SetFocus(m_rename);
    }

    void CommitRename()
    {
        if (!m_renameNode)
            return;
        FsNode* n = m_renameNode;
        m_renameNode = nullptr;
        std::wstring name = Trim(m_rename->Text());
        m_rename->SetVisible(false);
        m_ui.SetFocus(nullptr);
        if (name.empty() || !FS().IsAlive(n))
            return;
        if (!GetShell().Settings().showExtensions && !n->IsFolder() && VirtualFS::KindFor(n->name) != FileKind::Other)
        {
            std::wstring ext;
            VirtualFS::SplitExtension(n->name, ext);
            name += ext;
        }
        if (name != n->name && !FS().Rename(n, name))
            ShowDialog(L"Переименование", L"Имя файла не должно содержать символов \\ / : * ? \" < > | и не должно совпадать с именем другого элемента.",
                       {{L"ОК", true}}, nullptr);
        m_dirty = true;
    }

    void CancelRename()
    {
        m_renameNode = nullptr;
        if (m_rename)
        {
            m_rename->SetVisible(false);
            if (m_ui.FocusedChild() == m_rename)
                m_ui.SetFocus(nullptr);
        }
    }

    // Адресная строка (режим редактирования)
    void EditAddress()
    {
        const Location& l = Cur().loc;
        m_address->SetVisible(true);
        m_address->SetText(l.node ? FS().PathOf(l.node) : LocName(l));
        m_address->SelectAll();
        m_ui.SetFocus(m_address);
    }

    void CommitAddress()
    {
        const std::wstring text = Trim(m_address->Text());
        m_address->SetVisible(false);
        m_ui.SetFocus(nullptr);
        if (text.empty())
            return;
        const std::wstring lower = ToLower(text);
        Location l;
        if (lower == L"главная" || lower == L"home")
        {
            Navigate(l);
            return;
        }
        if (lower == L"этот компьютер" || lower == L"this pc")
        {
            l.node = FS().Root();
            l.special = Special::None;
            Navigate(l);
            return;
        }
        if (lower == L"корзина")
        {
            l.node = FS().Bin();
            l.special = Special::None;
            Navigate(l);
            return;
        }
        FsNode* n = FS().Resolve(text, Cur().loc.node ? Cur().loc.node : FS().Home());
        if (n && n->IsFolder())
        {
            l.node = n;
            l.special = Special::None;
            Navigate(l);
        }
        else if (n)
            GetShell().Open(n);
        else
            ShowDialog(L"Проводник", L"Не удаётся найти «" + text + L"». Проверьте правильность написания и повторите попытку.",
                       {{L"ОК", true}}, nullptr);
    }

    void CancelAddress()
    {
        if (m_address)
        {
            m_address->SetVisible(false);
            if (m_ui.FocusedChild() == m_address)
                m_ui.SetFocus(nullptr);
        }
    }

    // ------------------------------------------------------------------ отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        if (m_dirty || m_fsVersion != FS().Version())
            Recompute();
        const Palette& p = Theme::P();
        const float W = Width(), H = Height();

        DrawTabs(r);
        // Полоса адреса и команд.
        r.FillRect({0.0f, kTabBarH, W, kMainY - kTabBarH}, p.layer);
        r.FillRect({0.0f, kMainY - 1.0f, W, 1.0f}, p.divider);
        // Основная область.
        r.FillRect({0.0f, kMainY, W, H - kMainY}, p.contentBg);
        DrawAddress(r);
        DrawCommandSeparators(r);
        if (m_showNav)
        {
            DrawNav(r);
            r.FillRect({NavW(), kMainY, 1.0f, H - kMainY - kStatusH}, p.divider);
        }
        const Rect c = ContentRect();
        r.PushClip(c);
        const Location& l = Cur().loc;
        if (Searching())
            DrawList(r, true);
        else if (l.special == Special::Home && !l.node)
            DrawHome(r);
        else if (l.special == Special::Network && !l.node)
            r.Text(L"Сетевые устройства не найдены.\nОбнаружение сети отключено.", c, p.textSecondary, 14.0f, FontWeight::Regular,
                   TextFlags::Middle | TextFlags::Wrap);
        else if (l.node == FS().Root())
            DrawThisPC(r);
        else if (IconLayout())
            DrawIcons(r);
        else
            DrawList(r, false);
        DrawBand(r);
        r.PopClip();
        if (m_showDetails)
            DrawDetailsPane(r);
        DrawStatus(r);

        // Состояние кнопок.
        const bool hasSel = !Cur().sel.empty();
        bool canModify = hasSel;
        for (FsNode* n : Cur().sel)
            canModify = canModify && !n->system;
        for (size_t i = 0; i < m_selButtons.size(); ++i)
            m_selButtons[i]->SetEnabled(i == 1 ? hasSel : canModify);
        if (m_pasteButton)
            m_pasteButton->SetEnabled(!FS().clip.empty() && Cur().loc.node && Cur().loc.node != FS().Root() && !InBin());
        m_backBtn->SetEnabled(!Cur().back.empty());
        m_fwdBtn->SetEnabled(!Cur().forward.empty());
        m_upBtn->SetEnabled(Cur().loc.node != nullptr);
        m_detailsBtn->checked = m_showDetails;
        m_search->placeholder = L"Поиск: " + LocName(Cur().loc);

        m_ui.Draw(r);
        if (m_dragging)
            DrawDragGhost(r);
    }

    void DrawTabs(Renderer& r)
    {
        const Palette& p = Theme::P();
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            const Rect tr = TabRect(i);
            const bool sel = i == m_cur;
            const bool hot = i == m_hoverTab;
            if (sel)
                r.FillRoundRect(tr, 8.0f, 8.0f, 0.0f, 0.0f, p.layer);
            else if (hot)
                r.FillRoundRect(tr.Inset(2.0f, 2.0f, 2.0f, 4.0f), 4.0f, p.subtleHover);
            else if (i + 1 != m_cur && i + 1 < static_cast<int>(m_tabs.size()) && i + 1 != m_hoverTab)
                r.FillRect({tr.Right() - 0.5f, tr.y + 8.0f, 1.0f, 16.0f}, p.divider);
            const Location& l = m_tabs[static_cast<size_t>(i)].loc;
            r.DrawAppIcon(LocIcon(l), {tr.x + 12.0f, tr.CenterY() - 8.0f, 16.0f, 16.0f});
            r.Text(LocName(l), {tr.x + 36.0f, tr.y, tr.w - 36.0f - 32.0f, tr.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            if (sel || hot)
            {
                const Rect xr{tr.Right() - 30.0f, tr.CenterY() - 11.0f, 22.0f, 22.0f};
                if (m_hoverTabClose && hot)
                    r.FillRoundRect(xr, 4.0f, p.subtleHover);
                r.Glyph(Icon::Close, {xr.CenterX() - 5.0f, xr.CenterY() - 5.0f, 10.0f, 10.0f}, p.text);
            }
        }
        const Rect nt = NewTabRect();
        if (m_hoverNewTab)
            r.FillRoundRect(nt, 4.0f, p.subtleHover);
        r.Glyph(Icon::Add, {nt.CenterX() - 6.0f, nt.CenterY() - 6.0f, 12.0f, 12.0f}, p.text);
    }

    void DrawAddress(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect ar = AddressRect();
        if (m_address->Visible())
            return;
        r.FillRoundRect(ar, 4.0f, m_hoverAddress ? p.controlHover : p.control);
        r.StrokeRoundRect(ar, 4.0f, p.controlStroke);
        m_crumbs.clear();
        const Location& l = Cur().loc;
        std::vector<std::pair<std::wstring, Location>> parts;
        if (l.node)
        {
            for (FsNode* n : FS().Chain(l.node))
            {
                Location cl;
                cl.node = n;
                cl.special = Special::None;
                parts.emplace_back(n->name, cl);
            }
        }
        else
            parts.emplace_back(LocName(l), l);
        r.DrawAppIcon(LocIcon(l), {ar.x + 10.0f, ar.CenterY() - 8.0f, 16.0f, 16.0f});
        float x = ar.x + 34.0f;
        // Если не помещается — показываем последние элементы.
        float total = 0.0f;
        std::vector<float> widths;
        for (auto& pr : parts)
        {
            widths.push_back(r.TextWidth(pr.first, 12.0f) + 12.0f);
            total += widths.back() + 18.0f;
        }
        size_t first = 0;
        while (first + 1 < parts.size() && total > ar.w - 50.0f)
        {
            total -= widths[first] + 18.0f;
            ++first;
        }
        if (first > 0)
        {
            r.Text(L"«", {x, ar.y, 14.0f, ar.h}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Middle);
            x += 16.0f;
        }
        for (size_t i = first; i < parts.size(); ++i)
        {
            const Rect chev{x, ar.y + 6.0f, 18.0f, ar.h - 12.0f};
            if (m_hoverCrumb == static_cast<int>(m_crumbs.size()) && m_hoverCrumbChevron)
                r.FillRoundRect(chev, 3.0f, p.subtleHover);
            r.Glyph(Icon::ChevronRight, {chev.CenterX() - 4.0f, chev.CenterY() - 4.0f, 8.0f, 8.0f}, p.textSecondary);
            x += 18.0f;
            const Rect seg{x, ar.y + 4.0f, widths[i], ar.h - 8.0f};
            if (m_hoverCrumb == static_cast<int>(m_crumbs.size()) && !m_hoverCrumbChevron)
                r.FillRoundRect(seg, 3.0f, p.subtleHover);
            r.Text(parts[i].first, seg, p.text, 12.0f, FontWeight::Regular, TextFlags::Middle);
            m_crumbs.push_back({seg, chev, parts[i].second});
            x += widths[i];
        }
    }

    void DrawCommandSeparators(Renderer& r)
    {
        const Palette& p = Theme::P();
        for (float sx : {m_sep1, m_sep2, m_sep3})
            r.FillRect({sx, kCmdY + 14.0f, 1.0f, 20.0f}, p.divider);
    }

    void DrawNav(Renderer& r)
    {
        const Palette& p = Theme::P();
        const std::vector<NavEntry> entries = NavEntries();
        r.PushClip(NavRect());
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const NavEntry& e = entries[static_cast<size_t>(i)];
            const Rect rr = NavRow(i, entries);
            if (e.separatorBefore)
                r.FillRect({12.0f, rr.y - 7.0f, NavW() - 24.0f, 1.0f}, p.divider);
            const bool sel = !Searching() && e.loc == Cur().loc;
            const bool hot = i == m_hoverNav;
            const bool drop = m_dragging && i == m_dropNav;
            if (sel || hot || drop)
                r.FillRoundRect(rr, 4.0f, sel ? p.selection : (drop ? p.selection : p.subtleHover));
            const float ix = rr.x + 22.0f + static_cast<float>(e.indent) * 16.0f;
            if (e.expandable)
                r.Glyph(e.expanded ? Icon::ChevronDown : Icon::ChevronRight, {ix - 18.0f, rr.CenterY() - 5.0f, 10.0f, 10.0f},
                        p.textSecondary);
            r.DrawAppIcon(e.icon, {ix, rr.CenterY() - 8.0f, 16.0f, 16.0f});
            r.Text(e.name, {ix + 26.0f, rr.y, rr.Right() - ix - 30.0f, rr.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        r.PopClip();
    }

    void DrawHeaderRow(Renderer& r, const Rect& c, bool search, bool bin)
    {
        const Palette& p = Theme::P();
        const Columns col = Cols();
        const float y = c.y - Cur().scroll + 2.0f;
        const wchar_t* names[4] = {L"Имя", L"Дата изменения", L"Тип", L"Размер"};
        if (bin)
        {
            names[1] = L"Исходное расположение";
            names[2] = L"Дата удаления";
        }
        if (search)
            names[2] = L"Папка";
        const float widths[4] = {col.name, col.date, col.type, col.size};
        float x = c.x + 12.0f;
        m_headerRects.clear();
        for (int i = 0; i < 4; ++i)
        {
            const Rect hr{x, y, widths[i], kHeaderH};
            m_headerRects.push_back(hr);
            if (i == m_hoverHeader)
                r.FillRoundRect(hr.Inset(0.0f, 4.0f, 2.0f, 4.0f), 4.0f, p.subtleHover);
            r.Text(names[i], {hr.x + 8.0f, hr.y, hr.w - 16.0f, hr.h}, p.textSecondary, 12.0f, FontWeight::Regular,
                   (i == 3 ? TextFlags::Right : TextFlags::Left) | TextFlags::VCenter);
            if (static_cast<int>(m_sortKey) == i && !bin)
                r.Glyph(m_sortAsc ? Icon::ChevronUp : Icon::ChevronDown, {hr.CenterX() - 4.0f, hr.y + 1.0f, 8.0f, 8.0f}, p.textTertiary);
            if (i < 3)
                r.FillRect({hr.Right() - 1.0f, hr.y + 8.0f, 1.0f, hr.h - 16.0f}, p.divider);
            x += widths[i];
        }
        r.FillRect({c.x + 8.0f, y + kHeaderH, c.w - 16.0f, 1.0f}, p.divider);
    }

    void DrawList(Renderer& r, bool search)
    {
        const Palette& p = Theme::P();
        const Rect c = ContentRect();
        VirtualFS& fs = FS();
        const bool bin = InBin() && !search;
        const bool showExt = GetShell().Settings().showExtensions;
        if (m_items.empty())
        {
            r.Text(search ? L"Нет элементов, удовлетворяющих условиям поиска." : L"Эта папка пуста.",
                   {c.x, c.y + 60.0f, c.w, 30.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center);
            return;
        }
        DrawHeaderRow(r, c, search, bin);
        const Columns col = Cols();
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const Rect rr = RowRect(i);
            if (rr.Bottom() < c.y || rr.y > c.Bottom())
                continue;
            FsNode* n = m_items[static_cast<size_t>(i)];
            const bool sel = IsSelected(n);
            const bool hot = i == m_hoverItem;
            const bool drop = m_dragging && i == m_dropItem;
            if (sel || drop)
            {
                r.FillRoundRect(rr, 4.0f, p.selection);
                if (i == m_focusIndex && IsActive())
                    r.StrokeRoundRect(rr, 4.0f, p.selectionStroke);
            }
            else if (hot)
                r.FillRoundRect(rr, 4.0f, p.selectionHover);
            const bool cut = fs.clipCut && std::find(fs.clip.begin(), fs.clip.end(), n) != fs.clip.end();
            r.PushOpacity(cut ? 0.5f : 1.0f);
            r.DrawAppIcon(fs.IconFor(n), {rr.x + 12.0f, rr.CenterY() - 8.0f, 16.0f, 16.0f});
            r.PopOpacity();
            float x = rr.x + 4.0f;
            if (n != m_renameNode)
                r.Text(VirtualFS::DisplayName(n, showExt), {x + 32.0f, rr.y, col.name - 40.0f, rr.h}, p.text, 12.0f,
                       FontWeight::Regular, TextFlags::VCenter);
            x += col.name;
            std::wstring c2 = FormatDateTime(n->modified), c3 = fs.TypeName(n);
            if (bin)
            {
                c2 = n->originalPath;
                c3 = FormatDateTime(n->deleted);
            }
            if (search)
                c3 = fs.PathOf(n->parent);
            r.Text(c2, {x + 8.0f, rr.y, col.date - 16.0f, rr.h}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            x += col.date;
            r.Text(c3, {x + 8.0f, rr.y, col.type - 16.0f, rr.h}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            x += col.type;
            if (!n->IsFolder())
                r.Text(FormatKB(fs.SizeOf(n)), {x + 8.0f, rr.y, col.size - 16.0f, rr.h}, p.textSecondary, 12.0f, FontWeight::Regular,
                       TextFlags::VCenter | TextFlags::Right);
        }
        DrawScrollbar(r);
    }

    void DrawIcons(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect c = ContentRect();
        VirtualFS& fs = FS();
        const bool showExt = GetShell().Settings().showExtensions;
        if (m_items.empty())
        {
            r.Text(L"Эта папка пуста.", {c.x, c.y + 60.0f, c.w, 30.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center);
            return;
        }
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const Rect cell = IconCell(i);
            if (cell.Bottom() < c.y || cell.y > c.Bottom())
                continue;
            FsNode* n = m_items[static_cast<size_t>(i)];
            const bool sel = IsSelected(n);
            const bool hot = i == m_hoverItem;
            if (sel || (m_dragging && i == m_dropItem))
                r.FillRoundRect(cell, 4.0f, p.selection);
            else if (hot)
                r.FillRoundRect(cell, 4.0f, p.selectionHover);
            const Rect iconBox{cell.CenterX() - 44.0f, cell.y + 8.0f, 88.0f, 72.0f};
            if (n->kind == FileKind::Image)
            {
                std::shared_ptr<Gdiplus::Bitmap> bmp = NodeBitmap(n);
                if (bmp)
                {
                    const float iw = static_cast<float>(bmp->GetWidth()), ih = static_cast<float>(bmp->GetHeight());
                    const float s = std::min(iconBox.w / iw, iconBox.h / ih);
                    const Rect ir{iconBox.CenterX() - iw * s * 0.5f, iconBox.CenterY() - ih * s * 0.5f, iw * s, ih * s};
                    r.Image(bmp.get(), ir);
                    r.StrokeRect(ir.Inflated(1.0f), p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x20));
                }
            }
            else
                r.DrawAppIcon(fs.IconFor(n), {iconBox.CenterX() - 36.0f, iconBox.CenterY() - 36.0f, 72.0f, 72.0f});
            if (n != m_renameNode)
                r.Text(VirtualFS::DisplayName(n, showExt), {cell.x + 4.0f, cell.y + 84.0f, cell.w - 8.0f, 36.0f}, p.text, 12.0f,
                       FontWeight::Regular, TextFlags::Center | TextFlags::Wrap);
        }
        DrawScrollbar(r);
    }

    void DrawHome(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect c = ContentRect();
        VirtualFS& fs = FS();
        const float sy = Cur().scroll;
        r.Glyph(Icon::ChevronDown, {c.x + 20.0f, c.y + 22.0f - sy, 10.0f, 10.0f}, p.textSecondary);
        r.Text(L"Быстрый доступ", {c.x + 38.0f, c.y + 12.0f - sy, 300.0f, 30.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        const std::vector<FsNode*> qa = QuickAccess();
        for (int i = 0; i < static_cast<int>(qa.size()); ++i)
        {
            const Rect t = QuickTile(i);
            const bool hot = m_hoverQuick == i;
            if (hot)
                r.FillRoundRect(t, 6.0f, p.selectionHover);
            r.DrawAppIcon(fs.IconFor(qa[static_cast<size_t>(i)]), {t.x + 12.0f, t.CenterY() - 20.0f, 40.0f, 40.0f});
            r.Text(qa[static_cast<size_t>(i)]->name, {t.x + 64.0f, t.y + 14.0f, t.w - 72.0f, 20.0f}, p.text, 12.0f);
            r.Glyph(Icon::Pin, {t.x + 64.0f, t.y + 40.0f, 12.0f, 12.0f}, p.textSecondary);
            r.Text(L"Хранится локально", {t.x + 80.0f, t.y + 36.0f, t.w - 88.0f, 20.0f}, p.textSecondary, 12.0f);
        }
        float y = HomeRecentTop() - sy;
        r.Glyph(Icon::ChevronDown, {c.x + 20.0f, y + 10.0f, 10.0f, 10.0f}, p.textSecondary);
        r.Text(L"Избранное", {c.x + 38.0f, y, 300.0f, 30.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(L"Добавив файлы в избранное, вы увидите их здесь.", {c.x + 38.0f, y + 32.0f, c.w - 60.0f, 22.0f}, p.textSecondary,
               12.0f);
        y += 80.0f;
        r.Glyph(Icon::ChevronDown, {c.x + 20.0f, y + 10.0f, 10.0f, 10.0f}, p.textSecondary);
        r.Text(L"Последние", {c.x + 38.0f, y, 300.0f, 30.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        // Таблица последних файлов.
        const Columns col = Cols();
        const float hy = y + 32.0f;
        r.Text(L"Имя", {c.x + 20.0f, hy, col.name, kHeaderH}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        r.Text(L"Дата изменения", {c.x + 12.0f + col.name + 8.0f, hy, col.date, kHeaderH}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        r.Text(L"Расположение", {c.x + 12.0f + col.name + col.date + 8.0f, hy, col.type + col.size, kHeaderH}, p.textSecondary, 12.0f,
               FontWeight::Regular, TextFlags::VCenter);
        r.FillRect({c.x + 8.0f, hy + kHeaderH, c.w - 16.0f, 1.0f}, p.divider);
        const bool showExt = GetShell().Settings().showExtensions;
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const Rect rr = HomeRow(i);
            FsNode* n = m_items[static_cast<size_t>(i)];
            if (IsSelected(n))
                r.FillRoundRect(rr, 4.0f, p.selection);
            else if (i == m_hoverItem)
                r.FillRoundRect(rr, 4.0f, p.selectionHover);
            r.DrawAppIcon(fs.IconFor(n), {rr.x + 12.0f, rr.CenterY() - 8.0f, 16.0f, 16.0f});
            r.Text(VirtualFS::DisplayName(n, showExt), {rr.x + 36.0f, rr.y, col.name - 40.0f, rr.h}, p.text, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter);
            r.Text(FormatDateTime(n->modified), {rr.x + 4.0f + col.name + 8.0f, rr.y, col.date - 16.0f, rr.h}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter);
            r.Text(fs.PathOf(n->parent), {rr.x + 4.0f + col.name + col.date + 8.0f, rr.y, col.type + col.size - 16.0f, rr.h},
                   p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
    }

    void DrawThisPC(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect c = ContentRect();
        VirtualFS& fs = FS();
        r.Glyph(Icon::ChevronDown, {c.x + 20.0f, c.y + 22.0f, 10.0f, 10.0f}, p.textSecondary);
        r.Text(L"Устройства и диски", {c.x + 38.0f, c.y + 12.0f, 300.0f, 30.0f}, p.text, 14.0f, FontWeight::Semibold,
               TextFlags::VCenter);
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            FsNode* d = m_items[static_cast<size_t>(i)];
            const Rect t = DriveTile(i);
            if (IsSelected(d))
                r.FillRoundRect(t, 6.0f, p.selection);
            else if (i == m_hoverItem)
                r.FillRoundRect(t, 6.0f, p.selectionHover);
            r.DrawAppIcon(fs.IconFor(d), {t.x + 10.0f, t.CenterY() - 22.0f, 44.0f, 44.0f});
            r.Text(d->name, {t.x + 64.0f, t.y + 10.0f, t.w - 72.0f, 18.0f}, p.text, 12.0f);
            const float used = d->capacity ? static_cast<float>(static_cast<double>(d->capacity - d->freeSpace) / static_cast<double>(d->capacity)) : 0.0f;
            const Rect bar{t.x + 64.0f, t.y + 34.0f, t.w - 80.0f, 7.0f};
            r.FillRoundRect(bar, 3.5f, p.dark ? Color(255, 255, 255, 0x22) : Color(0, 0, 0, 0x14));
            r.FillRoundRect({bar.x, bar.y, bar.w * used, bar.h}, 3.5f, used > 0.9f ? p.critical : p.accent);
            r.Text(FormatSize(d->freeSpace) + L" свободно из " + FormatSize(d->capacity), {t.x + 64.0f, t.y + 46.0f, t.w - 72.0f, 18.0f},
                   p.textSecondary, 12.0f);
        }
    }

    void DrawScrollbar(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect c = ContentRect();
        const float content = ContentHeight();
        if (content <= c.h)
            return;
        const float maxS = content - c.h;
        const float th = std::max(30.0f, c.h * c.h / content);
        const float y = c.y + 4.0f + (c.h - 8.0f - th) * (Cur().scroll / maxS);
        r.FillRoundRect({c.Right() - 6.0f, y, 3.0f, th}, 1.5f, p.scrollThumb);
    }

    void DrawBand(Renderer& r)
    {
        if (!m_banding)
            return;
        const Rect band = Rect::LTRB(std::min(m_pressPos.x, m_dragPos.x), std::min(m_pressPos.y, m_dragPos.y),
                                     std::max(m_pressPos.x, m_dragPos.x), std::max(m_pressPos.y, m_dragPos.y));
        const Color acc = Theme::A().base;
        r.FillRect(band, acc.WithAlpha(0x40));
        r.StrokeRect(band, acc.WithAlpha(0xC0));
    }

    void DrawDragGhost(Renderer& r)
    {
        const Palette& p = Theme::P();
        const size_t n = Cur().sel.size();
        if (n == 0)
            return;
        r.PushOpacity(0.85f);
        r.DrawAppIcon(FS().IconFor(Cur().sel.front()), {m_dragPos.x - 20.0f, m_dragPos.y - 20.0f, 40.0f, 40.0f});
        if (n > 1)
        {
            r.FillCircle(m_dragPos.x + 18.0f, m_dragPos.y - 18.0f, 10.0f, p.accent);
            r.Text(std::to_wstring(n), {m_dragPos.x + 8.0f, m_dragPos.y - 28.0f, 20.0f, 20.0f}, p.textOnAccent, 12.0f, FontWeight::Semibold,
                   TextFlags::Middle);
        }
        if (m_dropTarget)
        {
            const std::wstring tip = (m_dragCopy ? L"Копировать в «" : L"Переместить в «") + m_dropTarget->name + L"»";
            const float tw = Renderer::Get().TextWidth(tip, 12.0f) + 16.0f;
            const Rect tr{m_dragPos.x + 16.0f, m_dragPos.y + 18.0f, tw, 24.0f};
            r.FillRoundRect(tr, 4.0f, p.tooltipBg);
            r.StrokeRoundRect(tr, 4.0f, p.flyoutStroke);
            r.Text(tip, tr, p.text, 12.0f, FontWeight::Regular, TextFlags::Middle);
        }
        r.PopOpacity();
    }

    void DrawDetailsPane(Renderer& r)
    {
        const Palette& p = Theme::P();
        VirtualFS& fs = FS();
        const Rect pane{Width() - DetailsW(), kMainY, DetailsW(), Height() - kMainY - kStatusH};
        r.FillRect({pane.x, pane.y, 1.0f, pane.h}, p.divider);
        const auto& sel = Cur().sel;
        FsNode* n = sel.size() == 1 ? sel.front() : nullptr;
        float y = pane.y + 24.0f;
        if (n)
        {
            if (n->kind == FileKind::Image)
            {
                std::shared_ptr<Gdiplus::Bitmap> bmp = NodeBitmap(n);
                if (bmp)
                {
                    const float iw = static_cast<float>(bmp->GetWidth()), ih = static_cast<float>(bmp->GetHeight());
                    const float s = std::min((pane.w - 48.0f) / iw, 170.0f / ih);
                    r.ImageRounded(bmp.get(), {pane.CenterX() - iw * s * 0.5f, y, iw * s, ih * s}, 4.0f);
                    y += ih * s + 16.0f;
                }
            }
            else
            {
                r.DrawAppIcon(fs.IconFor(n), {pane.CenterX() - 48.0f, y, 96.0f, 96.0f});
                y += 112.0f;
            }
            r.Text(n->name, {pane.x + 20.0f, y, pane.w - 40.0f, 40.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::Wrap);
            y += 48.0f;
            auto row = [&](const std::wstring& k, const std::wstring& v) {
                r.Text(k, {pane.x + 20.0f, y, pane.w - 40.0f, 18.0f}, p.textSecondary, 12.0f);
                r.Text(v, {pane.x + 20.0f, y + 18.0f, pane.w - 40.0f, 20.0f}, p.text, 12.0f);
                y += 44.0f;
            };
            row(L"Тип", fs.TypeName(n));
            if (!n->IsFolder())
                row(L"Размер", FormatSize(fs.SizeOf(n)));
            row(L"Дата изменения", FormatDateTime(n->modified));
            row(L"Расположение", fs.PathOf(n->parent));
        }
        else
        {
            const Location& l = Cur().loc;
            r.DrawAppIcon(sel.empty() ? LocIcon(l) : AppIcon::FileGeneric, {pane.CenterX() - 48.0f, y, 96.0f, 96.0f});
            y += 112.0f;
            const std::wstring text = sel.empty() ? LocName(l) : L"Выбрано элементов: " + std::to_wstring(sel.size());
            r.Text(text, {pane.x + 20.0f, y, pane.w - 40.0f, 24.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::Center);
            if (sel.empty())
                r.Text(L"Выберите файл, чтобы просмотреть сведения о нём.", {pane.x + 20.0f, y + 32.0f, pane.w - 40.0f, 40.0f},
                       p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center | TextFlags::Wrap);
        }
    }

    void DrawStatus(Renderer& r)
    {
        const Palette& p = Theme::P();
        VirtualFS& fs = FS();
        const float y = Height() - kStatusH;
        std::wstring s;
        const Location& l = Cur().loc;
        size_t count = m_items.size();
        if (l.special == Special::Home && !l.node && !Searching())
            count = QuickAccess().size() + m_items.size();
        s = L"Элементов: " + std::to_wstring(count);
        const auto& sel = Cur().sel;
        if (!sel.empty())
        {
            unsigned long long total = 0;
            bool anyFile = false;
            for (FsNode* n : sel)
            {
                if (!n->IsFolder())
                {
                    total += fs.SizeOf(n);
                    anyFile = true;
                }
            }
            s += L"    Выбрано: " + std::to_wstring(sel.size());
            if (anyFile)
                s += L"  " + FormatSize(total);
        }
        r.Text(s, {12.0f, y, Width() - 100.0f, kStatusH}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        // Переключатели вида.
        const Rect b1{Width() - 64.0f, y + 2.0f, 24.0f, 22.0f}, b2{Width() - 36.0f, y + 2.0f, 24.0f, 22.0f};
        const bool icons = CurrentView() == ViewMode::Icons;
        if (!icons)
            r.FillRoundRect(b1, 3.0f, p.subtleHover);
        else
            r.FillRoundRect(b2, 3.0f, p.subtleHover);
        r.Glyph(Icon::Details, {b1.CenterX() - 7.0f, b1.CenterY() - 7.0f, 14.0f, 14.0f}, p.text);
        r.Glyph(Icon::Tiles, {b2.CenterX() - 7.0f, b2.CenterY() - 7.0f, 14.0f, 14.0f}, p.text);
    }

    // ------------------------------------------------------------------ ввод
    struct Crumb
    {
        Rect seg;
        Rect chevron;
        Location loc;
    };

    int ItemAt(Point pt) const
    {
        const Rect c = ContentRect();
        if (!c.Contains(pt))
            return -1;
        const Location& l = Cur().loc;
        if (l.special == Special::Home && !l.node && !Searching())
        {
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
                if (HomeRow(i).Contains(pt))
                    return i;
            return -1;
        }
        if (l.node == FS().Root() && !Searching())
        {
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
                if (DriveTile(i).Contains(pt))
                    return i;
            return -1;
        }
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            const Rect rr = IconLayout() ? IconCell(i) : RowRect(i);
            if (rr.Contains(pt))
                return i;
        }
        return -1;
    }

    int QuickAt(Point pt) const
    {
        const Location& l = Cur().loc;
        if (!(l.special == Special::Home && !l.node) || Searching() || !ContentRect().Contains(pt))
            return -1;
        const std::vector<FsNode*> qa = QuickAccess();
        for (int i = 0; i < static_cast<int>(qa.size()); ++i)
            if (QuickTile(i).Contains(pt))
                return i;
        return -1;
    }

    int NavAt(Point pt, std::vector<NavEntry>& entries) const
    {
        if (!m_showNav || !NavRect().Contains(pt))
            return -1;
        entries = NavEntries();
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
            if (NavRow(i, entries).Contains(pt))
                return i;
        return -1;
    }

    int TabAt(Point pt, bool* onClose) const
    {
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            const Rect tr = TabRect(i);
            if (tr.Contains(pt))
            {
                if (onClose)
                    *onClose = Rect{tr.Right() - 30.0f, tr.CenterY() - 11.0f, 22.0f, 22.0f}.Contains(pt);
                return i;
            }
        }
        return -1;
    }

    bool IsDragArea(Point local) const override
    {
        if (local.y >= kTabBarH || local.x > Width() - 46.0f * 3.0f)
            return false;
        if (TabAt(local, nullptr) >= 0 || NewTabRect().Contains(local))
            return false;
        return true;
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseMove(e);
            return;
        }
        m_ui.OnMouseMove(e);
        if (!m_ui.HasCapture())
        {
            Widget* leaf = m_ui.HoverLeaf();
            if (leaf && !leaf->Tooltip().empty())
                GetShell().Tooltip(leaf, leaf->Tooltip(), LocalToScreen(leaf->Bounds()), false);
        }
        const Point pt = e.pos;
        m_dragPos = pt;
        if (m_banding)
        {
            const Rect band = Rect::LTRB(std::min(m_pressPos.x, pt.x), std::min(m_pressPos.y, pt.y), std::max(m_pressPos.x, pt.x),
                                         std::max(m_pressPos.y, pt.y));
            std::vector<FsNode*> sel = e.mods.ctrl ? m_bandBase : std::vector<FsNode*>{};
            for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            {
                if (band.Intersects(IconLayout() ? IconCell(i) : RowRect(i)))
                {
                    FsNode* n = m_items[static_cast<size_t>(i)];
                    if (std::find(sel.begin(), sel.end(), n) == sel.end())
                        sel.push_back(n);
                }
            }
            Cur().sel = sel;
            return;
        }
        if (m_pressItem >= 0 && !m_dragging && Distance(pt, m_pressPos) > 6.0f && !Cur().sel.empty() && !InBin())
            m_dragging = true;
        if (m_dragging)
        {
            m_dragCopy = e.mods.ctrl;
            m_dropTarget = nullptr;
            m_dropItem = -1;
            m_dropNav = -1;
            const int it = ItemAt(pt);
            if (it >= 0)
            {
                FsNode* n = m_items[static_cast<size_t>(it)];
                if (n->IsFolder() && !IsSelected(n))
                {
                    m_dropTarget = n;
                    m_dropItem = it;
                }
            }
            std::vector<NavEntry> entries;
            const int ni = NavAt(pt, entries);
            if (ni >= 0)
            {
                FsNode* n = entries[static_cast<size_t>(ni)].loc.node;
                if (n && n != FS().Root() && !IsSelected(n))
                {
                    m_dropTarget = n;
                    m_dropNav = ni;
                }
            }
            return;
        }
        m_hoverItem = ItemAt(pt);
        m_hoverQuick = QuickAt(pt);
        std::vector<NavEntry> entries;
        m_hoverNav = NavAt(pt, entries);
        m_hoverTab = TabAt(pt, &m_hoverTabClose);
        m_hoverNewTab = NewTabRect().Contains(pt);
        m_hoverAddress = AddressRect().Contains(pt);
        m_hoverCrumb = -1;
        for (int i = 0; i < static_cast<int>(m_crumbs.size()); ++i)
        {
            if (m_crumbs[static_cast<size_t>(i)].seg.Contains(pt))
            {
                m_hoverCrumb = i;
                m_hoverCrumbChevron = false;
            }
            else if (m_crumbs[static_cast<size_t>(i)].chevron.Contains(pt))
            {
                m_hoverCrumb = i;
                m_hoverCrumbChevron = true;
            }
        }
        m_hoverHeader = -1;
        if (!IconLayout() && Cur().loc.node && Cur().loc.node != FS().Root())
        {
            for (int i = 0; i < static_cast<int>(m_headerRects.size()); ++i)
                if (m_headerRects[static_cast<size_t>(i)].Contains(pt))
                    m_hoverHeader = i;
        }
        if (m_hoverTab >= 0)
            GetShell().Tooltip(&m_tabs, LocName(m_tabs[static_cast<size_t>(m_hoverTab)].loc), LocalToScreen(TabRect(m_hoverTab)), false);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_hoverItem = m_hoverNav = m_hoverTab = m_hoverQuick = m_hoverHeader = m_hoverCrumb = -1;
        m_hoverNewTab = m_hoverAddress = false;
    }

    void SelectClick(int idx, const KeyMods& mods)
    {
        FsNode* n = m_items[static_cast<size_t>(idx)];
        auto& sel = Cur().sel;
        if (mods.ctrl)
        {
            auto it = std::find(sel.begin(), sel.end(), n);
            if (it != sel.end())
                sel.erase(it);
            else
                sel.push_back(n);
            m_anchor = idx;
        }
        else if (mods.shift && m_anchor >= 0 && m_anchor < static_cast<int>(m_items.size()))
        {
            sel.clear();
            for (int i = std::min(m_anchor, idx); i <= std::max(m_anchor, idx); ++i)
                sel.push_back(m_items[static_cast<size_t>(i)]);
        }
        else
        {
            if (!IsSelected(n))
                sel = {n};
            m_anchor = idx;
        }
        m_focusIndex = idx;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        const Point pt = e.pos;
        // Поля ввода и кнопки.
        if (m_renameNode && !m_rename->Bounds().Contains(pt))
            CommitRename();
        if (m_address->Visible() && !m_address->Bounds().Contains(pt))
            CancelAddress();
        if (m_ui.OnMouseDown(e))
            return;
        m_pressPos = pt;
        m_dragPos = pt;
        m_pressItem = -1;
        if (e.button == MouseButton::Middle)
        {
            bool onClose = false;
            const int t = TabAt(pt, &onClose);
            if (t >= 0)
                CloseTab(t);
            return;
        }
        // Вкладки.
        bool onClose = false;
        const int tab = TabAt(pt, &onClose);
        if (tab >= 0)
        {
            if (onClose)
                CloseTab(tab);
            else
                SwitchTab(tab);
            return;
        }
        if (NewTabRect().Contains(pt))
        {
            NewTab({});
            return;
        }
        // Адресная строка.
        if (AddressRect().Contains(pt))
        {
            for (const Crumb& c : m_crumbs)
            {
                if (c.seg.Contains(pt))
                {
                    if (!(c.loc == Cur().loc))
                        Navigate(c.loc);
                    return;
                }
                if (c.chevron.Contains(pt))
                {
                    ShowCrumbMenu(c);
                    return;
                }
            }
            EditAddress();
            return;
        }
        // Строка состояния: переключение вида.
        if (pt.y >= Height() - kStatusH)
        {
            if (pt.x >= Width() - 64.0f && pt.x < Width() - 40.0f)
                SetView(ViewMode::Details);
            else if (pt.x >= Width() - 36.0f && pt.x < Width() - 12.0f)
                SetView(ViewMode::Icons);
            return;
        }
        // Навигация.
        std::vector<NavEntry> entries;
        const int ni = NavAt(pt, entries);
        if (ni >= 0)
        {
            const NavEntry& en = entries[static_cast<size_t>(ni)];
            const Rect rr = NavRow(ni, entries);
            if (e.button != MouseButton::Left)
                return; // контекстное меню — при отпускании
            if (en.expandable && en.loc.node == FS().Root() && pt.x < rr.x + 24.0f)
            {
                m_pcExpanded = !m_pcExpanded;
                return;
            }
            Navigate(en.loc);
            return;
        }
        // Плитки быстрого доступа.
        const int qi = QuickAt(pt);
        if (qi >= 0)
        {
            if (e.button == MouseButton::Left)
                OpenItem(QuickAccess()[static_cast<size_t>(qi)]);
            return;
        }
        const Rect c = ContentRect();
        if (!c.Contains(pt))
            return;
        // Заголовки колонок — сортировка.
        if (m_hoverHeader >= 0 && e.button == MouseButton::Left)
        {
            const SortKey k = static_cast<SortKey>(m_hoverHeader);
            if (k == m_sortKey)
                m_sortAsc = !m_sortAsc;
            else
            {
                m_sortKey = k;
                m_sortAsc = true;
            }
            m_dirty = true;
            return;
        }
        const int idx = ItemAt(pt);
        if (idx >= 0)
        {
            if (e.button == MouseButton::Left && e.clicks >= 2)
            {
                OpenItem(m_items[static_cast<size_t>(idx)]);
                return;
            }
            if (e.button == MouseButton::Right)
            {
                if (!IsSelected(m_items[static_cast<size_t>(idx)]))
                    Cur().sel = {m_items[static_cast<size_t>(idx)]};
                m_focusIndex = idx;
                return;
            }
            SelectClick(idx, e.mods);
            m_pressItem = idx;
            return;
        }
        if (e.button == MouseButton::Left)
        {
            if (!e.mods.ctrl)
                Cur().sel.clear();
            m_bandBase = Cur().sel;
            m_banding = true;
        }
        else if (e.button == MouseButton::Right)
            Cur().sel.clear();
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseUp(e);
            return;
        }
        m_ui.OnMouseUp(e);
        const bool wasDragging = m_dragging, wasBanding = m_banding;
        m_dragging = false;
        m_banding = false;
        if (wasDragging)
        {
            if (m_dropTarget)
            {
                std::vector<FsNode*> sel = Cur().sel;
                for (FsNode* n : sel)
                {
                    if (m_dragCopy)
                        FS().CopyTo(n, m_dropTarget);
                    else
                        FS().Move(n, m_dropTarget);
                }
                Cur().sel.clear();
                m_dirty = true;
            }
            m_dropTarget = nullptr;
            m_pressItem = -1;
            return;
        }
        if (e.button == MouseButton::Right && !wasBanding)
        {
            const Point screen = LocalToScreen(e.pos);
            std::vector<NavEntry> entries;
            const int ni = NavAt(e.pos, entries);
            const int qi = QuickAt(e.pos);
            if (ni >= 0)
            {
                if (FsNode* n = entries[static_cast<size_t>(ni)].loc.node)
                    ShowItemMenu({n}, screen, true);
            }
            else if (qi >= 0)
                ShowItemMenu({QuickAccess()[static_cast<size_t>(qi)]}, screen, true);
            else if (ContentRect().Contains(e.pos))
            {
                if (ItemAt(e.pos) >= 0 && !Cur().sel.empty())
                    ShowItemMenu(Cur().sel, screen, false);
                else
                    ShowBackgroundMenu(screen);
            }
            return;
        }
        if (e.button == MouseButton::Left && m_pressItem >= 0 && !e.mods.ctrl && !e.mods.shift && !wasBanding)
        {
            const int idx = ItemAt(e.pos);
            if (idx == m_pressItem)
                Cur().sel = {m_items[static_cast<size_t>(idx)]};
        }
        m_pressItem = -1;
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        if (m_showNav && NavRect().Contains(e.pos))
        {
            m_navScroll = Clamp(m_navScroll - e.wheel * 60.0f, 0.0f, 300.0f);
            return true;
        }
        const Rect c = ContentRect();
        float content = ContentHeight();
        if (Cur().loc.special == Special::Home && !Cur().loc.node && !Searching())
            content = HomeRow(static_cast<int>(m_items.size())).Bottom() + Cur().scroll - c.y + 20.0f;
        const float maxS = std::max(0.0f, content - c.h);
        Cur().scroll = Clamp(Cur().scroll - e.wheel * 90.0f, 0.0f, maxS);
        if (m_renameNode)
            CommitRename();
        return true;
    }

    void ShowCrumbMenu(const Crumb& c)
    {
        FsNode* n = c.loc.node;
        std::vector<MenuItem> items;
        if (n)
        {
            for (auto& ch : n->children)
            {
                if (!ch->IsFolder())
                    continue;
                FsNode* target = ch.get();
                items.push_back(MenuItem(ch->name, Icon::Folder, [this, target]() {
                    Location l;
                    l.node = target;
                    l.special = Special::None;
                    Navigate(l);
                }));
            }
        }
        if (items.empty())
            items.push_back(MenuItem(L"Нет вложенных папок", Icon::None, nullptr).Disabled());
        GetShell().ShowMenuBelow(LocalToScreen(c.chevron), std::move(items), 200.0f);
    }

    void ShowItemMenu(std::vector<FsNode*> nodes, Point screen, bool fromNav)
    {
        if (nodes.empty())
            return;
        FsNode* n = nodes.front();
        VirtualFS& fs = FS();
        std::vector<MenuItem> items;
        std::vector<MenuAction> top;
        if (InBin() && !fromNav)
        {
            items.push_back(MenuItem(L"Восстановить", Icon::Undo, [this]() { RestoreSelected(false); }));
            items.push_back(MenuItem(L"Вырезать", Icon::Cut, [this]() { ClipSelection(true); }, L"Ctrl+X"));
            items.push_back(MenuItem(L"Удалить", Icon::Delete, [this]() { DeleteSelection(true); }, L"Delete"));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Свойства", Icon::Properties, [n]() { GetShell().ShowProperties(n); }, L"Alt+Enter"));
            GetShell().ShowMenu(screen, std::move(items));
            return;
        }
        const bool sys = n->system;
        if (!fromNav)
        {
            top.push_back({Icon::Cut, L"Вырезать", [this]() { ClipSelection(true); }});
            top.push_back({Icon::Copy, L"Копировать", [this]() { ClipSelection(false); }});
            if (!sys)
                top.push_back({Icon::Rename, L"Переименовать", [this]() { StartRename(); }});
            top.push_back({Icon::Share, L"Поделиться", []() {
                               GetShell().ShowMessage(L"Поделиться", L"Функция «Поделиться» недоступна в этой демонстрации.");
                           }});
            if (!sys)
                top.push_back({Icon::Delete, L"Удалить", [this]() { DeleteSelection(false); }});
        }
        items.push_back(MenuItem(L"Открыть", Icon::Open, [this, n]() { OpenItem(n); }, L"Enter"));
        if (n->IsFolder())
        {
            items.push_back(MenuItem(L"Открыть в новой вкладке", Icon::NewTab, [this, n]() {
                Location l;
                l.node = n;
                l.special = Special::None;
                NewTab(l);
            }));
            items.push_back(MenuItem(L"Открыть в новом окне", Icon::OpenWith,
                                     [n]() { GetShell().Launch(AppId::Explorer, VirtualFS::NodeArg(n)); }));
            items.push_back(MenuItem(L"Закрепить на панели быстрого доступа", Icon::Pin, [this, n]() { PinFolder(n); }));
        }
        else if (n->kind == FileKind::Text || n->kind == FileKind::Other)
            items.push_back(MenuItem(L"Изменить в Блокноте", Icon::Edit,
                                     [n]() { GetShell().Launch(AppId::Notepad, VirtualFS::NodeArg(n)); }));
        else if (n->kind == FileKind::Image)
            items.push_back(MenuItem(L"Изменить в Paint", Icon::Brush, [n]() { GetShell().Launch(AppId::Paint, VirtualFS::NodeArg(n)); }));
        if (!sys && !fromNav && n->parent)
            items.push_back(MenuItem(L"Сжать в ZIP-файл", Icon::Folder, [this, n]() {
                std::wstring ext;
                const std::wstring base = n->IsFolder() ? n->name : VirtualFS::SplitExtension(n->name, ext);
                FsNode* zip = FS().NewFile(n->parent, base + L".zip", FileKind::Archive);
                zip->size = FS().SizeOf(n) / 2 + 1024;
                FS().Touch();
                Cur().sel = {zip};
                m_dirty = true;
            }));
        items.push_back(MenuItem(L"Копировать как путь", Icon::CopyPath,
                                 [n]() { SetClipboardText(GetShell().Hwnd(), L"\"" + GetShell().FS().PathOf(n) + L"\""); },
                                 L"Ctrl+Shift+C"));
        items.push_back(MenuItem(L"Свойства", Icon::Properties, [n]() { GetShell().ShowProperties(n); }, L"Alt+Enter"));
        if (n->IsFolder())
        {
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Открыть в Терминале", Icon::Terminal, [n]() {
                GetShell().Launch(AppId::Terminal, GetShell().FS().PathOf(n));
            }));
        }
        (void)fs;
        GetShell().ShowMenu(screen, std::move(items), std::move(top));
    }

    void ShowBackgroundMenu(Point screen)
    {
        FsNode* dir = Cur().loc.node;
        const ViewMode v = CurrentView();
        std::vector<MenuItem> view = {
            MenuItem::Check(L"Крупные значки", v == ViewMode::Icons, [this]() { SetView(ViewMode::Icons); }, true),
            MenuItem::Check(L"Таблица", v == ViewMode::Details, [this]() { SetView(ViewMode::Details); }, true),
        };
        auto set = [this](SortKey k) {
            m_sortKey = k;
            m_dirty = true;
        };
        std::vector<MenuItem> sort = {
            MenuItem::Check(L"Имя", m_sortKey == SortKey::Name, [set]() { set(SortKey::Name); }, true),
            MenuItem::Check(L"Дата изменения", m_sortKey == SortKey::Date, [set]() { set(SortKey::Date); }, true),
            MenuItem::Check(L"Тип", m_sortKey == SortKey::Type, [set]() { set(SortKey::Type); }, true),
            MenuItem::Check(L"Размер", m_sortKey == SortKey::Size, [set]() { set(SortKey::Size); }, true),
        };
        std::vector<MenuItem> items;
        items.push_back(MenuItem::Sub(L"Вид", Icon::Grid, view));
        items.push_back(MenuItem::Sub(L"Сортировка", Icon::Sort, sort));
        items.push_back(MenuItem::Sep());
        if (InBin())
        {
            items.push_back(MenuItem(L"Очистить корзину", Icon::Delete, [this]() { ConfirmEmptyBin(); })
                                .Disabled(FS().Bin()->children.empty()));
            items.push_back(MenuItem(L"Восстановить все объекты", Icon::Undo, [this]() { RestoreSelected(true); }));
        }
        else if (dir && dir != FS().Root())
        {
            items.push_back(MenuItem(L"Вставить", Icon::Paste, [this]() { Paste(); }, L"Ctrl+V").Disabled(FS().clip.empty()));
            std::vector<MenuItem> create = {
                MenuItem(L"Папку", Icon::Folder, [this, dir]() { CreateItem(dir, true, L"Новая папка", FileKind::Folder); }),
                MenuItem::Sep(),
                MenuItem(L"Текстовый документ", Icon::Documents,
                         [this, dir]() { CreateItem(dir, false, L"Новый текстовый документ.txt", FileKind::Text); }),
                MenuItem(L"Точечный рисунок", Icon::Pictures,
                         [this, dir]() { CreateItem(dir, false, L"Новый точечный рисунок.bmp", FileKind::Image); }),
            };
            items.push_back(MenuItem::Sub(L"Создать", Icon::Add, create));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Свойства", Icon::Properties, [dir]() { GetShell().ShowProperties(dir); }, L"Alt+Enter"));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Открыть в Терминале", Icon::Terminal, [this, dir]() {
                GetShell().Launch(AppId::Terminal, FS().PathOf(dir));
            }));
        }
        items.push_back(MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"));
        GetShell().ShowMenu(screen, std::move(items));
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        if (m_ui.FocusedChild() == m_search || m_ui.FocusedChild() == m_address || m_ui.FocusedChild() == m_rename)
        {
            if (m_ui.OnKeyDown(vk, mods))
                return true;
            if (vk == VK_ESCAPE)
            {
                m_ui.SetFocus(nullptr);
                return true;
            }
            return true;
        }
        const int n = static_cast<int>(m_items.size());
        auto moveFocus = [&](int idx) {
            if (n == 0)
                return;
            idx = Clamp(idx, 0, n - 1);
            m_focusIndex = idx;
            if (mods.shift && m_anchor >= 0)
            {
                Cur().sel.clear();
                for (int i = std::min(m_anchor, idx); i <= std::max(m_anchor, idx); ++i)
                    Cur().sel.push_back(m_items[static_cast<size_t>(i)]);
            }
            else
            {
                Cur().sel = {m_items[static_cast<size_t>(idx)]};
                m_anchor = idx;
            }
            // Прокрутка к элементу.
            const Rect c = ContentRect();
            const Rect rr = ItemRect(idx);
            if (rr.y < c.y + (IconLayout() ? 0.0f : kHeaderH))
                Cur().scroll = std::max(0.0f, Cur().scroll - (c.y + (IconLayout() ? 0.0f : kHeaderH) - rr.y) - 4.0f);
            else if (rr.Bottom() > c.Bottom())
                Cur().scroll += rr.Bottom() - c.Bottom() + 4.0f;
        };
        if (mods.alt)
        {
            if (vk == VK_LEFT)
                GoBack();
            else if (vk == VK_RIGHT)
                GoForward();
            else if (vk == VK_UP)
                GoUp();
            else if (vk == 'D')
                EditAddress();
            else if (vk == VK_RETURN && !Cur().sel.empty())
                GetShell().ShowProperties(Cur().sel.front());
            return true;
        }
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'A':
                SelectAll();
                return true;
            case 'C':
                ClipSelection(false);
                return true;
            case 'X':
                ClipSelection(true);
                return true;
            case 'V':
                Paste();
                return true;
            case 'T':
                NewTab({});
                return true;
            case 'W':
                CloseTab(m_cur);
                return true;
            case 'N':
                if (mods.shift && Cur().loc.node && Cur().loc.node != FS().Root() && !InBin())
                    CreateItem(Cur().loc.node, true, L"Новая папка", FileKind::Folder);
                else if (!mods.shift)
                    GetShell().Launch(AppId::Explorer, Cur().loc.node ? VirtualFS::NodeArg(Cur().loc.node) : L"");
                return true;
            case 'F':
            case 'E':
                m_ui.SetFocus(m_search);
                m_search->SelectAll();
                return true;
            case 'L':
                EditAddress();
                return true;
            case VK_TAB:
                if (m_tabs.size() > 1)
                    SwitchTab((m_cur + (mods.shift ? -1 : 1) + static_cast<int>(m_tabs.size())) % static_cast<int>(m_tabs.size()));
                return true;
            default:
                break;
            }
        }
        const int cols = IconLayout() ? IconCols() : 1;
        switch (vk)
        {
        case VK_DOWN:
            moveFocus(m_focusIndex < 0 ? 0 : m_focusIndex + cols);
            return true;
        case VK_UP:
            moveFocus(m_focusIndex < 0 ? 0 : m_focusIndex - cols);
            return true;
        case VK_RIGHT:
            if (IconLayout())
                moveFocus(m_focusIndex + 1);
            return true;
        case VK_LEFT:
            if (IconLayout())
                moveFocus(m_focusIndex - 1);
            return true;
        case VK_HOME:
            moveFocus(0);
            return true;
        case VK_END:
            moveFocus(n - 1);
            return true;
        case VK_RETURN:
            if (Cur().sel.size() == 1)
                OpenItem(Cur().sel.front());
            return true;
        case VK_BACK:
            GoBack();
            return true;
        case VK_DELETE:
            DeleteSelection(mods.shift);
            return true;
        case VK_F2:
            StartRename();
            return true;
        case VK_F3:
            m_ui.SetFocus(m_search);
            return true;
        case VK_F5:
            m_dirty = true;
            return true;
        case VK_ESCAPE:
            Cur().sel.clear();
            return true;
        default:
            break;
        }
        return false;
    }

    bool OnChar(wchar_t ch) override
    {
        if (HasDialog())
            return true;
        if (m_ui.FocusedChild())
            return m_ui.OnChar(ch);
        // Быстрый переход по первой букве.
        if (ch > 32 && !m_items.empty())
        {
            const int n = static_cast<int>(m_items.size());
            for (int k = 1; k <= n; ++k)
            {
                const int i = (std::max(0, m_focusIndex) + k) % n;
                if (StartsWithNoCase(m_items[static_cast<size_t>(i)]->name, std::wstring(1, ch)))
                {
                    m_focusIndex = i;
                    Cur().sel = {m_items[static_cast<size_t>(i)]};
                    m_anchor = i;
                    break;
                }
            }
        }
        return true;
    }

    CursorType Cursor(Point local) const override
    {
        if (HasDialog())
            return CursorType::Arrow;
        if (m_address->Visible() && m_address->Bounds().Contains(local))
            return CursorType::IBeam;
        return m_ui.Cursor(local);
    }

    // ------------------------------------------------------------------ поля
    std::vector<Tab> m_tabs;
    int m_cur = 0;
    std::vector<FsNode*> m_items;
    std::map<FsNode*, ViewMode> m_views;
    std::vector<FsNode*> m_pinned;
    unsigned m_fsVersion = 0;
    bool m_dirty = true;
    SortKey m_sortKey = SortKey::Name;
    bool m_sortAsc = true;
    bool m_showNav = true;
    bool m_showDetails = false;
    bool m_pcExpanded = true;
    float m_navScroll = 0.0f;

    TextBox* m_search = nullptr;
    TextBox* m_address = nullptr;
    TextBox* m_rename = nullptr;
    FsNode* m_renameNode = nullptr;
    Button* m_backBtn = nullptr;
    Button* m_fwdBtn = nullptr;
    Button* m_upBtn = nullptr;
    Button* m_detailsBtn = nullptr;
    Button* m_pasteButton = nullptr;
    std::vector<Button*> m_selButtons;
    float m_sep1 = 0.0f, m_sep2 = 0.0f, m_sep3 = 0.0f;

    std::vector<Crumb> m_crumbs;
    std::vector<Rect> m_headerRects;
    int m_hoverItem = -1;
    int m_hoverNav = -1;
    int m_hoverTab = -1;
    bool m_hoverTabClose = false;
    bool m_hoverNewTab = false;
    bool m_hoverAddress = false;
    int m_hoverCrumb = -1;
    bool m_hoverCrumbChevron = false;
    int m_hoverQuick = -1;
    int m_hoverHeader = -1;
    int m_focusIndex = -1;
    int m_anchor = -1;

    Point m_pressPos;
    Point m_dragPos;
    int m_pressItem = -1;
    bool m_banding = false;
    std::vector<FsNode*> m_bandBase;
    bool m_dragging = false;
    bool m_dragCopy = false;
    FsNode* m_dropTarget = nullptr;
    int m_dropItem = -1;
    int m_dropNav = -1;
};
} // namespace

std::unique_ptr<Window> CreateExplorer(const std::wstring& arg) { return std::make_unique<ExplorerWindow>(arg); }
