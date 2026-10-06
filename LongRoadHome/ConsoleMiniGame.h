// ConsoleMiniGame.h - мини-игра на «портативной консоли» (4 цвета, вид сверху).
// Продолжает работать, пока игрок смотрит в реальный мир.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Audio.h"
#include "Common.h"
#include "Renderer.h"

namespace lrh {

class ConsoleMiniGame {
public:
    static constexpr int LW = 64;  // логическое разрешение экрана консоли
    static constexpr int LH = 80;

    void reset();
    // hasControl: игрок сейчас смотрит на консоль (иначе машина едет прямо).
    void update(float dt, SharedState& s, Audio& audio, bool hasControl, bool left, bool right);
    // slide: 0 = консоль убрана, 1 = полностью поднята. threat - для светодиода на корпусе.
    void render(Renderer& r, const SharedState& s, float slide, float threat);
    // Множитель скорости машины (падает после аварии).
    float speedFactor() const { return slowT_ > 0.0f ? 0.45f : 1.0f; }

private:
    enum class Type { Fuel, Film, Rock, Cone, Oil, Car };
    struct Obj { Type type; float x, y; int w, h; };

    static constexpr int kRoadL = 12, kRoadR = 52;

    void spawn(const SharedState& s);
    void drawLcd(const SharedState& s);
    void spr(const char* const* rows, int h, int x, int y);
    void lcdRect(int x, int y, int w, int h, uint8_t c);

    uint8_t lcd_[LW * LH] = {};
    std::vector<Obj> objs_;
    float x_ = 32.0f, vx_ = 0.0f;
    float scroll_ = 0.0f;
    float spawnT_ = 1.5f, sinceFuel_ = 0.0f, sinceFilm_ = 0.0f;
    float slowT_ = 0.0f, invulnT_ = 0.0f, skidT_ = 0.0f, skidDir_ = 1.0f, shakeT_ = 0.0f;
    float popupT_ = 0.0f, clock_ = 0.0f;
    std::string popup_;
};

}  // namespace lrh
