// ============================================================================
//  ConsoleMiniGame.h — мини-игра на портативной консоли «GAMEKID».
//
//  Гонка с видом сверху в 4 оттенках (стиль старого карманного экрана
//  160x144). Игрок рулит A/D, собирает канистры (бензин для НАСТОЯЩЕЙ машины)
//  и предметы (камера, батарейка, замок), объезжает препятствия.
//
//  Важно: мини-игра НЕ ставится на паузу, когда игрок смотрит в реальный
//  мир, — она продолжает ехать сама (attended = false, руль отпущен).
//
//  Модуль также умеет рисовать само устройство (корпус, кнопки, экран) —
//  drawHandheld() — чтобы и Game, и RealWorldScene рисовали консоль
//  одинаково.
// ============================================================================
#pragma once

#include "Common.h"
#include "Renderer.h"

#include <memory>
#include <vector>

// 4 оттенка экрана консоли: от тёмного к светлому.
namespace gbpal {
constexpr uint32_t kDarkest = rgb(15, 56, 15);
constexpr uint32_t kDark = rgb(48, 98, 48);
constexpr uint32_t kLight = rgb(139, 172, 15);
constexpr uint32_t kLightest = rgb(155, 188, 15);
} // namespace gbpal

// События мини-игры, которые Game переводит в последствия реального мира.
enum class MiniEvent {
    PickedFuel,    // канистра: +бензин настоящей машине
    PickedCamera,  // камера (первая): игрок получает фотоаппарат
    PickedBattery, // батарейка (если камера уже есть): мгновенная перезарядка + усиленная вспышка
    PickedLock,    // замок: окна заблокированы на время, угроза снижается
    Crashed,       // столкновение: громкий писк консоли привлекает монстра
    Skid,          // занос на масле
};

// Типы объектов на трассе (для отрисовки и для тестового бота).
enum class MiniObjectType { Rock, Cone, Oil, Car, Fuel, Camera, Battery, Lock };

struct MiniObject {
    MiniObjectType type = MiniObjectType::Rock;
    float x = 0.0f;  // центр по горизонтали, пиксели экрана консоли
    float y = 0.0f;  // центр по вертикали (растёт вниз)
    float w = 8.0f;  // размер хитбокса
    float h = 8.0f;
    float vy = 0.0f; // собственная скорость (для машин), пикс/с вниз относительно дороги
    bool alive = true;
};

class ConsoleMiniGame {
public:
    ConsoleMiniGame();
    ~ConsoleMiniGame();
    ConsoleMiniGame(const ConsoleMiniGame&) = delete;
    ConsoleMiniGame& operator=(const ConsoleMiniGame&) = delete;

    // Полный сброс (новая поездка). seed — для детерминированности.
    void reset(uint32_t seed);

    // steer: -1 влево, 0, +1 вправо. attended — игрок смотрит на консоль
    // (если нет, steer игнорируется: руль никто не держит).
    // difficulty 0..1 — растёт с прогрессом поездки (скорость, плотность).
    // hasCamera — у игрока уже есть камера (тогда вместо камеры падают батарейки).
    void update(float dt, int steer, bool attended, float difficulty, bool hasCamera);

    // Нарисовать кадр мини-игры в холст размера cfg::kConsoleW x kConsoleH.
    void render(Canvas& screen) const;

    // Забрать накопленные события (очищает очередь).
    std::vector<MiniEvent> takeEvents();

    // ---- Сведения для HUD и тестов -------------------------------------------
    float playerX() const;        // центр машины игрока (пиксели экрана консоли)
    float playerY() const;        // центр по вертикали
    float roadCenterAt(float screenY) const; // центр дороги на высоте экрана
    float roadHalfWidth() const;
    float speed() const;          // текущая скорость прокрутки, пикс/с
    bool crashed() const;         // сейчас «оглушён» после аварии
    int score() const;            // очки (пройденная дистанция)
    int fuelCollected() const;    // собрано канистр за поездку
    int crashes() const;          // аварий за поездку
    const std::vector<MiniObject>& objects() const;

    // ---- Отрисовка самого устройства ------------------------------------------
    // Рисует горизонтальную консоль с экраном screen по центру (cx, cy — центр
    // экрана на холсте out). scale = 1 — экран 160x144 (вид «консоль у лица»),
    // scale = 0.5 — уменьшенная (консоль опущена на колени). Допустимы и
    // промежуточные значения (анимация подъёма).
    // dpad: -1/0/1 — какая сторона крестовины нажата.
    // brightness 0..1 — освещённость корпуса (экран светится сам).
    // screenGlow — добавочное свечение экрана на окружение (0..1).
    static void drawHandheld(Canvas& out, const Canvas& screen, int cx, int cy, float scale, int dpad,
                             float brightness, float screenGlow);

private:
    struct State; // всё состояние — в ConsoleMiniGame.cpp
    std::unique_ptr<State> st_;
};
