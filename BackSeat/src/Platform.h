// ============================================================================
//  Platform.h — окно WinAPI, сообщения, вывод буфера через StretchDIBits.
//
//  Заголовок намеренно не включает <windows.h>: всё WinAPI-состояние
//  спрятано в Platform.cpp (идиома pimpl).
// ============================================================================
#pragma once

#include "Input.h"
#include "Renderer.h"

#include <filesystem>
#include <memory>

class Platform {
public:
    Platform();
    ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    // Создаёт окно, вмещающее gameW x gameH с целым масштабом (по размеру экрана).
    bool create(const wchar_t* title, int gameW, int gameH);

    // Обрабатывает все сообщения в очереди. false — окно закрыто, пора выходить.
    bool pumpMessages(Input& input);

    // Выводит кадр в окно (масштабирование ближайшим соседом + чёрные поля).
    void present(const Canvas& frame);

    // Переключение «окно <-> полноэкранный режим без рамки».
    void toggleFullscreen();

    // true один раз после потери фокуса окном.
    bool takeFocusLost();

    // Окно свёрнуто (можно не рисовать и экономить CPU).
    bool minimized() const;

    // Высокоточное время в секундах с момента запуска.
    double now() const;

    // Файл сохранения рядом с .exe (достижения). Пустой путь — если узнать не удалось.
    std::filesystem::path saveFilePath() const;

    // Короткий сон (мс) для экономии CPU между кадрами.
    void sleepMs(int ms) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
