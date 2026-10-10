#pragma once
// Фабрики окон приложений и системных диалогов.

#include "../WindowManager.h"

struct FsNode;

std::unique_ptr<Window> CreateExplorer(const std::wstring& arg);
std::unique_ptr<Window> CreateNotepad(const std::wstring& arg);
std::unique_ptr<Window> CreateSettings(const std::wstring& arg);
std::unique_ptr<Window> CreateCalculator(const std::wstring& arg);
std::unique_ptr<Window> CreateTerminal(const std::wstring& arg);
std::unique_ptr<Window> CreateTaskManager(const std::wstring& arg);
std::unique_ptr<Window> CreatePaint(const std::wstring& arg);
std::unique_ptr<Window> CreatePhotos(const std::wstring& arg);
std::unique_ptr<Window> CreateClock(const std::wstring& arg);
std::unique_ptr<Window> CreateWeather(const std::wstring& arg);
std::unique_ptr<Window> CreateBrowser(const std::wstring& arg);
std::unique_ptr<Window> CreateMediaPlayer(const std::wstring& arg);
std::unique_ptr<Window> CreateCalendar(const std::wstring& arg);
std::unique_ptr<Window> CreatePlaceholder(AppId id, const std::wstring& arg);

std::unique_ptr<Window> CreateMessageBox(const std::wstring& title, const std::wstring& text, AppIcon icon);
std::unique_ptr<Window> CreateProperties(FsNode* node);
std::unique_ptr<Window> CreateShutdownDialog();
