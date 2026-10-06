// RealWorldScene.h - вид с заднего сиденья: окна, родители, монстр на крыше, камера.
#pragma once
#include <vector>
#include "Audio.h"
#include "Common.h"
#include "Monster.h"
#include "Renderer.h"

namespace lrh {

class RealWorldScene {
public:
    void reset();
    // threats=false: монстр «спит» (меню, остановка машины); остальная сцена живёт.
    void update(float dt, SharedState& s, Audio& audio, bool threats);
    // Снимок камерой. true - выстрел произведён (кадр потрачен).
    bool fireCamera(SharedState& s, Audio& audio);

    // Сама картинка (без HUD) и отдельно HUD/вспышка, чтобы консоль могла лечь между ними.
    void renderWorld(Renderer& r, const SharedState& s);
    void renderHud(Renderer& r, const SharedState& s, bool showHints);
    void renderJumpscare(Renderer& r, float t);

    Monster& monster() { return monster_; }
    const Monster& monster() const { return monster_; }
    float flashAmount() const { return flashT_; }

private:
    struct Prop { float z; int side; int kind; };  // kind 0 = фонарь, 1 = дерево, 2 = куст

    void drawWindshield(Renderer& r, const SharedState& s);
    void drawSideWindow(Renderer& r, bool right);
    void drawSunroof(Renderer& r);
    void drawInterior(Renderer& r, const SharedState& s);
    void drawMonster(Renderer& r);
    void drawCrawlShadow(Renderer& r);
    void drawCracks(Renderer& r, float cx, float cy, float stage, float seed);

    Monster monster_;
    std::vector<Prop> props_;
    float scroll_ = 0, clock_ = 0, spawnZ_ = 0;
    int propCounter_ = 0, poleCounter_ = 0;
    // свет фонарей и встречные машины
    float lightPh_ = 1.0f; int lightSide_ = -1;
    float oncT_ = 0, oncNext_ = 18.0f;
    // камера
    float flashT_ = 0, shutterT_ = 0;
    // тряска
    int shakeX_ = 0, shakeY_ = 0; float thud_ = 0, stepPh_ = 0;
    // родители
    float talkT_ = 1.0f; int speaker_ = 0; float talkLevel_ = 0, silenceT_ = 0, argue_ = 0.5f;
    float hintT_ = 0;
    // звёзды
    Pt stars_[70];
    int starCount_ = 0;
};

}  // namespace lrh
