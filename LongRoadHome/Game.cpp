// Game.cpp
#include "Game.h"
#include <cstdio>

namespace lrh {

bool Game::init(uint32_t seed) {
    g_rng.s = seed ? seed : 0xC0FFEEu;
    bool audioOk = audio_.init();
    s_.reset();
    real_.reset();
    console_.reset();
    enterMode(Mode::Menu);
    return audioOk;
}

void Game::shutdown() { audio_.shutdown(); }

void Game::enterMode(Mode m) {
    mode_ = m;
    modeT_ = 0.0f;
    if (m == Mode::EndMonster || m == Mode::EndFuel) audio_.silenceAll();
}

void Game::startRun() {
    s_.reset();
    real_.reset();
    console_.reset();
    view_ = View::Real;
    blend_ = 0.0f;
    audio_.silenceAll();
    audio_.stutter = 0.0f;
    enterMode(Mode::Playing);
}

void Game::setEngineSound() {
    audio_.engineRpm = 0.18f + 0.55f * s_.carSpeed;
    audio_.engineVol = 0.55f;
    audio_.roadVol = 0.45f * s_.carSpeed;
}

void Game::update(float dt) {
    clock_ += dt;
    if (input_.pressed(K_ESC)) quit_ = true;
    if (input_.pressed(K_M)) { muted_ = !muted_; audio_.master = muted_ ? 0.0f : 1.0f; }
    if (mode_ != Mode::Menu && input_.pressed(K_R)) startRun();   // перезапуск в любой момент

    switch (mode_) {
        case Mode::Menu:       updateMenu(dt); break;
        case Mode::Playing:    updatePlaying(dt); break;
        case Mode::Stalling:   updateStalling(dt); break;
        case Mode::Caught:     updateCaught(dt); break;
        case Mode::EndMonster:
        case Mode::EndFuel:
        case Mode::Win:        updateEnd(dt); break;
    }
    input_.endFrame();
}

// ---- Меню: машина едет, родители спорят, монстра ещё нет --------------------------
void Game::updateMenu(float dt) {
    modeT_ += dt;
    s_.carSpeed = 0.6f;
    s_.threat = 0.0f;
    real_.update(dt, s_, audio_, false);
    setEngineSound();
    audio_.heartVol = 0.0f;
    if (input_.pressed(K_ENTER) || input_.pressed(K_SPACE)) { audio_.play(Sfx::Beep); startRun(); }
}

// ---- Основной игровой цикл -----------------------------------------------------------
void Game::updatePlaying(float dt) {
    modeT_ += dt;
    s_.time += dt;

    // переключение сцен: игра в консоли НЕ ставится на паузу
    if (input_.pressed(K_TAB) || input_.pressed(K_SPACE)) {
        view_ = (view_ == View::Real) ? View::Console : View::Real;
        audio_.play(Sfx::Switch, 0.8f);
    }
    blend_ += ((view_ == View::Console ? 1.0f : 0.0f) - blend_) * saturate(dt * 12.0f);
    if (std::fabs((view_ == View::Console ? 1.0f : 0.0f) - blend_) < 0.01f) blend_ = (view_ == View::Console ? 1.0f : 0.0f);
    const bool consoleActive = view_ == View::Console && blend_ > 0.5f;

    // камера работает только в реальном мире
    if (input_.pressed(K_F) || (!consoleActive && input_.pressed(K_ENTER))) {
        if (consoleActive) audio_.play(Sfx::Click, 0.5f);
        else real_.fireCamera(s_, audio_);
    }
    s_.camCooldown = std::max(0.0f, s_.camCooldown - dt);

    // бензин
    s_.fuel -= kFuelDrain * dt;

    // скорость машины зависит от аварий в мини-игре
    s_.carSpeed += (console_.speedFactor() - s_.carSpeed) * saturate(dt * 2.5f);
    s_.progress += dt / kTripSeconds * s_.carSpeed;
    s_.bump = std::max(0.0f, s_.bump - dt * 3.0f);

    const bool left = input_.down(K_A) || input_.down(K_LEFT);
    const bool right = input_.down(K_D) || input_.down(K_RIGHT);
    console_.update(dt, s_, audio_, consoleActive, left, right);
    real_.update(dt, s_, audio_, true);

    // шкала угрозы: растёт быстро, спадает медленно
    float target = real_.monster().threatTarget();
    s_.threat += (target - s_.threat) * saturate(dt * (target > s_.threat ? 3.0f : 1.2f));

    // звук по состоянию
    setEngineSound();
    audio_.heartBpm = lerp(58.0f, 150.0f, s_.threat);
    audio_.heartVol = saturate((s_.threat - 0.2f) / 0.6f) * 0.9f;

    // --- условия конца ---
    if (s_.fuel <= 0.0f) {
        s_.fuel = 0.0f; s_.stalled = true;
        audio_.play(Sfx::Stall);
        audio_.stutter = 1.0f;
        enterMode(Mode::Stalling);
        return;
    }
    if (real_.monster().state() == MState::Reached) {
        audio_.silenceAll();
        audio_.play(Sfx::Scream, 1.0f);
        enterMode(Mode::Caught);
        return;
    }
    if (s_.progress >= 1.0f) {
        s_.progress = 1.0f;
        audio_.silenceAll();
        audio_.play(Sfx::Win);
        audio_.ambience = 0.1f;
        enterMode(Mode::Win);
    }
}

// ---- Бензин кончился: мотор чихает и глохнет, без скримера ----------------------------
void Game::updateStalling(float dt) {
    modeT_ += dt;
    s_.carSpeed += (0.0f - s_.carSpeed) * saturate(dt * 1.5f);
    s_.threat *= (1.0f - saturate(dt * 0.5f));
    real_.update(dt, s_, audio_, false);
    console_.update(dt, s_, audio_, false, false, false);
    blend_ += ((view_ == View::Console ? 1.0f : 0.0f) - blend_) * saturate(dt * 12.0f);
    audio_.engineRpm = 0.25f * (1.0f - saturate(modeT_ / 2.0f));
    audio_.engineVol = 0.55f * (1.0f - saturate(modeT_ / 2.4f));
    audio_.roadVol = 0.45f * s_.carSpeed;
    audio_.heartVol = 0.5f; audio_.heartBpm = 70.0f;
    if (modeT_ > 3.4f) enterMode(Mode::EndFuel);
}

// ---- Скример (только при проигрыше от монстра) -----------------------------------------
void Game::updateCaught(float dt) {
    modeT_ += dt;
    if (modeT_ > 1.8f) enterMode(Mode::EndMonster);
}

void Game::updateEnd(float dt) {
    modeT_ += dt;
    if (mode_ == Mode::Win) {
        audio_.engineVol = 0.0f;
        real_.update(dt, s_, audio_, false);  // продолжаем только фон (голоса родителей замолкают ниже)
        audio_.voiceDad = audio_.voiceMom = 0.0f;
        audio_.scrapeVol = 0.0f;
    }
}

// ============================================================================
//  Рендер
// ============================================================================
void Game::render() {
    switch (mode_) {
        case Mode::Menu: renderMenu(); break;
        case Mode::Playing:
        case Mode::Stalling: renderPlaying(); break;
        case Mode::Caught: real_.renderJumpscare(r_, modeT_); break;
        case Mode::EndMonster:
        case Mode::EndFuel: renderEnd(); break;
        case Mode::Win: renderWin(); break;
    }
    if (muted_) r_.text(4, 14, "MUTE", 0xFF8080);
}

void Game::renderMenu() {
    real_.renderWorld(r_, s_);
    r_.tint(0x000000, 0.55f);
    r_.textCentered(160, 18, "THE LONG", 0xD8DCEC, 3);
    r_.textCentered(160, 44, "ROAD HOME", 0xC83030, 3);
    r_.textCentered(160, 74, "A NIGHT DRIVE. SOMETHING IS ON THE ROOF.", 0x9AA0B8);
    r_.textCentered(160, 98, "A / D    STEER IN THE CONSOLE GAME", 0xB0B4C8);
    r_.textCentered(160, 108, "TAB / SPACE    SWITCH WORLD / CONSOLE", 0xB0B4C8);
    r_.textCentered(160, 118, "F / ENTER    CAMERA FLASH (REAL WORLD)", 0xB0B4C8);
    r_.textCentered(160, 128, "R RESTART   M MUTE   F11 FULLSCREEN   ESC QUIT", 0x808498);
    if (int(clock_ * 2.0f) & 1) r_.textCentered(160, 150, "PRESS ENTER TO START", 0xFFE090, 1);
}

void Game::renderPlaying() {
    real_.renderWorld(r_, s_);
    real_.renderHud(r_, s_, s_.time < 25.0f && blend_ < 0.5f);
    if (blend_ > 0.001f) {
        r_.tint(0x000000, 0.88f * smoothstep(blend_));
        console_.render(r_, s_, blend_, s_.threat);
    }
    if (mode_ == Mode::Stalling && modeT_ > 1.0f)
        r_.textCentered(160, 80, "THE ENGINE DIED...", 0xE0E0F0, 2);
}

void Game::renderEnd() {
    r_.clear(0x030305);
    r_.vignette(0.9f);
    const bool fuel = mode_ == Mode::EndFuel;
    const float a = saturate(modeT_ / 1.0f);
    r_.textCentered(160, 52, fuel ? "OUT OF FUEL" : "IT GOT IN.", scaleColor(fuel ? 0xE0B040 : 0xC83030, a), 3);
    r_.textCentered(160, 82, fuel ? "THE ENGINE DIED IN THE DARK." : "THE THING ON THE ROOF FOUND A WAY IN.", scaleColor(0x9AA0B8, a));
    char buf[48];
    std::snprintf(buf, sizeof(buf), "YOU GOT %d%% OF THE WAY HOME", int(s_.progress * 100.0f));
    r_.textCentered(160, 98, buf, scaleColor(0x808498, a));
    if (modeT_ > 1.0f && (int(clock_ * 2.0f) & 1)) r_.textCentered(160, 130, "PRESS R TO TRY AGAIN", 0xFFE090);
}

void Game::renderWin() {
    const float a = saturate(modeT_ / 2.0f);
    r_.vgradient(0, 0, kW, 120, mixColor(0x04061A, 0x2A2050, a), mixColor(0x0A1030, 0xF09060, a));
    for (int i = 0; i < 40; ++i) {  // гаснущие звёзды
        Rng sr; sr.s = uint32_t(i * 7919 + 13);
        r_.put(int(sr.range(0, 319)), int(sr.range(0, 90)), scaleColor(0xC8D0FF, 1.0f - a));
    }
    r_.ellipse(90, 118, 70, 30, 0xFFC080, 0.25f * a);   // рассвет
    r_.rect(0, 120, kW, 60, 0x10150F);
    // дом
    r_.rect(190, 84, 100, 44, 0x4A3A34);
    Pt roof[3] = {{180, 86}, {240, 50}, {300, 86}};
    r_.poly(roof, 3, 0x2A1A1C);
    r_.rect(232, 104, 16, 24, 0x20120C);
    r_.rect(203, 96, 18, 14, 0xFFD27A); r_.rect(259, 96, 18, 14, 0xFFD27A);
    r_.rect(211, 96, 2, 14, 0x4A3A34); r_.rect(203, 102, 18, 2, 0x4A3A34);
    r_.ellipse(240, 100, 14, 10, 0xFFD27A, 0.10f);       // свет с крыльца
    // машина у дома
    r_.rect(60, 122, 80, 14, 0x0A0B10);
    r_.rect(76, 112, 46, 12, 0x0A0B10);
    r_.rect(82, 114, 16, 8, 0x1C2230); r_.rect(102, 114, 16, 8, 0x1C2230);
    r_.ellipse(78, 137, 8, 8, 0x050507); r_.ellipse(124, 137, 8, 8, 0x050507);
    r_.ellipse(142, 128, 3, 2, 0xFFF0B0);
    r_.vignette(0.5f);
    r_.textCentered(160, 8, "YOU MADE IT HOME.", scaleColor(0xFFFFFF, a), 2);
    r_.textCentered(160, 30, "THE ROOF IS QUIET... FOR NOW.", scaleColor(0xE0D0C0, a));
    if (modeT_ > 2.0f && (int(clock_ * 2.0f) & 1)) r_.textCentered(160, 160, "PRESS R TO DRIVE AGAIN", 0xFFE090);
}

}  // namespace lrh
