// ============================================================================
//  Game.h — главный конечный автомат игры и связка всех модулей.
//
//  Состояния:
//    Menu      — титульный экран (ENTER — начать, ESC — выход)
//    Intro     — короткая вступительная надпись (любая клавиша — пропуск)
//    Playing   — поездка; внутри два вида: реальный мир / консоль (TAB/ПРОБЕЛ)
//    Paused    — пауза (ESC), R — заново, Q — в меню
//    Dying     — скример (монстр пролез / машина заглохла), ~1.5 с
//    GameOver  — экран проигрыша (R — заново, ESC — меню)
//    Arriving  — приезд домой (дом в лобовом стекле)
//    Victory   — экран победы (R — заново, ESC — меню)
//
//  Механики (всё считается здесь, в фиксированном шаге 60 Гц):
//    * бензин падает со временем и пополняется только канистрами из мини-игры;
//      0 — двигатель чихает несколько секунд, затем глохнет => проигрыш;
//    * путь до дома растёт со скоростью машины => победа;
//    * камера (из мини-игры): снимок со вспышкой отпугивает монстра в окне,
//      на которое наведён видоискатель; после снимка — перезарядка;
//    * батарейка: мгновенная перезарядка + усиленная вспышка;
//    * замок: окна заблокированы на время (взлом медленнее), угроза снижается;
//    * авария в мини-игре: громкий писк привлекает монстра.
// ============================================================================
#pragma once

#include "Audio.h"
#include "ConsoleMiniGame.h"
#include "Input.h"
#include "Monster.h"
#include "Parents.h"
#include "RealWorldScene.h"
#include "Renderer.h"

#include <filesystem>
#include <memory>

enum class GameState { Menu, Intro, Playing, Paused, Dying, GameOver, Arriving, Victory };
enum class ViewMode { RealWorld, Console };
enum class LoseReason { None, Monster, Fuel };

class Game {
public:
    explicit Game(Audio& audio);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Один фиксированный шаг логики (dt = 1/60). Input уже защёлкнут (beginTick).
    void update(float dt, const Input& input);
    // Отрисовать текущий кадр в холст 320x180.
    void render(Canvas& out);

    // Файл сохранения достижений (задаёт платформа; без него — только в памяти).
    void setSaveFile(const std::filesystem::path& file);

    // Окно потеряло фокус: в поездке ставим паузу.
    void onFocusLost();

    // Платформа читает эти флаги после update().
    bool wantsQuit() const;
    // true один раз после запроса переключения полноэкранного режима (F11).
    bool takeFullscreenToggle();

    // ---- Сведения для отладки и автотестов ------------------------------------
    GameState state() const;
    ViewMode view() const;
    LoseReason loseReason() const;
    float fuel() const;          // 0..100
    float tripProgress() const;  // 0..1
    float tripTime() const;      // секунд поездки
    bool hasCamera() const;
    float cameraCharge() const;  // 0..1
    Entry aim() const;
    const Monster& monster() const;
    const ConsoleMiniGame& miniGame() const;
    // Начать поездку сразу, минуя меню и вступление (для тестов).
    void debugStartTrip(uint32_t seed);

private:
    struct State; // всё состояние — в Game.cpp
    std::unique_ptr<State> st_;
};
