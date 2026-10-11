// Браузер в стиле Windows 11: вкладки в строке заголовка, адресная строка с историей «назад/вперёд»,
// панель избранного, страница новой вкладки, поиск, внутренние страницы (новости, погода, справка,
// журнал, параметры). Сети нет, поэтому внешние адреса показывают страницу «Нет подключения».

#include "../Shell.h"
#include "AppFactories.h"
#include "ImmUi.h"
#include "Photos.h"
#include "TabStrip.h"
#include "WeatherData.h"

namespace
{
constexpr float kTitleH = 40.0f;
constexpr float kToolH = 46.0f;
constexpr float kFavH = 34.0f;

enum Hk : int
{
    kHkNone = 0,
    kHkBack,
    kHkForward,
    kHkRefresh,
    kHkHome,
    kHkStar,
    kHkMenu,
    kHkFavItem,
    kHkLink,     // переход по внутренней ссылке (index — в m_links)
    kHkSearchBox,
    kHkClearHistory,
};

struct NewsItem
{
    const wchar_t* title;
    const wchar_t* source;
    uint32_t c1, c2;
    Icon icon;
    const wchar_t* body;
};

const NewsItem kNews[] = {
    {L"Как продлить жизнь комнатным растениям осенью: 7 простых советов", L"Дом и сад", 0x3BAA5C, 0x1E6B3A, Icon::Sun,
     L"С наступлением осени световой день сокращается, а батареи отопления пересушивают воздух. Переставьте растения ближе к окнам, "
     L"поливайте реже, но следите, чтобы земляной ком не пересыхал полностью. Опрыскивание и поддон с влажной галькой помогут "
     L"поддерживать влажность, а подкормки лучше отложить до весны."},
    {L"Синоптики рассказали, какой будет погода в выходные", L"Погода", 0x4F8DF5, 0x2B4FB8, Icon::Rain,
     L"В ближайшие дни ожидается переменная облачность, местами кратковременные дожди. Ночью температура опустится на несколько "
     L"градусов, поэтому синоптики советуют взять с собой тёплую одежду и зонт. Подробный прогноз — в приложении «Погода»."},
    {L"Пять привычек, которые помогают лучше высыпаться", L"Здоровье", 0x8E63E6, 0x5B37B0, Icon::Moon,
     L"Ложитесь и вставайте в одно и то же время, за час до сна откладывайте экраны, проветривайте спальню и не пейте кофе после "
     L"обеда. Короткая вечерняя прогулка тоже помогает быстрее заснуть. А режим «Ночной свет» в Windows уменьшит синий оттенок экрана."},
    {L"Как устроены современные интерфейсы: тени, размытие и анимации", L"Технологии", 0xF08A3C, 0xC2521C, Icon::Code,
     L"Материалы вроде Mica и Acrylic создают ощущение глубины: фон окна слегка окрашивается цветом обоев, а всплывающие панели "
     L"размывают содержимое под собой. Мягкие тени и плавные анимации подсказывают, какие элементы находятся «выше», а скруглённые "
     L"углы делают интерфейс дружелюбнее."},
    {L"Маршрут выходного дня: парки и набережные города", L"Путешествия", 0x18A8B8, 0x0C6E7A, Icon::Location,
     L"Начните прогулку с утреннего кофе на набережной, затем пройдите через старый парк к смотровой площадке. Осенью здесь особенно "
     L"красиво: клёны окрашиваются в золотой и красный. Завершить маршрут можно в уютном кафе у реки."},
    {L"Рецепт тыквенного супа за 30 минут", L"Еда", 0xE8B23A, 0xB07A12, Icon::Heart,
     L"Обжарьте лук и морковь, добавьте нарезанную тыкву и картофель, залейте бульоном и варите 20 минут. Пробейте блендером, "
     L"добавьте сливки, соль и мускатный орех. Подавайте с гренками и тыквенными семечками."},
};
constexpr int kNewsCount = 6;

std::wstring UrlOfNews(int i) { return L"browser://news/" + std::to_wstring(i); }

bool LooksLikeUrl(const std::wstring& s)
{
    if (s.find(L' ') != std::wstring::npos)
        return false;
    if (s.rfind(L"http://", 0) == 0 || s.rfind(L"https://", 0) == 0 || s.rfind(L"browser://", 0) == 0)
        return true;
    const size_t dot = s.find(L'.');
    return dot != std::wstring::npos && dot > 0 && dot + 1 < s.size();
}

std::wstring HostOf(const std::wstring& url)
{
    std::wstring s = url;
    const size_t scheme = s.find(L"://");
    if (scheme != std::wstring::npos)
        s = s.substr(scheme + 3);
    const size_t slash = s.find(L'/');
    if (slash != std::wstring::npos)
        s = s.substr(0, slash);
    return s;
}

struct Tab
{
    std::vector<std::wstring> history;
    int pos = -1;
    float scroll = 0.0f;
    double loadStart = -1.0;
    const std::wstring& Url() const
    {
        static const std::wstring empty;
        return pos >= 0 ? history[static_cast<size_t>(pos)] : empty;
    }
};

std::vector<std::wstring>& Favorites()
{
    static std::vector<std::wstring> v = {L"browser://newtab", L"browser://weather", L"browser://news", L"browser://help"};
    return v;
}

std::vector<std::pair<std::wstring, SYSTEMTIME>>& History()
{
    static std::vector<std::pair<std::wstring, SYSTEMTIME>> v;
    return v;
}

std::wstring TitleOf(const std::wstring& url)
{
    if (url == L"browser://newtab")
        return L"Новая вкладка";
    if (url.rfind(L"browser://search?q=", 0) == 0)
        return url.substr(19) + L" — Поиск";
    if (url == L"browser://news")
        return L"Новости";
    if (url.rfind(L"browser://news/", 0) == 0)
    {
        const int i = Clamp(static_cast<int>(std::wcstol(url.c_str() + 15, nullptr, 10)), 0, kNewsCount - 1);
        return kNews[i].title;
    }
    if (url == L"browser://weather")
        return L"Погода";
    if (url == L"browser://help")
        return L"Справка по Windows 11";
    if (url == L"browser://history")
        return L"Журнал";
    if (url == L"browser://settings")
        return L"Параметры";
    return HostOf(url);
}

Icon IconOf(const std::wstring& url)
{
    if (url == L"browser://newtab")
        return Icon::Home;
    if (url.rfind(L"browser://search", 0) == 0)
        return Icon::Search;
    if (url.rfind(L"browser://news", 0) == 0)
        return Icon::Widgets;
    if (url == L"browser://weather")
        return Icon::PartlyCloudy;
    if (url == L"browser://help")
        return Icon::Info;
    if (url == L"browser://history")
        return Icon::History;
    if (url == L"browser://settings")
        return Icon::Settings;
    return Icon::Globe;
}

class BrowserWindow : public Window
{
public:
    explicit BrowserWindow(const std::wstring& arg) : Window(AppId::Browser, L"Браузер")
    {
        SetInitialSize(1240.0f, 800.0f);
        m_minSize = {560.0f, 420.0f};
        m_strip.reserveRight = 46.0f * 3.0f + 56.0f;
        m_address = m_ui.Add<TextBox>(L"Введите поисковый запрос или адрес");
        m_address->pill = true;
        m_address->leadingIcon = Icon::Search;
        m_address->onEnter = [this]() {
            const std::wstring t = Trim(m_address->Text());
            if (!t.empty())
                Navigate(t);
            m_ui.SetFocus(nullptr);
        };
        m_address->onEscape = [this]() {
            m_address->SetText(Cur().Url());
            m_ui.SetFocus(nullptr);
        };
        m_tabs.emplace_back();
        const std::wstring a = Trim(arg);
        Navigate(a.empty() ? L"browser://newtab" : a);
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnLaunchArgs(const std::wstring& arg) override
    {
        NewTab();
        if (!Trim(arg).empty())
            Navigate(Trim(arg));
    }

    void OnResize() override { Layout(); }

private:
    Tab& Cur() { return m_tabs[static_cast<size_t>(m_cur)]; }
    const Tab& Cur() const { return m_tabs[static_cast<size_t>(m_cur)]; }

    Rect PageRect() const
    {
        const float top = kTitleH + kToolH + (m_showFav ? kFavH : 0.0f);
        return {0.0f, top, Width(), Height() - top};
    }

    void Layout()
    {
        const float right = Width() - 12.0f - 3.0f * 40.0f;
        m_address->SetBounds({4.0f + 4.0f * 40.0f + 8.0f, kTitleH + 7.0f, std::max(120.0f, right - 172.0f - 8.0f), 32.0f});
    }

    void Navigate(std::wstring target, bool addHistory = true)
    {
        std::wstring url = target;
        if (!LooksLikeUrl(target))
            url = L"browser://search?q=" + target;
        else if (url.find(L"://") == std::wstring::npos)
            url = L"https://" + url;
        Tab& t = Cur();
        if (addHistory)
        {
            if (t.pos + 1 < static_cast<int>(t.history.size()))
                t.history.erase(t.history.begin() + t.pos + 1, t.history.end());
            t.history.push_back(url);
            t.pos = static_cast<int>(t.history.size()) - 1;
        }
        t.scroll = 0.0f;
        t.loadStart = NowSeconds();
        if (url != L"browser://newtab")
        {
            History().insert(History().begin(), {url, LocalNow()});
            if (History().size() > 200)
                History().pop_back();
        }
        m_address->SetText(url == L"browser://newtab" ? L"" : url);
        UpdateTitle();
    }

    void UpdateTitle() { SetTitle(TitleOf(Cur().Url()) + L" — Браузер"); }

    void Back()
    {
        Tab& t = Cur();
        if (t.pos > 0)
        {
            --t.pos;
            Navigate(t.Url(), false);
        }
    }
    void Forward()
    {
        Tab& t = Cur();
        if (t.pos + 1 < static_cast<int>(t.history.size()))
        {
            ++t.pos;
            Navigate(t.Url(), false);
        }
    }

    void NewTab()
    {
        m_tabs.emplace_back();
        m_cur = static_cast<int>(m_tabs.size()) - 1;
        Navigate(L"browser://newtab");
        m_ui.SetFocus(m_address);
    }

    void CloseTab(int i)
    {
        if (m_tabs.size() <= 1)
        {
            Close();
            return;
        }
        m_tabs.erase(m_tabs.begin() + i);
        m_cur = Clamp(m_cur > i ? m_cur - 1 : m_cur, 0, static_cast<int>(m_tabs.size()) - 1);
        m_address->SetText(Cur().Url() == L"browser://newtab" ? L"" : Cur().Url());
        UpdateTitle();
    }

    void SwitchTab(int i)
    {
        m_cur = Clamp(i, 0, static_cast<int>(m_tabs.size()) - 1);
        m_address->SetText(Cur().Url() == L"browser://newtab" ? L"" : Cur().Url());
        UpdateTitle();
    }

    int AddLink(const std::wstring& url)
    {
        m_links.push_back(url);
        return static_cast<int>(m_links.size()) - 1;
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        const float W = Width();
        m_imm.Begin({0.0f, 0.0f, W, Height()});
        m_links.clear();
        std::vector<TabInfo> infos;
        for (const Tab& t : m_tabs)
        {
            TabInfo ti;
            ti.title = TitleOf(t.Url());
            ti.glyph = IconOf(t.Url());
            infos.push_back(ti);
        }
        const Color bar = p.dark ? Color(255, 255, 255, 0x0D) : Color(255, 255, 255, 0xB3);
        m_strip.Draw(r, W, infos, m_cur, bar, p.text, p.subtleHover, p.divider);
        r.FillRect({0.0f, kTitleH, W, kToolH + (m_showFav ? kFavH : 0.0f)}, bar);
        DrawToolbar(r);
        const Rect pr = PageRect();
        r.FillRect({0.0f, pr.y - 1.0f, W, 1.0f}, p.divider);
        r.PushClip(pr);
        m_imm.SetClip(pr);
        m_contentH = 0.0f;
        DrawPage(r, pr);
        Imm::Scrollbar(r, pr, Cur().scroll, m_contentH);
        // Индикатор загрузки
        const double since = NowSeconds() - Cur().loadStart;
        if (Cur().loadStart >= 0.0 && since < 0.45)
        {
            const float t = static_cast<float>(since / 0.45);
            r.FillRect({0.0f, pr.y, W * ApplyEase(Ease::OutCubic, t), 2.0f}, p.accent);
            RequestFrame();
        }
        r.PopClip();
        m_imm.ResetClip();
        m_ui.Draw(r);
    }

    void DrawToolbar(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Tab& t = Cur();
        const float y = kTitleH + 7.0f;
        const bool canBack = t.pos > 0, canFwd = t.pos + 1 < static_cast<int>(t.history.size());
        auto btn = [&](float x, Icon icon, int kind, const wchar_t* tip, bool enabled, bool active = false, Color fg = Color(0, 0, 0, 0)) {
            const Rect rc{x, y, 36.0f, 32.0f};
            if (!enabled)
            {
                r.Glyph(icon, {rc.CenterX() - 8.0f, rc.CenterY() - 8.0f, 16.0f, 16.0f}, p.textDisabled);
                return;
            }
            Imm::SubtleButton(r, m_imm, rc, icon, kind, 0, tip, active, fg);
        };
        btn(4.0f, Icon::ArrowLeft, kHkBack, L"Назад (Alt+Стрелка влево)", canBack);
        btn(44.0f, Icon::ArrowRight, kHkForward, L"Вперёд (Alt+Стрелка вправо)", canFwd);
        btn(84.0f, Icon::Refresh, kHkRefresh, L"Обновить (F5)", true);
        btn(124.0f, Icon::Home, kHkHome, L"Главная", true);
        const bool fav = std::find(Favorites().begin(), Favorites().end(), t.Url()) != Favorites().end();
        const Rect ab = m_address->Bounds();
        btn(ab.Right() + 8.0f, Icon::Star, kHkStar, fav ? L"Удалить из избранного" : L"Добавить в избранное (Ctrl+D)", true, false,
            fav ? Color::Hex(0xF2B200) : Color(0, 0, 0, 0));
        btn(Width() - 12.0f - 36.0f, Icon::More, kHkMenu, L"Настройки и прочее (Alt+F)", true);
        r.DrawAppIcon(AppIcon::User, {Width() - 12.0f - 76.0f, y + 4.0f, 24.0f, 24.0f});
        if (!m_showFav)
            return;
        float x = 12.0f;
        const float fy = kTitleH + kToolH;
        for (int i = 0; i < static_cast<int>(Favorites().size()); ++i)
        {
            const std::wstring& u = Favorites()[static_cast<size_t>(i)];
            const std::wstring title = TitleOf(u);
            const float w = std::min(200.0f, r.TextWidth(title, 12.0f) + 40.0f);
            const Rect rc{x, fy + 3.0f, w, 28.0f};
            if (x + w > Width() - 12.0f)
                break;
            if (m_imm.Pressed(kHkFavItem, i))
                r.FillRoundRect(rc, 4.0f, p.subtlePressed);
            else if (m_imm.Hot(kHkFavItem, i))
                r.FillRoundRect(rc, 4.0f, p.subtleHover);
            r.Glyph(IconOf(u), {rc.x + 8.0f, rc.CenterY() - 7.0f, 14.0f, 14.0f}, p.textSecondary);
            r.Text(title, {rc.x + 28.0f, rc.y, rc.w - 32.0f, rc.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            m_imm.Add(rc, kHkFavItem, i, u);
            x += w + 4.0f;
        }
    }

    // ---- страницы -----------------------------------------------------------------
    void DrawPage(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        const std::wstring& url = Cur().Url();
        r.FillRect(pr, p.dark ? Color::Hex(0x1F1F1F) : Color::Hex(0xF7F7F7));
        if (url == L"browser://newtab")
            DrawNewTab(r, pr);
        else if (url.rfind(L"browser://search?q=", 0) == 0)
            DrawSearch(r, pr, url.substr(19));
        else if (url == L"browser://news")
            DrawNewsList(r, pr);
        else if (url.rfind(L"browser://news/", 0) == 0)
            DrawArticle(r, pr, Clamp(static_cast<int>(std::wcstol(url.c_str() + 15, nullptr, 10)), 0, kNewsCount - 1));
        else if (url == L"browser://weather")
            DrawWeather(r, pr);
        else if (url == L"browser://help")
            DrawHelp(r, pr);
        else if (url == L"browser://history")
            DrawHistory(r, pr);
        else if (url == L"browser://settings")
            DrawSettings(r, pr);
        else
            DrawOffline(r, pr, url);
    }

    float Sy() const { return Cur().scroll; }

    void LinkText(Renderer& r, const Rect& rc, const std::wstring& text, const std::wstring& url, float size, FontWeight w = FontWeight::Regular)
    {
        const Palette& p = Theme::P();
        const int id = AddLink(url);
        const bool hot = m_imm.Hot(kHkLink, id);
        r.Text(text, rc, hot ? p.accentText : (p.dark ? Color::Hex(0x99CCFF) : Color::Hex(0x1A0DAB)), size, w, TextFlags::Wrap);
        if (hot)
        {
            const float tw = std::min(rc.w, r.TextWidth(text, size, w));
            r.FillRect({rc.x, rc.y + r.LineHeight(size, w) - 2.0f, tw, 1.0f}, p.accentText);
        }
        m_imm.Add({rc.x, rc.y, std::min(rc.w, r.TextWidth(text, size, w) + 4.0f), r.LineHeight(size, w)}, kHkLink, id, url);
    }

    void DrawNewTab(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        // Фоновое изображение дня
        if (std::shared_ptr<Gdiplus::Bitmap> bg = PhotoBitmap(2))
        {
            const float iw = static_cast<float>(bg->GetWidth()), ih = static_cast<float>(bg->GetHeight());
            const float s = std::max(pr.w / iw, 340.0f / ih);
            r.PushClip({pr.x, pr.y - Sy(), pr.w, 340.0f});
            r.Image(bg.get(), {pr.CenterX() - iw * s * 0.5f, pr.y - Sy(), iw * s, ih * s});
            r.FillGradientV({pr.x, pr.y - Sy() + 200.0f, pr.w, 140.0f}, 0.0f, Color(0, 0, 0, 0), p.dark ? Color::Hex(0x1F1F1F) : Color::Hex(0xF7F7F7));
            r.PopClip();
        }
        const SYSTEMTIME now = LocalNow();
        r.Text(Pad2(now.wHour) + L":" + Pad2(now.wMinute), {pr.x, pr.y + 40.0f - Sy(), pr.w, 60.0f}, Color(255, 255, 255), 44.0f, FontWeight::Semibold,
               TextFlags::Center);
        // Поле поиска
        const float sw = std::min(640.0f, pr.w - 80.0f);
        const Rect sb{pr.CenterX() - sw * 0.5f, pr.y + 124.0f - Sy(), sw, 48.0f};
        r.Shadow(sb, 24.0f, 16.0f, 4.0f, 0.25f);
        r.FillRoundRect(sb, 24.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.Glyph(Icon::Search, {sb.x + 20.0f, sb.CenterY() - 9.0f, 18.0f, 18.0f}, p.textSecondary);
        r.Text(L"Поиск в Интернете", {sb.x + 52.0f, sb.y, sb.w - 70.0f, sb.h}, p.textSecondary, 15.0f, FontWeight::Regular, TextFlags::VCenter);
        m_imm.Add(sb, kHkSearchBox);
        // Быстрые ссылки
        struct Quick
        {
            const wchar_t* title;
            const wchar_t* url;
            AppIcon icon;
        };
        static const Quick quick[] = {{L"Погода", L"browser://weather", AppIcon::Weather},
                                      {L"Новости", L"browser://news", AppIcon::Store},
                                      {L"Справка", L"browser://help", AppIcon::Tips},
                                      {L"Журнал", L"browser://history", AppIcon::Clock},
                                      {L"Карты", L"https://maps.example.com", AppIcon::Maps},
                                      {L"Почта", L"https://mail.example.com", AppIcon::Mail}};
        const float qw = 96.0f, gap = 12.0f, total = 6.0f * qw + 5.0f * gap;
        float qx = pr.CenterX() - total * 0.5f;
        const float qy = pr.y + 200.0f - Sy();
        for (int i = 0; i < 6; ++i)
        {
            const Rect rc{qx, qy, qw, 96.0f};
            const int id = AddLink(quick[i].url);
            const bool hot = m_imm.Hot(kHkLink, id);
            r.FillRoundRect(rc, 8.0f, hot ? (p.dark ? Color(0x3A, 0x3A, 0x3A, 0xE0) : Color(255, 255, 255, 0xF0)) : (p.dark ? Color(0x2B, 0x2B, 0x2B, 0xC8) : Color(255, 255, 255, 0xC8)));
            r.FillCircle(rc.CenterX(), rc.y + 34.0f, 22.0f, p.dark ? Color(255, 255, 255, 0x14) : Color(0, 0, 0, 0x08));
            r.DrawAppIcon(quick[i].icon, {rc.CenterX() - 14.0f, rc.y + 20.0f, 28.0f, 28.0f});
            r.Text(quick[i].title, {rc.x, rc.y + 64.0f, rc.w, 20.0f}, p.text, 12.0f, FontWeight::Regular, TextFlags::Center);
            m_imm.Add(rc, kHkLink, id, quick[i].url);
            qx += qw + gap;
        }
        // Лента
        float y = pr.y + 340.0f - Sy();
        r.Text(L"Моя лента", {pr.CenterX() - 520.0f, y, 400.0f, 28.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 44.0f;
        y = DrawNewsGrid(r, {pr.CenterX() - std::min(520.0f, pr.w * 0.5f - 24.0f), y, std::min(1040.0f, pr.w - 48.0f), 0.0f});
        m_contentH = y + Sy() - pr.y + 32.0f;
    }

    float DrawNewsGrid(Renderer& r, const Rect& area)
    {
        const Palette& p = Theme::P();
        const float gap = 16.0f;
        const int cols = area.w >= 900.0f ? 3 : (area.w >= 560.0f ? 2 : 1);
        const float cw = (area.w - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols), ch = 250.0f;
        for (int i = 0; i < kNewsCount; ++i)
        {
            const Rect nc{area.x + static_cast<float>(i % cols) * (cw + gap), area.y + static_cast<float>(i / cols) * (ch + gap), cw, ch};
            const int id = AddLink(UrlOfNews(i));
            const bool hot = m_imm.Hot(kHkLink, id);
            if (hot)
                r.Shadow(nc, 8.0f, 16.0f, 4.0f, p.dark ? 0.4f : 0.15f);
            r.FillRoundRect(nc, 8.0f, p.card.a < 200 ? (p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF)) : p.card);
            r.StrokeRoundRect(nc, 8.0f, p.cardStroke);
            const Rect img{nc.x, nc.y, nc.w, 140.0f};
            r.FillRoundRect(img, 8.0f, 8.0f, 0.0f, 0.0f, Color::Hex(kNews[i].c2));
            r.FillGradientV(img.Inset(0.0f, 8.0f, 0.0f, 0.0f), 0.0f, Color::Hex(kNews[i].c1), Color::Hex(kNews[i].c2));
            r.FillRoundRect({img.x, img.y, img.w, 12.0f}, 8.0f, 8.0f, 0.0f, 0.0f, Color::Hex(kNews[i].c1));
            r.Glyph(kNews[i].icon, {img.CenterX() - 26.0f, img.CenterY() - 26.0f, 52.0f, 52.0f}, Color(255, 255, 255, 0xE0), 1.3f);
            r.Text(std::wstring(kNews[i].source) + L" · " + std::to_wstring(i * 2 + 1) + L" ч назад", {nc.x + 16.0f, nc.y + 150.0f, nc.w - 32.0f, 18.0f},
                   p.textSecondary, 12.0f);
            r.Text(kNews[i].title, {nc.x + 16.0f, nc.y + 172.0f, nc.w - 32.0f, 70.0f}, hot ? p.accentText : p.text, 15.0f, FontWeight::Semibold,
                   TextFlags::Wrap);
            m_imm.Add(nc, kHkLink, id, L"");
        }
        return area.y + static_cast<float>((kNewsCount + cols - 1) / cols) * (ch + gap) - gap;
    }

    void SearchHeader(Renderer& r, const Rect& pr, float& y, const std::wstring& title)
    {
        const Palette& p = Theme::P();
        r.Text(title, {pr.x + 40.0f, y, pr.w - 80.0f, 36.0f}, p.text, 22.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 48.0f;
    }

    void DrawSearch(Renderer& r, const Rect& pr, const std::wstring& q)
    {
        const Palette& p = Theme::P();
        const float x = pr.x + std::max(40.0f, (pr.w - 760.0f) * 0.3f), w = std::min(720.0f, pr.w - 80.0f);
        float y = pr.y + 24.0f - Sy();
        r.Text(L"Результаты поиска: «" + q + L"»", {x, y, w, 34.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 36.0f;
        // Внутренние совпадения
        struct Res
        {
            std::wstring title, url, snippet;
        };
        std::vector<Res> res;
        for (int i = 0; i < kNewsCount; ++i)
        {
            if (ContainsNoCase(kNews[i].title, q) || ContainsNoCase(kNews[i].body, q) || ContainsNoCase(kNews[i].source, q))
                res.push_back({kNews[i].title, UrlOfNews(i), std::wstring(kNews[i].body).substr(0, 150) + L"…"});
        }
        if (ContainsNoCase(L"погода прогноз температура weather", q))
            res.push_back({L"Погода сейчас и прогноз на 10 дней", L"browser://weather", L"Текущая температура, осадки, ветер и прогноз на ближайшие дни."});
        if (ContainsNoCase(L"windows справка помощь горячие клавиши сочетания help", q))
            res.push_back({L"Справка по Windows 11 — основные возможности и сочетания клавиш", L"browser://help",
                           L"Пуск, привязка окон, виртуальные рабочие столы, снимки экрана и другие возможности этой копии интерфейса."});
        // Общие (внешние) результаты
        const std::wstring slug = ToLower(q);
        res.push_back({q + L" — Энциклопедия", L"https://wiki.example.org/" + slug, L"Статья о «" + q + L"»: история, определения, интересные факты и ссылки на источники."});
        res.push_back({L"Что такое " + q + L"? Объяснение простыми словами", L"https://znanie.example.com/" + slug,
                       L"Разбираемся, что означает «" + q + L"», где это встречается и почему об этом говорят."});
        res.push_back({q + L": новости и обсуждения", L"https://forum.example.net/search?q=" + slug, L"Последние обсуждения, вопросы и ответы пользователей по теме."});
        r.Text(L"Найдено результатов: примерно " + std::to_wstring(res.size() * 1370 + q.size() * 211), {x, y, w, 20.0f}, p.textSecondary, 12.0f);
        y += 32.0f;
        for (const Res& it : res)
        {
            r.Glyph(Icon::Globe, {x, y + 2.0f, 14.0f, 14.0f}, p.textSecondary);
            r.Text(it.url, {x + 22.0f, y, w - 22.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 22.0f;
            LinkText(r, {x, y, w, 26.0f}, it.title, it.url, 18.0f);
            y += 30.0f;
            const float h = r.TextHeight(it.snippet, 13.0f, FontWeight::Regular, w);
            r.Text(it.snippet, {x, y, w, h + 4.0f}, p.text, 13.0f, FontWeight::Regular, TextFlags::Wrap);
            y += h + 26.0f;
        }
        m_contentH = y + Sy() - pr.y + 24.0f;
    }

    void DrawNewsList(Renderer& r, const Rect& pr)
    {
        float y = pr.y + 24.0f - Sy();
        SearchHeader(r, pr, y, L"Новости");
        const float w = std::min(1040.0f, pr.w - 80.0f);
        y = DrawNewsGrid(r, {pr.CenterX() - w * 0.5f, y, w, 0.0f});
        m_contentH = y + Sy() - pr.y + 32.0f;
    }

    void DrawArticle(Renderer& r, const Rect& pr, int i)
    {
        const Palette& p = Theme::P();
        const NewsItem& n = kNews[i];
        const float w = std::min(760.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + 32.0f - Sy();
        r.Text(n.source, {x, y, w, 20.0f}, p.accentText, 13.0f, FontWeight::Semibold);
        y += 28.0f;
        const float th = r.TextHeight(n.title, 28.0f, FontWeight::Semibold, w);
        r.Text(n.title, {x, y, w, th + 4.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::Wrap);
        y += th + 20.0f;
        const Rect img{x, y, w, 320.0f};
        r.FillGradientV(img, 8.0f, Color::Hex(n.c1), Color::Hex(n.c2));
        r.Glyph(n.icon, {img.CenterX() - 48.0f, img.CenterY() - 48.0f, 96.0f, 96.0f}, Color(255, 255, 255, 0xE0), 1.5f);
        y += 340.0f;
        for (int k = 0; k < 2; ++k)
        {
            const std::wstring body = k == 0 ? std::wstring(n.body)
                                             : std::wstring(L"Это демонстрационная статья: содержимое создано локально, без подключения к Интернету. "
                                                            L"Другие материалы можно найти в разделе «Новости» или с помощью поиска.");
            const float h = r.TextHeight(body, 16.0f, FontWeight::Regular, w);
            r.Text(body, {x, y, w, h + 4.0f}, p.text, 16.0f, FontWeight::Regular, TextFlags::Wrap);
            y += h + 20.0f;
        }
        LinkText(r, {x, y, w, 24.0f}, L"← Все новости", L"browser://news", 14.0f);
        y += 48.0f;
        m_contentH = y + Sy() - pr.y;
    }

    void DrawWeather(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        using namespace WeatherData;
        const int city = HomeCity();
        const NowWx n = Now(city);
        const float w = std::min(900.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + 28.0f - Sy();
        const Rect card{x, y, w, 220.0f};
        r.FillGradientV(card, 12.0f, n.night ? Color::Hex(0x0B1630) : Color::Hex(0x2E7FE0), n.night ? Color::Hex(0x2B4470) : Color::Hex(0x86C3F6));
        const Color white(255, 255, 255);
        r.Text(std::wstring(L"Погода: ") + Cities::kCities[city].name, {card.x + 28.0f, card.y + 20.0f, w - 56.0f, 28.0f}, white, 18.0f, FontWeight::Semibold);
        DrawIcon(r, {card.x + 24.0f, card.y + 64.0f, 96.0f, 96.0f}, n.cond, n.night);
        r.Text(Deg(n.temp), {card.x + 136.0f, card.y + 58.0f, 200.0f, 100.0f}, white, 72.0f, FontWeight::Light, TextFlags::VCenter);
        r.Text(CondText(n.cond, n.night), {card.x + 28.0f, card.y + 170.0f, 300.0f, 24.0f}, white, 16.0f, FontWeight::Semibold);
        r.Text(L"Ощущается как " + Deg(n.feels) + L" · Влажность " + std::to_wstring(n.humidity) + L"% · Ветер " +
                   std::to_wstring(static_cast<int>(std::lround(n.wind))) + L" м/с",
               {card.x + 28.0f, card.y + 192.0f, w - 56.0f, 20.0f}, Color(255, 255, 255, 0xD0), 13.0f);
        // 5 дней
        const float dw = (w - 4.0f * 12.0f) / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const DayWx d = Day(city, n.today + 1 + i);
            const SYSTEMTIME date = DateOf(d.day);
            static const wchar_t* wd[] = {L"Вс", L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб"};
            const Rect dc{x + static_cast<float>(i) * (dw + 12.0f), card.Bottom() + 20.0f, dw, 150.0f};
            Imm::Card(r, dc);
            r.Text(std::wstring(wd[date.wDayOfWeek]) + L", " + std::to_wstring(date.wDay), {dc.x, dc.y + 12.0f, dc.w, 20.0f}, p.text, 14.0f, FontWeight::Semibold,
                   TextFlags::Center);
            DrawIcon(r, {dc.CenterX() - 24.0f, dc.y + 40.0f, 48.0f, 48.0f}, d.cond, false, !p.dark);
            r.Text(Deg(d.hi) + L"  " + Deg(d.lo), {dc.x, dc.y + 96.0f, dc.w, 22.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Center);
            r.Text(std::wstring(CondText(d.cond)), {dc.x + 4.0f, dc.y + 120.0f, dc.w - 8.0f, 18.0f}, p.textSecondary, 11.0f, FontWeight::Regular,
                   TextFlags::Center);
        }
        y = card.Bottom() + 190.0f;
        LinkText(r, {x, y, w, 24.0f}, L"Подробный прогноз в приложении «Погода» →", L"app:weather", 14.0f);
        y += 48.0f;
        m_contentH = y + Sy() - pr.y;
    }

    void DrawHelp(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        const float w = std::min(820.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + 32.0f - Sy();
        r.Text(L"Справка по Windows 11", {x, y, w, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 52.0f;
        const std::wstring intro = L"Эта оболочка воспроизводит интерфейс Windows 11 в одном окне: рабочий стол, панель задач, меню «Пуск», "
                                   L"центр уведомлений, быстрые параметры и набор приложений. Все окна, значки и обои нарисованы программно.";
        const float ih = r.TextHeight(intro, 15.0f, FontWeight::Regular, w);
        r.Text(intro, {x, y, w, ih + 4.0f}, p.text, 15.0f, FontWeight::Regular, TextFlags::Wrap);
        y += ih + 28.0f;
        r.Text(L"Сочетания клавиш", {x, y, w, 28.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 40.0f;
        static const wchar_t* keys[][2] = {
            {L"Win", L"Открыть или закрыть меню «Пуск»"},
            {L"Win + E", L"Проводник"},
            {L"Win + I", L"Параметры"},
            {L"Win + A / Win + N", L"Быстрые параметры / центр уведомлений и календарь"},
            {L"Win + W", L"Виджеты"},
            {L"Win + D / Win + M", L"Показать рабочий стол / свернуть все окна"},
            {L"Win + Tab", L"Представление задач"},
            {L"Win + стрелки", L"Прикрепить окно к краю экрана или развернуть его"},
            {L"Win + X", L"Меню быстрых ссылок"},
            {L"Win + L", L"Заблокировать экран"},
            {L"Alt + Tab", L"Переключение между окнами"},
            {L"Alt + F4", L"Закрыть окно (на рабочем столе — завершение работы)"},
            {L"PrtScn", L"Снимок экрана в буфер обмена и в папку «Снимки экрана»"},
            {L"Ctrl + Shift + Esc", L"Диспетчер задач"},
        };
        for (const auto& k : keys)
        {
            const Rect row{x, y, w, 40.0f};
            Imm::Card(r, row);
            const float kw = r.TextWidth(k[0], 13.0f, FontWeight::Semibold) + 20.0f;
            r.FillRoundRect({row.x + 12.0f, row.y + 8.0f, kw, 24.0f}, 4.0f, p.control);
            r.StrokeRoundRect({row.x + 12.0f, row.y + 8.0f, kw, 24.0f}, 4.0f, p.controlStrongStroke);
            r.Text(k[0], {row.x + 12.0f, row.y + 8.0f, kw, 24.0f}, p.text, 13.0f, FontWeight::Semibold, TextFlags::Middle);
            r.Text(k[1], {row.x + 200.0f, row.y, row.w - 210.0f, row.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 46.0f;
        }
        y += 16.0f;
        r.Text(L"Советы", {x, y, w, 28.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 36.0f;
        static const wchar_t* tips[] = {
            L"Перетащите окно к краю экрана, чтобы прикрепить его, или наведите курсор на кнопку «Развернуть», чтобы выбрать макет.",
            L"Тему, цвет акцента, обои и прозрачность можно изменить в «Параметры» → «Персонализация» — изменения применяются сразу.",
            L"Снимки экрана можно вставить в Paint сочетанием Ctrl+V и сохранить в папку «Изображения».",
            L"Сеанс фокусировки в «Часах» включает режим «Не беспокоить» и выключает его по окончании.",
            L"Чтобы выйти из оболочки, откройте «Пуск» → «Завершение работы» или нажмите Alt+F4 на рабочем столе.",
        };
        for (const wchar_t* t : tips)
        {
            r.FillCircle(x + 6.0f, y + 10.0f, 3.0f, p.accent);
            const float h = r.TextHeight(t, 14.0f, FontWeight::Regular, w - 24.0f);
            r.Text(t, {x + 20.0f, y, w - 20.0f, h + 4.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Wrap);
            y += h + 12.0f;
        }
        m_contentH = y + Sy() - pr.y + 32.0f;
    }

    void DrawHistory(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        const float w = std::min(820.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + 28.0f - Sy();
        r.Text(L"Журнал", {x, y, w - 200.0f, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        if (!History().empty())
            Imm::TextButton(r, m_imm, {x + w - 180.0f, y + 4.0f, 180.0f, 32.0f}, L"Очистить журнал", Icon::Delete, false, kHkClearHistory);
        y += 56.0f;
        if (History().empty())
            r.Text(L"Здесь появятся страницы, которые вы посещали.", {x, y, w, 24.0f}, p.textSecondary, 14.0f);
        for (const auto& h : History())
        {
            const Rect row{x, y, w, 44.0f};
            Imm::Card(r, row);
            r.Glyph(IconOf(h.first), {row.x + 14.0f, row.CenterY() - 8.0f, 16.0f, 16.0f}, p.textSecondary);
            r.Text(Pad2(h.second.wHour) + L":" + Pad2(h.second.wMinute), {row.Right() - 70.0f, row.y, 56.0f, row.h}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
            LinkText(r, {row.x + 42.0f, row.y + 12.0f, row.w - 130.0f, 22.0f}, TitleOf(h.first), h.first, 14.0f);
            y += 50.0f;
            if (y > pr.Bottom() + 200.0f)
                break;
        }
        m_contentH = y + Sy() - pr.y + 24.0f;
    }

    void DrawSettings(Renderer& r, const Rect& pr)
    {
        const Palette& p = Theme::P();
        const float w = std::min(820.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + 28.0f - Sy();
        r.Text(L"Параметры", {x, y, w, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 60.0f;
        const Rect a{x, y, w, 72.0f};
        Imm::Card(r, a);
        r.Text(L"Показывать панель избранного", {a.x + 20.0f, a.y + 14.0f, a.w - 120.0f, 22.0f}, p.text, 14.0f);
        r.Text(L"Ctrl+Shift+B", {a.x + 20.0f, a.y + 36.0f, a.w - 120.0f, 20.0f}, p.textSecondary, 12.0f);
        const Rect tg{a.Right() - 64.0f, a.CenterY() - 10.0f, 40.0f, 20.0f};
        const int id = AddLink(L"cmd:favbar");
        Imm::Toggle(r, tg, m_showFav, m_imm.Hot(kHkLink, id));
        m_imm.Add(tg.Inflated(6.0f), kHkLink, id);
        y += 88.0f;
        const Rect b{x, y, w, 72.0f};
        Imm::Card(r, b);
        r.Text(L"Внешний вид", {b.x + 20.0f, b.y + 14.0f, b.w - 40.0f, 22.0f}, p.text, 14.0f);
        r.Text(std::wstring(L"Используется тема Windows: ") + (p.dark ? L"тёмная" : L"светлая"), {b.x + 20.0f, b.y + 36.0f, b.w - 40.0f, 20.0f},
               p.textSecondary, 12.0f);
        y += 88.0f;
        const Rect c{x, y, w, 96.0f};
        Imm::Card(r, c);
        r.DrawAppIcon(AppIcon::Browser, {c.x + 20.0f, c.y + 20.0f, 32.0f, 32.0f});
        r.Text(L"О программе «Браузер»", {c.x + 68.0f, c.y + 16.0f, c.w - 90.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(L"Версия 130.0.2849.56 (демонстрационная)", {c.x + 68.0f, c.y + 38.0f, c.w - 90.0f, 20.0f}, p.textSecondary, 12.0f);
        r.Text(L"Встроенные страницы работают без сети; внешние адреса недоступны.", {c.x + 68.0f, c.y + 60.0f, c.w - 90.0f, 20.0f}, p.textSecondary, 12.0f);
        y += 112.0f;
        m_contentH = y + Sy() - pr.y;
    }

    void DrawOffline(Renderer& r, const Rect& pr, const std::wstring& url)
    {
        const Palette& p = Theme::P();
        const ShellSettings& st = GetShell().Settings();
        const bool disconnected = !st.wifi || st.airplane;
        const float w = std::min(620.0f, pr.w - 80.0f), x = pr.CenterX() - w * 0.5f;
        float y = pr.y + std::max(40.0f, pr.h * 0.16f) - Sy();
        // Значок: глобус с перечёркиванием
        r.Glyph(Icon::Globe, {x, y, 56.0f, 56.0f}, p.textSecondary, 1.3f);
        r.Line(x + 4.0f, y + 52.0f, x + 52.0f, y + 4.0f, p.critical, 2.5f);
        y += 80.0f;
        r.Text(disconnected ? L"Нет подключения к Интернету" : L"Не удаётся получить доступ к сайту", {x, y, w, 40.0f}, p.text, 26.0f, FontWeight::Semibold,
               TextFlags::VCenter);
        y += 52.0f;
        const std::wstring msg = disconnected ? std::wstring(L"Вы не подключены к сети. Включите Wi-Fi или отключите режим «в самолёте» в быстрых параметрах.")
                                              : L"Не удаётся найти IP-адрес сервера " + HostOf(url) + L". Эта демонстрационная копия Windows работает без доступа к Интернету.";
        const float mh = r.TextHeight(msg, 15.0f, FontWeight::Regular, w);
        r.Text(msg, {x, y, w, mh + 4.0f}, p.text, 15.0f, FontWeight::Regular, TextFlags::Wrap);
        y += mh + 20.0f;
        r.Text(L"Попробуйте сделать следующее:", {x, y, w, 22.0f}, p.text, 14.0f);
        y += 30.0f;
        static const wchar_t* tips[] = {L"Проверьте сетевые кабели, модем и маршрутизатор", L"Повторно подключитесь к сети Wi-Fi",
                                        L"Воспользуйтесь встроенными страницами: новости, погода, справка"};
        for (const wchar_t* t : tips)
        {
            r.FillCircle(x + 6.0f, y + 10.0f, 2.5f, p.text);
            r.Text(t, {x + 20.0f, y, w - 20.0f, 22.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            y += 26.0f;
        }
        y += 12.0f;
        r.Text(disconnected ? L"ERR_INTERNET_DISCONNECTED" : L"DNS_PROBE_FINISHED_NO_INTERNET", {x, y, w, 20.0f}, p.textSecondary, 12.0f);
        y += 40.0f;
        Imm::TextButton(r, m_imm, {x, y, 140.0f, 32.0f}, L"Обновить", Icon::None, true, kHkRefresh, 1);
        LinkText(r, {x + 160.0f, y + 6.0f, 240.0f, 22.0f}, L"Открыть новую вкладку", L"browser://newtab", 14.0f);
        m_contentH = 0.0f;
    }

    // ================================================================== действия
    void OpenLink(const std::wstring& url)
    {
        if (url == L"app:weather")
        {
            GetShell().Launch(AppId::Weather, L"");
            return;
        }
        if (url == L"cmd:favbar")
        {
            m_showFav = !m_showFav;
            return;
        }
        Navigate(url);
    }

    void ShowMainMenu()
    {
        std::vector<MenuItem> items = {
            MenuItem(L"Новая вкладка", Icon::NewTab, [this]() { NewTab(); }, L"Ctrl+T"),
            MenuItem(L"Новое окно", Icon::Add, []() { GetShell().Launch(AppId::Browser, L""); }, L"Ctrl+N"),
            MenuItem::Sep(),
            MenuItem(L"Избранное", Icon::Star, [this]() { m_showFav = !m_showFav; }, L"Ctrl+Shift+B"),
            MenuItem(L"Журнал", Icon::History, [this]() { Navigate(L"browser://history"); }, L"Ctrl+H"),
            MenuItem(L"Новости", Icon::Widgets, [this]() { Navigate(L"browser://news"); }),
            MenuItem::Sep(),
            MenuItem(L"Справка и отзывы", Icon::Info, [this]() { Navigate(L"browser://help"); }, L"F1"),
            MenuItem(L"Параметры", Icon::Settings, [this]() { Navigate(L"browser://settings"); }),
            MenuItem::Sep(),
            MenuItem(L"Закрыть браузер", Icon::Close, [this]() { Close(); }),
        };
        GetShell().ShowMenuBelow(LocalToScreen(Rect{Width() - 48.0f, kTitleH + 7.0f, 36.0f, 32.0f}), std::move(items), 280.0f);
    }

    void Activate(int kind, int index)
    {
        switch (kind)
        {
        case kHkBack:
            Back();
            break;
        case kHkForward:
            Forward();
            break;
        case kHkRefresh:
            Cur().loadStart = NowSeconds();
            break;
        case kHkHome:
            Navigate(L"browser://newtab");
            break;
        case kHkStar:
        {
            auto& f = Favorites();
            auto it = std::find(f.begin(), f.end(), Cur().Url());
            if (it != f.end())
                f.erase(it);
            else
                f.push_back(Cur().Url());
            break;
        }
        case kHkMenu:
            ShowMainMenu();
            break;
        case kHkFavItem:
            if (index < static_cast<int>(Favorites().size()))
                Navigate(Favorites()[static_cast<size_t>(index)]);
            break;
        case kHkLink:
            if (index < static_cast<int>(m_links.size()))
                OpenLink(m_links[static_cast<size_t>(index)]);
            break;
        case kHkSearchBox:
            m_ui.SetFocus(m_address);
            break;
        case kHkClearHistory:
            History().clear();
            break;
        default:
            break;
        }
    }

    // ================================================================== ввод
    bool IsDragArea(Point local) const override
    {
        if (local.y >= kTitleH || local.x > Width() - 46.0f * 3.0f)
            return false;
        const int n = static_cast<int>(m_tabs.size());
        return m_strip.HitTest(local, Width(), n, nullptr) < 0 && !m_strip.NewRect(Width(), n).Contains(local);
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        const int n = static_cast<int>(m_tabs.size());
        m_strip.UpdateHover(e.pos, Width(), n);
        if (m_strip.hover >= 0)
            GetShell().Tooltip(&m_strip, TitleOf(m_tabs[static_cast<size_t>(m_strip.hover)].Url()), LocalToScreen(m_strip.TabRect(m_strip.hover, Width(), n)),
                               false);
        else if (m_strip.hoverNew)
            GetShell().Tooltip(&m_tabs, L"Новая вкладка (Ctrl+T)", LocalToScreen(m_strip.NewRect(Width(), n)), false);
        m_imm.Move(e.pos, {Frame().x, Frame().y});
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_strip.ClearHover();
        m_imm.Leave();
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        if (e.pos.y < kTitleH)
        {
            bool onClose = false;
            const int n = static_cast<int>(m_tabs.size());
            const int tab = m_strip.HitTest(e.pos, Width(), n, &onClose);
            if (tab >= 0)
            {
                if (onClose || e.button == MouseButton::Middle)
                    CloseTab(tab);
                else
                    SwitchTab(tab);
                return;
            }
            if (m_strip.NewRect(Width(), n).Contains(e.pos))
            {
                NewTab();
                return;
            }
        }
        const bool wasFocused = m_address->Focused();
        Window::OnMouseDown(e);
        if (m_ui.HasCapture())
        {
            if (!wasFocused && m_address->Focused())
                m_address->SelectAll();
            return;
        }
        if (m_address->Focused())
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
        if (PageRect().Contains(e.pos))
            Cur().scroll = Clamp(Cur().scroll - e.wheel * 100.0f, 0.0f, std::max(0.0f, m_contentH - PageRect().h));
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return Window::OnKeyDown(vk, mods);
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'T':
                NewTab();
                return true;
            case 'W':
                CloseTab(m_cur);
                return true;
            case 'L':
                m_ui.SetFocus(m_address);
                m_address->SelectAll();
                return true;
            case 'H':
                Navigate(L"browser://history");
                return true;
            case 'N':
                GetShell().Launch(AppId::Browser, L"");
                return true;
            case 'D':
                Activate(kHkStar, 0);
                return true;
            case 'B':
                if (mods.shift)
                {
                    m_showFav = !m_showFav;
                    return true;
                }
                break;
            case VK_TAB:
                SwitchTab((m_cur + (mods.shift ? -1 : 1) + static_cast<int>(m_tabs.size())) % static_cast<int>(m_tabs.size()));
                return true;
            default:
                break;
            }
        }
        if (mods.alt && vk == VK_LEFT)
        {
            Back();
            return true;
        }
        if (mods.alt && vk == VK_RIGHT)
        {
            Forward();
            return true;
        }
        if (vk == VK_F5)
        {
            Cur().loadStart = NowSeconds();
            return true;
        }
        if (vk == VK_F1)
        {
            Navigate(L"browser://help");
            return true;
        }
        if (m_ui.FocusedChild() == nullptr && (vk == VK_PRIOR || vk == VK_NEXT || vk == VK_HOME || vk == VK_END))
        {
            const float page = PageRect().h - 60.0f, mx = std::max(0.0f, m_contentH - PageRect().h);
            float& s = Cur().scroll;
            s = vk == VK_HOME ? 0.0f : (vk == VK_END ? mx : Clamp(s + (vk == VK_NEXT ? page : -page), 0.0f, mx));
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

    CursorType Cursor(Point local) const override
    {
        if (!HasDialog())
        {
            const ImmHot* h = m_imm.Hit(local);
            if (h && (h->kind == kHkLink || h->kind == kHkSearchBox))
                return CursorType::Hand;
        }
        return Window::Cursor(local);
    }

    // ================================================================== поля
    std::vector<Tab> m_tabs;
    int m_cur = 0;
    TabStrip m_strip;
    TextBox* m_address = nullptr;
    ImmUi m_imm;
    std::vector<std::wstring> m_links;
    float m_contentH = 0.0f;
    bool m_showFav = true;
};
} // namespace

std::unique_ptr<Window> CreateBrowser(const std::wstring& arg) { return std::make_unique<BrowserWindow>(arg); }
