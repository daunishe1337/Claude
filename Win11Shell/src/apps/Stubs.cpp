// Временные заглушки (будут заменены полноценными приложениями).
#include "../Shell.h"
#include "AppFactories.h"

std::unique_ptr<Window> CreateMediaPlayer(const std::wstring& arg) { return CreatePlaceholder(AppId::MediaPlayer, arg); }
std::unique_ptr<Window> CreateCalendar(const std::wstring& arg) { return CreatePlaceholder(AppId::Calendar, arg); }
