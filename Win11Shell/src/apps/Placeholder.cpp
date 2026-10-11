// Небольшие приложения: Почта (папки, письма, создание и отправка), Магазин (подборка и установка),
// Камера (экран «камера не найдена», как на компьютере без камеры), Карты (карта мира с
// масштабом и перетаскиванием) и Советы.

#include "../Shell.h"
#include "AppFactories.h"
#include "Cities.h"
#include "ImmUi.h"
#include "WorldMap.h"

namespace
{
enum Hk : int
{
    kHkNone = 0,
    kHkFolder,
    kHkMessage,
    kHkCompose,
    kHkSend,
    kHkDiscard,
    kHkDelete,
    kHkApp,
    kHkGet,
    kHkZoomIn,
    kHkZoomOut,
    kHkCity,
    kHkTip,
    kHkTipPrev,
    kHkTipNext,
    kHkTipTry,
    kHkCamMode,
};

class MiniWindow : public Window
{
public:
    MiniWindow(AppId id, float w, float h) : Window(id, GetAppInfo(id).name)
    {
        SetInitialSize(w, h);
        m_minSize = {560.0f, 420.0f};
    }

protected:
    static constexpr float kTitleH = 48.0f;
    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void DrawTitle(Renderer& r, bool darkBg = false)
    {
        const Palette& p = Theme::P();
        r.DrawAppIcon(GetAppInfo(App()).icon, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(Title(), {48.0f, 0.0f, 300.0f, kTitleH}, darkBg ? Color(255, 255, 255) : (IsActive() ? p.text : p.textTertiary), 12.0f,
               FontWeight::Regular, TextFlags::VCenter);
    }

    virtual void Activate(int kind, int index) = 0;

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
        if (!HasDialog() && !m_ui.HasCapture())
            m_imm.Down(e.pos, e.clicks);
    }
    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        int kind = 0, index = 0;
        if (!HasDialog() && m_imm.Up(e.pos, kind, index) && e.button == MouseButton::Left)
            Activate(kind, index);
    }
    CursorType Cursor(Point local) const override
    {
        const ImmHot* h = m_imm.Hit(local);
        if (!HasDialog() && h && (h->kind == kHkApp || h->kind == kHkCity))
            return CursorType::Hand;
        return Window::Cursor(local);
    }

    ImmUi m_imm;
};

// ============================================================================ Почта
struct Mail
{
    std::wstring from, to, subject, body;
    SYSTEMTIME when{};
    bool unread = true;
    int folder = 0; // 0 входящие, 1 отправленные, 2 черновики, 3 удалённые
};

std::vector<Mail>& Mailbox()
{
    static std::vector<Mail> v;
    if (v.empty())
    {
        const SYSTEMTIME now = LocalNow();
        auto add = [&](const wchar_t* from, const wchar_t* subject, const wchar_t* body, int hoursAgo, bool unread) {
            Mail m;
            m.from = from;
            m.to = L"root@windows.local";
            m.subject = subject;
            m.body = body;
            m.when = now;
            m.when.wHour = static_cast<WORD>(std::max(0, static_cast<int>(now.wHour) - hoursAgo));
            m.unread = unread;
            v.push_back(m);
        };
        add(L"Команда Windows", L"Добро пожаловать в Windows 11",
            L"Здравствуйте!\n\nСпасибо, что выбрали Windows 11. Откройте «Пуск», чтобы найти приложения, или перетащите окно к краю экрана, "
            L"чтобы прикрепить его. Параметры персонализации помогут выбрать тему и цвет акцента.\n\nС уважением,\nкоманда Windows",
            0, true);
        add(L"Мария Иванова", L"Обед сегодня в 13:00?",
            L"Привет! Встречаемся сегодня в кафе у реки в 13:00? Событие уже есть в календаре.\n\nМария", 1, true);
        add(L"Служба поддержки", L"Ваш отчёт о проблеме получен",
            L"Мы получили ваш отчёт и уже работаем над ним. Номер обращения: 48213.\n\nЭто автоматическое сообщение, отвечать на него не нужно.", 3,
            false);
        add(L"Алексей Смирнов", L"Ревью интерфейса — материалы",
            L"Коллеги, прикладываю материалы к завтрашнему ревью: макеты окон, панель задач и меню «Пуск». Обратите внимание на тени и скругления.\n\nАлексей",
            5, false);
        add(L"Новости погоды", L"Прогноз на выходные",
            L"В выходные ожидается переменная облачность и кратковременные дожди. Подробности — в приложении «Погода».", 9, false);
    }
    return v;
}

class MailWindow : public MiniWindow
{
public:
    MailWindow() : MiniWindow(AppId::Mail, 1180.0f, 760.0f)
    {
        m_to = m_ui.Add<TextBox>(L"Кому");
        m_subject = m_ui.Add<TextBox>(L"Тема");
        m_body = m_ui.Add<TextBox>(L"Текст сообщения");
        for (TextBox* t : {m_to, m_subject, m_body})
            t->SetVisible(false);
        m_to->onEnter = [this]() { m_ui.SetFocus(m_subject); };
        m_subject->onEnter = [this]() { m_ui.SetFocus(m_body); };
        m_body->onEnter = [this]() { Activate(kHkSend, 0); };
    }

private:
    std::vector<int> List() const
    {
        std::vector<int> out;
        for (int i = static_cast<int>(Mailbox().size()) - 1; i >= 0; --i)
        {
            if (Mailbox()[static_cast<size_t>(i)].folder == m_folder)
                out.push_back(i);
        }
        if (m_folder == 0)
            std::reverse(out.begin(), out.end());
        return out;
    }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        DrawTitle(r);
        const float paneW = 240.0f, listW = std::min(380.0f, (Width() - paneW) * 0.42f);
        // Папки
        Imm::TextButton(r, m_imm, {16.0f, kTitleH + 8.0f, paneW - 32.0f, 36.0f}, L"Создать", Icon::Edit, true, kHkCompose);
        static const struct
        {
            Icon icon;
            const wchar_t* name;
        } folders[] = {{Icon::Mail, L"Входящие"}, {Icon::Share, L"Отправленные"}, {Icon::File, L"Черновики"}, {Icon::Delete, L"Удалённые"}};
        for (int i = 0; i < 4; ++i)
        {
            const Rect rr{8.0f, kTitleH + 64.0f + static_cast<float>(i) * 40.0f, paneW - 16.0f, 36.0f};
            Imm::NavRow(r, m_imm, rr, folders[i].icon, folders[i].name, m_folder == i && !m_composing, true, kHkFolder, i);
            int unread = 0;
            for (const Mail& m : Mailbox())
                unread += m.folder == i && m.unread ? 1 : 0;
            if (unread)
                r.Text(std::to_wstring(unread), {rr.Right() - 40.0f, rr.y, 30.0f, rr.h}, p.accentText, 12.0f, FontWeight::Semibold, TextFlags::VCenter | TextFlags::Right);
        }
        const Rect c{paneW, kTitleH, Width() - paneW, Height() - kTitleH};
        Imm::ContentLayer(r, c);
        // Список писем
        const Rect lr{c.x, c.y, listW, c.h};
        r.Text(folders[m_folder].name, {lr.x + 20.0f, lr.y + 14.0f, lr.w - 40.0f, 30.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.FillRect({lr.Right() - 1.0f, lr.y, 1.0f, lr.h}, p.divider);
        const std::vector<int> list = List();
        float y = lr.y + 56.0f;
        if (list.empty())
            r.Text(L"Здесь пока ничего нет", {lr.x, y + 20.0f, lr.w, 24.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Center);
        for (int idx : list)
        {
            const Mail& m = Mailbox()[static_cast<size_t>(idx)];
            const Rect rr{lr.x + 6.0f, y, lr.w - 12.0f, 76.0f};
            const bool sel = idx == m_sel && !m_composing;
            if (sel || m_imm.Hot(kHkMessage, idx))
                r.FillRoundRect(rr, 6.0f, sel ? p.subtlePressed : p.subtleHover);
            if (m.unread)
                r.FillRoundRect({rr.x, rr.y + 10.0f, 3.0f, rr.h - 20.0f}, 1.5f, p.accent);
            const std::wstring who = m_folder == 1 ? m.to : m.from;
            r.Text(who, {rr.x + 14.0f, rr.y + 8.0f, rr.w - 80.0f, 20.0f}, p.text, 14.0f, m.unread ? FontWeight::Semibold : FontWeight::Regular, TextFlags::VCenter);
            r.Text(Pad2(m.when.wHour) + L":" + Pad2(m.when.wMinute), {rr.Right() - 64.0f, rr.y + 8.0f, 52.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter | TextFlags::Right);
            r.Text(m.subject, {rr.x + 14.0f, rr.y + 30.0f, rr.w - 28.0f, 20.0f}, m.unread ? p.accentText : p.text, 13.0f, FontWeight::Regular, TextFlags::VCenter);
            std::wstring preview = m.body;
            std::replace(preview.begin(), preview.end(), L'\n', L' ');
            r.Text(preview, {rr.x + 14.0f, rr.y + 50.0f, rr.w - 28.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            m_imm.Add(rr, kHkMessage, idx);
            y += 80.0f;
        }
        // Область чтения или создания
        const Rect rd{lr.Right(), c.y, c.Right() - lr.Right(), c.h};
        LayoutCompose(rd);
        if (m_composing)
        {
            r.Text(L"Новое сообщение", {rd.x + 28.0f, rd.y + 16.0f, rd.w - 56.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
            Imm::TextButton(r, m_imm, {rd.x + 28.0f, rd.y + 196.0f + std::max(80.0f, rd.h - 300.0f) + 16.0f, 140.0f, 32.0f}, L"Отправить", Icon::Share, true, kHkSend);
            Imm::TextButton(r, m_imm, {rd.x + 180.0f, rd.y + 196.0f + std::max(80.0f, rd.h - 300.0f) + 16.0f, 140.0f, 32.0f}, L"Удалить", Icon::Delete, false,
                            kHkDiscard);
        }
        else if (m_sel >= 0 && m_sel < static_cast<int>(Mailbox().size()))
        {
            const Mail& m = Mailbox()[static_cast<size_t>(m_sel)];
            float ry = rd.y + 20.0f;
            const float h = r.TextHeight(m.subject, 22.0f, FontWeight::Semibold, rd.w - 120.0f);
            r.Text(m.subject, {rd.x + 28.0f, ry, rd.w - 120.0f, h + 4.0f}, p.text, 22.0f, FontWeight::Semibold, TextFlags::Wrap);
            Imm::SubtleButton(r, m_imm, {rd.Right() - 60.0f, ry, 36.0f, 34.0f}, Icon::Delete, kHkDelete, 0, L"Удалить");
            ry += h + 20.0f;
            r.FillCircle(rd.x + 48.0f, ry + 20.0f, 20.0f, Theme::A().base);
            r.Text(m.from.substr(0, 1), {rd.x + 28.0f, ry, 40.0f, 40.0f}, Color(255, 255, 255), 16.0f, FontWeight::Semibold, TextFlags::Middle);
            r.Text(m.from, {rd.x + 80.0f, ry, rd.w - 120.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
            r.Text(L"Кому: " + m.to + L" · " + Pad2(m.when.wHour) + L":" + Pad2(m.when.wMinute), {rd.x + 80.0f, ry + 20.0f, rd.w - 120.0f, 20.0f}, p.textSecondary, 12.0f);
            ry += 64.0f;
            r.FillRect({rd.x + 28.0f, ry, rd.w - 56.0f, 1.0f}, p.divider);
            ry += 20.0f;
            const float bh = r.TextHeight(m.body, 14.0f, FontWeight::Regular, rd.w - 56.0f);
            r.Text(m.body, {rd.x + 28.0f, ry, rd.w - 56.0f, bh + 6.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Wrap);
        }
        else
            r.Text(L"Выберите письмо, чтобы прочитать его", rd, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
        m_ui.Draw(r);
    }

    void LayoutCompose(const Rect& rd)
    {
        for (TextBox* t : {m_to, m_subject, m_body})
            t->SetVisible(m_composing);
        m_to->SetBounds({rd.x + 28.0f, rd.y + 64.0f, rd.w - 56.0f, 32.0f});
        m_subject->SetBounds({rd.x + 28.0f, rd.y + 108.0f, rd.w - 56.0f, 32.0f});
        m_body->SetBounds({rd.x + 28.0f, rd.y + 152.0f, rd.w - 56.0f, 32.0f});
    }

    void Activate(int kind, int index) override
    {
        switch (kind)
        {
        case kHkFolder:
            m_folder = index;
            m_sel = -1;
            m_composing = false;
            break;
        case kHkMessage:
            m_sel = index;
            m_composing = false;
            Mailbox()[static_cast<size_t>(index)].unread = false;
            break;
        case kHkCompose:
            m_composing = true;
            m_to->SetText(L"");
            m_subject->SetText(L"");
            m_body->SetText(L"");
            m_ui.SetFocus(m_to);
            break;
        case kHkSend:
        {
            Mail m;
            m.from = L"root";
            m.to = Trim(m_to->Text()).empty() ? L"(без получателя)" : Trim(m_to->Text());
            m.subject = Trim(m_subject->Text()).empty() ? L"(без темы)" : Trim(m_subject->Text());
            m.body = m_body->Text();
            m.when = LocalNow();
            m.unread = false;
            m.folder = 1;
            Mailbox().push_back(m);
            m_composing = false;
            m_ui.SetFocus(nullptr);
            GetShell().Notify(AppIcon::Mail, L"Почта", L"Сообщение помещено в «Отправленные»",
                              L"Без подключения к Интернету письмо будет отправлено позже: «" + m.subject + L"».");
            break;
        }
        case kHkDiscard:
            if (!Trim(m_subject->Text()).empty() || !Trim(m_body->Text()).empty())
            {
                Mail m;
                m.from = L"root";
                m.to = m_to->Text();
                m.subject = m_subject->Text();
                m.body = m_body->Text();
                m.when = LocalNow();
                m.unread = false;
                m.folder = 2;
                Mailbox().push_back(m);
            }
            m_composing = false;
            m_ui.SetFocus(nullptr);
            break;
        case kHkDelete:
            if (m_sel >= 0)
            {
                Mail& m = Mailbox()[static_cast<size_t>(m_sel)];
                if (m.folder == 3)
                    Mailbox().erase(Mailbox().begin() + m_sel);
                else
                    m.folder = 3;
                m_sel = -1;
            }
            break;
        default:
            break;
        }
    }

    int m_folder = 0;
    int m_sel = 0;
    bool m_composing = false;
    TextBox* m_to = nullptr;
    TextBox* m_subject = nullptr;
    TextBox* m_body = nullptr;
};

// ============================================================================ Магазин
struct StoreApp
{
    AppId id;
    const wchar_t* name;
    const wchar_t* category;
    float rating;
    uint32_t c1, c2;
    Icon glyph;
};

const StoreApp kStore[] = {
    {AppId::Paint, L"Paint", L"Творчество", 4.5f, 0, 0, Icon::None},
    {AppId::Calculator, L"Калькулятор", L"Утилиты", 4.7f, 0, 0, Icon::None},
    {AppId::Weather, L"Погода", L"Погода", 4.4f, 0, 0, Icon::None},
    {AppId::Clock, L"Часы", L"Продуктивность", 4.6f, 0, 0, Icon::None},
    {AppId::MediaPlayer, L"Медиаплеер", L"Музыка", 4.3f, 0, 0, Icon::None},
    {AppId::Terminal, L"Терминал", L"Разработчикам", 4.8f, 0, 0, Icon::None},
    {AppId::None, L"Заметки на облаке", L"Продуктивность", 4.2f, 0xF2C94C, 0xD9A41E, Icon::Edit},
    {AppId::None, L"Фитнес-трекер", L"Здоровье", 4.1f, 0x27AE60, 0x1E8449, Icon::Heart},
    {AppId::None, L"Пазлы", L"Игры", 4.6f, 0x9B51E0, 0x6C2EB9, Icon::Game},
    {AppId::None, L"Словарь", L"Образование", 4.0f, 0x2D9CDB, 0x1B6FA8, Icon::Language},
    {AppId::None, L"Фоторедактор", L"Фото", 4.3f, 0xEB5757, 0xB83232, Icon::Camera},
    {AppId::None, L"Шахматы", L"Игры", 4.7f, 0x4F4F4F, 0x222222, Icon::Grid},
};
constexpr int kStoreCount = static_cast<int>(sizeof(kStore) / sizeof(kStore[0]));

class StoreWindow : public MiniWindow
{
public:
    StoreWindow() : MiniWindow(AppId::Store, 1200.0f, 780.0f) {}

private:
    static std::vector<int>& Installed()
    {
        static std::vector<int> v;
        return v;
    }
    bool IsInstalled(int i) const
    {
        return kStore[i].id != AppId::None || std::find(Installed().begin(), Installed().end(), i) != Installed().end();
    }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        DrawTitle(r);
        const Rect c{0.0f, kTitleH, Width(), Height() - kTitleH};
        r.PushClip(c);
        m_imm.SetClip(c);
        float y = c.y + 12.0f - m_scroll;
        const float x = 32.0f, w = Width() - 64.0f;
        // Баннер
        const Rect hero{x, y, w, 260.0f};
        r.FillGradientV(hero, 12.0f, Theme::A().dark2, Theme::A().light1);
        for (int i = 0; i < 6; ++i)
            r.FillCircle(hero.Right() - 160.0f + static_cast<float>(i % 3) * 60.0f, hero.y + 70.0f + static_cast<float>(i / 3) * 100.0f, 70.0f,
                         Color(255, 255, 255, 0x14));
        r.Text(L"Главное в этом месяце", {hero.x + 36.0f, hero.y + 50.0f, w * 0.6f, 40.0f}, Color(255, 255, 255), 30.0f, FontWeight::Semibold);
        r.Text(L"Приложения, которые сделают Windows ещё удобнее: от рисования до работы с командной строкой.", {hero.x + 36.0f, hero.y + 100.0f, w * 0.5f, 48.0f},
               Color(255, 255, 255, 0xD8), 15.0f, FontWeight::Regular, TextFlags::Wrap);
        r.DrawAppIcon(AppIcon::Paint, {hero.Right() - 220.0f, hero.y + 60.0f, 140.0f, 140.0f});
        y += 290.0f;
        r.Text(L"Популярные приложения", {x, y, w, 28.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 44.0f;
        const float cw = 268.0f, ch = 120.0f, gap = 16.0f;
        const int cols = std::max(1, static_cast<int>((w + gap) / (cw + gap)));
        for (int i = 0; i < kStoreCount; ++i)
        {
            const StoreApp& a = kStore[i];
            const Rect rc{x + static_cast<float>(i % cols) * (cw + gap), y + static_cast<float>(i / cols) * (ch + gap), cw, ch};
            Imm::Card(r, rc, m_imm.Hot(kHkApp, i));
            m_imm.Add(rc, kHkApp, i);
            const Rect ic{rc.x + 16.0f, rc.y + 16.0f, 56.0f, 56.0f};
            if (a.id != AppId::None)
                r.DrawAppIcon(GetAppInfo(a.id).icon, ic);
            else
            {
                r.FillGradientV(ic, 12.0f, Color::Hex(a.c1), Color::Hex(a.c2));
                r.Glyph(a.glyph, ic.Inset(14.0f, 14.0f, 14.0f, 14.0f), Color(255, 255, 255), 1.3f);
            }
            r.Text(a.name, {rc.x + 86.0f, rc.y + 16.0f, rc.w - 100.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(a.category, {rc.x + 86.0f, rc.y + 38.0f, rc.w - 100.0f, 18.0f}, p.textSecondary, 12.0f);
            wchar_t rating[16];
            swprintf(rating, 16, L"%.1f", static_cast<double>(a.rating));
            r.Glyph(Icon::Star, {rc.x + 86.0f, rc.y + 60.0f, 12.0f, 12.0f}, Color::Hex(0xF2B200));
            r.Text(rating, {rc.x + 102.0f, rc.y + 56.0f, 60.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            const bool installing = m_installing == i;
            const std::wstring label = installing ? L"Установка…" : (IsInstalled(i) ? L"Открыть" : L"Получить");
            Imm::TextButton(r, m_imm, {rc.Right() - 112.0f, rc.Bottom() - 42.0f, 96.0f, 30.0f}, label, Icon::None, !IsInstalled(i), kHkGet, i);
            if (installing)
            {
                const float t = static_cast<float>((NowSeconds() - m_installStart) / 2.0);
                r.FillRoundRect({rc.x + 16.0f, rc.Bottom() - 14.0f, (rc.w - 32.0f) * Saturate(t), 3.0f}, 1.5f, p.accent);
                RequestFrame();
                if (t >= 1.0f)
                {
                    Installed().push_back(i);
                    m_installing = -1;
                    GetShell().Notify(AppIcon::Store, L"Магазин", L"Приложение установлено", std::wstring(a.name) + L" готово к использованию.");
                }
            }
        }
        y += static_cast<float>((kStoreCount + cols - 1) / cols) * (ch + gap);
        m_contentH = y + m_scroll - c.y + 24.0f;
        r.PopClip();
        m_imm.ResetClip();
        Imm::Scrollbar(r, c, m_scroll, m_contentH);
        m_ui.Draw(r);
    }

    void Activate(int kind, int index) override
    {
        if (kind != kHkGet && kind != kHkApp)
            return;
        const StoreApp& a = kStore[index];
        if (IsInstalled(index))
        {
            if (a.id != AppId::None)
                GetShell().Launch(a.id, L"");
            else if (kind == kHkGet)
                ShowDialog(a.name, L"Это демонстрационное приложение магазина: оно «установлено», но не содержит интерфейса.", {{L"ОК", true}}, nullptr);
        }
        else if (kind == kHkGet && m_installing < 0)
        {
            m_installing = index;
            m_installStart = NowSeconds();
        }
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        m_scroll = Clamp(m_scroll - e.wheel * 90.0f, 0.0f, std::max(0.0f, m_contentH - (Height() - kTitleH)));
        return true;
    }

    float m_scroll = 0.0f;
    float m_contentH = 0.0f;
    int m_installing = -1;
    double m_installStart = 0.0;
};

// ============================================================================ Камера
class CameraWindow : public MiniWindow
{
public:
    CameraWindow() : MiniWindow(AppId::Camera, 960.0f, 620.0f) {}
    bool DarkContent() const override { return true; }

private:
    void DrawContent(Renderer& r, const Rect& client) override
    {
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        r.FillRect({0.0f, 0.0f, Width(), Height()}, Color::Hex(0x1C1C1C));
        DrawTitle(r, true);
        const Color white(255, 255, 255), soft(255, 255, 255, 0xB0);
        const float cx = (Width() - 96.0f) * 0.5f, cy = client.CenterY() - 40.0f;
        // Значок камеры с перечёркиванием
        r.StrokeRoundRect({cx - 34.0f, cy - 70.0f, 68.0f, 48.0f}, 8.0f, soft, 2.0f);
        r.StrokeCircle(cx, cy - 46.0f, 14.0f, soft, 2.0f);
        r.Line(cx - 44.0f, cy - 14.0f, cx + 44.0f, cy - 82.0f, soft, 2.0f);
        r.Text(L"Не удаётся найти камеру", {40.0f, cy, Width() - 176.0f, 36.0f}, white, 22.0f, FontWeight::Semibold, TextFlags::Center);
        r.Text(L"Убедитесь, что камера подключена и правильно установлена, что она не заблокирована антивирусной программой "
               L"и что для неё установлены последние драйверы.",
               {Width() * 0.5f - 300.0f - 48.0f, cy + 44.0f, 600.0f, 60.0f}, soft, 14.0f, FontWeight::Regular, TextFlags::Center | TextFlags::Wrap);
        r.Text(L"Код ошибки: 0xA00F4244 <NoCamerasAreAttached>", {40.0f, cy + 112.0f, Width() - 176.0f, 22.0f}, soft, 12.0f, FontWeight::Regular, TextFlags::Center);
        // Правая панель режимов (недоступна без камеры)
        const float px = Width() - 96.0f;
        r.FillRect({px, kTitleH, 96.0f, Height() - kTitleH}, Color::Hex(0x141414));
        const Icon modes[] = {Icon::Camera, Icon::Videos};
        for (int i = 0; i < 2; ++i)
        {
            const Rect b{px + 28.0f, client.CenterY() - 70.0f + static_cast<float>(i) * 120.0f, 40.0f, 40.0f};
            const bool sel = m_mode == i;
            r.FillCircle(b.CenterX(), b.CenterY(), 20.0f, sel ? Color(255, 255, 255, 0x30) : Color(255, 255, 255, m_imm.Hot(kHkCamMode, i) ? 0x18 : 0x0A));
            r.Glyph(modes[i], {b.CenterX() - 9.0f, b.CenterY() - 9.0f, 18.0f, 18.0f}, white);
            m_imm.Add(b, kHkCamMode, i, i == 0 ? L"Фото" : L"Видео");
        }
        const Rect shutter{px + 18.0f, client.CenterY() - 10.0f, 60.0f, 60.0f};
        r.StrokeCircle(shutter.CenterX(), shutter.CenterY(), 28.0f, Color(255, 255, 255, 0x50), 2.0f);
        r.FillCircle(shutter.CenterX(), shutter.CenterY(), 22.0f, Color(255, 255, 255, 0x30));
        r.Glyph(m_mode == 0 ? Icon::Camera : Icon::Videos, {shutter.CenterX() - 10.0f, shutter.CenterY() - 10.0f, 20.0f, 20.0f}, Color(255, 255, 255, 0x60));
        m_ui.Draw(r);
    }

    void Activate(int kind, int index) override
    {
        if (kind == kHkCamMode)
            m_mode = index;
    }

    int m_mode = 0;
};

// ============================================================================ Карты
class MapsWindow : public MiniWindow
{
public:
    MapsWindow() : MiniWindow(AppId::Maps, 1180.0f, 760.0f) {}

private:
    Rect MapRect() const
    {
        const Rect v{0.0f, kTitleH, Width(), Height() - kTitleH};
        const float baseW = std::max(v.w, v.h * WorldMap::kAspect) * m_zoom;
        const float baseH = baseW / WorldMap::kAspect;
        return {v.CenterX() - baseW * 0.5f + m_pan.x, v.CenterY() - baseH * 0.5f + m_pan.y, baseW, baseH};
    }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        DrawTitle(r);
        const Rect v{0.0f, kTitleH, Width(), Height() - kTitleH};
        const Color ocean = p.dark ? Color::Hex(0x1E2A38) : Color::Hex(0xAAD3DF);
        const Color land = p.dark ? Color::Hex(0x3B4A3E) : Color::Hex(0xEAE6DC);
        r.FillRect(v, ocean);
        r.PushClip(v);
        m_imm.SetClip(v);
        const Rect m = MapRect();
        for (float dx : {-m.w, 0.0f, m.w})
        {
            const Rect mm = m.Translated(dx, 0.0f);
            if (mm.Right() < v.x || mm.x > v.Right())
                continue;
            WorldMap::FillShapes(r, mm, WorldMap::Land(), land);
            WorldMap::FillShapes(r, mm, WorldMap::Seas(), ocean);
            // Сетка параллелей и меридианов
            for (int lon = -150; lon <= 180; lon += 30)
            {
                const Point a = WorldMap::Project(mm, static_cast<float>(lon), WorldMap::kTopLat);
                r.FillRect({a.x, mm.y, 1.0f, mm.h}, Color(255, 255, 255, p.dark ? 0x10 : 0x40));
            }
            for (int lat = -45; lat <= 75; lat += 15)
            {
                const Point a = WorldMap::Project(mm, -180.0f, static_cast<float>(lat));
                r.FillRect({mm.x, a.y, mm.w, 1.0f}, Color(255, 255, 255, p.dark ? 0x10 : 0x40));
            }
            for (int i = 0; i < Cities::kCityCount; ++i)
            {
                const Cities::City& c = Cities::kCities[i];
                const Point q = WorldMap::Project(mm, c.lon, c.lat);
                if (!v.Contains(q))
                    continue;
                const bool hot = m_imm.Hot(kHkCity, i) || m_selCity == i;
                r.FillCircle(q.x, q.y, hot ? 6.0f : 4.0f, i == m_selCity ? Color::Hex(0xD13438) : (p.dark ? Color::Hex(0xE0E0E0) : Color::Hex(0x404040)));
                r.StrokeCircle(q.x, q.y, hot ? 6.0f : 4.0f, Color(255, 255, 255), 1.5f);
                if (m_zoom >= 1.6f || hot)
                    r.Text(c.name, {q.x + 8.0f, q.y - 10.0f, 160.0f, 20.0f}, p.text, 12.0f, hot ? FontWeight::Semibold : FontWeight::Regular, TextFlags::VCenter);
                m_imm.Add({q.x - 8.0f, q.y - 8.0f, 16.0f, 16.0f}, kHkCity, i, c.name);
            }
        }
        r.PopClip();
        m_imm.ResetClip();
        // Карточка выбранного города
        if (m_selCity >= 0)
        {
            const Cities::City& c = Cities::kCities[m_selCity];
            int off = 0;
            const SYSTEMTIME t = Cities::CityTime(c, off);
            const Rect card{20.0f, kTitleH + 20.0f, 320.0f, 132.0f};
            r.Shadow(card, 8.0f, 20.0f, 6.0f, 0.25f);
            r.FillRoundRect(card, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
            r.Text(c.name, {card.x + 20.0f, card.y + 16.0f, card.w - 40.0f, 28.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(c.country, {card.x + 20.0f, card.y + 46.0f, card.w - 40.0f, 20.0f}, p.textSecondary, 13.0f);
            wchar_t coords[64];
            swprintf(coords, 64, L"%.2f° %s, %.2f° %s", static_cast<double>(std::fabs(c.lat)), c.lat >= 0 ? L"с. ш." : L"ю. ш.",
                     static_cast<double>(std::fabs(c.lon)), c.lon >= 0 ? L"в. д." : L"з. д.");
            r.Text(coords, {card.x + 20.0f, card.y + 70.0f, card.w - 40.0f, 20.0f}, p.textSecondary, 12.0f);
            r.Text(L"Местное время: " + Pad2(t.wHour) + L":" + Pad2(t.wMinute), {card.x + 20.0f, card.y + 94.0f, card.w - 40.0f, 20.0f}, p.text, 13.0f);
        }
        // Кнопки масштаба
        const Rect zb{Width() - 60.0f, Height() - 120.0f, 40.0f, 80.0f};
        r.Shadow(zb, 6.0f, 12.0f, 4.0f, 0.2f);
        r.FillRoundRect(zb, 6.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        Imm::SubtleButton(r, m_imm, {zb.x + 2.0f, zb.y + 2.0f, 36.0f, 36.0f}, Icon::Add, kHkZoomIn, 0, L"Увеличить");
        r.FillRect({zb.x + 8.0f, zb.CenterY(), 24.0f, 1.0f}, p.divider);
        Imm::SubtleButton(r, m_imm, {zb.x + 2.0f, zb.y + 42.0f, 36.0f, 36.0f}, Icon::Minimize, kHkZoomOut, 0, L"Уменьшить");
        r.Text(L"Масштаб: " + std::to_wstring(static_cast<int>(m_zoom * 100.0f)) + L"%  ·  перетащите карту, чтобы переместить",
               {20.0f, Height() - 34.0f, 500.0f, 20.0f}, p.textSecondary, 12.0f);
        m_ui.Draw(r);
    }

    void Zoom(float f, Point at)
    {
        const Rect before = MapRect();
        const float nz = Clamp(m_zoom * f, 1.0f, 8.0f);
        const float k = nz / m_zoom;
        m_zoom = nz;
        // Точка под курсором остаётся на месте.
        const Rect v{0.0f, kTitleH, Width(), Height() - kTitleH};
        const Point rel{at.x - before.x, at.y - before.y};
        const Rect after = MapRect();
        m_pan.x += at.x - (after.x + rel.x * k);
        m_pan.y += at.y - (after.y + rel.y * k);
        const float maxY = std::max(0.0f, (MapRect().h - v.h) * 0.5f);
        m_pan.y = Clamp(m_pan.y, -maxY, maxY);
    }

    void Activate(int kind, int index) override
    {
        const Point center{Width() * 0.5f, kTitleH + (Height() - kTitleH) * 0.5f};
        if (kind == kHkZoomIn)
            Zoom(1.5f, center);
        else if (kind == kHkZoomOut)
            Zoom(1.0f / 1.5f, center);
        else if (kind == kHkCity)
            m_selCity = index;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        MiniWindow::OnMouseDown(e);
        const ImmHot* h = m_imm.Hit(e.pos);
        if (!h && e.pos.y > kTitleH && e.button == MouseButton::Left)
        {
            m_dragging = true;
            m_dragStart = e.pos;
            m_panStart = m_pan;
            m_selCity = -1;
        }
    }
    void OnMouseMove(const MouseEvent& e) override
    {
        MiniWindow::OnMouseMove(e);
        if (!m_dragging)
            return;
        m_pan = {m_panStart.x + e.pos.x - m_dragStart.x, m_panStart.y + e.pos.y - m_dragStart.y};
        const float maxY = std::max(0.0f, (MapRect().h - (Height() - kTitleH)) * 0.5f);
        m_pan.y = Clamp(m_pan.y, -maxY, maxY);
        // Карта бесконечна по горизонтали.
        const float w = MapRect().w;
        while (m_pan.x > w * 0.5f)
            m_pan.x -= w;
        while (m_pan.x < -w * 0.5f)
            m_pan.x += w;
    }
    void OnMouseUp(const MouseEvent& e) override
    {
        MiniWindow::OnMouseUp(e);
        m_dragging = false;
    }
    bool OnMouseWheel(const MouseEvent& e) override
    {
        Zoom(e.wheel > 0.0f ? 1.25f : 0.8f, e.pos);
        return true;
    }
    CursorType Cursor(Point local) const override
    {
        if (local.y > kTitleH && !m_imm.Hit(local))
            return CursorType::SizeAll;
        return MiniWindow::Cursor(local);
    }

    float m_zoom = 1.0f;
    Point m_pan;
    bool m_dragging = false;
    Point m_dragStart;
    Point m_panStart;
    int m_selCity = -1;
};

// ============================================================================ Советы
struct Tip
{
    Icon icon;
    const wchar_t* title;
    const wchar_t* text;
    AppId app;
    const wchar_t* arg;
};

const Tip kTips[] = {
    {Icon::Multitask, L"Прикрепляйте окна", L"Перетащите окно к краю экрана или нажмите Win + стрелку. Наведите курсор на кнопку «Развернуть», чтобы выбрать макет.",
     AppId::None, L""},
    {Icon::Personalize, L"Сделайте Windows своей", L"Выберите светлую или тёмную тему, цвет акцента и обои в разделе «Персонализация».", AppId::Settings,
     L"personalization"},
    {Icon::Focus, L"Сосредоточьтесь", L"Сеанс фокусировки в «Часах» включит режим «Не беспокоить» и напомнит о перерыве.", AppId::Clock, L"focus"},
    {Icon::Camera, L"Снимки экрана", L"Нажмите PrtScn — снимок попадёт в буфер обмена и в папку «Снимки экрана». Вставьте его в Paint клавишами Ctrl+V.",
     AppId::Paint, L""},
    {Icon::Terminal, L"Командная строка", L"В Терминале работают PowerShell и cmd: dir, cd, mkdir, start, tasklist и многое другое.", AppId::Terminal, L""},
    {Icon::Search, L"Ищите быстрее", L"Начните вводить текст в меню «Пуск», чтобы найти приложения, параметры и файлы.", AppId::None, L""},
};
constexpr int kTipCount = static_cast<int>(sizeof(kTips) / sizeof(kTips[0]));

class TipsWindow : public MiniWindow
{
public:
    TipsWindow() : MiniWindow(AppId::Tips, 1040.0f, 680.0f) {}

private:
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        DrawTitle(r);
        const Tip& t = kTips[m_cur];
        const Rect hero{32.0f, kTitleH + 16.0f, Width() - 64.0f, 300.0f};
        r.FillGradientV(hero, 12.0f, Theme::A().light2, Theme::A().base);
        r.FillCircle(hero.Right() - 170.0f, hero.CenterY(), 110.0f, Color(255, 255, 255, 0x24));
        r.Glyph(t.icon, {hero.Right() - 230.0f, hero.CenterY() - 60.0f, 120.0f, 120.0f}, Color(255, 255, 255), 1.6f);
        r.Text(L"Совет " + std::to_wstring(m_cur + 1) + L" из " + std::to_wstring(kTipCount), {hero.x + 36.0f, hero.y + 36.0f, 300.0f, 20.0f},
               Color(255, 255, 255, 0xD0), 13.0f);
        r.Text(t.title, {hero.x + 36.0f, hero.y + 64.0f, hero.w - 360.0f, 44.0f}, Color(255, 255, 255), 30.0f, FontWeight::Semibold);
        r.Text(t.text, {hero.x + 36.0f, hero.y + 120.0f, hero.w - 380.0f, 80.0f}, Color(255, 255, 255, 0xE8), 15.0f, FontWeight::Regular, TextFlags::Wrap);
        if (t.app != AppId::None)
            Imm::TextButton(r, m_imm, {hero.x + 36.0f, hero.Bottom() - 70.0f, 160.0f, 36.0f}, L"Попробовать", Icon::None, false, kHkTipTry);
        Imm::RoundButton(r, m_imm, {hero.x + hero.w * 0.5f - 52.0f, hero.Bottom() + 16.0f, 40.0f, 40.0f}, Icon::ChevronLeft, false, m_cur > 0, kHkTipPrev, 0,
                         L"Предыдущий совет");
        Imm::RoundButton(r, m_imm, {hero.x + hero.w * 0.5f + 12.0f, hero.Bottom() + 16.0f, 40.0f, 40.0f}, Icon::ChevronRight, false, m_cur + 1 < kTipCount,
                         kHkTipNext, 0, L"Следующий совет");
        // Все советы
        const float y = hero.Bottom() + 80.0f;
        r.Text(L"Все советы", {32.0f, y, 300.0f, 26.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        const float cw = (Width() - 64.0f - 2.0f * 16.0f) / 3.0f;
        for (int i = 0; i < kTipCount; ++i)
        {
            const Rect rc{32.0f + static_cast<float>(i % 3) * (cw + 16.0f), y + 40.0f + static_cast<float>(i / 3) * 76.0f, cw, 64.0f};
            Imm::Card(r, rc, m_imm.Hot(kHkTip, i) || i == m_cur);
            if (i == m_cur)
                r.FillRoundRect({rc.x, rc.y + 16.0f, 3.0f, rc.h - 32.0f}, 1.5f, p.accent);
            r.Glyph(kTips[i].icon, {rc.x + 18.0f, rc.CenterY() - 10.0f, 20.0f, 20.0f}, p.accentText);
            r.Text(kTips[i].title, {rc.x + 52.0f, rc.y, rc.w - 60.0f, rc.h}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            m_imm.Add(rc, kHkTip, i);
        }
        m_ui.Draw(r);
    }

    void Activate(int kind, int index) override
    {
        if (kind == kHkTip)
            m_cur = index;
        else if (kind == kHkTipPrev)
            m_cur = std::max(0, m_cur - 1);
        else if (kind == kHkTipNext)
            m_cur = std::min(kTipCount - 1, m_cur + 1);
        else if (kind == kHkTipTry && kTips[m_cur].app != AppId::None)
            GetShell().Launch(kTips[m_cur].app, kTips[m_cur].arg);
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (vk == VK_LEFT || vk == VK_RIGHT)
        {
            Activate(vk == VK_LEFT ? kHkTipPrev : kHkTipNext, 0);
            return true;
        }
        return MiniWindow::OnKeyDown(vk, mods);
    }

    int m_cur = 0;
};
} // namespace

std::unique_ptr<Window> CreatePlaceholder(AppId id, const std::wstring& /*arg*/)
{
    switch (id)
    {
    case AppId::Mail:
        return std::make_unique<MailWindow>();
    case AppId::Store:
        return std::make_unique<StoreWindow>();
    case AppId::Camera:
        return std::make_unique<CameraWindow>();
    case AppId::Maps:
        return std::make_unique<MapsWindow>();
    default:
        return std::make_unique<TipsWindow>();
    }
}
