// Системные диалоги: окно сообщения, «Свойства» файла/папки/диска и «Завершение работы Windows».

#include "../Shell.h"
#include "AppFactories.h"

namespace
{
// Общая основа диалогов: фиксированный размер, только кнопка «Закрыть».
class DialogBase : public Window
{
public:
    DialogBase(const std::wstring& title, float w, float h) : Window(AppId::System, title)
    {
        m_resizable = false;
        m_canMinimize = false;
        m_canMaximize = false;
        m_showInTaskbar = false;
        SetInitialSize(w, h);
    }

protected:
    // Нижняя полоса с кнопками.
    void DrawButtonBar(Renderer& r, float barH = 72.0f)
    {
        const Palette& p = Theme::P();
        r.FillRect({0.0f, Height() - barH, Width(), barH}, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
        r.FillRect({0.0f, Height() - barH, Width(), 1.0f}, p.divider);
    }
    void DrawPage(Renderer& r, const Rect& client, float barH = 72.0f)
    {
        const Palette& p = Theme::P();
        r.FillRect({0.0f, client.y, Width(), client.h - barH}, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
    }
};

// ---------------------------------------------------------------------------
class MessageWindow : public DialogBase
{
public:
    MessageWindow(const std::wstring& title, const std::wstring& text, AppIcon icon)
        : DialogBase(title, 460.0f, 200.0f), m_text(text), m_icon(icon)
    {
        Renderer& r = Renderer::Get();
        const float th = r.TextHeight(m_text, 14.0f, FontWeight::Regular, 460.0f - 24.0f - 72.0f);
        SetInitialSize(460.0f, std::max(190.0f, 32.0f + 28.0f + th + 28.0f + 72.0f));
        m_ok = m_ui.Add<Button>(L"ОК", Icon::None, ButtonStyle::Accent);
        m_ok->onClick = [this]() { ForceClose(); };
    }
    void OnResize() override { m_ok->SetBounds({Width() - 24.0f - 120.0f, Height() - 52.0f, 120.0f, 32.0f}); }
    void DrawContent(Renderer& r, const Rect& client) override
    {
        const Palette& p = Theme::P();
        DrawPage(r, client);
        DrawButtonBar(r);
        if (m_icon != AppIcon::None)
            r.DrawAppIcon(m_icon, {24.0f, client.y + 26.0f, 32.0f, 32.0f});
        else
        {
            r.FillCircle(40.0f, client.y + 42.0f, 16.0f, p.accent);
            r.Text(L"i", {24.0f, client.y + 26.0f, 32.0f, 32.0f}, p.textOnAccent, 20.0f, FontWeight::Semibold, TextFlags::Middle);
        }
        r.Text(m_text, {72.0f, client.y + 26.0f, Width() - 96.0f, client.h - 72.0f - 40.0f}, p.text, 14.0f,
               FontWeight::Regular, TextFlags::Wrap | TextFlags::NoEllipsis);
        m_ui.Draw(r);
    }
    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_SPACE)
        {
            ForceClose();
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

private:
    std::wstring m_text;
    AppIcon m_icon;
    Button* m_ok = nullptr;
};

// ---------------------------------------------------------------------------
std::wstring LongDateTime(const SYSTEMTIME& st)
{
    return std::to_wstring(st.wDay) + L" " + MonthNameGenitive(st.wMonth) + L" " + std::to_wstring(st.wYear) + L" г., " +
           Pad2(st.wHour) + L":" + Pad2(st.wMinute) + L":" + Pad2(st.wSecond);
}

std::wstring BytesLong(unsigned long long b) { return FormatSize(b) + L" (" + GroupDigits(static_cast<long long>(b)) + L" байт)"; }

class PropertiesWindow : public DialogBase
{
public:
    explicit PropertiesWindow(FsNode* node) : DialogBase(L"Свойства: " + node->name, 400.0f, 540.0f), m_node(node)
    {
        m_nameBox = m_ui.Add<TextBox>();
        m_nameBox->SetText(node->name);
        m_nameBox->SetEnabled(!node->system);
        auto* ok = m_ui.Add<Button>(L"ОК", Icon::None, ButtonStyle::Accent);
        ok->onClick = [this]() {
            Apply();
            ForceClose();
        };
        auto* cancel = m_ui.Add<Button>(L"Отмена");
        cancel->onClick = [this]() { ForceClose(); };
        m_apply = m_ui.Add<Button>(L"Применить");
        m_apply->onClick = [this]() { Apply(); };
        m_buttons = {ok, cancel, m_apply};
        auto* ro = m_ui.Add<CheckBox>(L"Только чтение", false);
        auto* hidden = m_ui.Add<CheckBox>(L"Скрытый", false);
        m_checks = {ro, hidden};
        for (CheckBox* c : m_checks)
            c->SetVisible(!node->IsFolder() || node->kind == FileKind::Folder);
    }

    void OnResize() override
    {
        const float w = Width();
        m_nameBox->SetBounds({76.0f, 32.0f + 60.0f, w - 76.0f - 24.0f, 32.0f});
        float x = w - 24.0f;
        for (auto it = m_buttons.rbegin(); it != m_buttons.rend(); ++it)
        {
            x -= 104.0f;
            (*it)->SetBounds({x, Height() - 52.0f, 100.0f, 32.0f});
            x -= 4.0f;
        }
        const bool drive = m_node->kind == FileKind::Drive;
        m_checks[0]->SetBounds({130.0f, Height() - 72.0f - 64.0f, 200.0f, 24.0f});
        m_checks[1]->SetBounds({130.0f, Height() - 72.0f - 36.0f, 200.0f, 24.0f});
        for (CheckBox* c : m_checks)
            c->SetVisible(!drive);
    }

    void Apply()
    {
        if (!m_node->system && m_nameBox->Text() != m_node->name)
        {
            if (GetShell().FS().Rename(m_node, m_nameBox->Text()))
                SetTitle(L"Свойства: " + m_node->name);
            else
                m_nameBox->SetText(m_node->name);
        }
    }

    void DrawContent(Renderer& r, const Rect& client) override
    {
        const Palette& p = Theme::P();
        VirtualFS& fs = GetShell().FS();
        // Вкладки.
        static const wchar_t* tabs[] = {L"Общие", L"Безопасность", L"Подробно", L"Предыдущие версии"};
        float tx = 16.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float tw = r.TextWidth(tabs[i], 12.0f) + 20.0f;
            const Rect tr{tx, client.y + 8.0f, tw, 30.0f};
            if (i == 0)
            {
                r.FillRoundRect(tr, 4.0f, 4.0f, 0.0f, 0.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
                r.FillRoundRect({tr.x + tw * 0.5f - 8.0f, tr.Bottom() - 3.0f, 16.0f, 3.0f}, 1.5f, p.accent);
            }
            r.Text(tabs[i], tr, i == 0 ? p.text : p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Middle);
            tx += tw + 2.0f;
        }
        const Rect page{0.0f, client.y + 38.0f, Width(), client.h - 38.0f - 72.0f};
        r.FillRect(page, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        DrawButtonBar(r);

        r.DrawAppIcon(fs.IconFor(m_node), {24.0f, client.y + 58.0f, 36.0f, 36.0f});
        float y = client.y + 112.0f;
        auto sep = [&]() {
            r.FillRect({24.0f, y + 4.0f, Width() - 48.0f, 1.0f}, p.divider);
            y += 14.0f;
        };
        auto row = [&](const std::wstring& k, const std::wstring& v) {
            r.Text(k, {24.0f, y, 104.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(v, {130.0f, y, Width() - 154.0f, 22.0f}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 26.0f;
        };
        sep();
        if (m_node->kind == FileKind::Drive)
        {
            row(L"Тип:", L"Локальный диск");
            row(L"Файловая система:", L"NTFS");
            sep();
            const unsigned long long used = m_node->capacity - m_node->freeSpace;
            row(L"Занято:", BytesLong(used));
            row(L"Свободно:", BytesLong(m_node->freeSpace));
            row(L"Ёмкость:", BytesLong(m_node->capacity));
            // Кольцевая диаграмма.
            const float cx = Width() * 0.5f, cy = y + 70.0f, rad = 56.0f;
            const float frac = static_cast<float>(static_cast<double>(used) / static_cast<double>(std::max(1ULL, m_node->capacity)));
            r.Arc(cx, cy, rad, 0.0f, 360.0f, p.dark ? Color::Hex(0x5A5A5A) : Color::Hex(0xD6D6D6), 22.0f);
            r.Arc(cx, cy, rad, -90.0f, 360.0f * frac, p.accent, 22.0f);
            r.Text(m_node->name, {0.0f, cy + rad + 18.0f, Width(), 20.0f}, p.text, 12.0f, FontWeight::Regular, TextFlags::Center);
        }
        else
        {
            row(L"Тип:", fs.TypeName(m_node));
            if (!m_node->IsFolder())
            {
                std::wstring app = L"—";
                switch (m_node->kind)
                {
                case FileKind::Text:
                case FileKind::Other:
                    app = L"Блокнот";
                    break;
                case FileKind::Image:
                    app = L"Фотографии";
                    break;
                case FileKind::Audio:
                case FileKind::Video:
                    app = L"Медиаплеер";
                    break;
                default:
                    break;
                }
                row(L"Приложение:", app);
            }
            sep();
            row(L"Расположение:", m_node->parent ? fs.PathOf(m_node->parent) : L"");
            const unsigned long long size = fs.SizeOf(m_node);
            row(L"Размер:", BytesLong(size));
            row(L"На диске:", BytesLong((size + 4095ULL) / 4096ULL * 4096ULL));
            if (m_node->IsFolder())
            {
                const int files = fs.CountItems(m_node, false), folders = fs.CountItems(m_node, true);
                row(L"Содержит:", L"Файлов: " + std::to_wstring(files) + L"; папок: " + std::to_wstring(folders));
            }
            sep();
            row(L"Создан:", LongDateTime(m_node->modified));
            row(L"Изменён:", LongDateTime(m_node->modified));
            if (!m_node->IsFolder())
                row(L"Открыт:", LongDateTime(LocalNow()));
            sep();
            r.Text(L"Атрибуты:", {24.0f, Height() - 72.0f - 64.0f, 104.0f, 24.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter);
        }
        m_apply->SetEnabled(!m_node->system && m_nameBox->Text() != m_node->name);
        m_ui.Draw(r);
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (vk == VK_ESCAPE)
        {
            ForceClose();
            return true;
        }
        if (vk == VK_RETURN)
        {
            Apply();
            ForceClose();
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

private:
    FsNode* m_node;
    TextBox* m_nameBox = nullptr;
    Button* m_apply = nullptr;
    std::vector<Button*> m_buttons;
    std::vector<CheckBox*> m_checks;
};

// ---------------------------------------------------------------------------
class ShutdownDialog : public DialogBase
{
public:
    ShutdownDialog() : DialogBase(L"Завершение работы Windows", 440.0f, 330.0f)
    {
        m_combo = m_ui.Add<ComboBox>();
        m_combo->items = {L"Смена пользователя", L"Выход", L"Спящий режим", L"Завершение работы", L"Перезагрузка"};
        m_combo->selected = 3;
        auto* ok = m_ui.Add<Button>(L"ОК", Icon::None, ButtonStyle::Accent);
        ok->onClick = [this]() { Confirm(); };
        auto* cancel = m_ui.Add<Button>(L"Отмена");
        cancel->onClick = [this]() { ForceClose(); };
        auto* help = m_ui.Add<Button>(L"Справка");
        help->SetEnabled(false);
        m_buttons = {ok, cancel, help};
    }
    void OnResize() override
    {
        m_combo->SetBounds({88.0f, 32.0f + 150.0f, Width() - 88.0f - 32.0f, 32.0f});
        float x = Width() - 24.0f;
        for (auto it = m_buttons.rbegin(); it != m_buttons.rend(); ++it)
        {
            x -= 100.0f;
            (*it)->SetBounds({x, Height() - 52.0f, 96.0f, 32.0f});
            x -= 6.0f;
        }
    }
    void Confirm()
    {
        const int sel = m_combo->selected;
        ForceClose();
        Shell& sh = GetShell();
        switch (sel)
        {
        case 0:
        case 1:
            sh.Lock();
            break;
        case 2:
            sh.Sleep();
            break;
        case 3:
            sh.Shutdown(false);
            break;
        default:
            sh.Shutdown(true);
            break;
        }
    }
    void DrawContent(Renderer& r, const Rect& client) override
    {
        const Palette& p = Theme::P();
        DrawPage(r, client);
        DrawButtonBar(r);
        const Rect banner{0.0f, client.y, Width(), 92.0f};
        r.FillGradientH(banner, 0.0f, Color::Hex(0x0A2F78), Color::Hex(0x1C6BD6));
        DrawWindowsBadge(r, {28.0f, banner.y + 26.0f, 40.0f, 40.0f});
        r.Text(L"Windows 11", {80.0f, banner.y, 300.0f, banner.h}, Color(255, 255, 255), 28.0f, FontWeight::Light,
               TextFlags::VCenter);
        r.Glyph(Icon::Power, {32.0f, client.y + 118.0f, 32.0f, 32.0f}, p.accentText, 1.4f);
        r.Text(L"Что должен сделать компьютер?", {88.0f, client.y + 116.0f, Width() - 120.0f, 24.0f}, p.text, 14.0f);
        static const wchar_t* desc[] = {L"Закрытие сеанса без завершения работы приложений.",
                                        L"Закрытие всех приложений и выход из системы.",
                                        L"Компьютер продолжит работать в режиме пониженного энергопотребления.",
                                        L"Закрытие всех приложений и выключение компьютера.",
                                        L"Закрытие всех приложений, выключение и повторное включение компьютера."};
        r.Text(desc[Clamp(m_combo->selected, 0, 4)], {88.0f, client.y + 196.0f, Width() - 120.0f, 40.0f}, p.textSecondary, 12.0f,
               FontWeight::Regular, TextFlags::Wrap);
        m_ui.Draw(r);
    }
    static void DrawWindowsBadge(Renderer& r, const Rect& rc)
    {
        // Собственная эмблема: четыре скруглённые плитки.
        const float t = rc.w * 0.46f, g = rc.w * 0.08f;
        for (int i = 0; i < 4; ++i)
            r.FillRoundRect({rc.x + static_cast<float>(i % 2) * (t + g), rc.y + static_cast<float>(i / 2) * (t + g), t, t},
                            t * 0.22f, Color(255, 255, 255, 0xE6));
    }
    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (vk == VK_RETURN)
        {
            Confirm();
            return true;
        }
        if (vk == VK_ESCAPE)
        {
            ForceClose();
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

private:
    ComboBox* m_combo = nullptr;
    std::vector<Button*> m_buttons;
};
} // namespace

std::unique_ptr<Window> CreateMessageBox(const std::wstring& title, const std::wstring& text, AppIcon icon)
{
    return std::make_unique<MessageWindow>(title, text, icon);
}

std::unique_ptr<Window> CreateProperties(FsNode* node) { return std::make_unique<PropertiesWindow>(node); }

std::unique_ptr<Window> CreateShutdownDialog() { return std::make_unique<ShutdownDialog>(); }
