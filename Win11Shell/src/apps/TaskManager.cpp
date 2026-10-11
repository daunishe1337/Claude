// Диспетчер задач Windows 11: процессы (группы, тепловая карта, сортировка, снятие задачи),
// производительность (реальные ЦП и память через GetSystemTimes/GlobalMemoryStatusEx, графики),
// журнал приложений, автозагрузка, пользователи, подробности, службы и параметры.

#include "../Shell.h"
#include "AppFactories.h"
#include "SysInfo.h"

#include <set>

namespace
{
constexpr float kTitleH = 48.0f;
constexpr float kRowH = 32.0f;
constexpr int kHistory = 60;

enum class Page
{
    Processes,
    Performance,
    AppHistory,
    Startup,
    Users,
    Details,
    Services,
    Settings,
};

struct NavDef
{
    Page page;
    const wchar_t* title;
    Icon icon;
};
const NavDef kNav[] = {
    {Page::Processes, L"Процессы", Icon::Processes},       {Page::Performance, L"Производительность", Icon::Performance},
    {Page::AppHistory, L"Журнал приложений", Icon::History}, {Page::Startup, L"Автозагрузка приложений", Icon::Startup},
    {Page::Users, L"Пользователи", Icon::User},             {Page::Details, L"Подробности", Icon::Details},
    {Page::Services, L"Службы", Icon::Services},
};

struct Proc
{
    int pid = 0;
    std::wstring name;
    std::wstring exe;
    std::wstring desc;
    AppIcon icon = AppIcon::None;
    int group = 0; // 0 — приложения, 1 — фоновые, 2 — процессы Windows
    AppId app = AppId::None;
    std::vector<Window*> windows;
    float cpu = 0.0f; // %
    float mem = 0.0f; // МБ
    float disk = 0.0f; // МБ/с
    float net = 0.0f;  // Мбит/с
    float weight = 1.0f;
    int threads = 8;
    bool efficiency = false;
    std::wstring user;
};

struct BgDef
{
    const wchar_t* name;
    const wchar_t* exe;
    int group;
    float cpuWeight;
    float memMb;
};
const BgDef kBackground[] = {
    {L"Antimalware Service Executable", L"MsMpEng.exe", 1, 1.6f, 182.0f},
    {L"Узел службы: локальная система", L"svchost.exe", 1, 0.8f, 96.0f},
    {L"Узел службы: сетевая служба", L"svchost.exe", 1, 0.5f, 24.0f},
    {L"Узел службы: локальная служба", L"svchost.exe", 1, 0.4f, 31.0f},
    {L"Поиск", L"SearchHost.exe", 1, 0.3f, 118.0f},
    {L"Меню «Пуск»", L"StartMenuExperienceHost.exe", 1, 0.2f, 64.0f},
    {L"Мини-приложения", L"Widgets.exe", 1, 0.2f, 42.0f},
    {L"Хост окон консоли", L"conhost.exe", 1, 0.1f, 6.0f},
    {L"Служба индексирования", L"SearchIndexer.exe", 1, 0.6f, 38.0f},
    {L"Защитник: хост службы", L"SecurityHealthService.exe", 1, 0.1f, 9.0f},
    {L"Запуск в фоновом режиме", L"RuntimeBroker.exe", 1, 0.2f, 18.0f},
    {L"Параметры звука", L"audiodg.exe", 1, 0.3f, 14.0f},
    {L"Диспетчер окон рабочего стола", L"dwm.exe", 2, 2.2f, 96.0f},
    {L"Проводник", L"explorer.exe", 2, 1.1f, 132.0f},
    {L"Процесс входа в Windows", L"winlogon.exe", 2, 0.05f, 7.0f},
    {L"Приложение служб и контроллеров", L"services.exe", 2, 0.2f, 9.0f},
    {L"Процесс исполнения клиент-сервер", L"csrss.exe", 2, 0.3f, 5.0f},
    {L"Локальная система безопасности", L"lsass.exe", 2, 0.2f, 22.0f},
    {L"Запуск приложений Windows", L"wininit.exe", 2, 0.02f, 6.0f},
    {L"Системные прерывания", L"—", 2, 0.4f, 0.0f},
    {L"System", L"System", 2, 0.9f, 0.1f},
    {L"Registry", L"Registry", 2, 0.02f, 52.0f},
};

struct ServiceDef
{
    const wchar_t* name;
    const wchar_t* desc;
    const wchar_t* group;
    bool running;
};
const ServiceDef kServices[] = {
    {L"AudioSrv", L"Windows Audio", L"LocalServiceNetworkRestricted", true},
    {L"BITS", L"Фоновая интеллектуальная служба передачи (BITS)", L"netsvcs", false},
    {L"BthAvctpSvc", L"Служба AVCTP", L"LocalService", true},
    {L"CryptSvc", L"Службы криптографии", L"NetworkService", true},
    {L"DcomLaunch", L"Модуль запуска процессов DCOM-сервера", L"DcomLaunch", true},
    {L"Dhcp", L"DHCP-клиент", L"LocalServiceNetworkRestricted", true},
    {L"Dnscache", L"DNS-клиент", L"NetworkService", true},
    {L"EventLog", L"Журнал событий Windows", L"LocalServiceNetworkRestricted", true},
    {L"FontCache", L"Служба кэша шрифтов Windows", L"LocalService", true},
    {L"LanmanWorkstation", L"Рабочая станция", L"NetworkService", true},
    {L"MpsSvc", L"Брандмауэр Защитника Windows", L"LocalServiceNoNetworkFirewall", true},
    {L"nsi", L"Служба интерфейса сохранения сети", L"LocalService", true},
    {L"Power", L"Питание", L"DcomLaunch", true},
    {L"ProfSvc", L"Служба профилей пользователей", L"netsvcs", true},
    {L"RpcSs", L"Удалённый вызов процедур (RPC)", L"rpcss", true},
    {L"Schedule", L"Планировщик заданий", L"netsvcs", true},
    {L"SENS", L"Служба уведомления о системных событиях", L"netsvcs", true},
    {L"Spooler", L"Диспетчер печати", L"", true},
    {L"SysMain", L"SysMain", L"LocalSystemNetworkRestricted", true},
    {L"Themes", L"Темы", L"netsvcs", true},
    {L"TrustedInstaller", L"Установщик модулей Windows", L"", false},
    {L"W32Time", L"Служба времени Windows", L"LocalService", false},
    {L"WlanSvc", L"Служба автонастройки WLAN", L"LocalSystemNetworkRestricted", true},
    {L"WSearch", L"Windows Search", L"", true},
    {L"wuauserv", L"Центр обновления Windows", L"netsvcs", false},
};

struct StartupDef
{
    AppId app;
    const wchar_t* publisher;
    bool enabled;
    const wchar_t* impact;
};

const wchar_t* ExeOf(AppId id)
{
    switch (id)
    {
    case AppId::Explorer:
        return L"explorer.exe";
    case AppId::Notepad:
        return L"Notepad.exe";
    case AppId::Settings:
        return L"SystemSettings.exe";
    case AppId::Calculator:
        return L"CalculatorApp.exe";
    case AppId::Terminal:
        return L"WindowsTerminal.exe";
    case AppId::TaskManager:
        return L"Taskmgr.exe";
    case AppId::Paint:
        return L"mspaint.exe";
    case AppId::Photos:
        return L"Photos.exe";
    case AppId::Clock:
        return L"Time.exe";
    case AppId::Weather:
        return L"Weather.exe";
    case AppId::Browser:
        return L"browser.exe";
    case AppId::Mail:
        return L"mail.exe";
    case AppId::Calendar:
        return L"calendar.exe";
    case AppId::Store:
        return L"WinStore.App.exe";
    case AppId::Camera:
        return L"WindowsCamera.exe";
    case AppId::MediaPlayer:
        return L"Microsoft.Media.Player.exe";
    case AppId::Maps:
        return L"Maps.exe";
    case AppId::Tips:
        return L"Tips.exe";
    case AppId::System:
    case AppId::None:
        break;
    }
    return L"app.exe";
}

std::wstring Fixed1(float v)
{
    const long long t = static_cast<long long>(std::fabs(v) * 10.0f + 0.5f);
    return std::wstring(v < 0.0f ? L"-" : L"") + std::to_wstring(t / 10) + L"," + std::to_wstring(t % 10);
}

std::wstring MemText(float mb) { return Fixed1(mb) + L"\u00A0МБ"; }

// Таблица: столбцы и строки (рисуются одной функцией для всех страниц).
struct Column
{
    std::wstring title;
    float width;      // < 0 — растягивается
    bool right;
    std::wstring big; // крупный итог над заголовком («12%»)
};

struct Row
{
    int kind = 1; // 0 — заголовок группы, 1 — строка, 2 — вложенная строка
    int id = 0;
    std::vector<std::wstring> cells;
    std::vector<float> heat; // тепловая карта для ячеек (-1 — нет)
    AppIcon icon = AppIcon::None;
    Icon glyph = Icon::None;
    bool expandable = false;
    bool expanded = false;
    bool dim = false;
};

class TaskManagerWindow : public Window
{
public:
    TaskManagerWindow() : Window(AppId::TaskManager, L"Диспетчер задач")
    {
        SetInitialSize(1080.0f, 720.0f);
        m_minSize = {660.0f, 460.0f};
        m_search = m_ui.Add<TextBox>(L"Введите имя, издателя или PID для поиска");
        m_search->leadingIcon = Icon::Search;
        m_search->fontSize = 13.0f;
        m_search->onChange = [this](const std::wstring& q) {
            m_query = Trim(q);
            m_scroll = 0.0f;
        };
        m_search->onEscape = [this]() {
            m_search->SetText(L"", true);
            m_ui.SetFocus(nullptr);
        };
        for (int i = 0; i < kHistory; ++i)
        {
            m_cpuHist.push_back(0.0f);
            m_memHist.push_back(0.0f);
            m_diskHist.push_back(0.0f);
            m_netHist.push_back(0.0f);
            m_gpuHist.push_back(0.0f);
        }
        m_mhz = SysInfo::ProcessorMhz();
        m_cpuName = SysInfo::ProcessorName();
        m_cores = SysInfo::LogicalProcessors();
        m_startup = {
            {AppId::Terminal, L"Демонстрация", false, L"Не измерено"}, {AppId::Weather, L"Демонстрация", true, L"Низкое"},
            {AppId::Mail, L"Демонстрация", true, L"Среднее"},          {AppId::Clock, L"Демонстрация", false, L"Не измерено"},
            {AppId::Browser, L"Демонстрация", true, L"Высокое"},
        };
        for (const ServiceDef& sd : kServices)
            m_serviceRunning.push_back(sd.running);
        m_sampler.Sample();
        Sample();
        SetPage(Page::Processes);
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnTimer1s() override
    {
        if (m_speed == 3)
            return;
        ++m_tick;
        const int every = m_speed == 2 ? 4 : 1; // «Низкая» — раз в 4 секунды
        if (m_tick % every == 0)
            Sample();
        RequestFrame();
    }

private:
    // ================================================================== данные
    void Sample()
    {
        const float cpu = m_sampler.Sample();
        const MEMORYSTATUSEX ms = SysInfo::Memory();
        m_memTotal = static_cast<float>(static_cast<double>(ms.ullTotalPhys) / (1024.0 * 1024.0));
        m_memUsed = static_cast<float>(static_cast<double>(ms.ullTotalPhys - ms.ullAvailPhys) / (1024.0 * 1024.0));
        m_cpu = Saturate(cpu);
        const ShellSettings& st = GetShell().Settings();
        // Диск и сеть — правдоподобная имитация.
        m_disk = Saturate(0.01f + RandomFloat(0.0f, 0.04f) + (RandomInt(0, 9) == 0 ? RandomFloat(0.05f, 0.35f) : 0.0f));
        m_net = (st.wifi && !st.airplane) ? RandomFloat(8.0f, 120.0f) + (RandomInt(0, 6) == 0 ? RandomFloat(200.0f, 1800.0f) : 0.0f) : 0.0f;
        m_gpu = Saturate(0.02f + m_cpu * 0.3f + RandomFloat(0.0f, 0.04f));
        Push(m_cpuHist, m_cpu);
        Push(m_memHist, m_memTotal > 0.0f ? m_memUsed / m_memTotal : 0.0f);
        Push(m_diskHist, m_disk);
        Push(m_netHist, m_net / 2000.0f);
        Push(m_gpuHist, m_gpu);
        BuildProcs();
    }

    static void Push(std::vector<float>& v, float x)
    {
        v.erase(v.begin());
        v.push_back(x);
    }

    float Smooth(int pid, float target)
    {
        auto it = m_smooth.find(pid);
        const float prev = it == m_smooth.end() ? target : it->second;
        const float v = prev + (target - prev) * 0.55f;
        m_smooth[pid] = v;
        return v;
    }

    void BuildProcs()
    {
        m_procs.clear();
        // Приложения: окна, сгруппированные по приложению.
        std::map<AppId, Proc> apps;
        for (auto& w : GetShell().WM().Windows())
        {
            if (w->IsClosing() || w->App() == AppId::System)
                continue;
            Proc& pr = apps[w->App()];
            if (pr.windows.empty())
            {
                pr.app = w->App();
                pr.name = GetAppInfo(w->App()).name;
                pr.icon = GetAppInfo(w->App()).icon;
                pr.exe = ExeOf(w->App());
                pr.desc = pr.name;
                pr.group = 0;
                pr.pid = 6000 + static_cast<int>(w->App()) * 412;
                pr.weight = w->App() == AppId::TaskManager ? 1.2f : 0.35f;
                pr.threads = 18 + static_cast<int>(w->App()) * 3;
            }
            pr.windows.push_back(w.get());
        }
        for (auto& kv : apps)
            m_procs.push_back(kv.second);
        int pid = 900;
        for (const BgDef& b : kBackground)
        {
            Proc pr;
            pr.name = b.name;
            pr.exe = b.exe;
            pr.desc = b.name;
            pr.group = b.group;
            pr.weight = b.cpuWeight;
            pr.mem = b.memMb;
            pr.pid = pid;
            pr.threads = 4 + (pid / 7) % 40;
            pid += 236 + (pid % 5) * 44;
            m_procs.push_back(pr);
        }
        // Распределение реальной загрузки ЦП и занятой памяти по процессам.
        float wsum = 0.0f, msum = 0.0f;
        for (const Proc& pr : m_procs)
        {
            wsum += pr.weight;
            msum += pr.group == 0 ? 0.0f : pr.mem;
        }
        const float totalCpu = m_cpu * 100.0f;
        const float appMem = std::max(200.0f, m_memUsed * 0.45f - msum);
        int appCount = 0;
        for (const Proc& pr : m_procs)
            appCount += pr.group == 0 ? 1 : 0;
        for (Proc& pr : m_procs)
        {
            const float share = pr.weight / std::max(0.01f, wsum);
            pr.cpu = Smooth(pr.pid, std::max(0.0f, totalCpu * share * RandomFloat(0.4f, 1.6f)));
            if (pr.group == 0)
                pr.mem = Smooth(pr.pid + 1, appMem / static_cast<float>(std::max(1, appCount)) * (0.6f + 0.15f * static_cast<float>(pr.windows.size())) *
                                                RandomFloat(0.97f, 1.03f));
            pr.disk = RandomInt(0, 12) == 0 ? RandomFloat(0.1f, 2.5f) : 0.0f;
            pr.net = (pr.app == AppId::Browser || pr.exe == L"svchost.exe") && m_net > 0.0f ? RandomFloat(0.0f, 0.6f) : 0.0f;
            pr.user = pr.group == 2 ? L"СИСТЕМА" : GetShell().FS().UserName();
            pr.efficiency = m_efficiency.count(pr.pid) > 0;
        }
    }

    // ================================================================== страницы
    void SetPage(Page p)
    {
        m_page = p;
        m_scroll = 0.0f;
        m_sel = -1;
        BuildCommands();
    }

    void BuildCommands()
    {
        for (Button* b : m_cmdButtons)
            m_ui.Remove(b);
        m_cmdButtons.clear();
        for (Widget* w : m_settingsWidgets)
            m_ui.Remove(w);
        m_settingsWidgets.clear();
        auto add = [&](const wchar_t* text, Icon icon, std::function<void()> fn) {
            auto* b = m_ui.Add<Button>(text, icon, ButtonStyle::Subtle);
            b->fontSize = 14.0f;
            b->onClick = std::move(fn);
            m_cmdButtons.push_back(b);
            return b;
        };
        switch (m_page)
        {
        case Page::Processes:
        case Page::Details:
            add(L"Запустить новую задачу", Icon::Add, [this]() { RunNewTask(); });
            add(L"Снять задачу", Icon::Stop, [this]() { EndSelected(); });
            add(L"Режим эффективности", Icon::Lightbulb, [this]() { ToggleEfficiency(); });
            add(L"", Icon::More, [this]() { ShowMoreMenu(); });
            break;
        case Page::Performance:
            add(L"Запустить новую задачу", Icon::Add, [this]() { RunNewTask(); });
            add(L"", Icon::More, [this]() {
                std::vector<MenuItem> items = {
                    MenuItem(L"Копировать", Icon::Copy, [this]() { CopyPerformance(); }, L"Ctrl+C"),
                    MenuItem(L"Монитор ресурсов", Icon::Chart, nullptr).Disabled(),
                };
                GetShell().ShowMenuBelow(LocalToScreen(m_cmdButtons.back()->Bounds()), std::move(items), 220.0f);
            });
            break;
        case Page::Startup:
            add(L"Запустить новую задачу", Icon::Add, [this]() { RunNewTask(); });
            add(L"Включить", Icon::Check, [this]() { SetStartup(true); });
            add(L"Отключить", Icon::Close, [this]() { SetStartup(false); });
            break;
        case Page::Services:
            add(L"Запустить", Icon::Play, [this]() { ServiceAction(1); });
            add(L"Остановить", Icon::Stop, [this]() { ServiceAction(0); });
            add(L"Перезапустить", Icon::Refresh, [this]() { ServiceAction(2); });
            break;
        case Page::AppHistory:
            add(L"Удалить журнал использования", Icon::Delete, [this]() { m_historyReset = LocalNow(); });
            break;
        case Page::Users:
            add(L"Запустить новую задачу", Icon::Add, [this]() { RunNewTask(); });
            add(L"Отключиться", Icon::SignOut, nullptr)->SetEnabled(false);
            break;
        case Page::Settings:
            BuildSettings();
            break;
        }
        OnResize();
    }

    void BuildSettings()
    {
        auto combo = [&](std::vector<std::wstring> items, int sel, std::function<void(int)> fn) {
            auto* c = m_ui.Add<ComboBox>();
            c->items = std::move(items);
            c->selected = sel;
            c->onChange = std::move(fn);
            m_settingsWidgets.push_back(c);
            return c;
        };
        auto toggle = [&](bool on, std::function<void(bool)> fn) {
            auto* t = m_ui.Add<ToggleSwitch>(on);
            t->onChange = std::move(fn);
            m_settingsWidgets.push_back(t);
            return t;
        };
        combo({L"Процессы", L"Производительность", L"Журнал приложений", L"Автозагрузка приложений", L"Пользователи", L"Подробности", L"Службы"},
              m_startPage, [this](int i) { m_startPage = i; });
        combo({L"Высокая", L"Обычная", L"Низкая", L"Приостановлено"}, m_speed, [this](int i) { m_speed = i; });
        toggle(m_onTop, [this](bool v) { m_onTop = v; });
        toggle(m_minimizeOnUse, [this](bool v) { m_minimizeOnUse = v; });
        toggle(m_hideMin, [this](bool v) { m_hideMin = v; });
        toggle(m_fullNames, [this](bool v) { m_fullNames = v; });
        toggle(m_allHistory, [this](bool v) { m_allHistory = v; });
        toggle(m_confirmEff, [this](bool v) { m_confirmEff = v; });
    }

    // Строки параметров (текст слева, элемент справа).
    struct SettingRow
    {
        const wchar_t* title;
        const wchar_t* desc;
        int group;
    };
    static const std::vector<SettingRow>& SettingRows()
    {
        static const std::vector<SettingRow> rows = {
            {L"Страница запуска по умолчанию", L"Страница, открывающаяся при запуске диспетчера задач", 0},
            {L"Частота обновления в реальном времени", L"Как часто обновляются данные о процессах и производительности", 0},
            {L"Всегда поверх остальных окон", L"", 1},
            {L"Сворачивать после использования", L"Сворачивать при переключении на другое окно", 1},
            {L"Скрывать при сворачивании", L"Оставлять значок только в области уведомлений", 1},
            {L"Показывать полное имя учётной записи", L"", 2},
            {L"Показывать журнал для всех процессов", L"", 2},
            {L"Запрашивать подтверждение режима эффективности", L"", 2},
        };
        return rows;
    }

    float NavW() const { return m_navExpanded && Width() >= 860.0f ? 240.0f : 56.0f; }
    Rect ContentRect() const { return {NavW(), kTitleH, Width() - NavW(), Height() - kTitleH}; }

    void OnResize() override
    {
        const float sw = std::min(420.0f, Width() - 440.0f);
        m_search->SetVisible(sw > 160.0f);
        m_search->SetBounds({std::floor((Width() - sw) * 0.5f), 8.0f, sw, 32.0f});
        const Rect c = ContentRect();
        float x = c.Right() - 16.0f;
        for (auto it = m_cmdButtons.rbegin(); it != m_cmdButtons.rend(); ++it)
        {
            Button* b = *it;
            const float w = b->text.empty() ? 40.0f : Renderer::Get().TextWidth(b->text, 14.0f) + 48.0f;
            x -= w;
            b->SetBounds({x, c.y + 12.0f, w, 36.0f});
            x -= 4.0f;
        }
        if (m_page == Page::Settings)
        {
            const std::vector<SettingRow>& rows = SettingRows();
            for (size_t i = 0; i < m_settingsWidgets.size() && i < rows.size(); ++i)
            {
                const Rect rr = SettingRect(static_cast<int>(i));
                const bool isCombo = i < 2;
                const float w = isCombo ? 240.0f : 120.0f;
                m_settingsWidgets[i]->SetBounds({rr.Right() - 16.0f - w, rr.CenterY() - 16.0f, w, 32.0f});
            }
        }
    }

    Rect SettingRect(int i) const
    {
        const Rect c = ContentRect();
        const std::vector<SettingRow>& rows = SettingRows();
        float y = c.y + 64.0f;
        int group = -1;
        for (int k = 0; k <= i; ++k)
        {
            if (rows[static_cast<size_t>(k)].group != group)
            {
                group = rows[static_cast<size_t>(k)].group;
                y += 36.0f;
            }
            if (k < i)
                y += 72.0f;
        }
        return {c.x + 24.0f, y, std::min(900.0f, c.w - 48.0f), 68.0f};
    }

    // ================================================================== действия
    Proc* SelectedProc()
    {
        for (Proc& pr : m_procs)
        {
            if (pr.pid == m_sel)
                return &pr;
        }
        return nullptr;
    }

    void EndSelected()
    {
        Proc* pr = SelectedProc();
        if (!pr)
            return;
        if (pr->group == 0)
        {
            std::vector<Window*> wins = pr->windows;
            PostAction([wins]() {
                for (Window* w : wins)
                {
                    if (GetShell().WM().Contains(w))
                        w->ForceClose();
                }
            });
            m_sel = -1;
            return;
        }
        const std::wstring name = pr->name;
        if (pr->group == 2)
            ShowDialog(L"Диспетчер задач", L"Не удаётся завершить процесс «" + name + L"».\nОтказано в доступе.", {{L"ОК", true}}, nullptr);
        else
            ShowDialog(L"Снять задачу «" + name + L"»?",
                       L"Завершение этого процесса может привести к нестабильной работе системы. Процесс будет перезапущен автоматически.",
                       {{L"Снять задачу", true}, {L"Отмена", false}}, nullptr);
    }

    void ToggleEfficiency()
    {
        Proc* pr = SelectedProc();
        if (!pr || pr->group == 2)
            return;
        const int pid = pr->pid;
        auto apply = [this, pid]() {
            if (m_efficiency.count(pid))
                m_efficiency.erase(pid);
            else
                m_efficiency.insert(pid);
        };
        if (m_confirmEff && !m_efficiency.count(pid))
            ShowDialog(L"Включить режим эффективности?",
                       L"Режим эффективности понижает приоритет процесса и повышает энергоэффективность. Это может привести к нестабильной работе "
                       L"некоторых процессов.",
                       {{L"Включить режим эффективности", true}, {L"Отмена", false}}, [apply](int r) {
                           if (r == 0)
                               apply();
                       });
        else
            apply();
    }

    void RunNewTask()
    {
        ShowInputDialog(L"Создание задачи", L"Введите имя программы, папки, документа или ресурса Интернета, которые требуется открыть.",
                        L"", L"ОК", [this](const std::wstring& text) {
                            std::wstring t = ToLower(Trim(text));
                            if (t.size() > 4 && t.substr(t.size() - 4) == L".exe")
                                t = t.substr(0, t.size() - 4);
                            Shell& sh = GetShell();
                            static const std::pair<const wchar_t*, AppId> map[] = {
                                {L"notepad", AppId::Notepad},      {L"calc", AppId::Calculator},      {L"explorer", AppId::Explorer},
                                {L"mspaint", AppId::Paint},        {L"taskmgr", AppId::TaskManager},  {L"cmd", AppId::Terminal},
                                {L"powershell", AppId::Terminal},  {L"wt", AppId::Terminal},          {L"control", AppId::Settings},
                                {L"ms-settings:", AppId::Settings}, {L"photos", AppId::Photos},       {L"browser", AppId::Browser},
                            };
                            for (const auto& m : map)
                            {
                                if (t == m.first)
                                {
                                    sh.Launch(m.second, t == L"cmd" ? L"::cmd" : L"");
                                    return;
                                }
                            }
                            if (t.rfind(L"http", 0) == 0)
                            {
                                sh.Launch(AppId::Browser, Trim(text));
                                return;
                            }
                            if (FsNode* n = sh.FS().Resolve(Trim(text), sh.FS().Home()))
                            {
                                sh.Open(n);
                                return;
                            }
                            ShowDialog(L"Диспетчер задач", L"Не удаётся найти «" + Trim(text) +
                                                               L"». Проверьте, правильно ли указано имя, и повторите попытку.",
                                       {{L"ОК", true}}, nullptr);
                        });
    }

    void SetStartup(bool enabled)
    {
        if (m_sel >= 0 && m_sel < static_cast<int>(m_startup.size()))
            m_startup[static_cast<size_t>(m_sel)].enabled = enabled;
    }

    void ServiceAction(int action)
    {
        if (m_sel < 0 || m_sel >= static_cast<int>(m_serviceRunning.size()))
            return;
        if (action == 2 || action == 1)
            m_serviceRunning[static_cast<size_t>(m_sel)] = true;
        else
            m_serviceRunning[static_cast<size_t>(m_sel)] = false;
    }

    void ShowMoreMenu()
    {
        std::vector<MenuItem> view = {
            MenuItem::Check(L"Группировать по типу", m_groupByType, [this]() { m_groupByType = !m_groupByType; }),
            MenuItem::Check(L"Развернуть все", false, [this]() {
                for (const Proc& pr : m_procs)
                {
                    if (pr.windows.size() > 1)
                        m_expanded.insert(pr.pid);
                }
            }),
            MenuItem::Check(L"Свернуть все", false, [this]() { m_expanded.clear(); }),
        };
        std::vector<MenuItem> items = {
            MenuItem::Sub(L"Просмотр", Icon::View, view),
            MenuItem::Sub(L"Значения состояния", Icon::Eye,
                          {MenuItem::Check(L"Показывать состояние приостановки", true, nullptr), MenuItem::Check(L"Скрыть состояние", false, nullptr)}),
        };
        GetShell().ShowMenuBelow(LocalToScreen(m_cmdButtons.back()->Bounds()), std::move(items), 240.0f);
    }

    void CopyPerformance()
    {
        std::wstring s = L"ЦП\r\n\t" + m_cpuName + L"\r\n\r\nИспользование\t" + std::to_wstring(RoundToInt(m_cpu * 100.0f)) +
                         L"%\r\nСкорость\t" + Fixed1(static_cast<float>(m_mhz) / 1000.0f) + L" ГГц\r\nВремя работы\t" + SysInfo::UptimeText() +
                         L"\r\nПамять\t" + Fixed1(m_memUsed / 1024.0f) + L"/" + Fixed1(m_memTotal / 1024.0f) + L" ГБ\r\n";
        SetClipboardText(GetShell().Hwnd(), s);
    }

    void ShowRowMenu(int pid, Point screen)
    {
        m_sel = pid;
        Proc* pr = SelectedProc();
        if (!pr)
            return;
        const bool app = pr->group == 0;
        Window* first = app && !pr->windows.empty() ? pr->windows.front() : nullptr;
        const std::wstring name = pr->name;
        std::vector<MenuItem> items;
        if (app)
        {
            items.push_back(MenuItem(L"Переключиться", Icon::OpenWith, [first]() {
                if (first && GetShell().WM().Contains(first))
                    GetShell().WM().Activate(first);
            }));
            items.push_back(MenuItem(L"Развернуть", Icon::Maximize, [first]() {
                if (first && GetShell().WM().Contains(first))
                    GetShell().WM().Maximize(first);
            }));
            items.push_back(MenuItem(L"Свернуть", Icon::Minimize, [first]() {
                if (first && GetShell().WM().Contains(first))
                    GetShell().WM().Minimize(first);
            }));
            items.push_back(MenuItem::Sep());
        }
        items.push_back(MenuItem(L"Снять задачу", Icon::Stop, [this]() { EndSelected(); }, L"Delete"));
        items.push_back(MenuItem(L"Режим эффективности", Icon::Lightbulb, [this]() { ToggleEfficiency(); }).Disabled(pr->group == 2));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Подробнее", Icon::Details, [this]() {
            const int keep = m_sel;
            SetPage(Page::Details);
            m_sel = keep;
        }));
        items.push_back(MenuItem(L"Открыть расположение файла", Icon::Folder, []() {
            GetShell().Launch(AppId::Explorer, L"C:\\Windows\\System32");
        }));
        items.push_back(MenuItem(L"Поиск в Интернете", Icon::Search, [name]() { GetShell().Launch(AppId::Browser, name); }));
        items.push_back(MenuItem(L"Свойства", Icon::Properties, [this, name]() {
            Proc* p2 = SelectedProc();
            ShowDialog(L"Свойства: " + name,
                       L"Имя файла: " + (p2 ? p2->exe : std::wstring()) + L"\nИД процесса: " + std::to_wstring(m_sel) + L"\nРасположение: C:\\Windows\\System32",
                       {{L"ОК", true}}, nullptr);
        }));
        GetShell().ShowMenu(screen, std::move(items));
    }

    // ================================================================== построение таблиц
    bool Matches(const Proc& pr) const
    {
        if (m_query.empty())
            return true;
        return ContainsNoCase(pr.name, m_query) || ContainsNoCase(pr.exe, m_query) || std::to_wstring(pr.pid).find(m_query) != std::wstring::npos;
    }

    float SortValue(const Proc& pr, int col) const
    {
        switch (col)
        {
        case 2:
            return pr.cpu;
        case 3:
            return pr.mem;
        case 4:
            return pr.disk;
        case 5:
            return pr.net;
        default:
            return 0.0f;
        }
    }

    void SortProcs(std::vector<const Proc*>& v) const
    {
        const int col = m_sortCol;
        const bool asc = m_sortAsc;
        std::stable_sort(v.begin(), v.end(), [this, col, asc](const Proc* a, const Proc* b) {
            if (col <= 1)
            {
                const int c = CompareNoCase(a->name, b->name);
                return asc ? c < 0 : c > 0;
            }
            const float va = SortValue(*a, col), vb = SortValue(*b, col);
            return asc ? va < vb : va > vb;
        });
    }

    void ProcessRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        const float memPct = m_memTotal > 0.0f ? m_memUsed / m_memTotal * 100.0f : 0.0f;
        cols = {
            {L"Имя", -1.0f, false, L""},
            {L"Состояние", 130.0f, false, L""},
            {L"ЦП", 84.0f, true, std::to_wstring(RoundToInt(m_cpu * 100.0f)) + L"%"},
            {L"Память", 104.0f, true, std::to_wstring(RoundToInt(memPct)) + L"%"},
            {L"Диск", 92.0f, true, std::to_wstring(RoundToInt(m_disk * 100.0f)) + L"%"},
            {L"Сеть", 96.0f, true, std::to_wstring(RoundToInt(m_net / 10.0f)) + L"%"},
        };
        const wchar_t* groupNames[] = {L"Приложения", L"Фоновые процессы", L"Процессы Windows"};
        for (int g = 0; g < 3; ++g)
        {
            std::vector<const Proc*> list;
            for (const Proc& pr : m_procs)
            {
                if ((m_groupByType ? pr.group == g : g == 0) && Matches(pr))
                    list.push_back(&pr);
            }
            if (list.empty())
                continue;
            SortProcs(list);
            if (m_groupByType)
            {
                Row h;
                h.kind = 0;
                h.cells = {std::wstring(groupNames[g]) + L" (" + std::to_wstring(list.size()) + L")"};
                rows.push_back(h);
            }
            for (const Proc* pr : list)
            {
                Row r;
                r.id = pr->pid;
                r.icon = pr->icon;
                r.glyph = pr->group == 0 ? Icon::None : (pr->group == 2 ? Icon::Settings : Icon::Apps);
                const bool multi = pr->windows.size() > 1;
                r.expandable = multi;
                r.expanded = multi && m_expanded.count(pr->pid) > 0;
                r.cells = {pr->name + (multi ? L" (" + std::to_wstring(pr->windows.size()) + L")" : L""), pr->efficiency ? L"Эффективность" : L"",
                           Fixed1(pr->cpu) + L"%", MemText(pr->mem), Fixed1(pr->disk) + L"\u00A0МБ/с", Fixed1(pr->net) + L"\u00A0Мбит/с"};
                r.heat = {-1.0f, -1.0f, Saturate(pr->cpu / 25.0f), Saturate(pr->mem / 600.0f), Saturate(pr->disk / 5.0f), Saturate(pr->net / 2.0f)};
                rows.push_back(r);
                if (r.expanded)
                {
                    for (Window* w : pr->windows)
                    {
                        Row c;
                        c.kind = 2;
                        c.id = -1;
                        c.icon = w->WindowIcon();
                        c.cells = {w->Title()};
                        rows.push_back(c);
                    }
                }
            }
        }
    }

    void DetailRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        cols = {{L"Имя", 230.0f, false, L""},        {L"ИД процесса", 100.0f, true, L""}, {L"Состояние", 120.0f, false, L""},
                {L"Имя пользователя", 140.0f, false, L""}, {L"ЦП", 60.0f, true, L""},    {L"Память (частный рабочий набор)", 210.0f, true, L""},
                {L"Описание", -1.0f, false, L""}};
        std::vector<const Proc*> list;
        for (const Proc& pr : m_procs)
        {
            if (Matches(pr))
                list.push_back(&pr);
        }
        std::stable_sort(list.begin(), list.end(), [](const Proc* a, const Proc* b) { return CompareNoCase(a->exe, b->exe) < 0; });
        for (const Proc* pr : list)
        {
            Row r;
            r.id = pr->pid;
            r.icon = pr->icon;
            r.glyph = pr->icon == AppIcon::None ? Icon::Apps : Icon::None;
            r.cells = {pr->exe, std::to_wstring(pr->pid), pr->efficiency ? L"Эффективность" : L"Выполняется", pr->user,
                       Pad2(std::min(99, RoundToInt(pr->cpu))), GroupDigits(static_cast<long long>(pr->mem * 1024.0f)) + L"\u00A0КБ", pr->desc};
            rows.push_back(r);
        }
    }

    void StartupRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        cols = {{L"Имя", -1.0f, false, L""}, {L"Издатель", 200.0f, false, L""}, {L"Состояние", 140.0f, false, L""}, {L"Влияние на запуск", 170.0f, false, L""}};
        for (size_t i = 0; i < m_startup.size(); ++i)
        {
            const StartupDef& sd = m_startup[i];
            Row r;
            r.id = static_cast<int>(i);
            r.icon = GetAppInfo(sd.app).icon;
            r.cells = {GetAppInfo(sd.app).name, sd.publisher, sd.enabled ? L"Включено" : L"Отключено", sd.enabled ? sd.impact : L"Нет"};
            rows.push_back(r);
        }
    }

    void ServiceRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        cols = {{L"Имя", 180.0f, false, L""}, {L"ИД процесса", 100.0f, true, L""}, {L"Описание", -1.0f, false, L""},
                {L"Состояние", 130.0f, false, L""}, {L"Группа", 220.0f, false, L""}};
        int i = 0;
        for (const ServiceDef& sd : kServices)
        {
            if (!m_query.empty() && !ContainsNoCase(sd.name, m_query) && !ContainsNoCase(sd.desc, m_query))
            {
                ++i;
                continue;
            }
            const bool run = m_serviceRunning[static_cast<size_t>(i)];
            Row r;
            r.id = i;
            r.glyph = Icon::Services;
            r.cells = {sd.name, run ? std::to_wstring(1100 + i * 52) : L"", sd.desc, run ? L"Выполняется" : L"Остановлена", sd.group};
            r.dim = !run;
            rows.push_back(r);
            ++i;
        }
    }

    void HistoryRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        cols = {{L"Имя", -1.0f, false, L""}, {L"Время ЦП", 140.0f, true, L""}, {L"Сеть", 120.0f, true, L""}, {L"Сеть с лимитом", 150.0f, true, L""},
                {L"Обновления плиток", 170.0f, true, L""}};
        int i = 0;
        for (AppId a : AllApps())
        {
            const AppInfo& ai = GetAppInfo(a);
            Row r;
            r.id = i;
            r.icon = ai.icon;
            const int minutes = (static_cast<int>(a) * 37) % 180;
            const float mb = static_cast<float>((static_cast<int>(a) * 53) % 400) / 3.0f;
            r.cells = {ai.name, std::to_wstring(minutes / 60) + L":" + Pad2(minutes % 60) + L":" + Pad2((minutes * 7) % 60), MemText(mb), L"0\u00A0МБ",
                       L"0\u00A0МБ"};
            r.heat = {-1.0f, Saturate(static_cast<float>(minutes) / 180.0f), Saturate(mb / 130.0f), 0.0f, 0.0f};
            rows.push_back(r);
            ++i;
        }
    }

    void UserRows(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        float cpu = 0.0f, mem = 0.0f, disk = 0.0f, net = 0.0f;
        for (const Proc& pr : m_procs)
        {
            if (pr.group == 2)
                continue;
            cpu += pr.cpu;
            mem += pr.mem;
            disk += pr.disk;
            net += pr.net;
        }
        cols = {{L"Пользователь", -1.0f, false, L""}, {L"Состояние", 130.0f, false, L""},
                {L"ЦП", 84.0f, true, std::to_wstring(RoundToInt(m_cpu * 100.0f)) + L"%"},
                {L"Память", 104.0f, true, std::to_wstring(RoundToInt(m_memTotal > 0.0f ? m_memUsed / m_memTotal * 100.0f : 0.0f)) + L"%"},
                {L"Диск", 92.0f, true, std::to_wstring(RoundToInt(m_disk * 100.0f)) + L"%"},
                {L"Сеть", 96.0f, true, std::to_wstring(RoundToInt(m_net / 10.0f)) + L"%"}};
        Row r;
        r.id = 1;
        r.icon = AppIcon::User;
        r.expandable = true;
        r.expanded = m_userExpanded;
        std::wstring user = GetShell().FS().UserName();
        if (m_fullNames)
            user += L" (" + user + L")";
        r.cells = {user + L" (" + std::to_wstring(m_procs.size()) + L")", L"", Fixed1(cpu) + L"%", MemText(mem), Fixed1(disk) + L"\u00A0МБ/с",
                   Fixed1(net) + L"\u00A0Мбит/с"};
        r.heat = {-1.0f, -1.0f, Saturate(cpu / 25.0f), Saturate(mem / 3000.0f), Saturate(disk / 5.0f), Saturate(net / 2.0f)};
        rows.push_back(r);
        if (m_userExpanded)
        {
            for (const Proc& pr : m_procs)
            {
                if (pr.group != 0)
                    continue;
                Row c;
                c.kind = 2;
                c.id = pr.pid;
                c.icon = pr.icon;
                c.cells = {pr.name, L"", Fixed1(pr.cpu) + L"%", MemText(pr.mem), Fixed1(pr.disk) + L"\u00A0МБ/с", Fixed1(pr.net) + L"\u00A0Мбит/с"};
                c.heat = {-1.0f, -1.0f, Saturate(pr.cpu / 25.0f), Saturate(pr.mem / 600.0f), Saturate(pr.disk / 5.0f), Saturate(pr.net / 2.0f)};
                rows.push_back(c);
            }
        }
    }

    void CurrentTable(std::vector<Column>& cols, std::vector<Row>& rows) const
    {
        cols.clear();
        rows.clear();
        switch (m_page)
        {
        case Page::Processes:
            ProcessRows(cols, rows);
            break;
        case Page::Details:
            DetailRows(cols, rows);
            break;
        case Page::Startup:
            StartupRows(cols, rows);
            break;
        case Page::Services:
            ServiceRows(cols, rows);
            break;
        case Page::AppHistory:
            HistoryRows(cols, rows);
            break;
        case Page::Users:
            UserRows(cols, rows);
            break;
        case Page::Performance:
        case Page::Settings:
            break;
        }
    }

    Rect TableRect() const
    {
        const Rect c = ContentRect();
        float top = c.y + 60.0f;
        if (m_page == Page::AppHistory || m_page == Page::Startup)
            top += 30.0f;
        return {c.x + 8.0f, top, c.w - 16.0f, c.Bottom() - top};
    }

    float HeaderH() const { return m_page == Page::Processes || m_page == Page::Users ? 52.0f : 36.0f; }

    std::vector<float> ColumnWidths(const std::vector<Column>& cols, float total) const
    {
        float fixed = 0.0f;
        int flex = 0;
        for (const Column& c : cols)
        {
            if (c.width < 0.0f)
                ++flex;
            else
                fixed += c.width;
        }
        std::vector<float> w;
        const float rest = std::max(160.0f, total - fixed);
        for (const Column& c : cols)
            w.push_back(c.width < 0.0f ? rest / static_cast<float>(std::max(1, flex)) : c.width);
        return w;
    }

    static Color Heat(float t)
    {
        const Palette& p = Theme::P();
        t = Saturate(t);
        if (p.dark)
            return LerpColor(Color::Hex(0x3A3317), Color::Hex(0x8F6400), t);
        return LerpColor(Color::Hex(0xFFF4C4), Color::Hex(0xFFB935), std::sqrt(t));
    }

    void DrawTable(Renderer& r)
    {
        const Palette& p = Theme::P();
        std::vector<Column> cols;
        std::vector<Row> rows;
        CurrentTable(cols, rows);
        m_rowCache = rows;
        const Rect tr = TableRect();
        const std::vector<float> widths = ColumnWidths(cols, tr.w - 12.0f);
        m_colRects.clear();
        // Заголовок
        const float hh = HeaderH();
        float x = tr.x;
        for (size_t i = 0; i < cols.size(); ++i)
        {
            const Rect hr{x, tr.y, widths[i], hh};
            m_colRects.push_back(hr);
            const bool hot = m_hoverCol == static_cast<int>(i);
            if (hot)
                r.FillRect(hr, p.subtleHover);
            const unsigned align = cols[i].right ? TextFlags::Right : TextFlags::Left;
            if (!cols[i].big.empty())
            {
                r.Text(cols[i].big, {hr.x + 8.0f, hr.y + 6.0f, hr.w - 16.0f, 22.0f}, p.text, 16.0f, FontWeight::Semibold, align | TextFlags::VCenter);
                r.Text(cols[i].title, {hr.x + 8.0f, hr.y + 28.0f, hr.w - 16.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, align | TextFlags::VCenter);
            }
            else
                r.Text(cols[i].title, {hr.x + (i == 0 ? 40.0f : 8.0f), hr.Bottom() - 26.0f, hr.w - 16.0f, 20.0f}, p.textSecondary, 12.0f,
                       FontWeight::Regular, align | TextFlags::VCenter);
            if (m_page == Page::Processes && m_sortCol == static_cast<int>(i))
                r.Glyph(m_sortAsc ? Icon::ChevronUp : Icon::ChevronDown, {hr.CenterX() - 4.0f, hr.y + 1.0f, 8.0f, 8.0f}, p.textTertiary);
            if (i > 0)
                r.FillRect({hr.x, hr.y + 8.0f, 1.0f, hr.h - 16.0f}, p.divider);
            x += widths[i];
        }
        r.FillRect({tr.x, tr.y + hh, tr.w, 1.0f}, p.divider);
        // Строки
        const Rect body{tr.x, tr.y + hh + 1.0f, tr.w, tr.h - hh - 1.0f};
        const float contentH = static_cast<float>(rows.size()) * kRowH;
        m_maxScroll = std::max(0.0f, contentH - body.h);
        m_scroll = Clamp(m_scroll, 0.0f, m_maxScroll);
        r.PushClip(body);
        for (size_t ri = 0; ri < rows.size(); ++ri)
        {
            const Row& row = rows[ri];
            const float y = body.y + static_cast<float>(ri) * kRowH - m_scroll;
            if (y + kRowH < body.y || y > body.Bottom())
                continue;
            const Rect rr{tr.x, y, tr.w, kRowH};
            if (row.kind == 0)
            {
                r.Text(row.cells[0], {rr.x + 12.0f, rr.y + 4.0f, rr.w - 24.0f, rr.h - 4.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
                continue;
            }
            const bool sel = row.kind == 1 && row.id == m_sel;
            const bool hot = static_cast<int>(ri) == m_hoverRow;
            // Ячейки с тепловой картой
            float cx = tr.x;
            for (size_t ci = 0; ci < cols.size() && ci < row.cells.size(); ++ci)
            {
                const Rect cell{cx, y, widths[ci], kRowH};
                if (ci < row.heat.size() && row.heat[ci] >= 0.0f && !sel)
                    r.FillRect(cell.Inset(0.0f, 0.0f, 1.0f, 1.0f), Heat(row.heat[ci]));
                cx += widths[ci];
            }
            if (sel)
                r.FillRoundRect(rr.Inset(2.0f, 1.0f, 2.0f, 1.0f), 4.0f, p.selection);
            else if (hot)
                r.FillRoundRect(rr.Inset(2.0f, 1.0f, 2.0f, 1.0f), 4.0f, p.selectionHover.MulAlpha(0.8f));
            cx = tr.x;
            const Color fg = row.dim ? p.textSecondary : p.text;
            for (size_t ci = 0; ci < cols.size() && ci < row.cells.size(); ++ci)
            {
                const Rect cell{cx, y, widths[ci], kRowH};
                if (ci == 0)
                {
                    float ix = cell.x + 12.0f + (row.kind == 2 ? 24.0f : 0.0f);
                    if (row.expandable)
                        r.Glyph(row.expanded ? Icon::ChevronDown : Icon::ChevronRight, {cell.x + 8.0f, cell.CenterY() - 5.0f, 10.0f, 10.0f},
                                p.textSecondary);
                    ix += row.expandable || row.kind == 1 ? 14.0f : 0.0f;
                    if (row.icon != AppIcon::None)
                        r.DrawAppIcon(row.icon, {ix, cell.CenterY() - 8.0f, 16.0f, 16.0f});
                    else if (row.glyph != Icon::None)
                        r.Glyph(row.glyph, {ix, cell.CenterY() - 8.0f, 16.0f, 16.0f}, p.textSecondary);
                    r.Text(row.cells[ci], {ix + 26.0f, cell.y, cell.Right() - ix - 30.0f, cell.h}, fg, 12.0f, FontWeight::Regular, TextFlags::VCenter);
                }
                else
                {
                    const bool isEff = row.cells[ci] == L"Эффективность";
                    if (isEff)
                        r.Glyph(Icon::Lightbulb, {cell.x + 8.0f, cell.CenterY() - 7.0f, 14.0f, 14.0f}, p.success);
                    r.Text(row.cells[ci], {cell.x + (isEff ? 26.0f : 8.0f), cell.y, cell.w - (isEff ? 34.0f : 16.0f), cell.h}, fg, 12.0f, FontWeight::Regular,
                           (cols[ci].right ? TextFlags::Right : TextFlags::Left) | TextFlags::VCenter);
                }
                cx += widths[ci];
            }
        }
        r.PopClip();
        if (m_maxScroll > 0.0f)
        {
            const float th = std::max(30.0f, body.h * body.h / contentH);
            const float ty = body.y + (body.h - th) * (m_scroll / m_maxScroll);
            r.FillRoundRect({tr.Right() - 5.0f, ty, 3.0f, th}, 1.5f, p.scrollThumb);
        }
        m_bodyRect = body;
    }

    // ---- Производительность ----------------------------------------------------------
    struct Sensor
    {
        const wchar_t* name;
        const std::vector<float>* hist;
        Color color;
    };
    std::vector<Sensor> Sensors() const
    {
        return {{L"ЦП", &m_cpuHist, Color::Hex(0x117DBB)},
                {L"Память", &m_memHist, Color::Hex(0x8B12AE)},
                {L"Диск 0 (C:)", &m_diskHist, Color::Hex(0x4DA60C)},
                {L"Wi-Fi", &m_netHist, Color::Hex(0xA74F01)},
                {L"Графический процессор 0", &m_gpuHist, Color::Hex(0x117DBB)}};
    }

    std::wstring SensorValue(int i) const
    {
        switch (i)
        {
        case 0:
            return std::to_wstring(RoundToInt(m_cpu * 100.0f)) + L"%  " + Fixed1(static_cast<float>(m_mhz) / 1000.0f) + L"\u00A0ГГц";
        case 1:
            return Fixed1(m_memUsed / 1024.0f) + L"/" + Fixed1(m_memTotal / 1024.0f) + L"\u00A0ГБ (" +
                   std::to_wstring(RoundToInt(m_memTotal > 0.0f ? m_memUsed / m_memTotal * 100.0f : 0.0f)) + L"%)";
        case 2:
            return L"SSD (NVMe)\n" + std::to_wstring(RoundToInt(m_disk * 100.0f)) + L"%";
        case 3:
            return GetShell().Settings().wifi ? L"О: " + std::to_wstring(RoundToInt(m_net * 0.2f)) + L" П: " + std::to_wstring(RoundToInt(m_net)) + L" Кбит/с"
                                               : L"Не подключено";
        default:
            return std::to_wstring(RoundToInt(m_gpu * 100.0f)) + L"%";
        }
    }

    static void DrawGraph(Renderer& r, const Rect& g, const std::vector<float>& h, Color c, bool grid)
    {
        const Palette& p = Theme::P();
        r.FillRect(g, p.dark ? Color::Hex(0x1C1C1C) : Color::Hex(0xFFFFFF));
        if (grid)
        {
            for (int i = 1; i < 10; ++i)
            {
                const float gy = g.y + g.h * static_cast<float>(i) / 10.0f;
                r.FillRect({g.x, std::floor(gy), g.w, 1.0f}, c.WithAlpha(0x22));
            }
            for (int i = 1; i < 20; ++i)
            {
                const float gx = g.x + g.w * static_cast<float>(i) / 20.0f;
                r.FillRect({std::floor(gx), g.y, 1.0f, g.h}, c.WithAlpha(0x22));
            }
        }
        std::vector<Point> pts;
        const size_t n = h.size();
        for (size_t i = 0; i < n; ++i)
        {
            const float px = g.x + g.w * static_cast<float>(i) / static_cast<float>(n - 1);
            const float py = g.Bottom() - g.h * Saturate(h[i]);
            pts.push_back({px, py});
        }
        std::vector<Point> poly = pts;
        poly.push_back({g.Right(), g.Bottom()});
        poly.push_back({g.x, g.Bottom()});
        r.FillPolygon(poly, c.WithAlpha(0x30));
        r.Polyline(pts, c, 1.2f);
        r.StrokeRect(g, c.WithAlpha(0xC0));
    }

    Rect SensorRect(int i) const
    {
        const Rect c = ContentRect();
        return {c.x + 8.0f, c.y + 64.0f + static_cast<float>(i) * 76.0f, std::min(260.0f, c.w * 0.3f), 72.0f};
    }

    void DrawPerformance(Renderer& r)
    {
        const Palette& p = Theme::P();
        const std::vector<Sensor> sensors = Sensors();
        for (int i = 0; i < static_cast<int>(sensors.size()); ++i)
        {
            const Rect sr = SensorRect(i);
            const bool sel = i == m_sensor;
            if (sel || i == m_hoverSensor)
                r.FillRoundRect(sr, 4.0f, sel ? p.subtleHover : p.subtlePressed);
            if (sel)
                r.FillRoundRect({sr.x, sr.CenterY() - 10.0f, 3.0f, 20.0f}, 1.5f, p.accent);
            DrawGraph(r, {sr.x + 12.0f, sr.y + 14.0f, 64.0f, 44.0f}, *sensors[static_cast<size_t>(i)].hist, sensors[static_cast<size_t>(i)].color, false);
            r.Text(sensors[static_cast<size_t>(i)].name, {sr.x + 88.0f, sr.y + 12.0f, sr.w - 94.0f, 20.0f}, p.text, 14.0f);
            r.Text(SensorValue(i), {sr.x + 88.0f, sr.y + 32.0f, sr.w - 94.0f, 36.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Wrap);
        }
        // Правая часть
        const Rect c = ContentRect();
        const float lx = SensorRect(0).Right() + 20.0f;
        const Rect area{lx, c.y + 60.0f, c.Right() - lx - 20.0f, c.Bottom() - c.y - 72.0f};
        if (area.w < 200.0f)
            return;
        const Sensor& s = sensors[static_cast<size_t>(m_sensor)];
        r.Text(s.name, {area.x, area.y, area.w * 0.5f, 34.0f}, p.text, 24.0f, FontWeight::Semibold, TextFlags::VCenter);
        std::wstring right;
        switch (m_sensor)
        {
        case 0:
            right = m_cpuName;
            break;
        case 1:
            right = Fixed1(m_memTotal / 1024.0f) + L"\u00A0ГБ";
            break;
        case 2:
            right = L"NVMe SSD 512 ГБ";
            break;
        case 3:
            right = L"Беспроводной сетевой адаптер Wi-Fi 6";
            break;
        default:
            right = L"Встроенная графика";
            break;
        }
        r.Text(right, {area.x + area.w * 0.4f, area.y, area.w * 0.6f, 34.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::Right | TextFlags::VCenter);
        const wchar_t* graphTitle = m_sensor == 0 ? L"% использования" : (m_sensor == 1 ? L"Использование памяти" : (m_sensor == 2 ? L"Активное время" : (m_sensor == 3 ? L"Пропускная способность" : L"3D")));
        r.Text(graphTitle, {area.x, area.y + 42.0f, area.w * 0.5f, 18.0f}, p.textSecondary, 12.0f);
        r.Text(m_sensor == 3 ? L"2 Мбит/с" : (m_sensor == 1 ? Fixed1(m_memTotal / 1024.0f) + L"\u00A0ГБ" : std::wstring(L"100%")),
               {area.x + area.w * 0.5f, area.y + 42.0f, area.w * 0.5f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Right);
        const float gh = std::max(120.0f, area.h - 300.0f);
        const Rect g{area.x, area.y + 62.0f, area.w, gh};
        DrawGraph(r, g, *s.hist, s.color, true);
        r.Text(L"60 секунд", {g.x, g.Bottom() + 2.0f, 120.0f, 18.0f}, p.textSecondary, 12.0f);
        r.Text(L"0", {g.Right() - 60.0f, g.Bottom() + 2.0f, 60.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Right);
        // Показатели
        float y = g.Bottom() + 30.0f;
        auto big = [&](float x, const std::wstring& label, const std::wstring& value) {
            r.Text(label, {x, y, 230.0f, 18.0f}, p.textSecondary, 12.0f);
            r.Text(value, {x, y + 18.0f, 230.0f, 30.0f}, p.text, 20.0f, FontWeight::Semilight);
        };
        // Правая колонка характеристик (если хватает места) и две колонки крупных показателей слева.
        const float rightW = std::min(300.0f, area.w * 0.46f);
        const bool rightCol = area.w >= 520.0f;
        const float rx = area.Right() - rightW;
        const float leftW = rightCol ? rx - area.x - 16.0f : area.w;
        const float c2 = area.x + std::min(240.0f, leftW * 0.5f);
        auto stat = [&](float x, float yy, const std::wstring& label, const std::wstring& value) {
            const float lw = rightW * 0.6f;
            r.Text(label, {x, yy, lw - 6.0f, 18.0f}, p.textSecondary, 12.0f);
            r.Text(value, {x + lw, yy, rightW - lw, 18.0f}, p.text, 12.0f);
        };
        int procCount = static_cast<int>(m_procs.size()) + 112;
        int threads = 1650;
        for (const Proc& pr : m_procs)
            threads += pr.threads;
        switch (m_sensor)
        {
        case 0:
            big(area.x, L"Использование", std::to_wstring(RoundToInt(m_cpu * 100.0f)) + L"%");
            big(c2, L"Скорость", Fixed1(static_cast<float>(m_mhz) / 1000.0f * (0.9f + m_cpu * 0.25f)) + L"\u00A0ГГц");
            y += 56.0f;
            big(area.x, L"Процессы", std::to_wstring(procCount));
            big(area.x + leftW / 3.0f, L"Потоки", GroupDigits(threads));
            big(area.x + leftW * 2.0f / 3.0f, L"Дескрипторы", GroupDigits(threads * 31));
            y += 56.0f;
            big(area.x, L"Время работы", SysInfo::UptimeText());
            if (rightCol)
            {
                float yy = g.Bottom() + 30.0f;
                stat(rx, yy, L"Базовая скорость:", Fixed1(static_cast<float>(m_mhz) / 1000.0f) + L"\u00A0ГГц");
                stat(rx, yy += 20.0f, L"Сокетов:", L"1");
                stat(rx, yy += 20.0f, L"Ядра:", std::to_wstring(std::max(1, m_cores / 2)));
                stat(rx, yy += 20.0f, L"Логических процессоров:", std::to_wstring(m_cores));
                stat(rx, yy += 20.0f, L"Виртуализация:", L"Включено");
                stat(rx, yy += 20.0f, L"Кэш L1:", L"640\u00A0КБ");
                stat(rx, yy += 20.0f, L"Кэш L2:", L"10,0\u00A0МБ");
                stat(rx, yy += 20.0f, L"Кэш L3:", L"24,0\u00A0МБ");
            }
            break;
        case 1:
        {
            big(area.x, L"Используется (сжатая)", Fixed1(m_memUsed / 1024.0f) + L"\u00A0ГБ (" + Fixed1(m_memUsed / 1024.0f * 0.04f) + L"\u00A0ГБ)");
            big(c2, L"Доступно", Fixed1((m_memTotal - m_memUsed) / 1024.0f) + L"\u00A0ГБ");
            y += 56.0f;
            big(area.x, L"Выделено", Fixed1(m_memUsed / 1024.0f * 1.3f) + L"/" + Fixed1(m_memTotal / 1024.0f * 1.6f) + L"\u00A0ГБ");
            big(c2, L"Кэшировано", Fixed1((m_memTotal - m_memUsed) / 1024.0f * 0.7f) + L"\u00A0ГБ");
            // Структура памяти
            const Rect bar{area.x, y + 84.0f, std::min(leftW, 420.0f), 18.0f};
            r.Text(L"Структура памяти", {bar.x, bar.y - 20.0f, 200.0f, 18.0f}, p.textSecondary, 12.0f);
            r.StrokeRect(bar, s.color);
            const float used = m_memTotal > 0.0f ? m_memUsed / m_memTotal : 0.0f;
            r.FillRect({bar.x + 1.0f, bar.y + 1.0f, (bar.w - 2.0f) * used, bar.h - 2.0f}, s.color.WithAlpha(0x70));
            r.FillRect({bar.x + 1.0f + (bar.w - 2.0f) * used, bar.y + 1.0f, (bar.w - 2.0f) * (1.0f - used) * 0.7f, bar.h - 2.0f}, s.color.WithAlpha(0x25));
            if (rightCol)
            {
                float yy = g.Bottom() + 30.0f;
                stat(rx, yy, L"Скорость:", L"3200 МТ/с");
                stat(rx, yy += 20.0f, L"Использовано гнёзд:", L"2 из 2");
                stat(rx, yy += 20.0f, L"Форм-фактор:", L"SODIMM");
                stat(rx, yy += 20.0f, L"Зарезервировано аппаратно:", L"212\u00A0МБ");
            }
            break;
        }
        case 2:
            big(area.x, L"Активное время", std::to_wstring(RoundToInt(m_disk * 100.0f)) + L"%");
            big(c2, L"Среднее время отклика", Fixed1(0.2f + m_disk * 3.0f) + L"\u00A0мс");
            y += 56.0f;
            big(area.x, L"Скорость чтения", Fixed1(m_disk * 120.0f) + L"\u00A0МБ/с");
            big(c2, L"Скорость записи", Fixed1(m_disk * 45.0f) + L"\u00A0МБ/с");
            if (rightCol)
            {
                float yy = g.Bottom() + 30.0f;
                stat(rx, yy, L"Ёмкость:", L"477\u00A0ГБ");
                stat(rx, yy += 20.0f, L"Отформатировано:", L"476\u00A0ГБ");
                stat(rx, yy += 20.0f, L"Системный диск:", L"Да");
                stat(rx, yy += 20.0f, L"Файл подкачки:", L"Да");
                stat(rx, yy += 20.0f, L"Тип:", L"SSD (NVMe)");
            }
            break;
        case 3:
        {
            const ShellSettings& st = GetShell().Settings();
            big(area.x, L"Отправка", std::to_wstring(RoundToInt(m_net * 0.2f)) + L" Кбит/с");
            big(c2, L"Получение", std::to_wstring(RoundToInt(m_net)) + L" Кбит/с");
            if (rightCol)
            {
                float yy = g.Bottom() + 30.0f;
                stat(rx, yy, L"Имя адаптера:", L"Беспроводная сеть");
                stat(rx, yy += 20.0f, L"SSID:", st.wifi && !st.airplane ? st.network : std::wstring(L"—"));
                stat(rx, yy += 20.0f, L"Тип подключения:", L"802.11ax");
                stat(rx, yy += 20.0f, L"IPv4-адрес:", L"192.168.1.42");
                stat(rx, yy += 20.0f, L"IPv6-адрес:", L"fe80::1c2b:3a4d:5e6f:7081%12");
            }
            break;
        }
        default:
            big(area.x, L"Использование", std::to_wstring(RoundToInt(m_gpu * 100.0f)) + L"%");
            big(c2, L"Общая память GPU", Fixed1(m_gpu * 2.0f + 0.4f) + L"/7,9\u00A0ГБ");
            break;
        }
    }

    // ---- Параметры -------------------------------------------------------------------------
    void DrawSettings(Renderer& r)
    {
        const Palette& p = Theme::P();
        const std::vector<SettingRow>& rows = SettingRows();
        const wchar_t* groups[] = {L"Общие", L"Параметры окна", L"Другие параметры"};
        int group = -1;
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            const Rect rr = SettingRect(i);
            if (rows[static_cast<size_t>(i)].group != group)
            {
                group = rows[static_cast<size_t>(i)].group;
                r.Text(groups[group], {rr.x + 2.0f, rr.y - 32.0f, rr.w, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
            }
            r.FillRoundRect(rr, 4.0f, p.card);
            r.StrokeRoundRect(rr, 4.0f, p.cardStroke);
            const SettingRow& sr = rows[static_cast<size_t>(i)];
            if (*sr.desc)
            {
                r.Text(sr.title, {rr.x + 20.0f, rr.CenterY() - 19.0f, rr.w - 300.0f, 20.0f}, p.text, 14.0f);
                r.Text(sr.desc, {rr.x + 20.0f, rr.CenterY() + 1.0f, rr.w - 300.0f, 18.0f}, p.textSecondary, 12.0f);
            }
            else
                r.Text(sr.title, {rr.x + 20.0f, rr.y, rr.w - 300.0f, rr.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        }
    }

    // ================================================================== отрисовка окна
    std::vector<NavDef> NavItems() const { return std::vector<NavDef>(std::begin(kNav), std::end(kNav)); }
    Rect NavRect(int i) const { return {4.0f, kTitleH + 48.0f + static_cast<float>(i) * 40.0f, NavW() - 8.0f, 36.0f}; }
    Rect HamburgerRect() const { return {4.0f, kTitleH + 4.0f, 48.0f, 36.0f}; }
    Rect SettingsNavRect() const { return {4.0f, Height() - 44.0f, NavW() - 8.0f, 36.0f}; }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        // Заголовок окна
        r.DrawAppIcon(AppIcon::TaskManager, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(L"Диспетчер задач", {44.0f, 0.0f, 200.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        // Навигация
        const Rect hb = HamburgerRect();
        if (m_hoverNav == -2)
            r.FillRoundRect(hb, 4.0f, p.subtleHover);
        r.Glyph(Icon::Hamburger, {hb.x + 16.0f, hb.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        const std::vector<NavDef> nav = NavItems();
        const bool labels = NavW() > 100.0f;
        for (int i = 0; i < static_cast<int>(nav.size()); ++i)
        {
            const Rect nr = NavRect(i);
            const bool sel = nav[static_cast<size_t>(i)].page == m_page;
            if (sel || m_hoverNav == i)
                r.FillRoundRect(nr, 4.0f, sel ? p.subtleHover : p.subtlePressed);
            if (sel)
                r.FillRoundRect({nr.x, nr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Glyph(nav[static_cast<size_t>(i)].icon, {nr.x + 16.0f, nr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            if (labels)
                r.Text(nav[static_cast<size_t>(i)].title, {nr.x + 48.0f, nr.y, nr.w - 52.0f, nr.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        const Rect sn = SettingsNavRect();
        if (m_page == Page::Settings || m_hoverNav == 100)
            r.FillRoundRect(sn, 4.0f, m_page == Page::Settings ? p.subtleHover : p.subtlePressed);
        if (m_page == Page::Settings)
            r.FillRoundRect({sn.x, sn.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
        r.Glyph(Icon::Settings, {sn.x + 16.0f, sn.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        if (labels)
            r.Text(L"Параметры", {sn.x + 48.0f, sn.y, sn.w - 52.0f, sn.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        // Область содержимого — отдельный слой со скруглённым углом.
        const Rect c = ContentRect();
        r.FillRoundRect(c, 8.0f, 0.0f, 0.0f, 0.0f, p.dark ? Color(255, 255, 255, 0x08) : Color(255, 255, 255, 0x90));
        r.StrokeRoundRect({c.x, c.y, c.w + 20.0f, c.h + 20.0f}, 8.0f, p.cardStroke);
        // Заголовок страницы
        const wchar_t* title = L"Параметры";
        for (const NavDef& nd : kNav)
        {
            if (nd.page == m_page)
                title = nd.title;
        }
        r.Text(title, {c.x + 24.0f, c.y + 12.0f, c.w * 0.4f, 36.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        if (m_page == Page::AppHistory)
            r.Text(L"Использование ресурсов с " + FormatDate(m_historyReset) + L" для текущей учётной записи пользователя.", {c.x + 24.0f, c.y + 56.0f, c.w - 48.0f, 22.0f},
                   p.textSecondary, 12.0f);
        if (m_page == Page::Startup)
            r.Text(L"Время последнего запуска BIOS: 4,2 с", {c.x + 24.0f, c.y + 56.0f, c.w - 48.0f, 22.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::Right);
        // Состояние кнопок
        if (m_page == Page::Processes || m_page == Page::Details)
        {
            const Proc* pr = nullptr;
            for (const Proc& x : m_procs)
            {
                if (x.pid == m_sel)
                    pr = &x;
            }
            if (m_cmdButtons.size() >= 3)
            {
                m_cmdButtons[1]->SetEnabled(pr != nullptr);
                m_cmdButtons[2]->SetEnabled(pr != nullptr && pr->group != 2);
            }
        }
        switch (m_page)
        {
        case Page::Performance:
            DrawPerformance(r);
            break;
        case Page::Settings:
            DrawSettings(r);
            break;
        default:
            DrawTable(r);
            break;
        }
        m_ui.Draw(r);
    }

    // ================================================================== ввод
    int RowAt(Point pt) const
    {
        if (!m_bodyRect.Contains(pt))
            return -1;
        const int i = static_cast<int>(std::floor((pt.y - m_bodyRect.y + m_scroll) / kRowH));
        return i >= 0 && i < static_cast<int>(m_rowCache.size()) ? i : -1;
    }

    int NavAt(Point pt) const
    {
        if (HamburgerRect().Contains(pt))
            return -2;
        if (SettingsNavRect().Contains(pt))
            return 100;
        for (int i = 0; i < static_cast<int>(std::size(kNav)); ++i)
        {
            if (NavRect(i).Contains(pt))
                return i;
        }
        return -1;
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        const Point pt = e.pos;
        m_hoverNav = pt.x < NavW() ? NavAt(pt) : -1;
        m_hoverRow = RowAt(pt);
        m_hoverCol = -1;
        for (size_t i = 0; i < m_colRects.size(); ++i)
        {
            if (m_colRects[i].Contains(pt) && (m_page == Page::Processes || m_page == Page::Users))
                m_hoverCol = static_cast<int>(i);
        }
        m_hoverSensor = -1;
        if (m_page == Page::Performance)
        {
            for (int i = 0; i < 5; ++i)
            {
                if (SensorRect(i).Contains(pt))
                    m_hoverSensor = i;
            }
        }
        if (m_hoverNav >= 0 && NavW() < 100.0f && m_hoverNav < static_cast<int>(std::size(kNav)))
            GetShell().Tooltip(&m_hoverNav, kNav[m_hoverNav].title, LocalToScreen(NavRect(m_hoverNav)), false);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_hoverNav = m_hoverRow = m_hoverCol = m_hoverSensor = -1;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        const Point pt = e.pos;
        if (pt.x < NavW() && pt.y > kTitleH)
        {
            if (e.button != MouseButton::Left)
                return;
            const int n = NavAt(pt);
            if (n == -2)
            {
                m_navExpanded = !m_navExpanded;
                OnResize();
            }
            else if (n == 100)
                SetPage(Page::Settings);
            else if (n >= 0)
                SetPage(kNav[n].page);
            return;
        }
        Window::OnMouseDown(e);
        if (m_ui.HasCapture())
            return;
        if (m_page == Page::Performance)
        {
            for (int i = 0; i < 5; ++i)
            {
                if (SensorRect(i).Contains(pt))
                    m_sensor = i;
            }
            return;
        }
        // Сортировка по заголовкам
        for (size_t i = 0; i < m_colRects.size(); ++i)
        {
            if (m_colRects[i].Contains(pt) && (m_page == Page::Processes) && e.button == MouseButton::Left)
            {
                const int col = static_cast<int>(i);
                if (m_sortCol == col)
                    m_sortAsc = !m_sortAsc;
                else
                {
                    m_sortCol = col;
                    m_sortAsc = col <= 1;
                }
                return;
            }
        }
        const int ri = RowAt(pt);
        if (ri < 0)
        {
            if (m_bodyRect.Contains(pt))
                m_sel = -1;
            return;
        }
        const Row& row = m_rowCache[static_cast<size_t>(ri)];
        if (row.kind == 0)
            return;
        if (row.kind == 1)
            m_sel = row.id;
        // Раскрытие группы
        if (row.expandable && e.button == MouseButton::Left && (pt.x < m_bodyRect.x + 28.0f || e.clicks >= 2))
        {
            if (m_page == Page::Users)
                m_userExpanded = !m_userExpanded;
            else if (m_expanded.count(row.id))
                m_expanded.erase(row.id);
            else
                m_expanded.insert(row.id);
            return;
        }
        if (e.clicks >= 2 && e.button == MouseButton::Left && m_page == Page::Processes)
        {
            if (Proc* pr = SelectedProc())
            {
                if (pr->group == 0 && !pr->windows.empty())
                {
                    Window* w = pr->windows.front();
                    PostAction([w]() {
                        if (GetShell().WM().Contains(w))
                            GetShell().WM().Activate(w);
                    });
                }
            }
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog() || e.button != MouseButton::Right)
            return;
        const int ri = RowAt(e.pos);
        if (ri < 0)
            return;
        const Row& row = m_rowCache[static_cast<size_t>(ri)];
        if (row.kind != 1)
            return;
        if (m_page == Page::Processes || m_page == Page::Details)
            ShowRowMenu(row.id, LocalToScreen(e.pos));
        else if (m_page == Page::Services)
        {
            m_sel = row.id;
            std::vector<MenuItem> items = {
                MenuItem(L"Запустить", Icon::Play, [this]() { ServiceAction(1); }).Disabled(m_serviceRunning[static_cast<size_t>(row.id)]),
                MenuItem(L"Остановить", Icon::Stop, [this]() { ServiceAction(0); }).Disabled(!m_serviceRunning[static_cast<size_t>(row.id)]),
                MenuItem(L"Перезапустить", Icon::Refresh, [this]() { ServiceAction(2); }),
                MenuItem::Sep(),
                MenuItem(L"Открыть службы", Icon::Services, nullptr).Disabled(),
                MenuItem(L"Поиск в Интернете", Icon::Search, [n = std::wstring(kServices[row.id].name)]() { GetShell().Launch(AppId::Browser, n); }),
            };
            GetShell().ShowMenu(LocalToScreen(e.pos), std::move(items));
        }
        else if (m_page == Page::Startup)
        {
            m_sel = row.id;
            const bool en = m_startup[static_cast<size_t>(row.id)].enabled;
            std::vector<MenuItem> items = {
                MenuItem(en ? L"Отключить" : L"Включить", en ? Icon::Close : Icon::Check, [this, en]() { SetStartup(!en); }),
                MenuItem(L"Открыть расположение файла", Icon::Folder, []() { GetShell().Launch(AppId::Explorer, L"C:\\Program Files"); }),
                MenuItem(L"Поиск в Интернете", Icon::Search, nullptr).Disabled(),
                MenuItem(L"Свойства", Icon::Properties, nullptr).Disabled(),
            };
            GetShell().ShowMenu(LocalToScreen(e.pos), std::move(items));
        }
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        m_scroll = Clamp(m_scroll - e.wheel * 90.0f, 0.0f, m_maxScroll);
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        if (m_ui.FocusedChild() == m_search)
            return Window::OnKeyDown(vk, mods);
        if (mods.ctrl && (vk == 'F' || vk == 'E'))
        {
            m_ui.SetFocus(m_search);
            m_search->SelectAll();
            return true;
        }
        if (mods.ctrl && vk == VK_TAB)
        {
            const int n = static_cast<int>(std::size(kNav));
            int cur = 0;
            for (int i = 0; i < n; ++i)
            {
                if (kNav[i].page == m_page)
                    cur = i;
            }
            SetPage(kNav[(cur + (mods.shift ? n - 1 : 1)) % n].page);
            return true;
        }
        if (vk == VK_DELETE && (m_page == Page::Processes || m_page == Page::Details))
        {
            EndSelected();
            return true;
        }
        if (vk == VK_UP || vk == VK_DOWN)
        {
            int idx = -1;
            for (size_t i = 0; i < m_rowCache.size(); ++i)
            {
                if (m_rowCache[i].kind == 1 && m_rowCache[i].id == m_sel)
                    idx = static_cast<int>(i);
            }
            const int dir = vk == VK_DOWN ? 1 : -1;
            for (int i = idx + dir; i >= 0 && i < static_cast<int>(m_rowCache.size()); i += dir)
            {
                if (m_rowCache[static_cast<size_t>(i)].kind == 1)
                {
                    m_sel = m_rowCache[static_cast<size_t>(i)].id;
                    const float y = static_cast<float>(i) * kRowH;
                    if (y < m_scroll)
                        m_scroll = y;
                    else if (y + kRowH > m_scroll + m_bodyRect.h)
                        m_scroll = y + kRowH - m_bodyRect.h;
                    break;
                }
            }
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

    // ================================================================== поля
    Page m_page = Page::Processes;
    TextBox* m_search = nullptr;
    std::wstring m_query;
    std::vector<Button*> m_cmdButtons;
    std::vector<Widget*> m_settingsWidgets;
    bool m_navExpanded = true;
    int m_hoverNav = -1;
    // Данные
    SysInfo::CpuSampler m_sampler;
    std::vector<Proc> m_procs;
    std::map<int, float> m_smooth;
    std::set<int> m_efficiency;
    std::set<int> m_expanded;
    float m_cpu = 0.0f;
    float m_memUsed = 0.0f;
    float m_memTotal = 1.0f;
    float m_disk = 0.0f;
    float m_net = 0.0f;
    float m_gpu = 0.0f;
    std::vector<float> m_cpuHist, m_memHist, m_diskHist, m_netHist, m_gpuHist;
    DWORD m_mhz = 2900;
    std::wstring m_cpuName;
    int m_cores = 8;
    int m_tick = 0;
    std::vector<StartupDef> m_startup;
    std::vector<bool> m_serviceRunning;
    SYSTEMTIME m_historyReset = AddDays(LocalNow(), -30);
    // Таблица
    std::vector<Row> m_rowCache;
    std::vector<Rect> m_colRects;
    Rect m_bodyRect;
    float m_scroll = 0.0f;
    float m_maxScroll = 0.0f;
    int m_sel = -1;
    int m_hoverRow = -1;
    int m_hoverCol = -1;
    int m_sortCol = 2;
    bool m_sortAsc = false;
    bool m_groupByType = true;
    bool m_userExpanded = true;
    // Производительность
    int m_sensor = 0;
    int m_hoverSensor = -1;
    // Параметры
    int m_startPage = 0;
    int m_speed = 1;
    bool m_onTop = false;
    bool m_minimizeOnUse = false;
    bool m_hideMin = false;
    bool m_fullNames = false;
    bool m_allHistory = false;
    bool m_confirmEff = true;
};
} // namespace

std::unique_ptr<Window> CreateTaskManager(const std::wstring& /*arg*/) { return std::make_unique<TaskManagerWindow>(); }
