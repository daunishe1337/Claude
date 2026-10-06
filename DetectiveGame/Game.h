#pragma once
// Game.h - сцены игры: вступительная катсцена -> машина (телефон с заказами)
// -> поездка -> миссия (хождение по локации).
#include <string>
#include "Map.h"
#include "Orders.h"
#include "Player.h"
#include "Renderer.h"
#include "UI.h"

class Game {
public:
    Game();

    void Update(double dt, const InputState& in);
    void Render(Renderer& r);
    // Текст интерфейса поверх буфера (GDI). mouseCaptured - для подсказки.
    void DrawOverlay(HDC dc, UI& ui, int w, int h, bool mouseCaptured);
    void OnKey(int vk, bool repeat);

    bool InMission() const { return scene_ == Scene::Mission; }

private:
    enum class Scene { Intro, Car, Driving, Mission };

    void EnterCar(double nextOrderDelay);
    void StartDriving(int orderIndex);
    void StartMission();
    void ReturnToCar();
    bool NearExit() const;

    void RenderIntro(Renderer& r);
    void DrawIntroOverlay(HDC dc, UI& ui, int w, int h);
    void DrawPhone(HDC dc, UI& ui, int w, int h);

    Scene scene_ = Scene::Intro;
    double sceneTime_ = 0.0;  // время в текущей сцене
    double totalTime_ = 0.0;  // общее время (для анимаций)

    Map map_;
    Player player_;
    OrderBook orders_;

    int cursor_ = 0;           // выбранный пункт среди пришедших заказов
    int currentOrder_ = -1;    // заказ, который выполняем
    double arrivalTimer_ = 0.0; // через сколько придёт следующее сообщение
    double newMsgFlash_ = 0.0;  // индикатор "новое сообщение"
    bool showMap_ = false;      // мини-карта (клавиша M)
};
