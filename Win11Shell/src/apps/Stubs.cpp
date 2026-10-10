// Временные заглушки (будут заменены полноценными приложениями).
#include "../Shell.h"
#include "AppFactories.h"

std::unique_ptr<Window> CreateExplorer(const std::wstring& arg) { return CreatePlaceholder(AppId::Explorer, arg); }
std::unique_ptr<Window> CreateNotepad(const std::wstring& arg) { return CreatePlaceholder(AppId::Notepad, arg); }
std::unique_ptr<Window> CreateSettings(const std::wstring& arg) { return CreatePlaceholder(AppId::Settings, arg); }
std::unique_ptr<Window> CreateCalculator(const std::wstring& arg) { return CreatePlaceholder(AppId::Calculator, arg); }
std::unique_ptr<Window> CreateTerminal(const std::wstring& arg) { return CreatePlaceholder(AppId::Terminal, arg); }
std::unique_ptr<Window> CreateTaskManager(const std::wstring& arg) { return CreatePlaceholder(AppId::TaskManager, arg); }
std::unique_ptr<Window> CreatePaint(const std::wstring& arg) { return CreatePlaceholder(AppId::Paint, arg); }
std::unique_ptr<Window> CreateClock(const std::wstring& arg) { return CreatePlaceholder(AppId::Clock, arg); }
std::unique_ptr<Window> CreateWeather(const std::wstring& arg) { return CreatePlaceholder(AppId::Weather, arg); }
std::unique_ptr<Window> CreateBrowser(const std::wstring& arg) { return CreatePlaceholder(AppId::Browser, arg); }
std::unique_ptr<Window> CreateMediaPlayer(const std::wstring& arg) { return CreatePlaceholder(AppId::MediaPlayer, arg); }
std::unique_ptr<Window> CreateCalendar(const std::wstring& arg) { return CreatePlaceholder(AppId::Calendar, arg); }
