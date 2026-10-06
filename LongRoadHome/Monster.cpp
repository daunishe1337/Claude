// Monster.cpp
#include "Monster.h"
#include "Common.h"

namespace lrh {

void Monster::reset() {
    *this = Monster();
    events_.reserve(16);
}

float Monster::slotPan() const {
    return slot_ == Slot::Left ? -1.0f : (slot_ == Slot::Right ? 1.0f : 0.0f);
}

float Monster::scrapePan() const {
    // Ползущий монстр идёт от центра (сзади) к своему окну.
    if (state_ == MState::Crawling) return slotPan() * smoothstep(crawl_);
    return slotPan();
}

float Monster::threatTarget() const {
    switch (state_) {
        case MState::Dormant: return 0.0f;
        case MState::Crawling: return 0.08f + 0.22f * crawl_;
        case MState::AtWindow: return 0.35f + 0.15f * saturate(t_ / lingerDur_);
        case MState::Breaching: return 0.50f + 0.50f * breach_;
        case MState::Retreating: return 0.20f * (1.0f - retreatT());
        case MState::Reached: return 1.0f;
    }
    return 0.0f;
}

void Monster::startCrawl() {
    // Выбираем цель, стараясь не повторяться подряд.
    Slot pick;
    do { pick = Slot(g_rng.irange(0, 2)); } while (pick == lastSlot_ && g_rng.chance(0.8f));
    slot_ = lastSlot_ = pick;
    state_ = MState::Crawling;
    t_ = 0; crawl_ = 0; breach_ = 0; crackStage_ = 0;
    crawlDur_ = mixf(9.0f, 5.0f, aggr_) * g_rng.range(0.9f, 1.1f);
    stepT_ = 0.2f;
}

void Monster::update(float dt, float gameTime) {
    aggr_ = saturate(gameTime / 180.0f);
    t_ += dt;

    // порывы скрежета (для звука)
    gustT_ -= dt;
    if (gustT_ <= 0) { gustT_ = g_rng.range(0.2f, 0.7f); gust_ = g_rng.range(0.3f, 1.0f); }

    float scrapeTarget = 0;
    switch (state_) {
        case MState::Dormant:
            if (t_ >= wait_) startCrawl();
            break;

        case MState::Crawling: {
            crawl_ = saturate(t_ / crawlDur_);
            scrapeTarget = 0.30f + 0.40f * gust_;
            stepT_ -= dt;
            if (stepT_ <= 0) {
                emit(MEventType::Step, scrapePan());
                stepT_ = mixf(0.55f, 0.28f, aggr_) * g_rng.range(0.85f, 1.15f);
            }
            if (t_ >= crawlDur_) {
                state_ = MState::AtWindow; t_ = 0; crawl_ = 1.0f;
                lingerDur_ = mixf(3.2f, 1.6f, aggr_);
                knockT_ = 0.3f;
                emit(MEventType::Arrive, slotPan());
            }
            break;
        }

        case MState::AtWindow:
            scrapeTarget = 0.12f;
            knockT_ -= dt;
            if (knockT_ <= 0) { emit(MEventType::Knock, slotPan()); knockT_ = g_rng.range(0.55f, 0.85f); }
            if (t_ >= lingerDur_) {
                state_ = MState::Breaching; t_ = 0;
                breachDur_ = mixf(6.0f, 3.0f, aggr_);
                emit(MEventType::Hiss, slotPan());
            }
            break;

        case MState::Breaching:
            scrapeTarget = 0.55f;
            breach_ = saturate(t_ / breachDur_);
            if (crackStage_ < 2 && breach_ >= 0.33f * float(crackStage_ + 1)) {
                ++crackStage_;
                emit(MEventType::Crack, slotPan());
            }
            if (breach_ >= 1.0f) { state_ = MState::Reached; t_ = 0; emit(MEventType::Reached, slotPan()); }
            break;

        case MState::Retreating:
            scrapeTarget = 0.5f * (1.0f - retreatT());
            crawl_ = 1.0f - saturate(retreatT());
            if (t_ >= retreatDur_) {
                state_ = MState::Dormant; t_ = 0; breach_ = 0; crawl_ = 0;
                wait_ = mixf(11.0f, 4.5f, aggr_) * g_rng.range(0.8f, 1.2f);
            }
            break;

        case MState::Reached:
            break;
    }
    scrape_ += (scrapeTarget - scrape_) * saturate(dt * 8.0f);
}

bool Monster::onFlash() {
    if (!atWindow()) return false;
    state_ = MState::Retreating;
    t_ = 0; retreatDur_ = 2.0f;
    emit(MEventType::Scared, slotPan());
    return true;
}

}  // namespace lrh
