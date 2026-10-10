#include "Apps.h"

#include "apps/AppFactories.h"

namespace
{
const std::vector<AppInfo>& Registry()
{
    static const std::vector<AppInfo> infos = {
        {AppId::Explorer, L"Проводник", AppIcon::Explorer, false, L"Системное приложение"},
        {AppId::Notepad, L"Блокнот", AppIcon::Notepad, false, L"Приложение"},
        {AppId::Settings, L"Параметры", AppIcon::Settings, true, L"Системное приложение"},
        {AppId::Calculator, L"Калькулятор", AppIcon::Calculator, false, L"Приложение"},
        {AppId::Terminal, L"Терминал", AppIcon::Terminal, false, L"Приложение"},
        {AppId::TaskManager, L"Диспетчер задач", AppIcon::TaskManager, true, L"Системное приложение"},
        {AppId::Paint, L"Paint", AppIcon::Paint, false, L"Приложение"},
        {AppId::Photos, L"Фотографии", AppIcon::Photos, false, L"Приложение"},
        {AppId::Clock, L"Часы", AppIcon::Clock, true, L"Приложение"},
        {AppId::Weather, L"Погода", AppIcon::Weather, true, L"Приложение"},
        {AppId::Browser, L"Браузер", AppIcon::Browser, false, L"Приложение"},
        {AppId::Mail, L"Почта", AppIcon::Mail, true, L"Приложение"},
        {AppId::Calendar, L"Календарь", AppIcon::Calendar, true, L"Приложение"},
        {AppId::Store, L"Магазин", AppIcon::Store, true, L"Приложение"},
        {AppId::Camera, L"Камера", AppIcon::Camera, true, L"Приложение"},
        {AppId::MediaPlayer, L"Медиаплеер", AppIcon::MediaPlayer, true, L"Приложение"},
        {AppId::Maps, L"Карты", AppIcon::Maps, true, L"Приложение"},
        {AppId::Tips, L"Советы", AppIcon::Tips, true, L"Приложение"},
        {AppId::System, L"Система", AppIcon::Settings, false, L"Системный компонент"},
    };
    return infos;
}
} // namespace

const AppInfo& GetAppInfo(AppId id)
{
    static const AppInfo none{};
    for (const AppInfo& a : Registry())
    {
        if (a.id == id)
            return a;
    }
    return none;
}

const std::vector<AppId>& AllApps()
{
    static const std::vector<AppId> apps = [] {
        std::vector<AppId> v;
        for (const AppInfo& a : Registry())
        {
            if (a.id != AppId::System)
                v.push_back(a.id);
        }
        return v;
    }();
    return apps;
}

const std::vector<AppId>& StartPinnedApps()
{
    static const std::vector<AppId> apps = {
        AppId::Browser,  AppId::Mail,       AppId::Calendar,    AppId::Store,  AppId::Photos,      AppId::Settings,
        AppId::Explorer, AppId::Calculator, AppId::Clock,       AppId::Notepad, AppId::Paint,      AppId::Terminal,
        AppId::Weather,  AppId::TaskManager, AppId::MediaPlayer, AppId::Camera, AppId::Maps,       AppId::Tips,
    };
    return apps;
}

const std::vector<AppId>& TaskbarPinnedApps()
{
    static const std::vector<AppId> apps = {AppId::Explorer, AppId::Browser, AppId::Notepad, AppId::Calculator,
                                            AppId::Settings, AppId::Terminal};
    return apps;
}

std::unique_ptr<Window> CreateAppWindow(AppId id, const std::wstring& arg)
{
    switch (id)
    {
    case AppId::Explorer:
        return CreateExplorer(arg);
    case AppId::Notepad:
        return CreateNotepad(arg);
    case AppId::Settings:
        return CreateSettings(arg);
    case AppId::Calculator:
        return CreateCalculator(arg);
    case AppId::Terminal:
        return CreateTerminal(arg);
    case AppId::TaskManager:
        return CreateTaskManager(arg);
    case AppId::Paint:
        return CreatePaint(arg);
    case AppId::Photos:
        return CreatePhotos(arg);
    case AppId::Clock:
        return CreateClock(arg);
    case AppId::Weather:
        return CreateWeather(arg);
    case AppId::Browser:
        return CreateBrowser(arg);
    case AppId::MediaPlayer:
        return CreateMediaPlayer(arg);
    case AppId::Calendar:
        return CreateCalendar(arg);
    case AppId::Mail:
    case AppId::Store:
    case AppId::Camera:
    case AppId::Maps:
    case AppId::Tips:
        return CreatePlaceholder(id, arg);
    case AppId::System:
    case AppId::None:
        break;
    }
    return nullptr;
}
