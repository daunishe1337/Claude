// Game.h - машина состояний игры, склейка сцен, звука и ввода.
#pragma once
#include <cstdint>
#include "Audio.h"
#include "ConsoleMiniGame.h"
#include "Input.h"
#include "RealWorldScene.h"
#include "Renderer.h"

namespace lrh {

enum class Mode { Menu, Playing, Stalling, Caught, EndMonster, EndFuel, Win };
enum class View { Real, Console };

class Game {
public:
    bool init(uint32_t seed);   // true, если звук открылся (игра работает и без него)
    void shutdown();
    void update(float dt);      // один фиксированный шаг 1/60 с
    void render();

    Input& input() { return input_; }
    const Renderer& renderer() const { return r_; }
    bool wantsQuit() const { return quit_; }
    Mode mode() const { return mode_; }

private:
    void startRun();
    void updateMenu(float dt);
    void updatePlaying(float dt);
    void updateStalling(float dt);
    void updateCaught(float dt);
    void updateEnd(float dt);
    void setEngineSound();
    void enterMode(Mode m);

    void renderMenu();
    void renderPlaying();
    void renderEnd();
    void renderWin();

    Input input_;
    Renderer r_;
    Audio audio_;
    SharedState s_;
    RealWorldScene real_;
    ConsoleMiniGame console_;

    Mode mode_ = Mode::Menu;
    View view_ = View::Real;
    float blend_ = 0.0f;   // 0 = реальный мир, 1 = консоль
    float modeT_ = 0.0f;
    float clock_ = 0.0f;
    bool quit_ = false, muted_ = false;
};

}  // namespace lrh
