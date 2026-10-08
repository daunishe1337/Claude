// Игра: игрок, взаимодействие, режимы (меню/игра/телефон/монитор), экономика, сохранения
#pragma once
#include "computer.hpp"
#include "core.hpp"
#include "parts.hpp"
#include "ui.hpp"
#include "world.hpp"
#include <deque>
#include <map>
#include <string>
#include <vector>

enum class Mode { Menu, Play, Pause, Settings, Phone, Screen };

struct Order {
    int tmpl = 0;
    int reward = 0;
};

struct OrderTemplate {
    const char* title;
    const char* client;
    const char* desc;
    int reward;
    int minScore;
    int minRam;
    RamType ramType;     // None — любая
    float minGpuPerf;    // 0 — без требований; >0 — нужна поддерживаемая видеокарта
    bool needX3d;
    bool needNvme;
    int minFans;
    bool needRgb;
};
const std::vector<OrderTemplate>& orderTemplates();

struct Toast {
    std::string text;
    Color color;
    float t;
};

struct Settings {
    float sens = 0.12f;
    float fov = 72;
    bool fullscreen = false;
    bool vsync = true;
    bool showFps = false;
    bool sound = true;
};

struct Target {
    int entity = 0;          // предмет под прицелом (0 — нет)
    bool monitor = false;
    float t = 1e9f;
    Vec3 point{0, 0, 0};
    bool hitStatic = false;  // попали в стол/пол/стену
    // Установка удерживаемого предмета
    int slotParent = 0;
    int slot = -1;
    std::string slotError;
    int pasteCpu = 0;
};

class Game {
public:
    World world;
    std::map<int, Computer> comps;
    double money = 1500;
    int bestScore = 0;
    int ordersDone = 0;
    double totalEarned = 0;
    std::vector<Order> orders;
    Settings settings;
    Mode mode = Mode::Menu;
    Mode settingsBack = Mode::Menu;

    // Игрок
    Vec3 pos{0, 0, 1.2f};
    float yaw = 0, pitch = -8;
    float velY = 0;
    bool onGround = true;
    float eye = 1.62f;
    int heldId = 0;
    int heldYaw = 0;
    Target target;

    float time = 0;
    float autosaveT = 0;
    float saveFlash = 0;
    std::deque<Toast> toasts;
    struct Delivery {
        const PartDef* def;
        float t;
    };
    std::vector<Delivery> deliveries;
    int phoneTab = 0, phoneCat = 0, phoneScroll = 0;
    int screenComp = 0;     // id корпуса, подключённого к монитору
    bool hasSave = false;
    bool quit = false;
    float fps = 60;
    bool debugShots = false;

    Game();
    void newGame();
    bool save();
    bool load();
    std::string savePath() const;
    void loadSettings();
    void saveSettings();

    void frame(float dt);   // обновление + отрисовка

    // Вызывается из интерфейсов
    void toast(const std::string& s, Color c = ui::theme::text);
    bool buy(const PartDef* d);
    void earn(double v);
    Computer& comp(int caseId);
    Computer* monitorComputer();
    std::vector<Entity*> shipItems();
    double sellValue(Entity* e);
    void sellAll();
    std::vector<std::string> checkOrder(const OrderTemplate& t, Entity* cs);
    void completeOrder(int idx);
    Order randomOrder();
    void beep(float f, float d);
    SimEvents simEvents();

    // Отрисовка интерфейсов (game_ui.cpp / os.cpp)
    void drawShop(Rect area, float fs, int& cat, int& scroll);
    void drawPhone(Rect r);
    void drawOS(Computer& c, float w, float h);
    void drawHUD(int w, int h);
    void drawMenu(int w, int h);
    void drawPause(int w, int h);
    void drawSettings(int w, int h);

private:
    void updatePlay(float dt);
    void updatePlayer(float dt);
    void computeTarget();
    void handleInteraction();
    void render(int w, int h);
    void renderWorld(const Mat4& proj, const Mat4& view, bool withOs);
    Vec3 eyePos() const { return v3(pos.x, pos.y + eye, pos.z); }
    Vec3 forward() const;
    void updateHeld();
    std::string canInstall(Entity& item, Entity& parent, int slot);
    void install(Entity& item, Entity& parent, int slot);
    void takeOut(Entity& e);
    void placeHeld(Vec3 at);
    void dropHeld();
    bool isHeldOrInside(Entity* e);
    bool caseIsOn(Entity* e);
    void openMode(Mode m);
    void setupShots();
    int shotStep = 0;
    int shotFrames = 0;
    std::string shotName;
};
