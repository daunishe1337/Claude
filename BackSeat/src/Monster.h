// ============================================================================
//  Monster.h — существо на крыше и его конечный автомат.
//
//  Цикл:  Dormant (где-то в темноте) -> Landing (удар о крышу)
//         -> Crawling (ползёт по крыше к цели: левое окно / люк / правое окно)
//         -> Peeking (появляется в окне, угроза растёт медленно)
//         -> BreakingIn (бьёт стекло, угроза растёт быстро)
//         -> Entered (угроза 100%: игрок проиграл)
//  Вспышка камеры по верному окну -> Repelled (вздрагивает, визжит) ->
//  Retreating (убегает по крыше и спрыгивает) -> Dormant.
//  Иногда монстр сам отступает (Crawling/Peeking -> Retreating) — «дразнит».
//
//  Скорость и агрессия растут со временем (параметр aggression 0..1).
//  Модуль ничего не рисует и не играет звуки — он выдаёт события
//  (MonsterEventInfo), а Game/RealWorldScene превращают их в звук и картинку.
// ============================================================================
#pragma once

#include "Common.h"

#include <memory>
#include <vector>

// Точки проникновения (и одновременно цели прицеливания камеры).
enum class Entry : int { Left = 0, Sunroof = 1, Right = 2, Count = 3, None = -1 };

enum class MonsterState {
    Dormant,    // нет на машине; ждёт (таймер зависит от агрессии)
    Landing,    // только что запрыгнул на крышу (~0.8 с)
    Crawling,   // ползёт по крыше к цели
    Peeking,    // показался в окне цели
    BreakingIn, // ломится внутрь
    Repelled,   // отшатнулся от вспышки (~0.5 с)
    Retreating, // уходит по крыше и спрыгивает
    Entered,    // пролез в салон — проигрыш
};

enum class MonsterEvent {
    Landed,   // тяжёлый удар о крышу (звук RoofThud, тряска)
    Step,     // шаг/перехват (RoofStep), pan — позиция
    Scrape,   // скрежет когтей (RoofScrape)
    Appear,   // показался в окне (тихое рычание)
    Growl,    // рычание у окна
    Knock,    // лёгкий стук по стеклу
    Bang,     // сильный удар по стеклу (тряска, трещины)
    Crack,    // трещина стала больше
    Screech,  // визг от вспышки
    LeftRoof, // спрыгнул с крыши
    BrokeIn,  // проник в салон
};

struct MonsterEventInfo {
    MonsterEvent type = MonsterEvent::Step;
    float pan = 0.0f;       // -1 лево .. 1 право (для стереозвука)
    float intensity = 1.0f; // 0..1 сила (громкость/тряска)
    Entry entry = Entry::None;
};

class Monster {
public:
    Monster();
    ~Monster();
    Monster(const Monster&) = delete;
    Monster& operator=(const Monster&) = delete;

    // Полный сброс. firstDelay — сколько секунд до первого появления.
    void reset(uint32_t seed, float firstDelay = 20.0f);

    // aggression 0..1 — растёт с прогрессом поездки.
    // breakRateMul — множитель скорости взлома (замок даёт < 1).
    // lightLevel 0..1 — насколько ярко снаружи (фонари) — справочно, может
    // слегка тормозить рост угрозы.
    void update(float dt, float aggression, float breakRateMul, float lightLevel);

    // Попытка отпугнуть вспышкой, нацеленной в aimed.
    // Попадание, если монстр виден в этом окне (Peeking/BreakingIn с target ==
    // aimed, либо Crawling прямо над люком при aimed == Sunroof).
    // superFlash — усиленная вспышка (после батарейки): сильнее снижает угрозу и
    // дольше держит монстра вдали. Возвращает true при попадании.
    bool tryRepel(Entry aimed, bool superFlash);

    // Шум (авария в мини-игре) привлекает монстра: сокращает ожидание в Dormant.
    void attract(float seconds);

    // Снижение угрозы извне (предмет «замок»).
    void reduceThreat(float amount);

    // Принудительное бегство (приезд домой: свет дома/фар отпугивает).
    // Если монстр на крыше — уходит (Retreating) без визга. permanently —
    // больше не возвращается до reset().
    void flee(bool permanently);

    // ---- Состояние для рендера/звука/HUD ---------------------------------------
    MonsterState state() const;
    float stateTime() const;     // секунд в текущем состоянии
    Entry target() const;        // к какому окну идёт/у какого окна (None в Dormant)
    float roofX() const;         // позиция на крыше: -1 (лево) .. 1 (право)
    float roofZ() const;         // 0 (задняя часть крыши) .. 1 (передняя)
    bool onRoof() const;         // Landing/Crawling/Peeking/BreakingIn/Repelled/Retreating
    float threat() const;        // 0..100: насколько близко к проникновению
    float appear() const;        // 0..1: насколько монстр «выглянул» в окно target()
    float crack(Entry e) const;  // 0..1: косметические трещины на стекле окна e
    bool visibleAt(Entry e) const; // виден ли сейчас в окне e (для попадания камерой)
    float bangPulse() const;     // 0..1: затухающий импульс последнего удара (тряска)
    float stepPulse() const;     // 0..1: затухающий импульс последнего шага (вмятина)
    float timeToNextVisit() const; // для отладки: секунд до появления (в Dormant)
    int timesRepelled() const;   // сколько раз отпугнут за поездку

    // Забрать накопленные события (очищает очередь).
    std::vector<MonsterEventInfo> takeEvents();

private:
    struct State; // всё состояние — в Monster.cpp
    std::unique_ptr<State> st_;
};
