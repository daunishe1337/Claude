// ============================================================================
//  Monster.cpp — конечный автомат существа на крыше (см. Monster.h).
//
//  Модуль чисто логический: двигает монстра по крыше, копит угрозу, ведёт
//  трещины на стёклах и складывает события в очередь. Рисуют и озвучивают
//  их Game/RealWorldScene/Audio.
//
//  Все «ручки» баланса — именованные константы ниже. Пары «...0 / ...1» —
//  значения при агрессии a = 0 (начало поездки) и a = 1; между ними линейная
//  интерполяция. Game передаёт a = прогресс * 0.9.
//
//  Баланс проверен симуляцией (шаг 1/60 с, 300 с пути, первый визит на 22-й
//  секунде, 50 зёрен):
//    * без защиты: от появления в окне до проникновения ~16 с в начале пути
//      и ~9 с в конце;
//    * внимательный игрок (реакция 0.7 с, перезарядка вспышки 4 с) не
//      проигрывает; за поездку 13..16 визитов, угроза не выше ~5;
//    * рассеянный игрок (реакция 2.5 с, каждая вспышка с шансом 30% не в то
//      окно) проигрывает примерно в половине поездок, почти всегда во второй
//      половине пути: два промаха подряд в конце пути уже не исправить.
// ============================================================================
#include "Monster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

// ---- Пауза между визитами (Dormant) ----------------------------------------
constexpr float kDormant0 = 20.0f;        // пауза при a = 0, с
constexpr float kDormant1 = 9.0f;         // ... при a = 1
constexpr float kDormantJitterLo = 0.8f;  // случайный множитель паузы
constexpr float kDormantJitterHi = 1.25f;
constexpr float kSuperFlashBonus = 8.0f;  // +секунд паузы после усиленной вспышки
constexpr float kTeaseFactor = 0.6f;      // ушёл сам («дразнил») — вернётся скорее
constexpr float kAttractFloor = 1.0f;     // шум не сокращает ожидание ниже этого, с
constexpr float kThreatDecay = 1.2f;      // спад угрозы, пока монстра нет, ед./с
constexpr float kNeverTime = 9999.0f;     // timeToNextVisit() после flee(true)

// ---- Приземление (Landing) ---------------------------------------------------
constexpr float kLandTime = 0.9f;         // длительность приземления, с
constexpr float kLandXSpread = 0.6f;      // точка приземления: roofX в [-0.6, 0.6]
constexpr float kLandZ = 0.1f;            // ... у заднего края крыши
constexpr float kLandZJitter = 0.03f;
constexpr float kLandGripTime = 0.5f;     // когти цепляются за крышу — тихий скрежет
constexpr float kLandGripLoud = 0.45f;
constexpr float kRepeatTargetWeight = 0.4f; // вес прошлой цели при выборе новой

// ---- Ползёт по крыше (Crawling) ----------------------------------------------
constexpr float kCrawlSpeed0 = 0.22f;     // скорость, единиц крыши в секунду
constexpr float kCrawlSpeed1 = 0.6f;
constexpr float kStep0 = 0.42f;           // интервал шагов, с
constexpr float kStep1 = 0.24f;
constexpr float kStepJitter = 0.15f;      // ±15% к интервалу шагов
constexpr float kStepLoudLo = 0.5f;       // громкость шага
constexpr float kStepLoudHi = 0.8f;
constexpr float kSurge = 0.6f;            // движение рывками: скорость ±60% в такт шагам
constexpr float kScrapeChance = 0.25f;    // доля шагов со скрежетом когтей...
constexpr float kScrapeDelay = 0.45f;     // ...он тянется между шагами (доля интервала)
constexpr float kScrapeLoudLo = 0.4f;
constexpr float kScrapeLoudHi = 0.75f;
constexpr float kSwitchChance0 = 0.25f;   // на полпути передумать и сменить окно
constexpr float kSwitchChance1 = 0.08f;
constexpr float kGiveUpChance0 = 0.12f;   // на полпути бросить и уйти («дразнит»)
constexpr float kGiveUpChance1 = 0.04f;
constexpr float kSwitchScrapeLoud = 0.8f; // резкий разворот — громкий скрежет

// ---- Выглядывает в окно (Peeking) ----------------------------------------------
constexpr float kAppearTime = 0.7f;       // появление 0 -> 1, с
constexpr float kPeek0 = 3.5f;            // сколько смотрит, прежде чем ломиться, с
constexpr float kPeek1 = 1.6f;
constexpr float kPeekThreat0 = 2.5f;      // рост угрозы, ед./с
constexpr float kPeekThreat1 = 5.0f;
constexpr float kAppearLoud = 0.55f;
constexpr float kGrowlMin = 1.2f;         // интервал рычания, с
constexpr float kGrowlMax = 2.0f;
constexpr float kGrowlLoudLo = 0.6f;
constexpr float kGrowlLoudHi = 0.85f;
constexpr float kKnockAppear = 0.6f;      // стучит, только когда уже видно лицо
constexpr float kKnockMin = 0.8f;         // интервал стука, с
constexpr float kKnockMax = 1.4f;
constexpr float kKnockLoudLo = 0.3f;
constexpr float kKnockLoudHi = 0.5f;
constexpr float kSelfRetreatAggr = 0.5f;  // в начале пути иногда уходит сам...
constexpr float kSelfRetreatChance = 0.1f; // ...с такой вероятностью

// ---- Ломится (BreakingIn) ------------------------------------------------------
constexpr float kBreak0 = 6.5f;           // рост угрозы, ед./с
constexpr float kBreak1 = 14.0f;
constexpr float kLightResist = 0.3f;      // под фонарём ломится до 30% медленнее
constexpr float kBang0 = 0.95f;           // интервал ударов, с
constexpr float kBang1 = 0.5f;
constexpr float kBangJitter = 0.1f;       // ±10% к интервалу ударов
constexpr float kFirstBang = 0.3f;        // первый удар — почти сразу
constexpr float kBangLoudBase = 0.7f;     // громкость удара 0.7..1.0 (растёт с угрозой)
constexpr float kBreakGrowlMin = 2.2f;    // рычание от натуги между ударами, с
constexpr float kBreakGrowlMax = 3.4f;
constexpr int kCrackSteps = 5;            // трещина растёт ступенями по 0.2
constexpr float kMaxThreat = 100.0f;

// ---- Вспышка камеры (Repelled) ---------------------------------------------------
constexpr float kVisibleAppear = 0.3f;    // с какого appear монстр «виден» в окне
constexpr float kSunroofHalfW = 0.3f;     // зона над люком для Crawling: |roofX| < 0.3
constexpr float kSunroofZ0 = 0.35f;       // ... и roofZ в [0.35, 0.75]
constexpr float kSunroofZ1 = 0.75f;
constexpr float kRepelThreat = 32.0f;     // снижение угрозы от вспышки
constexpr float kRepelThreatSuper = 50.0f; // ... от усиленной вспышки
constexpr float kRepelTime = 0.5f;        // вздрагивает, с
constexpr float kRepelAppearFall = 2.5f;  // лицо исчезает из окна за 0.4 с
constexpr float kScreechLoud = 0.85f;

// ---- Уходит (Retreating) ---------------------------------------------------------
constexpr float kRetreatTime = 1.2f;      // пробежка к краю крыши, с
constexpr float kWithdrawFall = 1.4f;     // спокойный уход из окна: appear за ~0.7 с
constexpr float kWithdrawHold = 0.6f;     // бежит, когда appear упал на 0.6 (голова скрылась)
constexpr float kRetreatStep = 0.17f;     // частые шаги
constexpr float kRetreatStepLoudLo = 0.55f;
constexpr float kRetreatStepLoudHi = 0.8f;
constexpr float kRetreatEdgeX = 0.95f;    // спрыгивает с ближайшего бокового края
constexpr float kLeftRoofLoud = 0.85f;

// ---- Импульсы для тряски/вмятин --------------------------------------------------
constexpr float kBangPulseDecay = 3.0f;   // ед./с
constexpr float kStepPulseDecay = 6.0f;

// ---- Прочее ------------------------------------------------------------------------
constexpr float kMaxDt = 0.1f;            // защита от длинных кадров
constexpr float kMaxBreakMul = 4.0f;      // предел множителя скорости взлома
constexpr std::size_t kEventReserve = 32; // очередь событий без аллокаций в игре
constexpr std::size_t kEventCap = 256;    // если события никто не забирает

// Точки у окон (куда ползёт монстр) и стереопанорама звуков у каждого окна.
// Индекс = static_cast<int>(Entry): Left, Sunroof, Right.
constexpr int kEntries = static_cast<int>(Entry::Count);
constexpr std::array<float, kEntries> kAnchorX = {-0.9f, 0.0f, 0.9f};
constexpr std::array<float, kEntries> kAnchorZ = {0.45f, 0.5f, 0.45f};
constexpr std::array<float, kEntries> kEntryPan = {-0.8f, 0.0f, 0.8f};

// Индекс окна в массивах выше или -1 для None/Count.
inline int entryIndex(Entry e) {
    const int i = static_cast<int>(e);
    return (i >= 0 && i < kEntries) ? i : -1;
}
inline float entryPan(Entry e) {
    const int i = entryIndex(e);
    return i >= 0 ? kEntryPan[static_cast<std::size_t>(i)] : 0.0f;
}
inline Entry entryFromIndex(int i) { return static_cast<Entry>(clampi(i, 0, kEntries - 1)); }

// Перемешивание зерна, чтобы монстр не повторял случайность других модулей,
// получивших то же зерно.
inline uint32_t mixSeed(uint32_t x) {
    x ^= 0x4D4F4E53u; // "MONS"
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

// Мягкий разгон и торможение (0..1 -> 0..1).
inline float easeInOut(float t) {
    t = saturate(t);
    return t * t * (3.0f - 2.0f * t);
}

// Безопасные входные параметры: NaN/inf заменяются значением по умолчанию.
inline float sane(float v, float lo, float hi, float fallback) {
    return std::isfinite(v) ? clampf(v, lo, hi) : fallback;
}

// Что монстр решил при старте ползания (бросок кубика один раз на заход).
enum class Feint { None, Switch, GiveUp };

} // namespace

// ============================================================================
//  Состояние
// ============================================================================
struct Monster::State {
    Rng rng;
    MonsterState state = MonsterState::Dormant;
    float stateT = 0.0f;       // секунд в текущем состоянии
    float aggr = 0.0f;         // агрессия последнего update()

    // Позиция на крыше и цель.
    float x = 0.0f, z = 0.0f;
    Entry target = Entry::None;
    Entry lastTarget = Entry::None; // цель прошлого визита (её выбирают реже)

    // Таймеры. timer — длительность/остаток текущей фазы (Dormant: до визита,
    // Peeking: сколько смотреть). Остальные — до следующего звука.
    float timer = 0.0f;
    float stepTimer = 0.0f;
    float stepInterval = 1.0f;
    float growlTimer = 0.0f;
    float knockTimer = 0.0f;
    float bangTimer = 0.0f;
    float scrapeTimer = -1.0f; // отложенный скрежет после шага (< 0 — нет)
    bool gripped = false;      // Landing: скрежет когтей уже был

    // Ползание: пройдено/всего до цели (для решения «на полпути»).
    Feint feint = Feint::None;
    float crawlDone = 0.0f;
    float crawlTotal = 0.0f;

    // Отступление: откуда и куда бежит, задержка, пока голова уходит из окна.
    float fromX = 0.0f, fromZ = 0.0f, toX = 0.0f, toZ = 0.0f;
    float withdrawDelay = 0.0f;
    bool teasing = false;      // уходит сам — вернётся скорее
    bool superHit = false;     // отпугнут усиленной вспышкой — вернётся позже
    bool fledForever = false;  // flee(true): больше не появляется

    // Угроза, появление в окне, трещины, импульсы.
    float threat = 0.0f;
    float appear = 0.0f;
    std::array<float, kEntries> cracks{};
    float bangPulse = 0.0f;
    float stepPulse = 0.0f;
    int timesRepelled = 0;

    std::vector<MonsterEventInfo> events;

    State() { events.reserve(kEventReserve); }

    // ---- Вспомогательное ----
    void enter(MonsterState ns) {
        state = ns;
        stateT = 0.0f;
        scrapeTimer = -1.0f;
    }
    void emit(MonsterEvent type, float pan, float intensity, Entry e) {
        if (events.size() >= kEventCap) events.erase(events.begin()); // никто не забирает
        MonsterEventInfo ev;
        ev.type = type;
        ev.pan = clampf(pan, -1.0f, 1.0f);
        ev.intensity = saturate(intensity);
        ev.entry = e;
        events.push_back(ev);
    }
    // Индекс текущей цели в массивах окон (вне визита цели нет — берём люк).
    std::size_t slot() const {
        const int i = entryIndex(target);
        return static_cast<std::size_t>(i >= 0 ? i : static_cast<int>(Entry::Sunroof));
    }
    float targetX() const { return kAnchorX[slot()]; }
    float targetZ() const { return kAnchorZ[slot()]; }
    float distToTarget() const { return std::hypot(targetX() - x, targetZ() - z); }
    float jitter(float k) { return rng.range(1.0f - k, 1.0f + k); }

    // ---- Переходы ----
    void startLanding();
    void startCrawl(Entry t);
    void startPeeking();
    void startBreaking();
    void startRetreat(bool byItself);
    void finishRetreat();
    void breakIn();
    Entry pickTarget();
    Entry pickOtherTarget();

    // ---- Шаги по состояниям ----
    void updateDormant(float dt);
    void updateLanding();
    void updateCrawling(float dt);
    void updatePeeking(float dt, float breakMul);
    void updateBreaking(float dt, float breakMul, float light);
    void updateRepelled(float dt);
    void updateRetreating(float dt);

    void step(float loudLo, float loudHi, float interval);
    void tickScrape(float dt);
    void bang();
};

// ============================================================================
//  Переходы между состояниями
// ============================================================================
// Удар о крышу у заднего края.
void Monster::State::startLanding() {
    x = rng.signedUnit() * kLandXSpread;
    z = kLandZ + rng.signedUnit() * kLandZJitter;
    target = Entry::None;
    appear = 0.0f;
    gripped = false;
    stepPulse = 1.0f;
    emit(MonsterEvent::Landed, x, 1.0f, Entry::None);
    enter(MonsterState::Landing);
}

// Выбор окна: все равновероятны, кроме прошлого (вес 0.4) — так монстр
// реже повторяется, но и не становится предсказуемым.
Entry Monster::State::pickTarget() {
    std::array<float, kEntries> w{};
    float sum = 0.0f;
    for (int i = 0; i < kEntries; ++i) {
        w[static_cast<std::size_t>(i)] = (entryFromIndex(i) == lastTarget) ? kRepeatTargetWeight : 1.0f;
        sum += w[static_cast<std::size_t>(i)];
    }
    float r = rng.next01() * sum;
    for (int i = 0; i < kEntries; ++i) {
        r -= w[static_cast<std::size_t>(i)];
        if (r < 0.0f) return entryFromIndex(i);
    }
    return entryFromIndex(kEntries - 1);
}

// Одно из двух других окон (для смены цели на полпути).
Entry Monster::State::pickOtherTarget() {
    const int cur = entryIndex(target);
    const int shift = rng.chance(0.5f) ? 1 : 2;
    return entryFromIndex((cur + shift) % kEntries);
}

void Monster::State::startCrawl(Entry t) {
    target = t;
    crawlDone = 0.0f;
    crawlTotal = distToTarget();
    // Финт решается один раз на заход: сменить окно или бросить на полпути.
    const float pSwitch = lerpf(kSwitchChance0, kSwitchChance1, aggr);
    const float pGiveUp = lerpf(kGiveUpChance0, kGiveUpChance1, aggr);
    const float r = rng.next01();
    feint = r < pSwitch ? Feint::Switch : (r < pSwitch + pGiveUp ? Feint::GiveUp : Feint::None);
    // Первый шаг — вскоре после того, как поднялся.
    stepInterval = lerpf(kStep0, kStep1, aggr) * jitter(kStepJitter);
    stepTimer = stepInterval * 0.5f;
    enter(MonsterState::Crawling);
}

void Monster::State::startPeeking() {
    x = targetX();
    z = targetZ();
    appear = 0.0f;
    timer = lerpf(kPeek0, kPeek1, aggr);
    growlTimer = rng.range(kGrowlMin, kGrowlMax);
    knockTimer = rng.range(kKnockMin, kKnockMax);
    emit(MonsterEvent::Appear, entryPan(target), kAppearLoud, target);
    enter(MonsterState::Peeking);
}

void Monster::State::startBreaking() {
    bangTimer = kFirstBang;
    growlTimer = rng.range(kBreakGrowlMin, kBreakGrowlMax);
    enter(MonsterState::BreakingIn);
}

// Уход к заднему краю крыши и ближайшему боку. Если лицо ещё в окне —
// сначала убирает голову, потом бежит.
void Monster::State::startRetreat(bool byItself) {
    teasing = byItself;
    fromX = x;
    fromZ = z;
    float side = 0.0f;
    if (x < -0.01f) side = -1.0f;
    else if (x > 0.01f) side = 1.0f;
    else side = rng.chance(0.5f) ? -1.0f : 1.0f;
    toX = side * kRetreatEdgeX;
    toZ = 0.0f;
    withdrawDelay = appear * kWithdrawHold / kWithdrawFall;
    stepTimer = withdrawDelay + kRetreatStep * 0.5f;
    enter(MonsterState::Retreating);
}

// Спрыгнул: пауза до следующего визита зависит от агрессии и от того, как ушёл.
void Monster::State::finishRetreat() {
    stepPulse = 1.0f; // толчок при прыжке с крыши
    emit(MonsterEvent::LeftRoof, x, kLeftRoofLoud, Entry::None);
    float wait = lerpf(kDormant0, kDormant1, aggr) * rng.range(kDormantJitterLo, kDormantJitterHi);
    if (superHit) wait += kSuperFlashBonus;
    if (teasing) wait *= kTeaseFactor;
    timer = wait;
    superHit = false;
    teasing = false;
    lastTarget = target;
    target = Entry::None;
    appear = 0.0f;
    feint = Feint::None;
    enter(MonsterState::Dormant);
}

void Monster::State::breakIn() {
    threat = kMaxThreat;
    appear = 1.0f;
    cracks[slot()] = 1.0f;
    bangPulse = 1.0f;
    emit(MonsterEvent::BrokeIn, entryPan(target), 1.0f, target);
    enter(MonsterState::Entered);
}

// ============================================================================
//  Звуковые «действия»
// ============================================================================
// Шаг по крыше: вмятина + звук. Изредка следом, между шагами, — скрежет
// когтей (не одновременно с шагом, чтобы звуки не сливались).
void Monster::State::step(float loudLo, float loudHi, float interval) {
    stepPulse = 1.0f;
    emit(MonsterEvent::Step, x, rng.range(loudLo, loudHi), target);
    if (rng.chance(kScrapeChance)) scrapeTimer = interval * kScrapeDelay;
}

void Monster::State::tickScrape(float dt) {
    if (scrapeTimer < 0.0f) return;
    scrapeTimer -= dt;
    if (scrapeTimer > 0.0f) return;
    scrapeTimer = -1.0f;
    emit(MonsterEvent::Scrape, x, rng.range(kScrapeLoudLo, kScrapeLoudHi), target);
}

// Удар по стеклу: тряска, трещина растёт до уровня угрозы ступенями по 0.2.
void Monster::State::bang() {
    bangPulse = 1.0f;
    const float pan = entryPan(target);
    const float loud = kBangLoudBase + 0.2f * threat / kMaxThreat + 0.1f * rng.next01();
    emit(MonsterEvent::Bang, pan, loud, target);
    float& c = cracks[slot()];
    const float next = std::max(c, saturate(threat / kMaxThreat));
    const int before = static_cast<int>(std::floor(c * static_cast<float>(kCrackSteps)));
    const int after = static_cast<int>(std::floor(next * static_cast<float>(kCrackSteps)));
    c = next;
    // Последняя ступень (1.0) — это уже не трещина, а разбитое стекло (BrokeIn).
    if (after > before && after < kCrackSteps)
        emit(MonsterEvent::Crack, pan, 0.5f + 0.1f * static_cast<float>(after), target);
}

// ============================================================================
//  Обновление по состояниям
// ============================================================================
void Monster::State::updateDormant(float dt) {
    threat = std::max(0.0f, threat - kThreatDecay * dt);
    if (fledForever) return;
    timer -= dt;
    if (timer <= 0.0f) startLanding();
}

void Monster::State::updateLanding() {
    if (!gripped && stateT >= kLandGripTime) {
        gripped = true;
        emit(MonsterEvent::Scrape, x, kLandGripLoud, Entry::None);
    }
    if (stateT >= kLandTime) startCrawl(pickTarget());
}

void Monster::State::updateCrawling(float dt) {
    // Шаги: интервал слегка случайный; движение — рывками в такт шагам
    // (быстрее всего в момент шага), средняя скорость остаётся прежней.
    stepTimer -= dt;
    if (stepTimer <= 0.0f) {
        stepInterval = lerpf(kStep0, kStep1, aggr) * jitter(kStepJitter);
        stepTimer += stepInterval;
        if (stepTimer <= 0.0f) stepTimer = stepInterval;
        step(kStepLoudLo, kStepLoudHi, stepInterval);
    }
    tickScrape(dt);
    const float phase = saturate(1.0f - stepTimer / stepInterval);
    const float surge = 1.0f + kSurge * std::cos(kTau * phase);
    const float speed = lerpf(kCrawlSpeed0, kCrawlSpeed1, aggr) * surge;

    const float dx = targetX() - x, dz = targetZ() - z;
    const float d = std::hypot(dx, dz);
    const float move = std::min(d, speed * dt);
    if (d > 1e-6f) {
        x += dx / d * move;
        z += dz / d * move;
    }
    crawlDone += move;

    // Финт на полпути.
    if (feint != Feint::None && crawlDone >= crawlTotal * 0.5f) {
        if (feint == Feint::GiveUp) {
            startRetreat(true);
            return;
        }
        feint = Feint::None;
        target = pickOtherTarget();
        crawlDone = 0.0f;
        crawlTotal = distToTarget();
        emit(MonsterEvent::Scrape, x, kSwitchScrapeLoud, target);
        return;
    }
    if (d - move <= 1e-4f) startPeeking();
}

void Monster::State::updatePeeking(float dt, float breakMul) {
    appear = std::min(1.0f, appear + dt / kAppearTime);
    threat = std::min(kMaxThreat, threat + lerpf(kPeekThreat0, kPeekThreat1, aggr) * dt * breakMul);
    const float pan = entryPan(target);
    growlTimer -= dt;
    if (growlTimer <= 0.0f) {
        growlTimer = rng.range(kGrowlMin, kGrowlMax);
        emit(MonsterEvent::Growl, pan, rng.range(kGrowlLoudLo, kGrowlLoudHi), target);
    }
    if (appear > kKnockAppear) {
        knockTimer -= dt;
        if (knockTimer <= 0.0f) {
            knockTimer = rng.range(kKnockMin, kKnockMax);
            emit(MonsterEvent::Knock, pan, rng.range(kKnockLoudLo, kKnockLoudHi), target);
        }
    }
    if (stateT < timer) return;
    // В начале пути он иногда только пугает и уходит сам.
    if (aggr < kSelfRetreatAggr && rng.chance(kSelfRetreatChance)) startRetreat(true);
    else startBreaking();
}

void Monster::State::updateBreaking(float dt, float breakMul, float light) {
    appear = std::min(1.0f, appear + dt / kAppearTime);
    const float rate = lerpf(kBreak0, kBreak1, aggr) * breakMul * (1.0f - kLightResist * light);
    threat = std::min(kMaxThreat, threat + rate * dt);
    bangTimer -= dt;
    if (bangTimer <= 0.0f) {
        const float interval = lerpf(kBang0, kBang1, aggr) * jitter(kBangJitter);
        bangTimer += interval;
        if (bangTimer <= 0.0f) bangTimer = interval;
        bang();
    }
    growlTimer -= dt;
    if (growlTimer <= 0.0f) {
        growlTimer = rng.range(kBreakGrowlMin, kBreakGrowlMax);
        emit(MonsterEvent::Growl, entryPan(target), rng.range(kGrowlLoudHi, 1.0f), target);
    }
    if (threat >= kMaxThreat) breakIn();
}

void Monster::State::updateRepelled(float dt) {
    appear = std::max(0.0f, appear - kRepelAppearFall * dt);
    if (stateT >= kRepelTime) startRetreat(false);
}

void Monster::State::updateRetreating(float dt) {
    appear = std::max(0.0f, appear - kWithdrawFall * dt);
    const float t = (stateT - withdrawDelay) / kRetreatTime;
    const float e = easeInOut(t);
    x = lerpf(fromX, toX, e);
    z = lerpf(fromZ, toZ, e);
    stepTimer -= dt;
    if (stepTimer <= 0.0f) {
        stepTimer += kRetreatStep * jitter(kStepJitter);
        if (stepTimer <= 0.0f) stepTimer = kRetreatStep;
        step(kRetreatStepLoudLo, kRetreatStepLoudHi, kRetreatStep);
    }
    tickScrape(dt);
    if (t >= 1.0f) finishRetreat();
}

// ============================================================================
//  Публичный интерфейс
// ============================================================================
Monster::Monster() : st_(std::make_unique<State>()) { reset(1u); }
Monster::~Monster() = default;

void Monster::reset(uint32_t seed, float firstDelay) {
    State& s = *st_;
    s = State();
    s.rng.reseed(mixSeed(seed));
    s.timer = sane(firstDelay, 0.0f, 1.0e6f, 20.0f);
}

void Monster::update(float dt, float aggression, float breakRateMul, float lightLevel) {
    State& s = *st_;
    if (!(dt > 0.0f)) return; // отсекает и NaN
    dt = std::min(dt, kMaxDt);
    s.aggr = sane(aggression, 0.0f, 1.0f, 0.0f);
    const float breakMul = sane(breakRateMul, 0.0f, kMaxBreakMul, 1.0f);
    const float light = sane(lightLevel, 0.0f, 1.0f, 0.0f);

    s.stateT += dt;
    s.bangPulse = approach(s.bangPulse, 0.0f, kBangPulseDecay * dt);
    s.stepPulse = approach(s.stepPulse, 0.0f, kStepPulseDecay * dt);

    switch (s.state) {
    case MonsterState::Dormant: s.updateDormant(dt); break;
    case MonsterState::Landing: s.updateLanding(); break;
    case MonsterState::Crawling: s.updateCrawling(dt); break;
    case MonsterState::Peeking: s.updatePeeking(dt, breakMul); break;
    case MonsterState::BreakingIn: s.updateBreaking(dt, breakMul, light); break;
    case MonsterState::Repelled: s.updateRepelled(dt); break;
    case MonsterState::Retreating: s.updateRetreating(dt); break;
    case MonsterState::Entered: break; // конец игры: ждём reset()
    }
}

bool Monster::tryRepel(Entry aimed, bool superFlash) {
    State& s = *st_;
    if (!visibleAt(aimed)) return false;
    const float pan = s.state == MonsterState::Crawling ? s.x : entryPan(aimed);
    s.threat = std::max(0.0f, s.threat - (superFlash ? kRepelThreatSuper : kRepelThreat));
    s.superHit = superFlash;
    ++s.timesRepelled;
    s.emit(MonsterEvent::Screech, pan, superFlash ? 1.0f : kScreechLoud, aimed);
    s.enter(MonsterState::Repelled);
    return true;
}

void Monster::attract(float seconds) {
    State& s = *st_;
    if (s.state != MonsterState::Dormant || s.fledForever || !(seconds > 0.0f)) return;
    if (s.timer > kAttractFloor) s.timer = std::max(kAttractFloor, s.timer - seconds);
}

void Monster::delay(float seconds) {
    State& s = *st_;
    if (s.state != MonsterState::Dormant || !(seconds > 0.0f)) return;
    s.timer = std::min(s.timer + seconds, 1.0e6f);
}

void Monster::reduceThreat(float amount) {
    State& s = *st_;
    if (s.state == MonsterState::Entered || !(amount > 0.0f)) return;
    s.threat = std::max(0.0f, s.threat - amount);
}

void Monster::flee(bool permanently) {
    State& s = *st_;
    if (s.state == MonsterState::Entered) return;
    if (permanently) s.fledForever = true;
    switch (s.state) {
    case MonsterState::Landing:
    case MonsterState::Crawling:
    case MonsterState::Peeking:
    case MonsterState::BreakingIn: s.startRetreat(false); break;
    default: break; // Dormant — и так нет; Repelled/Retreating — уже уходит
    }
}

// ---- Состояние для рендера/звука/HUD ---------------------------------------
MonsterState Monster::state() const { return st_->state; }
float Monster::stateTime() const { return st_->stateT; }
Entry Monster::target() const { return st_->target; }
float Monster::roofX() const { return st_->x; }
float Monster::roofZ() const { return st_->z; }

bool Monster::onRoof() const {
    const MonsterState m = st_->state;
    return m != MonsterState::Dormant && m != MonsterState::Entered;
}

float Monster::threat() const { return st_->threat; }

float Monster::appear() const {
    switch (st_->state) {
    case MonsterState::Peeking:
    case MonsterState::BreakingIn:
    case MonsterState::Repelled:
    case MonsterState::Retreating: return st_->appear;
    case MonsterState::Entered: return 1.0f;
    default: return 0.0f;
    }
}

float Monster::crack(Entry e) const {
    const int i = entryIndex(e);
    return i >= 0 ? st_->cracks[static_cast<std::size_t>(i)] : 0.0f;
}

bool Monster::visibleAt(Entry e) const {
    const State& s = *st_;
    if (entryIndex(e) < 0) return false;
    if ((s.state == MonsterState::Peeking || s.state == MonsterState::BreakingIn) && s.target == e &&
        s.appear > kVisibleAppear)
        return true;
    return s.state == MonsterState::Crawling && e == Entry::Sunroof && std::fabs(s.x) < kSunroofHalfW &&
           s.z >= kSunroofZ0 && s.z <= kSunroofZ1;
}

float Monster::bangPulse() const { return st_->bangPulse; }
float Monster::stepPulse() const { return st_->stepPulse; }

float Monster::timeToNextVisit() const {
    const State& s = *st_;
    if (s.state != MonsterState::Dormant) return 0.0f;
    return s.fledForever ? kNeverTime : std::max(0.0f, s.timer);
}

int Monster::timesRepelled() const { return st_->timesRepelled; }

// Копия очереди (без аллокации, если событий нет) — собственный буфер
// с зарезервированной ёмкостью остаётся у монстра.
std::vector<MonsterEventInfo> Monster::takeEvents() {
    State& s = *st_;
    std::vector<MonsterEventInfo> out(s.events.begin(), s.events.end());
    s.events.clear();
    return out;
}
