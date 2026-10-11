#include "Apps.h"

#include "apps/AppFactories.h"

namespace
{
const std::vector<AppInfo>& Registry()
{
    static const std::vector<AppInfo> infos = {
        {AppId::Explorer, L"Проводник", AppIcon::Explorer, false, L"Системное приложение", L"explorer files файлы папки"},
        {AppId::Notepad, L"Блокнот", AppIcon::Notepad, true, L"Приложение", L"notepad text текст"},
        {AppId::Settings, L"Параметры", AppIcon::Settings, true, L"Системное приложение", L"settings control panel панель управления"},
        {AppId::Calculator, L"Калькулятор", AppIcon::Calculator, false, L"Приложение", L"calculator calc"},
        {AppId::Terminal, L"Терминал", AppIcon::Terminal, false, L"Приложение", L"terminal cmd powershell console консоль командная строка"},
        {AppId::TaskManager, L"Диспетчер задач", AppIcon::TaskManager, true, L"Системное приложение", L"task manager taskmgr процессы"},
        {AppId::Paint, L"Paint", AppIcon::Paint, false, L"Приложение", L"mspaint рисование"},
        {AppId::Photos, L"Фотографии", AppIcon::Photos, false, L"Приложение", L"photos images pictures фото изображения"},
        {AppId::Clock, L"Часы", AppIcon::Clock, true, L"Приложение", L"clock alarm timer stopwatch focus будильник таймер секундомер фокусировка"},
        {AppId::Weather, L"Погода", AppIcon::Weather, true, L"Приложение", L"weather прогноз"},
        {AppId::Browser, L"Браузер", AppIcon::Browser, false, L"Приложение", L"browser internet web интернет"},
        {AppId::Mail, L"Почта", AppIcon::Mail, true, L"Приложение", L"mail email"},
        {AppId::Calendar, L"Календарь", AppIcon::Calendar, true, L"Приложение", L"calendar"},
        {AppId::Store, L"Магазин", AppIcon::Store, true, L"Приложение", L"store apps приложения"},
        {AppId::Camera, L"Камера", AppIcon::Camera, true, L"Приложение", L"camera фото"},
        {AppId::MediaPlayer, L"Медиаплеер", AppIcon::MediaPlayer, true, L"Приложение", L"media player music video музыка видео"},
        {AppId::Maps, L"Карты", AppIcon::Maps, true, L"Приложение", L"maps карта"},
        {AppId::Tips, L"Советы", AppIcon::Tips, true, L"Приложение", L"tips help справка"},
        {AppId::System, L"Система", AppIcon::Settings, false, L"Системный компонент", L""},
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
