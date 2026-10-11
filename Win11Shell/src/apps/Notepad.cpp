// Блокнот Windows 11: вкладки, меню «Файл/Правка/Просмотр», редактор с кареткой и выделением,
// буфер обмена, отмена/повтор, поиск и замена, переход к строке, масштаб, перенос по словам,
// строка состояния, сохранение в виртуальную файловую систему и запрос при закрытии.

#include "../Shell.h"
#include "AppFactories.h"
#include "TabStrip.h"

namespace
{
constexpr float kTitleH = 40.0f;
constexpr float kMenuH = 38.0f;
constexpr float kEditY = kTitleH + kMenuH;
constexpr float kStatusH = 30.0f;
constexpr float kPadX = 12.0f;
constexpr float kPadY = 8.0f;
constexpr float kBarW = 12.0f;       // зона полосы прокрутки
constexpr float kBasePt = 11.0f;     // Consolas 11 пт по умолчанию
constexpr int kTabCols = 4;

enum class EditKind
{
    Other,
    Typing,
    Backspace,
    Delete,
};

struct EditOp
{
    size_t pos = 0;
    std::wstring removed;
    std::wstring inserted;
    size_t caretBefore = 0;
    size_t anchorBefore = 0;
    size_t caretAfter = 0;
    double time = 0.0;
    EditKind kind = EditKind::Other;
};

struct Doc
{
    std::wstring text;
    std::wstring savedText;
    FsNode* node = nullptr;
    std::wstring name = L"Без имени";
    size_t caret = 0;
    size_t anchor = 0;
    float desiredX = -1.0f; // желаемая колонка при движении вверх/вниз
    float scrollY = 0.0f;
    float scrollX = 0.0f;
    std::vector<EditOp> undo;
    std::vector<EditOp> redo;
    bool Dirty() const { return text != savedText; }
};

struct VisLine
{
    size_t start = 0;
    size_t end = 0; // без '\n'
};

enum class Overlay
{
    None,
    SaveAs,
    Open,
    GoTo,
};

std::wstring Normalize(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == L'\r')
        {
            out.push_back(L'\n');
            if (i + 1 < s.size() && s[i + 1] == L'\n')
                ++i;
        }
        else if (s[i] != 0)
            out.push_back(s[i]);
    }
    return out;
}

std::wstring ToCrLf(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + s.size() / 16);
    for (wchar_t c : s)
    {
        if (c == L'\n')
            out += L"\r\n";
        else
            out.push_back(c);
    }
    return out;
}

class NotepadWindow : public Window
{
public:
    explicit NotepadWindow(const std::wstring& arg) : Window(AppId::Notepad, L"Блокнот")
    {
        SetInitialSize(980.0f, 640.0f);
        m_minSize = {420.0f, 280.0f};
        m_tabs.reserve(8);
        m_tabs.emplace_back();
        BuildChrome();
        if (!arg.empty())
            OpenArg(arg, true);
        UpdateTitle();
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnLaunchArgs(const std::wstring& arg) override { OpenArg(arg, false); }

    bool OnClose() override
    {
        // Последовательно спросить о каждой вкладке с несохранёнными изменениями.
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            if (m_tabs[i].Dirty())
            {
                SwitchTab(static_cast<int>(i));
                AskSave(static_cast<int>(i), [this]() { Close(); });
                return false;
            }
        }
        return true;
    }

private:
    // ================================================================== вкладки
    Doc& Cur() { return m_tabs[static_cast<size_t>(m_cur)]; }
    const Doc& Cur() const { return m_tabs[static_cast<size_t>(m_cur)]; }

    void UpdateTitle()
    {
        const Doc& d = Cur();
        SetTitle((d.Dirty() ? L"*" : L"") + d.name + L" — Блокнот");
    }

    void OpenArg(const std::wstring& arg, bool initial)
    {
        FsNode* n = GetShell().FS().FromArg(arg);
        if (!n || n->IsFolder())
            return;
        (void)initial;
        OpenNode(n);
    }

    void OpenNode(FsNode* n)
    {
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            if (m_tabs[i].node == n)
            {
                SwitchTab(static_cast<int>(i));
                return;
            }
        }
        Doc d;
        d.node = n;
        d.name = n->name;
        d.text = Normalize(n->content);
        d.savedText = d.text;
        // Пустая нетронутая вкладка «Без имени» заменяется открытым файлом.
        Doc& cur = Cur();
        if (!cur.node && cur.text.empty() && cur.undo.empty())
            cur = std::move(d);
        else
        {
            m_tabs.push_back(std::move(d));
            m_cur = static_cast<int>(m_tabs.size()) - 1;
        }
        Invalidate();
        UpdateTitle();
    }

    void NewTab()
    {
        m_tabs.emplace_back();
        SwitchTab(static_cast<int>(m_tabs.size()) - 1);
    }

    void SwitchTab(int i)
    {
        m_cur = Clamp(i, 0, static_cast<int>(m_tabs.size()) - 1);
        m_ui.SetFocus(nullptr);
        Invalidate();
        UpdateTitle();
    }

    void CloseTab(int i)
    {
        if (i < 0 || i >= static_cast<int>(m_tabs.size()))
            return;
        if (m_tabs[static_cast<size_t>(i)].Dirty())
        {
            SwitchTab(i);
            AskSave(i, [this, i]() { RemoveTab(i); });
            return;
        }
        RemoveTab(i);
    }

    void RemoveTab(int i)
    {
        if (m_tabs.size() <= 1)
        {
            ForceClose();
            return;
        }
        m_tabs.erase(m_tabs.begin() + i);
        if (m_cur >= static_cast<int>(m_tabs.size()))
            m_cur = static_cast<int>(m_tabs.size()) - 1;
        else if (m_cur > i)
            --m_cur;
        SwitchTab(m_cur);
    }

    // Запрос «Сохранить изменения?» для вкладки; then() — продолжение после «Сохранить»/«Не сохранять».
    void AskSave(int index, std::function<void()> then)
    {
        const std::wstring name = m_tabs[static_cast<size_t>(index)].name;
        ShowDialog(L"Блокнот", L"Вы хотите сохранить изменения в «" + name + L"»?",
                   {{L"Сохранить", true}, {L"Не сохранять", false}, {L"Отмена", false}},
                   [this, index, then](int r) {
                       if (r == 2 || index >= static_cast<int>(m_tabs.size()))
                           return;
                       Doc& d = m_tabs[static_cast<size_t>(index)];
                       if (r == 1)
                       {
                           d.savedText = d.text; // изменения отброшены
                           PostAction(then);
                           return;
                       }
                       if (d.node && GetShell().FS().IsAlive(d.node))
                       {
                           SaveDoc(d);
                           PostAction(then);
                       }
                       else
                           ShowSaveAs(then);
                   });
    }

    // ================================================================== сохранение и открытие
    void SaveDoc(Doc& d)
    {
        VirtualFS& fs = GetShell().FS();
        fs.SaveText(d.node, d.text);
        d.savedText = d.text;
        d.name = d.node->name;
        UpdateTitle();
    }

    void Save()
    {
        Doc& d = Cur();
        if (d.node && GetShell().FS().IsAlive(d.node) && !GetShell().FS().InBin(d.node))
            SaveDoc(d);
        else
            ShowSaveAs(nullptr);
    }

    void SaveAll()
    {
        for (Doc& d : m_tabs)
        {
            if (d.Dirty() && d.node && GetShell().FS().IsAlive(d.node))
                SaveDoc(d);
        }
        if (Cur().Dirty())
            ShowSaveAs(nullptr);
    }

    std::wstring SuggestName() const
    {
        const Doc& d = Cur();
        if (d.node)
            return d.node->name;
        // Как в новом Блокноте: имя по первой строке текста.
        std::wstring first;
        for (wchar_t c : d.text)
        {
            if (c == L'\n')
            {
                if (!Trim(first).empty())
                    break;
                first.clear();
                continue;
            }
            if (std::wstring(L"\\/:*?\"<>|\t").find(c) == std::wstring::npos)
                first.push_back(c);
            if (first.size() > 40)
                break;
        }
        first = Trim(first);
        return (first.empty() ? std::wstring(L"Без имени") : first) + L".txt";
    }

    std::vector<FsNode*> SaveFolders() const
    {
        VirtualFS& fs = GetShell().FS();
        std::vector<FsNode*> v = {fs.Documents(), fs.Desktop(), fs.Downloads()};
        const Doc& d = Cur();
        if (d.node && d.node->parent && std::find(v.begin(), v.end(), d.node->parent) == v.end() && fs.IsAlive(d.node->parent))
            v.insert(v.begin(), d.node->parent);
        return v;
    }

    void ShowSaveAs(std::function<void()> then)
    {
        OpenOverlay(Overlay::SaveAs);
        m_afterSave = std::move(then);
        m_overlayFolders = SaveFolders();
        std::vector<std::wstring> names;
        int sel = 0;
        for (size_t i = 0; i < m_overlayFolders.size(); ++i)
        {
            names.push_back(m_overlayFolders[i]->name);
            if (Cur().node && m_overlayFolders[i] == Cur().node->parent)
                sel = static_cast<int>(i);
        }
        m_ovCombo->items = names;
        m_ovCombo->selected = sel;
        m_ovCombo->SetVisible(true);
        m_ovEdit->SetText(SuggestName());
        std::wstring ext;
        const std::wstring base = VirtualFS::SplitExtension(m_ovEdit->Text(), ext);
        m_ovEdit->Select(0, static_cast<int>(base.size()));
        m_ovOk->text = L"Сохранить";
        LayoutOverlay();
    }

    void CommitSaveAs()
    {
        VirtualFS& fs = GetShell().FS();
        std::wstring name = Trim(m_ovEdit->Text());
        if (name.empty())
            return;
        std::wstring ext;
        VirtualFS::SplitExtension(name, ext);
        if (ext.empty())
            name += L".txt";
        if (!VirtualFS::ValidName(name))
        {
            CloseOverlay();
            ShowDialog(L"Сохранение", L"Имя файла не должно содержать следующих знаков: \\ / : * ? \" < > |", {{L"ОК", true}},
                       nullptr);
            return;
        }
        const int fi = Clamp(m_ovCombo->selected, 0, static_cast<int>(m_overlayFolders.size()) - 1);
        FsNode* dir = m_overlayFolders[static_cast<size_t>(fi)];
        std::function<void()> then = m_afterSave;
        m_afterSave = nullptr;
        CloseOverlay();
        auto finish = [this, then](FsNode* target) {
            Doc& d = Cur();
            d.node = target;
            SaveDoc(d);
            if (then)
                PostAction(then);
        };
        if (FsNode* existing = fs.Find(dir, name))
        {
            if (existing->IsFolder())
                return;
            ShowDialog(L"Подтверждение сохранения", L"Файл «" + name + L"» уже существует.\nВы хотите заменить его?",
                       {{L"Да", true}, {L"Нет", false}}, [existing, finish](int r) {
                           if (r == 0)
                               finish(existing);
                       });
            return;
        }
        finish(fs.NewFile(dir, name, FileKind::Text, L""));
    }

    void ShowOpen()
    {
        OpenOverlay(Overlay::Open);
        m_openFiles.clear();
        VirtualFS& fs = GetShell().FS();
        std::function<void(FsNode*)> collect = [&](FsNode* dir) {
            for (auto& c : dir->children)
            {
                if (c->IsFolder())
                    collect(c.get());
                else if (c->kind == FileKind::Text)
                    m_openFiles.push_back(c.get());
            }
        };
        collect(fs.Home());
        std::sort(m_openFiles.begin(), m_openFiles.end(), [](const FsNode* a, const FsNode* b) {
            FILETIME fa{}, fb{};
            SystemTimeToFileTime(&a->modified, &fa);
            SystemTimeToFileTime(&b->modified, &fb);
            return CompareFileTime(&fa, &fb) > 0;
        });
        m_openSel = m_openFiles.empty() ? -1 : 0;
        m_openScroll = 0.0f;
        m_ovCombo->SetVisible(false);
        m_ovEdit->SetVisible(false);
        m_ovUi.SetFocus(nullptr);
        m_ovOk->text = L"Открыть";
        LayoutOverlay();
    }

    void CommitOpen()
    {
        if (m_openSel < 0 || m_openSel >= static_cast<int>(m_openFiles.size()))
            return;
        FsNode* n = m_openFiles[static_cast<size_t>(m_openSel)];
        CloseOverlay();
        OpenNode(n);
    }

    void ShowGoTo()
    {
        OpenOverlay(Overlay::GoTo);
        m_ovCombo->SetVisible(false);
        m_ovEdit->SetText(std::to_wstring(CaretLogicalLine() + 1));
        m_ovEdit->SelectAll();
        m_ovOk->text = L"Перейти";
        LayoutOverlay();
    }

    void CommitGoTo()
    {
        const std::wstring s = Trim(m_ovEdit->Text());
        long long line = 0;
        for (wchar_t c : s)
        {
            if (c < L'0' || c > L'9')
            {
                line = -1;
                break;
            }
            line = std::min(line * 10 + (c - L'0'), 100000000LL);
        }
        CloseOverlay();
        const long long total = static_cast<long long>(LogicalLineCount());
        if (line < 1 || line > total)
        {
            ShowDialog(L"Блокнот — переход на строку", L"Номер строки выходит за пределы общего числа строк.", {{L"ОК", true}},
                       nullptr);
            return;
        }
        Doc& d = Cur();
        size_t pos = 0;
        for (long long k = 1; k < line; ++k)
        {
            const size_t nl = d.text.find(L'\n', pos);
            if (nl == std::wstring::npos)
                break;
            pos = nl + 1;
        }
        d.caret = d.anchor = pos;
        d.desiredX = -1.0f;
        EnsureCaretVisible();
    }

    // ---- общий оверлей (диалог внутри окна) ------------------------------------------
    void OpenOverlay(Overlay o)
    {
        m_overlay = o;
        m_overlayAnim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
        m_ovEdit->SetVisible(true);
        m_ovUi.SetFocus(m_ovEdit);
    }

    void CloseOverlay()
    {
        m_overlay = Overlay::None;
        m_ovUi.SetFocus(nullptr);
        m_afterSave = nullptr;
    }

    Rect OverlayRect() const
    {
        const float w = std::min(m_overlay == Overlay::Open ? 520.0f : 440.0f, Width() - 40.0f);
        float h = 200.0f;
        if (m_overlay == Overlay::SaveAs)
            h = 300.0f;
        else if (m_overlay == Overlay::Open)
            h = 420.0f;
        h = std::min(h, Height() - 40.0f);
        return {std::floor((Width() - w) * 0.5f), std::floor(kTitleH + (Height() - kTitleH - h) * 0.45f), w, h};
    }

    Rect OpenListRect() const
    {
        const Rect d = OverlayRect();
        return {d.x + 24.0f, d.y + 64.0f, d.w - 48.0f, d.h - 64.0f - 96.0f};
    }

    void LayoutOverlay()
    {
        const Rect d = OverlayRect();
        if (m_overlay == Overlay::SaveAs)
        {
            m_ovCombo->SetBounds({d.x + 24.0f, d.y + 86.0f, d.w - 48.0f, 32.0f});
            m_ovEdit->SetBounds({d.x + 24.0f, d.y + 150.0f, d.w - 48.0f, 32.0f});
        }
        else
            m_ovEdit->SetBounds({d.x + 24.0f, d.y + 88.0f, d.w - 48.0f, 32.0f});
        const float bw = (d.w - 48.0f - 8.0f) * 0.5f;
        m_ovOk->SetBounds({d.x + 24.0f, d.Bottom() - 56.0f, bw, 32.0f});
        m_ovCancel->SetBounds({d.x + 32.0f + bw, d.Bottom() - 56.0f, bw, 32.0f});
    }

    void CommitOverlay()
    {
        switch (m_overlay)
        {
        case Overlay::SaveAs:
            CommitSaveAs();
            break;
        case Overlay::Open:
            CommitOpen();
            break;
        case Overlay::GoTo:
            CommitGoTo();
            break;
        case Overlay::None:
            break;
        }
    }

    void DrawOverlay(Renderer& r)
    {
        if (m_overlay == Overlay::None)
            return;
        const Palette& p = Theme::P();
        const float t = m_overlayAnim.Value();
        r.FillRect({0.0f, kTitleH, Width(), Height() - kTitleH}, p.smoke.MulAlpha(t));
        r.PushOpacity(t);
        const Rect d = OverlayRect();
        r.Shadow(d, 8.0f, 28.0f, 10.0f, p.dark ? 0.5f : 0.3f);
        r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.FillRoundRect({d.x, d.Bottom() - 80.0f, d.w, 80.0f}, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
        r.FillRect({d.x, d.Bottom() - 80.0f, d.w, 1.0f}, p.divider);
        r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
        const wchar_t* title = L"";
        switch (m_overlay)
        {
        case Overlay::SaveAs:
            title = L"Сохранить как";
            r.Text(L"Папка", {d.x + 24.0f, d.y + 60.0f, d.w - 48.0f, 20.0f}, p.text, 14.0f);
            r.Text(L"Имя файла", {d.x + 24.0f, d.y + 124.0f, d.w - 48.0f, 20.0f}, p.text, 14.0f);
            r.Text(L"Тип файла: Текстовые документы (*.txt)    Кодировка: UTF-8", {d.x + 24.0f, d.y + 190.0f, d.w - 48.0f, 20.0f},
                   p.textSecondary, 12.0f);
            break;
        case Overlay::GoTo:
            title = L"Переход на строку";
            r.Text(L"Номер строки:", {d.x + 24.0f, d.y + 62.0f, d.w - 48.0f, 20.0f}, p.text, 14.0f);
            break;
        case Overlay::Open:
        {
            title = L"Открыть";
            const Rect lr = OpenListRect();
            r.FillRoundRect(lr, 4.0f, p.dark ? Color(255, 255, 255, 0x08) : Color(0, 0, 0, 0x05));
            r.StrokeRoundRect(lr, 4.0f, p.controlStroke);
            r.PushClip(lr.Inset(1.0f, 1.0f, 1.0f, 1.0f));
            if (m_openFiles.empty())
                r.Text(L"Текстовые документы не найдены.", lr, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
            VirtualFS& fs = GetShell().FS();
            for (int i = 0; i < static_cast<int>(m_openFiles.size()); ++i)
            {
                const Rect row{lr.x + 4.0f, lr.y + 4.0f + static_cast<float>(i) * 40.0f - m_openScroll, lr.w - 8.0f, 38.0f};
                if (row.Bottom() < lr.y || row.y > lr.Bottom())
                    continue;
                FsNode* n = m_openFiles[static_cast<size_t>(i)];
                if (i == m_openSel)
                    r.FillRoundRect(row, 4.0f, p.selection);
                else if (i == m_openHover)
                    r.FillRoundRect(row, 4.0f, p.subtleHover);
                r.DrawAppIcon(AppIcon::FileText, {row.x + 8.0f, row.CenterY() - 10.0f, 20.0f, 20.0f});
                r.Text(n->name, {row.x + 38.0f, row.y + 2.0f, row.w - 46.0f, 18.0f}, p.text, 13.0f);
                r.Text(fs.PathOf(n->parent) + L"  ·  " + FormatDateTime(n->modified), {row.x + 38.0f, row.y + 19.0f, row.w - 46.0f, 16.0f},
                       p.textSecondary, 11.0f);
            }
            r.PopClip();
            break;
        }
        case Overlay::None:
            break;
        }
        r.Text(title, {d.x + 24.0f, d.y + 20.0f, d.w - 48.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold);
        m_ovOk->SetEnabled(m_overlay != Overlay::Open || m_openSel >= 0);
        m_ovUi.Draw(r);
        r.PopOpacity();
    }

    int OpenRowAt(Point pt) const
    {
        const Rect lr = OpenListRect();
        if (!lr.Contains(pt))
            return -1;
        const int i = static_cast<int>(std::floor((pt.y - lr.y - 4.0f + m_openScroll) / 40.0f));
        return i >= 0 && i < static_cast<int>(m_openFiles.size()) ? i : -1;
    }

    // ================================================================== меню и интерфейс
    void BuildChrome()
    {
        auto menu = [&](const wchar_t* caption, float x, float w, int which) {
            auto* b = m_ui.Add<Button>(caption, Icon::None, ButtonStyle::Subtle);
            b->fontSize = 14.0f;
            b->SetBounds({x, kTitleH + 3.0f, w, 32.0f});
            b->onClick = [this, b, which]() { ShowMainMenu(which, LocalToScreen(b->Bounds())); };
            m_menuButtons.push_back(b);
        };
        menu(L"Файл", 6.0f, 56.0f, 0);
        menu(L"Правка", 64.0f, 70.0f, 1);
        menu(L"Просмотр", 136.0f, 86.0f, 2);
        m_gear = m_ui.Add<Button>(L"", Icon::Settings, ButtonStyle::Subtle);
        m_gear->SetTooltip(L"Параметры");
        m_gear->onClick = [this]() { ShowSettingsMenu(LocalToScreen(m_gear->Bounds())); };

        // Панель поиска и замены.
        m_findBox = m_ui.Add<TextBox>(L"Найти");
        m_findBox->fontSize = 14.0f;
        m_findBox->onEnter = [this]() { FindNext(!CurrentMods().shift); };
        m_findBox->onEscape = [this]() { HideFind(); };
        m_findBox->onChange = [this](const std::wstring&) { m_findFailed = false; };
        m_replaceBox = m_ui.Add<TextBox>(L"Заменить");
        m_replaceBox->fontSize = 14.0f;
        m_replaceBox->onEnter = [this]() { ReplaceOne(); };
        m_replaceBox->onEscape = [this]() { HideFind(); };
        m_findToggle = m_ui.Add<Button>(L"", Icon::ChevronRight, ButtonStyle::Subtle);
        m_findToggle->SetTooltip(L"Переключение замены");
        m_findToggle->iconSize = 12.0f;
        m_findToggle->onClick = [this]() { ShowFind(!m_replaceMode); };
        m_findNext = m_ui.Add<Button>(L"", Icon::ArrowDown, ButtonStyle::Subtle);
        m_findNext->SetTooltip(L"Найти далее (F3)");
        m_findNext->onClick = [this]() { FindNext(true); };
        m_findPrev = m_ui.Add<Button>(L"", Icon::ArrowUp, ButtonStyle::Subtle);
        m_findPrev->SetTooltip(L"Найти ранее (Shift+F3)");
        m_findPrev->onClick = [this]() { FindNext(false); };
        m_findOpts = m_ui.Add<Button>(L"", Icon::Settings, ButtonStyle::Subtle);
        m_findOpts->SetTooltip(L"Параметры поиска");
        m_findOpts->onClick = [this]() {
            std::vector<MenuItem> items = {
                MenuItem::Check(L"С учётом регистра", m_matchCase, [this]() { m_matchCase = !m_matchCase; }),
                MenuItem::Check(L"Циклический поиск", m_wrapAround, [this]() { m_wrapAround = !m_wrapAround; }),
            };
            GetShell().ShowMenuBelow(LocalToScreen(m_findOpts->Bounds()), std::move(items), 200.0f);
        };
        m_findClose = m_ui.Add<Button>(L"", Icon::Close, ButtonStyle::Subtle);
        m_findClose->SetTooltip(L"Закрыть (Esc)");
        m_findClose->iconSize = 12.0f;
        m_findClose->onClick = [this]() { HideFind(); };
        m_replaceBtn = m_ui.Add<Button>(L"Заменить", Icon::None, ButtonStyle::Standard);
        m_replaceBtn->onClick = [this]() { ReplaceOne(); };
        m_replaceAllBtn = m_ui.Add<Button>(L"Заменить все", Icon::None, ButtonStyle::Standard);
        m_replaceAllBtn->onClick = [this]() { ReplaceAll(); };
        SetFindVisible(false);

        // Оверлей
        m_ovCombo = m_ovUi.Add<ComboBox>();
        m_ovEdit = m_ovUi.Add<TextBox>();
        m_ovEdit->onEnter = [this]() { CommitOverlay(); };
        m_ovEdit->onEscape = [this]() { CloseOverlay(); };
        m_ovOk = m_ovUi.Add<Button>(L"ОК", Icon::None, ButtonStyle::Accent);
        m_ovOk->onClick = [this]() { CommitOverlay(); };
        m_ovCancel = m_ovUi.Add<Button>(L"Отмена", Icon::None, ButtonStyle::Standard);
        m_ovCancel->onClick = [this]() { CloseOverlay(); };
    }

    Rect FindRect() const
    {
        const Rect e = EditorRect();
        const float w = std::min(470.0f, e.w - 24.0f);
        return {e.Right() - w - 20.0f, e.y + 8.0f, w, m_replaceMode ? 92.0f : 50.0f};
    }

    void SetFindVisible(bool v)
    {
        m_findVisible = v;
        for (Widget* w : std::initializer_list<Widget*>{m_findBox, m_findToggle, m_findNext, m_findPrev, m_findOpts, m_findClose})
            w->SetVisible(v);
        for (Widget* w : std::initializer_list<Widget*>{m_replaceBox, m_replaceBtn, m_replaceAllBtn})
            w->SetVisible(v && m_replaceMode);
        LayoutFind();
    }

    void LayoutFind()
    {
        const Rect f = FindRect();
        const float x0 = f.x + 6.0f;
        m_findToggle->SetBounds({x0, f.y + 9.0f, 28.0f, 32.0f});
        const float bx = f.Right() - 8.0f - 4.0f * 36.0f;
        m_findBox->SetBounds({x0 + 32.0f, f.y + 9.0f, bx - x0 - 36.0f, 32.0f});
        m_findNext->SetBounds({bx, f.y + 9.0f, 34.0f, 32.0f});
        m_findPrev->SetBounds({bx + 36.0f, f.y + 9.0f, 34.0f, 32.0f});
        m_findOpts->SetBounds({bx + 72.0f, f.y + 9.0f, 34.0f, 32.0f});
        m_findClose->SetBounds({bx + 108.0f, f.y + 9.0f, 34.0f, 32.0f});
        const float rw = std::min(240.0f, f.w - 140.0f);
        m_replaceBox->SetBounds({x0 + 32.0f, f.y + 51.0f, rw, 32.0f});
        const float bw = (f.Right() - 8.0f - (x0 + 36.0f + rw) - 4.0f) * 0.5f;
        m_replaceBtn->SetBounds({x0 + 36.0f + rw, f.y + 51.0f, bw, 32.0f});
        m_replaceAllBtn->SetBounds({x0 + 40.0f + rw + bw, f.y + 51.0f, bw, 32.0f});
        m_replaceBtn->fontSize = m_replaceAllBtn->fontSize = 12.0f;
        m_findToggle->icon = m_replaceMode ? Icon::ChevronDown : Icon::ChevronRight;
    }

    void ShowFind(bool replace)
    {
        m_replaceMode = replace;
        SetFindVisible(true);
        // Выделенный фрагмент (в пределах строки) — в поле поиска.
        const Doc& d = Cur();
        if (d.caret != d.anchor)
        {
            const std::wstring sel = SelectedText();
            if (sel.find(L'\n') == std::wstring::npos && sel.size() < 200)
                m_findBox->SetText(sel);
        }
        m_ui.SetFocus(replace && !m_findBox->Text().empty() ? m_replaceBox : m_findBox);
        if (m_ui.FocusedChild() == m_findBox)
            m_findBox->SelectAll();
    }

    void HideFind()
    {
        SetFindVisible(false);
        m_ui.SetFocus(nullptr);
    }

    void OnResize() override
    {
        if (m_gear)
            m_gear->SetBounds({Width() - 48.0f, kTitleH + 3.0f, 40.0f, 32.0f});
        if (m_findBox)
            LayoutFind();
        if (m_overlay != Overlay::None)
            LayoutOverlay();
        Invalidate();
    }

    void ShowMainMenu(int which, const Rect& anchor)
    {
        std::vector<MenuItem> items;
        const Doc& d = Cur();
        const bool hasSel = d.caret != d.anchor;
        if (which == 0)
        {
            items.push_back(MenuItem(L"Новая вкладка", Icon::None, [this]() { NewTab(); }, L"Ctrl+N"));
            items.push_back(MenuItem(L"Новое окно", Icon::None, []() { GetShell().WM().Add(CreateNotepad(L"")); }, L"Ctrl+Shift+N"));
            items.push_back(MenuItem(L"Открыть", Icon::None, [this]() { ShowOpen(); }, L"Ctrl+O"));
            std::vector<MenuItem> recent;
            for (FsNode* n : RecentFiles())
                recent.push_back(MenuItem(n->name, Icon::None, [this, n]() {
                    if (GetShell().FS().IsAlive(n))
                        OpenNode(n);
                }));
            if (recent.empty())
                recent.push_back(MenuItem(L"Нет последних файлов", Icon::None, nullptr).Disabled());
            items.push_back(MenuItem::Sub(L"Последние", Icon::None, recent));
            items.push_back(MenuItem(L"Сохранить", Icon::None, [this]() { Save(); }, L"Ctrl+S"));
            items.push_back(MenuItem(L"Сохранить как", Icon::None, [this]() { ShowSaveAs(nullptr); }, L"Ctrl+Shift+S"));
            items.push_back(MenuItem(L"Сохранить все", Icon::None, [this]() { SaveAll(); }, L"Ctrl+Alt+S"));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Параметры страницы", Icon::None, nullptr).Disabled());
            items.push_back(MenuItem(L"Печать", Icon::None, nullptr, L"Ctrl+P").Disabled());
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Закрыть вкладку", Icon::None, [this]() { CloseTab(m_cur); }, L"Ctrl+W"));
            items.push_back(MenuItem(L"Закрыть окно", Icon::None, [this]() { Close(); }, L"Ctrl+Shift+W"));
            items.push_back(MenuItem(L"Выход", Icon::None, [this]() { Close(); }));
        }
        else if (which == 1)
        {
            items.push_back(MenuItem(L"Отменить", Icon::None, [this]() { Undo(); }, L"Ctrl+Z").Disabled(d.undo.empty()));
            items.push_back(MenuItem(L"Вернуть", Icon::None, [this]() { Redo(); }, L"Ctrl+Y").Disabled(d.redo.empty()));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Вырезать", Icon::None, [this]() { Cut(); }, L"Ctrl+X").Disabled(!hasSel));
            items.push_back(MenuItem(L"Копировать", Icon::None, [this]() { Copy(); }, L"Ctrl+C").Disabled(!hasSel));
            items.push_back(MenuItem(L"Вставить", Icon::None, [this]() { Paste(); }, L"Ctrl+V"));
            items.push_back(MenuItem(L"Удалить", Icon::None, [this]() { DeleteSelection(); }, L"Del").Disabled(!hasSel));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Поиск в Интернете", Icon::None, [this]() { WebSearch(); }, L"Ctrl+E").Disabled(!hasSel));
            items.push_back(MenuItem(L"Найти", Icon::None, [this]() { ShowFind(false); }, L"Ctrl+F"));
            items.push_back(MenuItem(L"Найти далее", Icon::None, [this]() { FindNext(true); }, L"F3"));
            items.push_back(MenuItem(L"Найти ранее", Icon::None, [this]() { FindNext(false); }, L"Shift+F3"));
            items.push_back(MenuItem(L"Заменить", Icon::None, [this]() { ShowFind(true); }, L"Ctrl+H"));
            items.push_back(MenuItem(L"Перейти", Icon::None, [this]() { ShowGoTo(); }, L"Ctrl+G"));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Выделить все", Icon::None, [this]() { SelectAll(); }, L"Ctrl+A"));
            items.push_back(MenuItem(L"Время и дата", Icon::None, [this]() { InsertDateTime(); }, L"F5"));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Шрифт", Icon::None, [this]() { ShowSettingsMenu(LocalToScreen(m_gear->Bounds())); }));
        }
        else
        {
            std::vector<MenuItem> zoom = {
                MenuItem(L"Увеличить", Icon::ZoomIn, [this]() { Zoom(+1); }, L"Ctrl+Плюс"),
                MenuItem(L"Уменьшить", Icon::ZoomOut, [this]() { Zoom(-1); }, L"Ctrl+Минус"),
                MenuItem(L"Восстановить масштаб по умолчанию", Icon::None, [this]() { Zoom(0); }, L"Ctrl+0"),
            };
            items.push_back(MenuItem::Sub(L"Масштаб", Icon::ZoomIn, zoom));
            items.push_back(MenuItem::Check(L"Строка состояния", m_statusBar, [this]() {
                m_statusBar = !m_statusBar;
                OnResize();
            }));
            items.push_back(MenuItem::Check(L"Перенос по словам", m_wrap, [this]() {
                m_wrap = !m_wrap;
                Cur().scrollX = 0.0f;
                Invalidate();
                EnsureCaretVisible();
            }));
        }
        GetShell().ShowMenuBelow(anchor, std::move(items), which == 2 ? 230.0f : 260.0f);
    }

    void ShowSettingsMenu(const Rect& anchor)
    {
        std::vector<MenuItem> fonts = {
            MenuItem::Check(L"Consolas", m_face == FontFace::Mono, [this]() { SetFace(FontFace::Mono); }, true),
            MenuItem::Check(L"Cascadia Mono", m_face == FontFace::Terminal, [this]() { SetFace(FontFace::Terminal); }, true),
        };
        std::vector<MenuItem> sizes;
        for (int pt : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28})
        {
            sizes.push_back(MenuItem::Check(std::to_wstring(pt), std::fabs(m_pt - static_cast<float>(pt)) < 0.1f, [this, pt]() {
                m_pt = static_cast<float>(pt);
                Invalidate();
                EnsureCaretVisible();
            }, true));
        }
        std::vector<MenuItem> items = {
            MenuItem::Sub(L"Семейство шрифтов", Icon::TextTool, fonts),
            MenuItem::Sub(L"Размер шрифта", Icon::None, sizes),
            MenuItem::Sep(),
            MenuItem::Check(L"Перенос по словам", m_wrap, [this]() {
                m_wrap = !m_wrap;
                Cur().scrollX = 0.0f;
                Invalidate();
            }),
            MenuItem::Check(L"Строка состояния", m_statusBar, [this]() {
                m_statusBar = !m_statusBar;
                OnResize();
            }),
            MenuItem::Sep(),
            MenuItem(L"О Блокноте", Icon::Info, [this]() {
                ShowDialog(L"О Блокноте", L"Блокнот — демонстрационная копия текстового редактора Windows 11.\n"
                                          L"Файлы сохраняются в виртуальной файловой системе.",
                           {{L"ОК", true}}, nullptr);
            }),
        };
        GetShell().ShowMenuBelow(anchor, std::move(items), 240.0f);
    }

    void SetFace(FontFace f)
    {
        m_face = f;
        Invalidate();
        EnsureCaretVisible();
    }

    std::vector<FsNode*> RecentFiles() const
    {
        std::vector<FsNode*> v;
        for (FsNode* n : GetShell().FS().Recent(20))
        {
            if (n->kind == FileKind::Text && v.size() < 8)
                v.push_back(n);
        }
        return v;
    }

    // ================================================================== метрики и раскладка
    float FontSize() const { return m_pt * 96.0f / 72.0f * static_cast<float>(m_zoom) / 100.0f; }
    float CharW() const
    {
        Renderer& r = Renderer::Get();
        const float fs = FontSize();
        if (fs != m_cwSize || m_face != m_cwFace || r.Scale() != m_cwScale)
        {
            m_cwSize = fs;
            m_cwFace = m_face;
            m_cwScale = r.Scale();
            m_cw = r.MonoCharWidth(fs, m_face);
            m_lh = std::ceil(r.LineHeight(fs, FontWeight::Regular, m_face));
        }
        return m_cw;
    }
    float LineH() const
    {
        CharW();
        return m_lh;
    }
    Rect EditorRect() const { return {0.0f, kEditY, Width(), Height() - kEditY - (m_statusBar ? kStatusH : 0.0f)}; }
    Rect TextRect() const
    {
        const Rect e = EditorRect();
        return {e.x + kPadX, e.y + kPadY, e.w - kPadX - kBarW, e.h - kPadY - (m_wrap ? 0.0f : kBarW)};
    }

    void Invalidate() { m_layoutValid = false; }

    void Layout() const
    {
        const Doc& d = Cur();
        const float cw = CharW();
        const Rect tr = TextRect();
        const int maxCols = m_wrap ? std::max(8, static_cast<int>(std::floor((tr.w - 4.0f) / cw))) : 0x3FFFFFFF;
        if (m_layoutValid && m_layoutDoc == &d && m_layoutText == d.text.size() && m_layoutCols == maxCols &&
            m_layoutHash == m_editSerial)
            return;
        m_layoutValid = true;
        m_layoutDoc = &d;
        m_layoutText = d.text.size();
        m_layoutCols = maxCols;
        m_layoutHash = m_editSerial;
        m_lines.clear();
        m_maxCols = 0;
        const std::wstring& t = d.text;
        size_t ls = 0;
        for (;;)
        {
            size_t le = t.find(L'\n', ls);
            if (le == std::wstring::npos)
                le = t.size();
            size_t s = ls;
            for (;;)
            {
                int col = 0;
                size_t i = s;
                size_t lastBreak = std::wstring::npos;
                while (i < le)
                {
                    const int w = t[i] == L'\t' ? kTabCols - col % kTabCols : 1;
                    if (col + w > maxCols)
                        break;
                    col += w;
                    if (t[i] == L' ' || t[i] == L'\t')
                        lastBreak = i + 1;
                    ++i;
                }
                if (i >= le)
                {
                    m_lines.push_back({s, le});
                    m_maxCols = std::max(m_maxCols, col);
                    break;
                }
                size_t brk = (lastBreak != std::wstring::npos && lastBreak > s) ? lastBreak : i;
                if (brk <= s)
                    brk = s + 1;
                m_lines.push_back({s, brk});
                m_maxCols = std::max(m_maxCols, maxCols);
                s = brk;
            }
            if (le >= t.size())
                break;
            ls = le + 1;
        }
    }

    int LineOf(size_t off) const
    {
        Layout();
        int lo = 0, hi = static_cast<int>(m_lines.size()) - 1;
        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            if (m_lines[static_cast<size_t>(mid)].start <= off)
                lo = mid;
            else
                hi = mid - 1;
        }
        return lo;
    }

    int ColOf(const VisLine& l, size_t off) const
    {
        const std::wstring& t = Cur().text;
        int col = 0;
        for (size_t i = l.start; i < off && i < l.end; ++i)
            col += t[i] == L'\t' ? kTabCols - col % kTabCols : 1;
        return col;
    }

    // Конец визуальной строки, на который можно поставить каретку.
    size_t LineCaretEnd(int li) const
    {
        const VisLine& l = m_lines[static_cast<size_t>(li)];
        const std::wstring& t = Cur().text;
        const bool wrapped = l.end < t.size() && t[l.end] != L'\n';
        return wrapped && l.end > l.start ? l.end - 1 : l.end;
    }

    size_t OffsetAtCol(int li, float col) const
    {
        const VisLine& l = m_lines[static_cast<size_t>(li)];
        const std::wstring& t = Cur().text;
        int c = 0;
        for (size_t i = l.start; i < l.end; ++i)
        {
            const int w = t[i] == L'\t' ? kTabCols - c % kTabCols : 1;
            if (col < static_cast<float>(c) + static_cast<float>(w) * 0.5f)
                return i;
            c += w;
        }
        return LineCaretEnd(li);
    }

    size_t OffsetAt(Point pt) const
    {
        Layout();
        const Rect tr = TextRect();
        const float lh = LineH();
        const Doc& d = Cur();
        const int n = static_cast<int>(m_lines.size());
        const int li = Clamp(static_cast<int>(std::floor((pt.y - tr.y + d.scrollY) / lh)), 0, n - 1);
        return OffsetAtCol(li, std::max(0.0f, (pt.x - tr.x + d.scrollX) / CharW()));
    }

    float ContentHeight() const
    {
        Layout();
        return static_cast<float>(m_lines.size()) * LineH();
    }
    float MaxScrollY() const
    {
        const Rect tr = TextRect();
        return std::max(0.0f, ContentHeight() - tr.h + kPadY);
    }
    float ContentWidth() const
    {
        Layout();
        return static_cast<float>(m_maxCols) * CharW() + 8.0f;
    }
    float MaxScrollX() const { return m_wrap ? 0.0f : std::max(0.0f, ContentWidth() - TextRect().w); }

    void ClampScroll()
    {
        Doc& d = Cur();
        d.scrollY = Clamp(d.scrollY, 0.0f, MaxScrollY());
        d.scrollX = Clamp(d.scrollX, 0.0f, MaxScrollX());
    }

    void EnsureCaretVisible()
    {
        Layout();
        Doc& d = Cur();
        const Rect tr = TextRect();
        const float lh = LineH();
        const int li = LineOf(d.caret);
        const float y = static_cast<float>(li) * lh;
        if (y < d.scrollY)
            d.scrollY = y;
        else if (y + lh > d.scrollY + tr.h)
            d.scrollY = y + lh - tr.h;
        if (!m_wrap)
        {
            const float x = static_cast<float>(ColOf(m_lines[static_cast<size_t>(li)], d.caret)) * CharW();
            const float margin = std::min(tr.w * 0.25f, 60.0f);
            if (x < d.scrollX)
                d.scrollX = std::max(0.0f, x - margin);
            else if (x > d.scrollX + tr.w - 4.0f)
                d.scrollX = x - tr.w + margin;
        }
        ClampScroll();
        m_blinkStart = NowSeconds();
    }

    size_t LogicalLineCount() const
    {
        const std::wstring& t = Cur().text;
        return static_cast<size_t>(std::count(t.begin(), t.end(), L'\n')) + 1;
    }
    size_t CaretLogicalLine() const
    {
        const Doc& d = Cur();
        return static_cast<size_t>(std::count(d.text.begin(), d.text.begin() + static_cast<std::ptrdiff_t>(d.caret), L'\n'));
    }
    size_t CaretLogicalCol() const
    {
        const Doc& d = Cur();
        const size_t ls = d.caret == 0 ? 0 : d.text.rfind(L'\n', d.caret - 1);
        return ls == std::wstring::npos ? d.caret : (d.caret == 0 ? 0 : d.caret - ls - 1);
    }

    // ================================================================== правка
    std::wstring SelectedText() const
    {
        const Doc& d = Cur();
        const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
        return d.text.substr(a, b - a);
    }

    void Edit(size_t pos, size_t removeLen, const std::wstring& ins, EditKind kind)
    {
        Doc& d = Cur();
        pos = std::min(pos, d.text.size());
        removeLen = std::min(removeLen, d.text.size() - pos);
        if (removeLen == 0 && ins.empty())
            return;
        EditOp op;
        op.pos = pos;
        op.removed = d.text.substr(pos, removeLen);
        op.inserted = ins;
        op.caretBefore = d.caret;
        op.anchorBefore = d.anchor;
        op.time = NowSeconds();
        op.kind = kind;
        d.text.replace(pos, removeLen, ins);
        d.caret = d.anchor = pos + ins.size();
        op.caretAfter = d.caret;
        bool merged = false;
        if (!d.undo.empty() && kind != EditKind::Other)
        {
            EditOp& last = d.undo.back();
            const bool recent = op.time - last.time < 2.0;
            if (recent && last.kind == kind)
            {
                if (kind == EditKind::Typing && op.removed.empty() && last.pos + last.inserted.size() == pos && ins != L"\n" &&
                    !(ins == L" " && !last.inserted.empty() && last.inserted.back() != L' '))
                {
                    last.inserted += ins;
                    merged = true;
                }
                else if (kind == EditKind::Backspace && last.inserted.empty() && op.pos + op.removed.size() == last.pos)
                {
                    last.pos = op.pos;
                    last.removed = op.removed + last.removed;
                    merged = true;
                }
                else if (kind == EditKind::Delete && last.inserted.empty() && op.pos == last.pos)
                {
                    last.removed += op.removed;
                    merged = true;
                }
                if (merged)
                {
                    last.caretAfter = d.caret;
                    last.time = op.time;
                }
            }
        }
        if (!merged)
        {
            d.undo.push_back(std::move(op));
            if (d.undo.size() > 1000)
                d.undo.erase(d.undo.begin());
        }
        d.redo.clear();
        Changed();
    }

    void Changed()
    {
        ++m_editSerial;
        Invalidate();
        Cur().desiredX = -1.0f;
        EnsureCaretVisible();
        UpdateTitle();
    }

    void InsertText(const std::wstring& s, EditKind kind = EditKind::Other)
    {
        Doc& d = Cur();
        const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
        Edit(a, b - a, s, b > a ? EditKind::Other : kind);
    }

    void DeleteSelection()
    {
        Doc& d = Cur();
        if (d.caret == d.anchor)
            return;
        const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
        Edit(a, b - a, L"", EditKind::Other);
    }

    void Undo()
    {
        Doc& d = Cur();
        if (d.undo.empty())
            return;
        EditOp op = d.undo.back();
        d.undo.pop_back();
        d.text.replace(op.pos, op.inserted.size(), op.removed);
        d.caret = std::min(op.caretBefore, d.text.size());
        d.anchor = std::min(op.anchorBefore, d.text.size());
        d.redo.push_back(std::move(op));
        if (!d.undo.empty())
            d.undo.back().time = 0.0; // новый ввод не сливается с более старой правкой
        Changed();
    }

    void Redo()
    {
        Doc& d = Cur();
        if (d.redo.empty())
            return;
        EditOp op = d.redo.back();
        d.redo.pop_back();
        d.text.replace(op.pos, op.removed.size(), op.inserted);
        d.caret = d.anchor = std::min(op.caretAfter, d.text.size());
        op.time = 0.0; // не сливать с новым вводом
        d.undo.push_back(std::move(op));
        Changed();
    }

    void Copy()
    {
        const std::wstring s = SelectedText();
        if (!s.empty())
            SetClipboardText(GetShell().Hwnd(), ToCrLf(s));
    }

    void Cut()
    {
        if (Cur().caret == Cur().anchor)
            return;
        Copy();
        DeleteSelection();
    }

    void Paste()
    {
        const std::wstring s = Normalize(GetClipboardText(GetShell().Hwnd()));
        if (!s.empty())
            InsertText(s);
    }

    void SelectAll()
    {
        Doc& d = Cur();
        d.anchor = 0;
        d.caret = d.text.size();
    }

    void InsertDateTime()
    {
        const SYSTEMTIME now = LocalNow();
        InsertText(FormatTime(now) + L" " + FormatDate(now));
    }

    void WebSearch()
    {
        const std::wstring s = Trim(SelectedText());
        if (!s.empty())
            GetShell().Launch(AppId::Browser, s);
    }

    void Zoom(int dir)
    {
        static const int steps[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 350, 400, 450, 500};
        if (dir == 0)
            m_zoom = 100;
        else
        {
            int idx = 0;
            for (int i = 0; i < static_cast<int>(sizeof(steps) / sizeof(steps[0])); ++i)
            {
                if (steps[i] <= m_zoom)
                    idx = i;
            }
            idx = Clamp(idx + dir, 0, static_cast<int>(sizeof(steps) / sizeof(steps[0])) - 1);
            m_zoom = steps[idx];
        }
        Invalidate();
        EnsureCaretVisible();
    }

    // ---- навигация ----------------------------------------------------------------
    size_t WordLeft(size_t pos) const
    {
        const std::wstring& t = Cur().text;
        while (pos > 0 && (t[pos - 1] == L' ' || t[pos - 1] == L'\t'))
            --pos;
        if (pos > 0 && t[pos - 1] == L'\n')
            return pos - 1;
        if (pos > 0 && IsWordChar(t[pos - 1]))
        {
            while (pos > 0 && IsWordChar(t[pos - 1]))
                --pos;
        }
        else if (pos > 0)
        {
            while (pos > 0 && !IsWordChar(t[pos - 1]) && t[pos - 1] != L' ' && t[pos - 1] != L'\t' && t[pos - 1] != L'\n')
                --pos;
        }
        return pos;
    }

    size_t WordRight(size_t pos) const
    {
        const std::wstring& t = Cur().text;
        const size_t n = t.size();
        if (pos < n && t[pos] == L'\n')
            return pos + 1;
        if (pos < n && IsWordChar(t[pos]))
        {
            while (pos < n && IsWordChar(t[pos]))
                ++pos;
        }
        else
        {
            while (pos < n && !IsWordChar(t[pos]) && t[pos] != L' ' && t[pos] != L'\t' && t[pos] != L'\n')
                ++pos;
        }
        while (pos < n && (t[pos] == L' ' || t[pos] == L'\t'))
            ++pos;
        return pos;
    }

    void MoveCaret(size_t pos, bool extend, bool keepX = false)
    {
        Doc& d = Cur();
        d.caret = std::min(pos, d.text.size());
        if (!extend)
            d.anchor = d.caret;
        if (!keepX)
            d.desiredX = -1.0f;
        EnsureCaretVisible();
    }

    void MoveVertical(int lines, bool extend)
    {
        Layout();
        Doc& d = Cur();
        const int li = LineOf(d.caret);
        if (d.desiredX < 0.0f)
            d.desiredX = static_cast<float>(ColOf(m_lines[static_cast<size_t>(li)], d.caret));
        const int target = li + lines;
        if (target < 0)
        {
            MoveCaret(0, extend, true);
            return;
        }
        if (target >= static_cast<int>(m_lines.size()))
        {
            MoveCaret(d.text.size(), extend, true);
            return;
        }
        MoveCaret(OffsetAtCol(target, d.desiredX), extend, true);
    }

    void SelectWordAt(size_t pos)
    {
        Doc& d = Cur();
        const std::wstring& t = d.text;
        size_t a = pos, b = pos;
        if (pos < t.size() && IsWordChar(t[pos]))
        {
            while (a > 0 && IsWordChar(t[a - 1]))
                --a;
            while (b < t.size() && IsWordChar(t[b]))
                ++b;
        }
        else if (pos < t.size() && t[pos] != L'\n')
            b = pos + 1;
        d.anchor = a;
        d.caret = b;
        m_wordA = a;
        m_wordB = b;
    }

    void SelectLineAt(size_t pos)
    {
        Doc& d = Cur();
        const std::wstring& t = d.text;
        size_t a = pos == 0 ? 0 : t.rfind(L'\n', pos - 1);
        a = a == std::wstring::npos ? 0 : (pos == 0 ? 0 : a + 1);
        size_t b = t.find(L'\n', pos);
        b = b == std::wstring::npos ? t.size() : b + 1;
        d.anchor = a;
        d.caret = b;
    }

    // ================================================================== поиск
    size_t FindIn(const std::wstring& hay, const std::wstring& needle, size_t from, bool forward) const
    {
        if (needle.empty() || needle.size() > hay.size())
            return std::wstring::npos;
        auto eq = [this](wchar_t a, wchar_t b) { return m_matchCase ? a == b : std::towlower(a) == std::towlower(b); };
        const size_t last = hay.size() - needle.size();
        if (forward)
        {
            for (size_t i = from; i <= last; ++i)
            {
                size_t k = 0;
                while (k < needle.size() && eq(hay[i + k], needle[k]))
                    ++k;
                if (k == needle.size())
                    return i;
            }
        }
        else
        {
            size_t i = std::min(from, last + 1);
            while (i > 0)
            {
                --i;
                size_t k = 0;
                while (k < needle.size() && eq(hay[i + k], needle[k]))
                    ++k;
                if (k == needle.size())
                    return i;
            }
        }
        return std::wstring::npos;
    }

    bool FindNext(bool forward)
    {
        const std::wstring q = m_findBox->Text();
        if (q.empty())
        {
            ShowFind(false);
            return false;
        }
        Doc& d = Cur();
        const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
        size_t pos = FindIn(d.text, q, forward ? b : a, forward);
        if (pos == std::wstring::npos && m_wrapAround)
            pos = FindIn(d.text, q, forward ? 0 : d.text.size(), forward);
        if (pos == std::wstring::npos)
        {
            m_findFailed = true;
            ShowDialog(L"Блокнот", L"Не удаётся найти «" + q + L"»", {{L"ОК", true}}, nullptr);
            return false;
        }
        m_findFailed = false;
        d.anchor = pos;
        d.caret = pos + q.size();
        EnsureCaretVisible();
        return true;
    }

    void ReplaceOne()
    {
        const std::wstring q = m_findBox->Text();
        if (q.empty())
            return;
        const std::wstring sel = SelectedText();
        const bool match = sel.size() == q.size() && FindIn(sel, q, 0, true) == 0;
        if (match)
            InsertText(m_replaceBox->Text());
        FindNext(true);
    }

    void ReplaceAll()
    {
        const std::wstring q = m_findBox->Text();
        if (q.empty())
            return;
        Doc& d = Cur();
        std::wstring out;
        size_t pos = 0;
        int count = 0;
        for (;;)
        {
            const size_t f = FindIn(d.text, q, pos, true);
            if (f == std::wstring::npos)
                break;
            out.append(d.text, pos, f - pos);
            out += m_replaceBox->Text();
            pos = f + q.size();
            ++count;
        }
        if (count == 0)
        {
            ShowDialog(L"Блокнот", L"Не удаётся найти «" + q + L"»", {{L"ОК", true}}, nullptr);
            return;
        }
        out.append(d.text, pos, std::wstring::npos);
        const size_t caret = std::min(d.caret, out.size());
        Edit(0, d.text.size(), out, EditKind::Other);
        d.caret = d.anchor = caret;
        EnsureCaretVisible();
    }

    // ================================================================== отрисовка
    std::wstring Expand(const VisLine& l) const
    {
        const std::wstring& t = Cur().text;
        std::wstring s;
        s.reserve(l.end - l.start + 8);
        int col = 0;
        for (size_t i = l.start; i < l.end; ++i)
        {
            if (t[i] == L'\t')
            {
                const int w = kTabCols - col % kTabCols;
                s.append(static_cast<size_t>(w), L' ');
                col += w;
            }
            else
            {
                s.push_back(t[i]);
                ++col;
            }
        }
        return s;
    }

    bool EditorFocused() const { return IsActive() && m_ui.FocusedChild() == nullptr && m_overlay == Overlay::None && !HasDialog(); }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        const float W = Width();
        // Вкладки на Mica.
        std::vector<TabInfo> infos;
        for (const Doc& d : m_tabs)
        {
            TabInfo ti;
            ti.title = d.name;
            ti.modified = d.Dirty();
            infos.push_back(ti);
        }
        const Color layer = p.dark ? Color(255, 255, 255, 0x0D) : Color(255, 255, 255, 0xB3);
        m_strip.Draw(r, W, infos, m_cur, layer, p.text, p.subtleHover, p.divider);
        // Строка меню и редактор.
        r.FillRect({0.0f, kTitleH, W, kMenuH}, layer);
        const Rect e = EditorRect();
        r.FillRect(e, p.editorBg);
        r.FillRect({0.0f, e.y, W, 1.0f}, p.divider);
        DrawEditor(r);
        if (m_statusBar)
            DrawStatus(r);
        if (m_findVisible)
        {
            const Rect f = FindRect();
            r.Shadow(f, 8.0f, 16.0f, 4.0f, p.dark ? 0.45f : 0.18f);
            r.FillRoundRect(f, 8.0f, p.dark ? Color::Hex(0x2C2C2C) : Color::Hex(0xF9F9F9));
            r.StrokeRoundRect(f, 8.0f, p.surfaceStroke.MulAlpha(0.6f));
        }
        for (Button* b : m_menuButtons)
            b->checked = false;
        m_ui.Draw(r);
        if (m_findVisible && m_findFailed)
        {
            const Rect fb = m_findBox->Bounds();
            r.StrokeRoundRect(fb, 4.0f, p.critical);
        }
        DrawOverlay(r);
    }

    void DrawEditor(Renderer& r)
    {
        const Palette& p = Theme::P();
        Layout();
        ClampScroll();
        const Doc& d = Cur();
        const Rect e = EditorRect();
        const Rect tr = TextRect();
        const float lh = LineH(), cw = CharW(), fs = FontSize();
        const size_t s0 = std::min(d.caret, d.anchor), s1 = std::max(d.caret, d.anchor);
        const bool focused = EditorFocused();
        const Color selFill = focused ? p.accent : (p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x1A));
        const Color selText = focused ? p.textOnAccent : p.text;

        r.PushClip({e.x, e.y + 1.0f, e.w - kBarW, e.h - 1.0f});
        const int first = std::max(0, static_cast<int>(std::floor(d.scrollY / lh)));
        const int last = std::min(static_cast<int>(m_lines.size()) - 1, static_cast<int>(std::ceil((d.scrollY + tr.h + kPadY) / lh)));
        const std::wstring& t = d.text;
        for (int li = first; li <= last; ++li)
        {
            const VisLine& l = m_lines[static_cast<size_t>(li)];
            const float y = tr.y + static_cast<float>(li) * lh - d.scrollY;
            const float x0 = tr.x - d.scrollX;
            const std::wstring s = Expand(l);
            const Rect lr{x0, y, std::max(tr.w + d.scrollX, static_cast<float>(s.size()) * cw + 40.0f), lh};
            if (!s.empty())
                r.Text(s, lr, p.text, fs, FontWeight::Regular, TextFlags::VCenter | TextFlags::NoEllipsis, m_face);
            // Выделение
            if (s1 > s0 && s0 <= l.end && s1 >= l.start)
            {
                const size_t a = std::max(s0, l.start), b = std::min(s1, l.end);
                float xa = x0 + static_cast<float>(ColOf(l, a)) * cw;
                float xb = x0 + static_cast<float>(ColOf(l, b)) * cw;
                const bool nl = s1 > l.end && l.end < t.size() && t[l.end] == L'\n';
                if (nl)
                    xb += cw * 0.6f;
                if (xb > xa)
                {
                    const Rect sel{xa, y, xb - xa, lh};
                    r.FillRect(sel, selFill);
                    if (!s.empty())
                    {
                        r.PushClip(sel);
                        r.Text(s, lr, selText, fs, FontWeight::Regular, TextFlags::VCenter | TextFlags::NoEllipsis, m_face);
                        r.PopClip();
                    }
                }
            }
        }
        // Каретка
        if (focused)
        {
            r.NoteCaret();
            if (std::fmod(NowSeconds() - m_blinkStart, 1.06) < 0.53)
            {
                const int li = LineOf(d.caret);
                const VisLine& l = m_lines[static_cast<size_t>(li)];
                const float cx = std::floor(tr.x - d.scrollX + static_cast<float>(ColOf(l, d.caret)) * cw) + 0.5f;
                const float cy = tr.y + static_cast<float>(li) * lh - d.scrollY;
                r.Line(cx, cy, cx, cy + lh, p.text, 1.0f, false);
            }
        }
        r.PopClip();
        DrawScrollbars(r);
    }

    Rect VThumb() const
    {
        const Rect e = EditorRect();
        const float viewH = TextRect().h;
        const float maxS = MaxScrollY();
        const float trackY = e.y + 4.0f, trackH = e.h - 8.0f - (MaxScrollX() > 0.0f ? kBarW : 0.0f);
        const float th = std::max(28.0f, trackH * viewH / (viewH + maxS));
        const float y = trackY + (maxS > 0.0f ? (trackH - th) * (Cur().scrollY / maxS) : 0.0f);
        return {e.Right() - kBarW, y, kBarW, th};
    }

    Rect HThumb() const
    {
        const Rect e = EditorRect();
        const Rect tr = TextRect();
        const float maxS = MaxScrollX();
        const float trackX = e.x + 4.0f, trackW = e.w - 8.0f - kBarW;
        const float tw = std::max(28.0f, trackW * tr.w / (tr.w + maxS));
        const float x = trackX + (maxS > 0.0f ? (trackW - tw) * (Cur().scrollX / maxS) : 0.0f);
        return {x, e.Bottom() - kBarW, tw, kBarW};
    }

    void DrawScrollbars(Renderer& r)
    {
        const Palette& p = Theme::P();
        if (MaxScrollY() > 0.0f)
        {
            const Rect th = VThumb();
            const bool wide = m_barHover == 1 || m_dragBar == 1;
            const float w = wide ? 6.0f : 2.0f;
            if (wide)
                r.FillRoundRect({th.x + 1.0f, EditorRect().y + 2.0f, kBarW - 2.0f, EditorRect().h - 4.0f}, 5.0f,
                                p.dark ? Color(255, 255, 255, 0x0A) : Color(0, 0, 0, 0x08));
            r.FillRoundRect({th.Right() - 3.0f - w, th.y, w, th.h}, w * 0.5f, p.scrollThumb);
        }
        if (MaxScrollX() > 0.0f)
        {
            const Rect th = HThumb();
            const bool wide = m_barHover == 2 || m_dragBar == 2;
            const float h = wide ? 6.0f : 2.0f;
            r.FillRoundRect({th.x, th.Bottom() - 3.0f - h, th.w, h}, h * 0.5f, p.scrollThumb);
        }
    }

    void DrawStatus(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Doc& d = Cur();
        const float y = Height() - kStatusH;
        const Rect bar{0.0f, y, Width(), kStatusH};
        r.FillRect({0.0f, y, Width(), 1.0f}, p.divider);
        const std::wstring pos = L"Стр " + std::to_wstring(CaretLogicalLine() + 1) + L", стлб " + std::to_wstring(CaretLogicalCol() + 1);
        const size_t total = d.text.size();
        std::wstring chars;
        const size_t sel = std::max(d.caret, d.anchor) - std::min(d.caret, d.anchor);
        const long long totalLL = static_cast<long long>(total);
        if (sel > 0)
            chars = GroupDigits(static_cast<long long>(sel)) + L" из " + GroupDigits(totalLL) + L" " +
                    PluralRu(totalLL, L"символа", L"символов", L"символов");
        else
            chars = GroupDigits(totalLL) + L" " + PluralRu(totalLL, L"символ", L"символа", L"символов");
        float x = 16.0f;
        auto seg = [&](const std::wstring& s, float w) {
            r.Text(s, {x, bar.y, w, bar.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            x += w;
            r.FillRect({x - 12.0f, bar.y + 8.0f, 1.0f, bar.h - 16.0f}, p.divider);
        };
        seg(pos, 150.0f);
        seg(chars, 200.0f);
        // Правая группа
        const float wz = 64.0f, wl = 124.0f, we = 76.0f;
        x = Width() - 16.0f - we - wl - wz;
        if (x > 380.0f)
        {
            r.FillRect({x - 12.0f, bar.y + 8.0f, 1.0f, bar.h - 16.0f}, p.divider);
            seg(std::to_wstring(m_zoom) + L"%", wz);
            seg(L"Windows (CRLF)", wl);
            r.Text(L"UTF-8", {x, bar.y, we, bar.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
    }

    // ================================================================== ввод
    bool IsDragArea(Point local) const override
    {
        if (local.y >= kTitleH || local.x > Width() - 46.0f * 3.0f)
            return false;
        const int n = static_cast<int>(m_tabs.size());
        if (m_strip.HitTest(local, Width(), n, nullptr) >= 0 || m_strip.NewRect(Width(), n).Contains(local))
            return false;
        return true;
    }

    int BarAt(Point pt) const
    {
        const Rect e = EditorRect();
        if (!e.Contains(pt))
            return 0;
        if (MaxScrollY() > 0.0f && pt.x >= e.Right() - kBarW)
            return 1;
        if (MaxScrollX() > 0.0f && pt.y >= e.Bottom() - kBarW)
            return 2;
        return 0;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        if (m_overlay != Overlay::None)
        {
            if (m_ovUi.OnMouseDown(e))
                return;
            if (m_overlay == Overlay::Open)
            {
                const int row = OpenRowAt(e.pos);
                if (row >= 0)
                {
                    m_openSel = row;
                    if (e.clicks >= 2)
                        CommitOpen();
                }
            }
            return;
        }
        const Point pt = e.pos;
        // Вкладки
        if (pt.y < kTitleH)
        {
            bool onClose = false;
            const int n = static_cast<int>(m_tabs.size());
            const int tab = m_strip.HitTest(pt, Width(), n, &onClose);
            if (tab >= 0)
            {
                if (onClose || e.button == MouseButton::Middle)
                    CloseTab(tab);
                else
                    SwitchTab(tab);
            }
            else if (m_strip.NewRect(Width(), n).Contains(pt))
                NewTab();
            return;
        }
        if (m_ui.OnMouseDown(e))
            return;
        const Rect er = EditorRect();
        if (!er.Contains(pt))
            return;
        const int bar = BarAt(pt);
        if (bar != 0 && e.button == MouseButton::Left)
        {
            const Rect th = bar == 1 ? VThumb() : HThumb();
            Doc& d = Cur();
            if (!th.Contains(pt))
            {
                // Щелчок по дорожке — на страницу.
                const Rect tr = TextRect();
                if (bar == 1)
                    d.scrollY += pt.y < th.y ? -tr.h : tr.h;
                else
                    d.scrollX += pt.x < th.x ? -tr.w : tr.w;
                ClampScroll();
            }
            m_dragBar = bar;
            m_dragStart = pt;
            m_dragScroll = bar == 1 ? d.scrollY : d.scrollX;
            return;
        }
        Doc& d = Cur();
        const size_t hit = OffsetAt(pt);
        if (e.button == MouseButton::Right)
        {
            const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
            if (hit < a || hit > b || a == b)
                MoveCaret(hit, false);
            return;
        }
        if (e.button != MouseButton::Left)
            return;
        const double now = NowSeconds();
        const bool triple = e.clicks >= 2 && now - m_lastDouble < 0.5 && Distance(pt, m_lastDoublePos) < 4.0f;
        if (e.clicks >= 2 && !triple)
        {
            m_lastDouble = now;
            m_lastDoublePos = pt;
            SelectWordAt(hit);
            m_selMode = 1;
        }
        else if (triple || (now - m_lastDouble < 0.5 && Distance(pt, m_lastDoublePos) < 4.0f))
        {
            SelectLineAt(hit);
            m_lastDouble = 0.0;
            m_selMode = 2;
        }
        else
        {
            MoveCaret(hit, e.mods.shift);
            m_selMode = 0;
        }
        m_selecting = true;
        m_blinkStart = now;
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseMove(e);
            return;
        }
        if (m_overlay != Overlay::None)
        {
            m_ovUi.OnMouseMove(e);
            m_openHover = m_overlay == Overlay::Open ? OpenRowAt(e.pos) : -1;
            return;
        }
        const Point pt = e.pos;
        Doc& d = Cur();
        if (m_dragBar != 0)
        {
            const Rect e2 = EditorRect();
            if (m_dragBar == 1)
            {
                const float trackH = e2.h - 8.0f - VThumb().h;
                if (trackH > 0.0f)
                    d.scrollY = m_dragScroll + (pt.y - m_dragStart.y) / trackH * MaxScrollY();
            }
            else
            {
                const float trackW = e2.w - 8.0f - kBarW - HThumb().w;
                if (trackW > 0.0f)
                    d.scrollX = m_dragScroll + (pt.x - m_dragStart.x) / trackW * MaxScrollX();
            }
            ClampScroll();
            return;
        }
        if (m_selecting)
        {
            const Rect tr = TextRect();
            // Автопрокрутка у краёв.
            if (pt.y < tr.y)
                d.scrollY -= std::min(40.0f, tr.y - pt.y);
            else if (pt.y > tr.Bottom())
                d.scrollY += std::min(40.0f, pt.y - tr.Bottom());
            if (!m_wrap)
            {
                if (pt.x < tr.x)
                    d.scrollX -= std::min(40.0f, tr.x - pt.x);
                else if (pt.x > tr.Right())
                    d.scrollX += std::min(40.0f, pt.x - tr.Right());
            }
            ClampScroll();
            const size_t hit = OffsetAt(pt);
            if (m_selMode == 1)
            {
                // Выделение по словам.
                const std::wstring& t = d.text;
                size_t a = hit, b = hit;
                while (a > 0 && a <= t.size() && IsWordChar(t[a - 1]))
                    --a;
                while (b < t.size() && IsWordChar(t[b]))
                    ++b;
                if (hit < m_wordA)
                {
                    d.anchor = m_wordB;
                    d.caret = a;
                }
                else
                {
                    d.anchor = m_wordA;
                    d.caret = std::max(b, m_wordB);
                }
            }
            else
                d.caret = hit;
            m_blinkStart = NowSeconds();
            return;
        }
        Window::OnMouseMove(e);
        m_strip.UpdateHover(pt, Width(), static_cast<int>(m_tabs.size()));
        if (m_strip.hover >= 0)
            GetShell().Tooltip(&m_strip, m_tabs[static_cast<size_t>(m_strip.hover)].name,
                               LocalToScreen(m_strip.TabRect(m_strip.hover, Width(), static_cast<int>(m_tabs.size()))), false);
        m_barHover = BarAt(pt);
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseUp(e);
            return;
        }
        if (m_overlay != Overlay::None)
        {
            m_ovUi.OnMouseUp(e);
            return;
        }
        m_ui.OnMouseUp(e);
        m_selecting = false;
        m_dragBar = 0;
        if (e.button == MouseButton::Right && EditorRect().Contains(e.pos) && BarAt(e.pos) == 0)
            ShowContextMenu(LocalToScreen(e.pos));
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_ovUi.OnMouseLeave();
        m_strip.ClearHover();
        m_barHover = 0;
        m_openHover = -1;
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        if (m_overlay == Overlay::Open)
        {
            const float maxS = std::max(0.0f, static_cast<float>(m_openFiles.size()) * 40.0f + 8.0f - OpenListRect().h);
            m_openScroll = Clamp(m_openScroll - e.wheel * 80.0f, 0.0f, maxS);
            return true;
        }
        if (m_overlay != Overlay::None)
            return true;
        if (e.mods.ctrl)
        {
            Zoom(e.wheel > 0.0f ? 1 : -1);
            return true;
        }
        Doc& d = Cur();
        if (e.mods.shift && !m_wrap)
            d.scrollX -= e.wheel * CharW() * 8.0f;
        else
            d.scrollY -= e.wheel * LineH() * 3.0f;
        ClampScroll();
        return true;
    }

    CursorType Cursor(Point local) const override
    {
        if (HasDialog())
            return CursorType::Arrow;
        if (m_overlay != Overlay::None)
            return m_ovUi.Cursor(local);
        if (m_dragBar != 0)
            return CursorType::Arrow;
        if (m_selecting)
            return CursorType::IBeam;
        const CursorType c = m_ui.Cursor(local);
        if (c != CursorType::Arrow)
            return c;
        for (auto& w : m_ui.Children())
        {
            if (w->Visible() && w->HitTest(local))
                return CursorType::Arrow;
        }
        if (m_findVisible && FindRect().Contains(local))
            return CursorType::Arrow;
        if (EditorRect().Contains(local) && BarAt(local) == 0)
            return CursorType::IBeam;
        return CursorType::Arrow;
    }

    void ShowContextMenu(Point screen)
    {
        const Doc& d = Cur();
        const bool hasSel = d.caret != d.anchor;
        std::vector<MenuItem> items = {
            MenuItem(L"Отменить", Icon::Undo, [this]() { Undo(); }, L"Ctrl+Z").Disabled(d.undo.empty()),
            MenuItem::Sep(),
            MenuItem(L"Вырезать", Icon::Cut, [this]() { Cut(); }, L"Ctrl+X").Disabled(!hasSel),
            MenuItem(L"Копировать", Icon::Copy, [this]() { Copy(); }, L"Ctrl+C").Disabled(!hasSel),
            MenuItem(L"Вставить", Icon::Paste, [this]() { Paste(); }, L"Ctrl+V"),
            MenuItem(L"Удалить", Icon::Delete, [this]() { DeleteSelection(); }, L"Del").Disabled(!hasSel),
            MenuItem::Sep(),
            MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"),
            MenuItem::Sep(),
            MenuItem(L"Поиск в Интернете", Icon::Search, [this]() { WebSearch(); }, L"Ctrl+E").Disabled(!hasSel),
        };
        GetShell().ShowMenu(screen, std::move(items));
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        if (m_overlay != Overlay::None)
        {
            if (m_overlay == Overlay::Open)
            {
                if (vk == VK_ESCAPE)
                    CloseOverlay();
                else if (vk == VK_RETURN)
                    CommitOpen();
                else if (vk == VK_DOWN || vk == VK_UP)
                {
                    const int n = static_cast<int>(m_openFiles.size());
                    if (n > 0)
                        m_openSel = Clamp(m_openSel + (vk == VK_DOWN ? 1 : -1), 0, n - 1);
                }
                return true;
            }
            if (m_ovUi.OnKeyDown(vk, mods))
                return true;
            if (vk == VK_ESCAPE)
                CloseOverlay();
            return true;
        }
        // Сочетания, работающие всегда.
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'N':
                if (mods.shift)
                    GetShell().WM().Add(CreateNotepad(L""));
                else
                    NewTab();
                return true;
            case 'O':
                ShowOpen();
                return true;
            case 'S':
                if (mods.alt)
                    SaveAll();
                else if (mods.shift)
                    ShowSaveAs(nullptr);
                else
                    Save();
                return true;
            case 'W':
                if (mods.shift)
                    Close();
                else
                    CloseTab(m_cur);
                return true;
            case 'F':
                ShowFind(false);
                return true;
            case 'H':
                ShowFind(true);
                return true;
            case 'G':
                ShowGoTo();
                return true;
            case VK_TAB:
                if (m_tabs.size() > 1)
                    SwitchTab((m_cur + (mods.shift ? -1 : 1) + static_cast<int>(m_tabs.size())) % static_cast<int>(m_tabs.size()));
                return true;
            case VK_OEM_PLUS:
            case VK_ADD:
                Zoom(1);
                return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                Zoom(-1);
                return true;
            case '0':
            case VK_NUMPAD0:
                Zoom(0);
                return true;
            default:
                break;
            }
        }
        if (vk == VK_F3)
        {
            FindNext(!mods.shift);
            return true;
        }
        if (m_ui.FocusedChild())
        {
            if (vk == VK_TAB && m_findVisible)
            {
                Widget* f = m_ui.FocusedChild();
                m_ui.SetFocus(f == m_findBox && m_replaceMode ? static_cast<Widget*>(m_replaceBox) : static_cast<Widget*>(m_findBox));
                return true;
            }
            if (m_ui.OnKeyDown(vk, mods))
                return true;
            if (vk == VK_ESCAPE)
            {
                HideFind();
                return true;
            }
            return true;
        }
        return EditorKey(vk, mods);
    }

    bool EditorKey(UINT vk, const KeyMods& mods)
    {
        Doc& d = Cur();
        const bool sh = mods.shift;
        const bool hasSel = d.caret != d.anchor;
        const size_t a = std::min(d.caret, d.anchor), b = std::max(d.caret, d.anchor);
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'A':
                SelectAll();
                return true;
            case 'C':
            case VK_INSERT:
                Copy();
                return true;
            case 'X':
                Cut();
                return true;
            case 'V':
                Paste();
                return true;
            case 'Z':
                Undo();
                return true;
            case 'Y':
                Redo();
                return true;
            case 'E':
                WebSearch();
                return true;
            case VK_LEFT:
                MoveCaret(WordLeft(d.caret), sh);
                return true;
            case VK_RIGHT:
                MoveCaret(WordRight(d.caret), sh);
                return true;
            case VK_HOME:
                MoveCaret(0, sh);
                return true;
            case VK_END:
                MoveCaret(d.text.size(), sh);
                return true;
            case VK_BACK:
                if (hasSel)
                    DeleteSelection();
                else
                {
                    const size_t w = WordLeft(d.caret);
                    Edit(w, d.caret - w, L"", EditKind::Other);
                }
                return true;
            case VK_DELETE:
                if (hasSel)
                    DeleteSelection();
                else
                {
                    const size_t w = WordRight(d.caret);
                    Edit(d.caret, w - d.caret, L"", EditKind::Other);
                }
                return true;
            case VK_UP:
                d.scrollY -= LineH();
                ClampScroll();
                return true;
            case VK_DOWN:
                d.scrollY += LineH();
                ClampScroll();
                return true;
            default:
                return false;
            }
        }
        Layout();
        const int visibleLines = std::max(1, static_cast<int>(TextRect().h / LineH()) - 1);
        switch (vk)
        {
        case VK_LEFT:
            if (hasSel && !sh)
                MoveCaret(a, false);
            else if (d.caret > 0)
                MoveCaret(d.caret - 1, sh);
            return true;
        case VK_RIGHT:
            if (hasSel && !sh)
                MoveCaret(b, false);
            else
                MoveCaret(d.caret + 1, sh);
            return true;
        case VK_UP:
            MoveVertical(-1, sh);
            return true;
        case VK_DOWN:
            MoveVertical(1, sh);
            return true;
        case VK_PRIOR:
            d.scrollY -= static_cast<float>(visibleLines) * LineH();
            MoveVertical(-visibleLines, sh);
            return true;
        case VK_NEXT:
            d.scrollY += static_cast<float>(visibleLines) * LineH();
            MoveVertical(visibleLines, sh);
            return true;
        case VK_HOME:
        {
            const int li = LineOf(d.caret);
            const VisLine& l = m_lines[static_cast<size_t>(li)];
            // «Умный» Home: сначала к первому непробельному символу.
            size_t fnw = l.start;
            while (fnw < l.end && (d.text[fnw] == L' ' || d.text[fnw] == L'\t'))
                ++fnw;
            MoveCaret(d.caret == fnw ? l.start : fnw, sh);
            return true;
        }
        case VK_END:
            MoveCaret(LineCaretEnd(LineOf(d.caret)), sh);
            return true;
        case VK_BACK:
            if (hasSel)
                DeleteSelection();
            else if (d.caret > 0)
                Edit(d.caret - 1, 1, L"", EditKind::Backspace);
            return true;
        case VK_DELETE:
            if (hasSel)
                DeleteSelection();
            else if (d.caret < d.text.size())
                Edit(d.caret, 1, L"", EditKind::Delete);
            return true;
        case VK_RETURN:
            InsertText(L"\n", EditKind::Typing);
            return true;
        case VK_TAB:
            InsertText(L"\t", EditKind::Typing);
            return true;
        case VK_F5:
            InsertDateTime();
            return true;
        case VK_ESCAPE:
            if (m_findVisible)
                HideFind();
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
        if (m_overlay != Overlay::None)
        {
            if (m_overlay != Overlay::Open)
                m_ovUi.OnChar(ch);
            return true;
        }
        if (m_ui.FocusedChild())
            return m_ui.OnChar(ch);
        if (ch < 32 || ch == 127)
            return true; // управляющие символы обрабатываются в OnKeyDown
        if (CurrentMods().ctrl && !CurrentMods().alt)
            return true;
        InsertText(std::wstring(1, ch), EditKind::Typing);
        return true;
    }

    // ================================================================== поля
    std::vector<Doc> m_tabs;
    int m_cur = 0;
    TabStrip m_strip;

    // Раскладка (кэш)
    mutable std::vector<VisLine> m_lines;
    mutable bool m_layoutValid = false;
    mutable const Doc* m_layoutDoc = nullptr;
    mutable size_t m_layoutText = 0;
    mutable int m_layoutCols = 0;
    mutable unsigned m_layoutHash = 0;
    mutable int m_maxCols = 0;
    mutable float m_cw = 8.0f;
    mutable float m_lh = 18.0f;
    mutable float m_cwSize = 0.0f;
    mutable FontFace m_cwFace = FontFace::UI;
    mutable float m_cwScale = 0.0f;
    unsigned m_editSerial = 1;

    // Вид
    float m_pt = kBasePt;
    FontFace m_face = FontFace::Mono;
    int m_zoom = 100;
    bool m_wrap = false;
    bool m_statusBar = true;
    double m_blinkStart = 0.0;

    // Мышь
    bool m_selecting = false;
    int m_selMode = 0; // 0 — символы, 1 — слова, 2 — строки
    size_t m_wordA = 0;
    size_t m_wordB = 0;
    double m_lastDouble = 0.0;
    Point m_lastDoublePos;
    int m_dragBar = 0;
    int m_barHover = 0;
    Point m_dragStart;
    float m_dragScroll = 0.0f;

    // Меню и поиск
    std::vector<Button*> m_menuButtons;
    Button* m_gear = nullptr;
    TextBox* m_findBox = nullptr;
    TextBox* m_replaceBox = nullptr;
    Button* m_findToggle = nullptr;
    Button* m_findNext = nullptr;
    Button* m_findPrev = nullptr;
    Button* m_findOpts = nullptr;
    Button* m_findClose = nullptr;
    Button* m_replaceBtn = nullptr;
    Button* m_replaceAllBtn = nullptr;
    bool m_findVisible = false;
    bool m_replaceMode = false;
    bool m_matchCase = false;
    bool m_wrapAround = true;
    bool m_findFailed = false;

    // Оверлей
    Overlay m_overlay = Overlay::None;
    Tween m_overlayAnim;
    Panel m_ovUi;
    ComboBox* m_ovCombo = nullptr;
    TextBox* m_ovEdit = nullptr;
    Button* m_ovOk = nullptr;
    Button* m_ovCancel = nullptr;
    std::vector<FsNode*> m_overlayFolders;
    std::function<void()> m_afterSave;
    std::vector<FsNode*> m_openFiles;
    int m_openSel = -1;
    int m_openHover = -1;
    float m_openScroll = 0.0f;
};
} // namespace

std::unique_ptr<Window> CreateNotepad(const std::wstring& arg) { return std::make_unique<NotepadWindow>(arg); }
