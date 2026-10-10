#pragma once
// Реестр приложений: идентификаторы, названия, иконки и фабрика окон.

#include "Common.h"
#include "Icons.h"

enum class AppId
{
    None,
    Explorer,
    Notepad,
    Settings,
    Calculator,
    Terminal,
    TaskManager,
    Paint,
    Photos,
    Clock,
    Weather,
    Browser,
    Mail,
    Calendar,
    Store,
    Camera,
    MediaPlayer,
    Maps,
    Tips,
    System, // служебные диалоги (свойства, сообщения)
};

struct AppInfo
{
    AppId id = AppId::None;
    const wchar_t* name = L"";
    AppIcon icon = AppIcon::None;
    bool singleInstance = false;
    const wchar_t* kind = L"Приложение";
};

const AppInfo& GetAppInfo(AppId id);
const std::vector<AppId>& AllApps();
const std::vector<AppId>& StartPinnedApps();
const std::vector<AppId>& TaskbarPinnedApps();

class Window;
std::unique_ptr<Window> CreateAppWindow(AppId id, const std::wstring& arg);
