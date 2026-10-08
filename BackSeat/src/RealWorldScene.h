// ============================================================================
//  RealWorldScene.h — «реальный мир»: вид с заднего сиденья ночной машины.
//
//  Что рисуется (320x180):
//    * потолок салона с люком (через стекло — ночное небо);
//    * лобовое стекло: дорога в свете фар, разметка, фонари, встречные огни;
//    * левое и правое задние боковые окна: деревья/столбы с параллаксом;
//    * спинки передних сидений и силуэты родителей (головы двигаются, когда
//      они спорят), зеркало заднего вида;
//    * тряска машины, проезжающие фонари (полоса тёплого света по салону);
//    * монстр: вмятины/пыль на потолке, когда он ползёт, тень над люком,
//      руки и голова в окне цели (вниз головой с крыши), трещины на стекле;
//    * опущенная на колени консоль (уменьшенный экран мини-игры);
//    * HUD реального мира: бензин, путь до дома, угроза, камера, видоискатель.
//
//  Модуль не меняет игровую логику (угроза, бензин — в Game/Monster), он
//  только визуализирует переданное состояние и хранит косметику (прокрутка
//  пейзажа, фонари, тряска, частицы, полароид).
// ============================================================================
#pragma once

#include "Common.h"
#include "Monster.h"
#include "Renderer.h"

#include <memory>

class Parents;

// Всё, что сцене нужно знать от Game для одного кадра.
struct RealWorldView {
    float time = 0.0f;          // секунд с начала поездки
    float tripProgress = 0.0f;  // 0..1 путь до дома
    float kmLeft = 0.0f;        // километров до дома (для HUD)
    float carSpeed = 1.0f;      // 0..1 скорость машины (0 — стоит)
    float engineAlive = 1.0f;   // 1 — двигатель работает, 0 — заглох (гаснет приборка)
    float fuel = 100.0f;        // 0..100
    bool lowFuelWarning = false;
    bool hasCamera = false;
    float cameraCharge = 1.0f;  // 0..1, 1 — вспышка готова
    bool superFlash = false;    // заряжена усиленная вспышка (после батарейки)
    Entry aim = Entry::None;    // куда наведён видоискатель (None — прямо)
    bool showAim = false;       // рисовать видоискатель
    bool lockActive = false;    // действует «замок»
    float lockLeft = 0.0f;      // секунд замка осталось
    float threat = 0.0f;        // 0..100 (дублирует Monster::threat для удобства)
    float arrive = 0.0f;        // 0..1 — последовательность приезда (дом впереди)
    bool showHud = true;        // HUD (выключается в меню/скринсейвере)
    bool showConsole = true;    // опущенная консоль внизу экрана
    int consoleDpad = 0;        // нажатая сторона крестовины (для анимации)
    bool endless = false;       // бесконечный режим: вместо пути до дома — пройденные км
    float kmDriven = 0.0f;      // пройдено км (для HUD бесконечного режима)
    float facelessTurn = 0.0f;  // 0..1: родители оборачиваются к ребёнку, а лиц у них нет
};

class RealWorldScene {
public:
    RealWorldScene();
    ~RealWorldScene();
    RealWorldScene(const RealWorldScene&) = delete;
    RealWorldScene& operator=(const RealWorldScene&) = delete;

    void reset(uint32_t seed);

    // Косметика: прокрутка пейзажа (зависит от скорости), фонари, тряска
    // дороги, частицы пыли с потолка, анимация полароида.
    void update(float dt, float carSpeed, const Monster& monster);

    // Реакция на событие монстра (пыль с потолка, вмятина, тряска).
    void onMonsterEvent(const MonsterEventInfo& e);
    // Дополнительная тряска (0..1) — от удара, заглохшего двигателя и т.п.
    void addShake(float amount);

    // Сделан снимок: frame — кадр сцены в момент вспышки; hit — попали ли
    // в монстра. Сцена покажет «полароид» с уменьшенным снимком.
    void capturePhoto(const Canvas& frame, bool hit);

    // Полная отрисовка реального мира в холст 320x180.
    // consoleScreen — текущий кадр мини-игры (160x144) для опущенной консоли.
    void render(Canvas& out, const RealWorldView& view, const Monster& monster,
                const Parents& parents, const Canvas& consoleScreen) const;

    // Скример на весь экран (только при проигрыше). t — секунд с начала.
    void renderJumpscare(Canvas& out, float t) const;

    // Куда смотрит точка экрана (для прицеливания мышью): окно или None.
    Entry aimFromPoint(int x, int y) const;
    // Центр окна на экране (для подсказок/анимаций).
    Vec2 entryCenter(Entry e) const;

    // Насколько ярко снаружи прямо сейчас (проезд под фонарём) 0..1.
    float lightLevel() const;
    // true один раз, когда машина проезжает под фонарём (звук «вжух»).
    bool takeStreetlightPassed();
    // Текущее смещение тряски в пикселях (для консоли/HUD в режиме консоли).
    int shakeX() const;
    int shakeY() const;

private:
    struct State; // всё состояние — в RealWorldScene.cpp
    std::unique_ptr<State> st_;
};
