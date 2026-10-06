// Game.cpp
#include "Game.h"
#include <cmath>
#include <cwchar>
#include "Scenes.h"

#pragma comment(lib, "user32.lib")

namespace {

// Тайминги вступления (секунды)
const double kDayEnd = 7.0;
const double kDuskEnd = 14.0;
const double kFireEnd = 24.0;      // дальше - машина
const double kFlashStart = 22.0;   // вспышка перед "перемещением" в машину
const double kFlashPeak = 24.0;
const double kFlashEnd = 25.5;
const double kIntroEnd = 33.5;

struct Caption {
    double from, to;
    const wchar_t* text;
    bool hero; // реплика героя
};

const Caption kCaptions[] = {
    { 0.5, 6.5,   L"Сегодня мне наконец повезло: нашлась работа. Частный следователь. Платят мало, но платят.", false },
    { 7.5, 13.5,  L"Весь день на ногах. Единственное желание - вернуться домой и уснуть.", false },
    { 14.5, 18.5, L"Что это за свет?.. Дом. Мой дом... Он горит.", false },
    { 18.5, 23.5, L"Окна лопаются от жара. Я не успею. Не успею!..", false },
    { 26.0, 30.5, L"Следователь: «Ох уж эти флешбеки...»", true },
    { 31.0, 33.5, L"Телефон на панели вибрирует. Неизвестный номер.", false },
};

const double kDrivingTime = 3.6;
const double kNearExit = 2.2;

} // namespace

Game::Game() {}

// ---------------- Переходы между сценами ----------------

void Game::EnterCar(double nextOrderDelay) {
    scene_ = Scene::Car;
    sceneTime_ = 0.0;
    arrivalTimer_ = (!orders_.HasAvailable() && orders_.HasHidden()) ? nextOrderDelay : 0.0;
    std::vector<int> vis = orders_.VisibleIndices();
    cursor_ = vis.empty() ? 0 : static_cast<int>(vis.size()) - 1;
}

void Game::StartDriving(int orderIndex) {
    currentOrder_ = orderIndex;
    scene_ = Scene::Driving;
    sceneTime_ = 0.0;
}

void Game::StartMission() {
    map_.Load(orders_.At(currentOrder_).mapId);
    player_.Spawn(map_);
    scene_ = Scene::Mission;
    sceneTime_ = 0.0;
}

void Game::ReturnToCar() {
    // Прототип: возвращение в машину считает заказ выполненным.
    // На этапе 7 заказ будет закрываться обвинением.
    orders_.Complete(currentOrder_);
    currentOrder_ = -1;
    EnterCar(3.0);
}

bool Game::NearExit() const {
    double dx = player_.x - map_.StartX(), dy = player_.y - map_.StartY();
    return std::sqrt(dx * dx + dy * dy) < kNearExit;
}

// ---------------- Обновление ----------------

void Game::Update(double dt, const InputState& in) {
    totalTime_ += dt;
    sceneTime_ += dt;
    if (newMsgFlash_ > 0.0) newMsgFlash_ -= dt;

    switch (scene_) {
    case Scene::Intro:
        if (sceneTime_ >= kIntroEnd) EnterCar(2.0);
        break;
    case Scene::Car:
        if (arrivalTimer_ > 0.0) {
            arrivalTimer_ -= dt;
            if (arrivalTimer_ <= 0.0 && orders_.RevealNext()) {
                newMsgFlash_ = 4.0;
                MessageBeep(MB_ICONASTERISK); // "звук" входящего сообщения
                cursor_ = static_cast<int>(orders_.VisibleIndices().size()) - 1;
            }
        }
        break;
    case Scene::Driving:
        if (sceneTime_ >= kDrivingTime) StartMission();
        break;
    case Scene::Mission:
        player_.Update(in, map_, dt);
        break;
    }
}

void Game::OnKey(int vk, bool repeat) {
    switch (scene_) {
    case Scene::Intro:
        if (!repeat && (vk == VK_SPACE || vk == VK_RETURN)) EnterCar(2.0);
        break;
    case Scene::Car: {
        std::vector<int> vis = orders_.VisibleIndices();
        if (vis.empty()) break;
        if (vk == VK_UP && cursor_ > 0) --cursor_;
        if (vk == VK_DOWN && cursor_ < static_cast<int>(vis.size()) - 1) ++cursor_;
        if (!repeat && vk == VK_RETURN) {
            int idx = vis[cursor_];
            if (orders_.At(idx).status == OrderStatus::Available) StartDriving(idx);
        }
        break;
    }
    case Scene::Mission:
        if (!repeat && vk == 'R' && NearExit()) ReturnToCar();
        break;
    default:
        break;
    }
}

// ---------------- Рендер (пиксельный буфер) ----------------

void Game::RenderIntro(Renderer& r) {
    double t = sceneTime_;
    if (t < kDayEnd)          Scenes::DrawDayStreet(r, t);
    else if (t < kDuskEnd)    Scenes::DrawDusk(r, t - kDayEnd);
    else if (t < kFireEnd)    Scenes::DrawBurningHouse(r, t);
    else                      Scenes::DrawCarView(r, t, 1.0, 0.0);

    r.ApplyVignette(0.8);

    // Киношные чёрные полосы сверху и снизу
    r.FillRect(0, 0, Renderer::kWidth, 28, Rgb(0, 0, 0));
    r.FillRect(0, Renderer::kHeight - 56, Renderer::kWidth, 56, Rgb(0, 0, 0));

    // Белая вспышка: дом -> машина ("флешбек" заканчивается)
    if (t >= kFlashStart && t < kFlashPeak)
        r.Fade(Rgb(255, 255, 255), (t - kFlashStart) / (kFlashPeak - kFlashStart));
    else if (t >= kFlashPeak && t < kFlashEnd)
        r.Fade(Rgb(255, 255, 255), 1.0 - (t - kFlashPeak) / (kFlashEnd - kFlashPeak));
}

void Game::Render(Renderer& r) {
    switch (scene_) {
    case Scene::Intro:
        RenderIntro(r);
        break;
    case Scene::Car:
        Scenes::DrawCarView(r, totalTime_, 0.15, 0.0);
        r.ApplyVignette(0.9);
        break;
    case Scene::Driving: {
        Scenes::DrawCarView(r, totalTime_, 0.0, 1.0);
        r.ApplyVignette(0.9);
        double fade = 0.0;
        if (sceneTime_ < 0.5) fade = 1.0 - sceneTime_ / 0.5;
        else if (sceneTime_ > kDrivingTime - 0.5) fade = (sceneTime_ - (kDrivingTime - 0.5)) / 0.5;
        r.Fade(Rgb(0, 0, 0), fade);
        break;
    }
    case Scene::Mission: {
        // Мерцающий "фонарик": радиус слегка дрожит
        double radius = 9.5 + 0.5 * std::sin(totalTime_ * 13.0) + 0.3 * std::sin(totalTime_ * 31.0);
        r.DrawTopDown(map_, player_, radius);
        r.ApplyVignette(0.45);
        break;
    }
    }
}

// ---------------- Интерфейс (GDI) ----------------

void Game::DrawIntroOverlay(HDC dc, UI& ui, int w, int h) {
    double t = sceneTime_;
    for (const Caption& c : kCaptions) {
        if (t >= c.from && t < c.to) {
            // Нижняя чёрная полоса: последние 56/300 высоты окна
            RECT bar = { 40, h * 244 / 300 + 4, w - 40, h - 4 };
            ui.DrawString(dc, c.text, bar, c.hero ? RGB(255, 220, 120) : RGB(235, 235, 235),
                          DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        }
    }
    RECT skip = { 0, 4, w - 12, 26 };
    ui.DrawString(dc, L"Пробел - пропустить", skip, RGB(150, 150, 150), DT_RIGHT, FontSize::Small);
}

void Game::DrawPhone(HDC dc, UI& ui, int w, int h) {
    RECT panel = { w * 55 / 100, 28, w - 24, h - 28 };
    ui.FillBox(dc, panel, RGB(14, 16, 22), RGB(80, 100, 125), 26, 2);

    int px = panel.left + 18, pw = panel.right - panel.left - 36;
    RECT head = { px, panel.top + 12, px + pw, panel.top + 44 };
    ui.DrawString(dc, OrderBook::SenderNumber(), head, RGB(120, 225, 140), DT_LEFT, FontSize::Large);
    RECT sub = { px, panel.top + 46, px + pw, panel.top + 66 };
    ui.DrawString(dc, L"Неизвестный отправитель", sub, RGB(140, 150, 160), DT_LEFT, FontSize::Small);

    std::vector<int> vis = orders_.VisibleIndices();
    int y = panel.top + 78;
    if (vis.empty()) {
        RECT wait = { px, y + 40, px + pw, y + 100 };
        ui.DrawString(dc, L"Сообщений пока нет...", wait, RGB(150, 160, 170), DT_CENTER);
        return;
    }

    // Список пришедших заказов
    for (int i = 0; i < static_cast<int>(vis.size()); ++i) {
        const Order& o = orders_.At(vis[i]);
        RECT row = { px, y, px + pw, y + 32 };
        bool sel = (i == cursor_);
        ui.FillBox(dc, row, sel ? RGB(36, 52, 74) : RGB(22, 26, 34),
                   sel ? RGB(255, 200, 80) : RGB(45, 55, 70), 10, sel ? 2 : 1);
        RECT t1 = { row.left + 10, row.top + 4, row.right - 110, row.bottom };
        ui.DrawString(dc, o.time + L"  " + o.title, t1, RGB(225, 225, 225), DT_LEFT | DT_SINGLELINE);
        RECT t2 = { row.right - 110, row.top + 4, row.right - 10, row.bottom };
        bool done = o.status == OrderStatus::Done;
        ui.DrawString(dc, done ? L"выполнен" : L"новый", t2, done ? RGB(130, 130, 130) : RGB(255, 120, 100),
                      DT_RIGHT | DT_SINGLELINE, FontSize::Small);
        y += 38;
    }

    // Текст выбранного сообщения
    const Order& cur = orders_.At(vis[cursor_]);
    int bottom = panel.bottom - 40;
    RECT box = { px, y + 6, px + pw, bottom };
    if (box.bottom > box.top + 40) {
        ui.FillBox(dc, box, RGB(26, 34, 48), RGB(60, 75, 95), 14, 1);
        RECT text = { box.left + 12, box.top + 10, box.right - 12, box.bottom - 30 };
        ui.DrawString(dc, cur.text, text, RGB(235, 235, 235), DT_LEFT | DT_WORDBREAK);
        RECT addr = { box.left + 12, box.bottom - 28, box.right - 12, box.bottom - 6 };
        ui.DrawString(dc, L"Адрес: " + cur.address, addr, RGB(255, 200, 80), DT_LEFT | DT_SINGLELINE, FontSize::Small);
    }

    RECT hint = { px, panel.bottom - 32, px + pw, panel.bottom - 8 };
    std::wstring hintText = cur.status == OrderStatus::Available
        ? L"Вверх/Вниз - выбрать    Enter - ехать на заказ"
        : L"Вверх/Вниз - выбрать    (заказ уже выполнен)";
    ui.DrawString(dc, hintText, hint, RGB(150, 160, 170), DT_CENTER | DT_SINGLELINE, FontSize::Small);
}

void Game::DrawOverlay(HDC dc, UI& ui, int w, int h, bool mouseCaptured) {
    switch (scene_) {
    case Scene::Intro:
        DrawIntroOverlay(dc, ui, w, h);
        break;
    case Scene::Car: {
        DrawPhone(dc, ui, w, h);
        if (newMsgFlash_ > 0.0 && std::fmod(totalTime_, 0.8) < 0.5) {
            RECT n = { 20, 20, w / 2, 50 };
            ui.DrawString(dc, L"● Новое сообщение", n, RGB(255, 90, 80), DT_LEFT, FontSize::Large);
        }
        RECT cap = { 20, h - 64, w * 55 / 100 - 20, h - 12 };
        ui.DrawString(dc, L"Ночь. Дождь. Двигатель тихо урчит. Телефон лежит на приборной панели.",
                      cap, RGB(190, 190, 190), DT_LEFT | DT_WORDBREAK, FontSize::Small);
        if (orders_.AllDone()) {
            RECT end = { 20, h / 2 - 40, w * 55 / 100 - 20, h / 2 + 60 };
            ui.DrawString(dc, L"Все заказы выполнены. Номер больше не пишет... пока.",
                          end, RGB(255, 220, 140), DT_CENTER | DT_WORDBREAK, FontSize::Large);
        }
        break;
    }
    case Scene::Driving: {
        const Order& o = orders_.At(currentOrder_);
        int dots = static_cast<int>(sceneTime_ * 3.0) % 4;
        std::wstring text = L"Еду: " + o.address + std::wstring(dots, L'.');
        RECT r1 = { 20, h - 70, w - 20, h - 20 };
        ui.DrawString(dc, text, r1, RGB(230, 230, 230), DT_CENTER, FontSize::Large);
        break;
    }
    case Scene::Mission: {
        std::wstring title = L"Заказ: " + orders_.At(currentOrder_).title + L"   (" + map_.Name() + L")";
        RECT r0 = { 12, 8, w - 12, 36 };
        ui.DrawString(dc, title, r0, RGB(255, 200, 80));

        wchar_t line[128];
        const double kPi = 3.14159265358979323846;
        swprintf_s(line, L"X: %.2f   Y: %.2f   Угол: %.0f°", player_.x, player_.y, player_.angle * 180.0 / kPi);
        RECT r1 = { 12, h - 92, w - 12, h - 64 };
        ui.DrawString(dc, line, r1, RGB(255, 255, 255));

        if (NearExit()) {
            RECT r2 = { 12, h - 64, w - 12, h - 36 };
            ui.DrawString(dc, L"[R] - вернуться в машину", r2, RGB(255, 220, 120));
        }
        std::wstring hint = mouseCaptured
            ? L"WASD - ходьба, мышь/стрелки - поворот, Esc - освободить мышь"
            : L"Щёлкните в окне, чтобы захватить мышь. WASD, стрелки - управление";
        RECT r3 = { 12, h - 36, w - 12, h - 8 };
        ui.DrawString(dc, hint, r3, RGB(200, 200, 200));
        break;
    }
    }
}
