// Monster.h - логика монстра на крыше. Чистая логика: без рендера и звука.
// Наружу отдаёт состояние и очередь событий (шаг, стук, треск...), которые
// RealWorldScene превращает в звук и картинку.
#pragma once
#include <vector>

namespace lrh {

enum class MState { Dormant, Crawling, AtWindow, Breaching, Retreating, Reached };
enum class Slot { Left = 0, Top = 1, Right = 2 };
enum class MEventType { Step, Knock, Crack, Hiss, Arrive, Scared, Reached };

struct MEvent {
    MEventType type;
    float pan;  // -1..1, откуда слышен звук
};

class Monster {
public:
    void reset();
    // gameTime - секунды с начала забега (определяет агрессию).
    void update(float dt, float gameTime);
    // Игрок сделал снимок. true - монстр отпугнут.
    bool onFlash();

    MState state() const { return state_; }
    Slot slot() const { return slot_; }
    float stateTime() const { return t_; }
    float crawl() const { return crawl_; }        // 0..1 положение на крыше (0 - сзади, 1 - у цели)
    float breach() const { return breach_; }      // 0..1 прогресс «проникновения»
    float retreatT() const { return retreatDur_ > 0 ? t_ / retreatDur_ : 0; }
    float aggression() const { return aggr_; }
    bool atWindow() const { return state_ == MState::AtWindow || state_ == MState::Breaching; }
    float threatTarget() const;                   // целевое значение шкалы угрозы
    float scrapeLevel() const { return scrape_; } // громкость скрежета
    float scrapePan() const;
    float slotPan() const;                        // -1 / 0 / +1
    std::vector<MEvent>& events() { return events_; }

private:
    void startCrawl();
    void emit(MEventType t, float pan) { events_.push_back({t, pan}); }
    static float mixf(float a, float b, float t) { return a + (b - a) * t; }

    MState state_ = MState::Dormant;
    Slot slot_ = Slot::Left;
    Slot lastSlot_ = Slot::Top;
    float t_ = 0;           // время в текущем состоянии
    float wait_ = 14.0f;    // пауза до появления
    float crawlDur_ = 8, lingerDur_ = 3, breachDur_ = 5, retreatDur_ = 2;
    float crawl_ = 0, breach_ = 0, aggr_ = 0, scrape_ = 0;
    float stepT_ = 0, knockT_ = 0, gust_ = 0, gustT_ = 0;
    int crackStage_ = 0;
    std::vector<MEvent> events_;
};

}  // namespace lrh
