// ============================================================================
//  ConsoleMiniGame.cpp — мини-игра «FUEL RUN» на карманной консоли «GAMEKID»
//  и отрисовка самого устройства (см. ConsoleMiniGame.h).
//
//  Экран 160x144, строго 4 оттенка gbpal. Машина игрока стоит на месте по
//  вертикали (y = 112), а дорога прокручивается вниз. Объекты хранятся в
//  экранных координатах; мировая координата строки экрана
//      worldY = dist + (144 - y)
//  задаёт изгибы дороги, разметку, текстуры и придорожные деревья, поэтому
//  render() — чистая функция от состояния (вся анимация идёт в update()).
//
//  Машины на трассе едут медленнее дороги: собственная скорость vy < 0
//  (вперёд относительно асфальта), на экране они ползут вниз медленнее
//  камней. Положение машины поперёк дороги хранится как смещение от центра
//  («полоса»), так что машины сами следуют изгибам.
//
//  Гарантия проезда: в «полосном» пространстве (смещение от центра дороги)
//  у каждого ряда препятствий остаётся щель не уже машины + 8 пикселей.
// ============================================================================
#include "ConsoleMiniGame.h"

#include "Font.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
//  Размеры и настройка игрового процесса
// ---------------------------------------------------------------------------
constexpr int kW = cfg::kConsoleW;      // 160
constexpr int kH = cfg::kConsoleH;      // 144
constexpr int kHudH = 11;               // полоса HUD: строки 0..10
constexpr float kPlayerY = 112.0f;      // центр машины игрока по вертикали
constexpr float kPlayerHalfW = 4.0f;    // половина хитбокса машины (спрайт 10x15)
constexpr float kPlayerHalfH = 6.5f;
constexpr float kCarLen = 15.0f;        // длина машины игрока
constexpr float kSteerSpeed = 95.0f;    // предельная боковая скорость, пикс/с
constexpr float kSteerAccel = 450.0f;   // боковое ускорение при нажатии
constexpr float kFriction = 380.0f;     // гашение боковой скорости без руля
constexpr float kSpeedMin = 70.0f;      // скорость прокрутки при difficulty = 0
constexpr float kSpeedMax = 125.0f;     // ... и при difficulty = 1
constexpr float kAccel = 85.0f;         // разгон (0 -> 125 примерно за 1.5 с)
constexpr float kDecel = 70.0f;         // торможение на траве
constexpr float kCrashDecel = 520.0f;   // резкая остановка при аварии
constexpr float kOffRoadSpeed = 0.55f;  // доля скорости на траве

constexpr float kBootTime = 1.15f;      // логотип и название
constexpr float kWipeTime = 0.2f;       // «шторка» от заставки к трассе
constexpr float kIntroTime = 1.6f;      // старт гонки («GO!»)
constexpr float kGoEnd = 2.3f;          // надпись «GO!» исчезает

constexpr float kCrashTime = 1.2f;      // оглушение после аварии
constexpr float kInvulnTime = 1.5f;     // неуязвимость после оглушения
constexpr float kSkidTime = 0.8f;       // занос на масле
constexpr float kFlashTime = 0.03f;     // инверсия палитры (не больше двух кадров)
constexpr float kBoomTime = 0.45f;      // «взрыв» при аварии
constexpr float kItemFlashTime = 1.5f;  // значок подобранного предмета в HUD
constexpr float kPopTime = 1.0f;        // всплывающая надпись

constexpr float kKerbW = 3.0f;          // ширина бордюра
constexpr float kGapNeed = 18.0f;       // минимальный проезд: ширина машины + 8
constexpr float kPickupGapY = 22.0f;    // бонусы держатся дальше от препятствий
constexpr float kRetry = 0.12f;         // повтор неудачного спавна
constexpr float kLaneChangeSpeed = 34.0f; // перестроение попутной машины, пикс/с
constexpr size_t kMaxObjects = 48;
constexpr float kSubStep = 1.0f / 60.0f;

// ---------------------------------------------------------------------------
//  Палитра экрана: в спрайтах '1' — самый тёмный ... '4' — самый светлый
// ---------------------------------------------------------------------------
constexpr uint32_t kC1 = gbpal::kDarkest;
constexpr uint32_t kC2 = gbpal::kDark;
constexpr uint32_t kC3 = gbpal::kLight;
constexpr uint32_t kC4 = gbpal::kLightest;
const uint32_t kPal[16] = {0u, kC1, kC2, kC3, kC4};
const uint32_t kPalShadow[16] = {0u, kC2, kC2, kC2, kC2};  // тень на асфальте
const uint32_t kPalBlink[16] = {0u, kC4, kC3, kC2, kC1};   // «вспышка» бонуса (негатив)

// Матрица Байера 4x4 для пиксельного «растворения» дыма.
const int kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// ---------------------------------------------------------------------------
//  Спрайты (символьные массивы, '.' — прозрачно)
// ---------------------------------------------------------------------------
struct Sprite {
    const char* const* rows = nullptr;
    int w = 0;
    int h = 0;
};

template <size_t N>
Sprite makeSprite(const char* const (&rows)[N]) {
    return Sprite{rows, static_cast<int>(std::strlen(rows[0])), static_cast<int>(N)};
}

// clang-format off
// Машина игрока, вид сверху, носом вверх: светлый кузов, тёмные стёкла.
const char* const kCarRows[] = {
    "..111111..",
    ".14444431.",
    "1144444311",
    "1144444311",
    ".11222211.",
    ".12222221.",
    ".14444431.",
    ".14444431.",
    ".14444431.",
    ".12222221.",
    "1142222311",
    "1144444311",
    "1144444311",
    ".11111111.",
    "..1....1..",
};
// Попутный седан: тёмный кузов, светлые стёкла, фонари сзади.
const char* const kSedanRows[] = {
    "..111111..",
    ".12222221.",
    "1122222211",
    "1123333211",
    "1123333211",
    ".12111121.",
    ".12222221.",
    ".12222221.",
    ".12111121.",
    "1123333211",
    "1122222211",
    "1122222211",
    ".14222241.",
    "..111111..",
};
// Фургон: кабина и светлый кузов с рёбрами.
const char* const kTruckRows[] = {
    "..111111..",
    ".12222221.",
    "1123333211",
    "1123333211",
    "1122222211",
    ".11111111.",
    "1111111111",
    "1333333331",
    "1322222231",
    "1333333331",
    "1333333331",
    "1322222231",
    "1333333331",
    "1333333331",
    "1322222231",
    "1333333331",
    "1111111111",
    ".41....14.",
};
const char* const kRockRows[] = {
    "...1111...",
    "..143321..",
    ".14332221.",
    "1433222221",
    "1332222121",
    "1222222221",
    ".12221221.",
    "..111111..",
};
// Дорожный конус: полосатый силуэт на широком основании.
const char* const kConeRows[] = {
    "...1...",
    "..121..",
    "..141..",
    ".12221.",
    ".14441.",
    "1222221",
    "1111111",
};
const char* const kOilRows[] = {
    "....111111...",
    "..1111111111.",
    ".111441111221",
    "1114111112211",
    "1111111111111",
    ".111112211111",
    "..11111111...",
};
// Канистра с выпуклым «крестом».
const char* const kFuelRows[] = {
    ".111.11.",
    ".1.1.1..",
    "11111111",
    "14444441",
    "14144141",
    "14411441",
    "14411441",
    "14144141",
    "14444441",
    "11111111",
};
const char* const kCameraRows[] = {
    "...111....",
    "1111111111",
    "1222442141",
    "1224114221",
    "1224114221",
    "1222442221",
    "1222222221",
    "1111111111",
};
const char* const kBatteryRows[] = {
    "1111111111.",
    "1442442441.",
    "14424424411",
    "14424424411",
    "14424424411",
    "1442442441.",
    "1111111111.",
};
const char* const kLockRows[] = {
    "..1111..",
    ".11..11.",
    ".1....1.",
    ".1....1.",
    "11111111",
    "14444441",
    "14411441",
    "14411441",
    "14444441",
    "11111111",
};
// Значки HUD (рисуются светлым по тёмной полосе).
const char* const kHudFlagRows[] = {
    "442424",
    "424242",
    "442424",
    "424242",
    "4.....",
    "4.....",
    "4.....",
};
const char* const kHudCanRows[] = {
    ".44.44.",
    "4444444",
    "4.....4",
    "4.4.4.4",
    "4..4..4",
    "4.4.4.4",
    "4.....4",
    "4444444",
};
// Логотип «GAMEKID» (оригинальный рубленый шрифт), 45x7.
const char* const kLogoRows[] = {
    ".####." "." ".####." "." "##...##" "." "######" "." "##..##" "." "##" "." "#####.",
    "##..##" "." "##..##" "." "###.###" "." "##...." "." "##.##." "." "##" "." "##..##",
    "##...." "." "##..##" "." "#######" "." "##...." "." "####.." "." "##" "." "##..##",
    "##.###" "." "######" "." "##.#.##" "." "#####." "." "###..." "." "##" "." "##..##",
    "##..##" "." "##..##" "." "##...##" "." "##...." "." "####.." "." "##" "." "##..##",
    "##..##" "." "##..##" "." "##...##" "." "##...." "." "##.##." "." "##" "." "##..##",
    ".####." "." "##..##" "." "##...##" "." "######" "." "##..##" "." "##" "." "#####.",
};
// clang-format on

const Sprite kSprCar = makeSprite(kCarRows);
const Sprite kSprSedan = makeSprite(kSedanRows);
const Sprite kSprTruck = makeSprite(kTruckRows);
const Sprite kSprRock = makeSprite(kRockRows);
const Sprite kSprCone = makeSprite(kConeRows);
const Sprite kSprOil = makeSprite(kOilRows);
const Sprite kSprFuel = makeSprite(kFuelRows);
const Sprite kSprCamera = makeSprite(kCameraRows);
const Sprite kSprBattery = makeSprite(kBatteryRows);
const Sprite kSprLock = makeSprite(kLockRows);
const Sprite kSprHudFlag = makeSprite(kHudFlagRows);
const Sprite kSprHudCan = makeSprite(kHudCanRows);
const Sprite kSprLogo = makeSprite(kLogoRows);

// Рисует спрайт с центром (cx, cy). orient: 0 — носом вверх, 1 — вправо,
// 2 — вниз, 3 — влево. shear (только orient 0): поворот «носом» на ±1 пиксель —
// верхние строки (капот) сдвигаются в сторону поворота.
void drawSpr(Canvas& c, const Sprite& s, int cx, int cy, const uint32_t* pal, int orient = 0,
             int shear = 0) {
    const int o = orient & 3;
    const bool sideways = (o & 1) != 0;
    const int bw = sideways ? s.h : s.w;
    const int bh = sideways ? s.w : s.h;
    const int x0 = cx - bw / 2, y0 = cy - bh / 2;
    const int band = (s.h * 2) / 5;
    for (int r = 0; r < s.h; ++r) {
        const char* row = s.rows[r];
        const int sh = r < band ? shear : 0;
        for (int col = 0; col < s.w; ++col) {
            const char ch = row[col];
            if (ch < '1' || ch > '4') continue;
            int dx = col + sh, dy = r;
            if (o == 1) { dx = s.h - 1 - r; dy = col; }
            else if (o == 2) { dx = s.w - 1 - col; dy = s.h - 1 - r; }
            else if (o == 3) { dx = r; dy = s.w - 1 - col; }
            c.plot(x0 + dx, y0 + dy, pal[ch - '0']);
        }
    }
}

// Логотип «GAMEKID»: каждый пиксель — квадрат размером px (может быть < 1:
// уменьшенный логотип на опущенной консоли превращается в мелкую надпись).
void drawLogo(Canvas& c, float x0, float y0, float px, uint32_t col) {
    const int size = std::max(1, roundi(px));
    for (int r = 0; r < kSprLogo.h; ++r) {
        const char* row = kSprLogo.rows[r];
        for (int i = 0; i < kSprLogo.w; ++i) {
            if (row[i] != '#') continue;
            const int x = static_cast<int>(std::floor(x0 + static_cast<float>(i) * px));
            const int y = static_cast<int>(std::floor(y0 + static_cast<float>(r) * px));
            c.fillRect(x, y, size, size, col);
        }
    }
}

// Текст с обводкой со всех восьми сторон — «наклейка», которая читается и на
// светлом асфальте, и на тёмной траве.
void drawOutlinedText(Canvas& c, int x, int y, const char* text, uint32_t fill, uint32_t outline,
                      int scale = 1) {
    for (int dy = -scale; dy <= scale; dy += scale)
        for (int dx = -scale; dx <= scale; dx += scale)
            if (dx != 0 || dy != 0) font::drawText(c, x + dx, y + dy, text, outline, scale);
    font::drawText(c, x, y, text, fill, scale);
}

// ---------------------------------------------------------------------------
//  Дорога и вспомогательная математика
// ---------------------------------------------------------------------------
// Центр дороги в мировой координате: две медленные синусоиды дают плавные,
// не повторяющиеся на глаз изгибы (размах ±33 пикселя).
float roadCenterWorld(double wy) {
    return static_cast<float>(80.0 + 23.0 * std::sin(wy * 0.0041) + 10.0 * std::sin(wy * 0.0113 + 1.3));
}

// Быстрый целочисленный хеш (для текстур, привязанных к миру).
inline uint32_t ihash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

inline long long floorDiv(long long a, long long b) {
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// Асфальт: светлый; крупные «выгоревшие» пятна — россыпь самого светлого
// оттенка разной густоты, и редкие тёмные камешки. Россыпь случайная, а не
// упорядоченная: у уменьшенной вдвое консоли регулярный узор дал бы муар.
inline uint32_t roadPixel(int x, uint32_t wy) {
    const uint32_t h = ihash(static_cast<uint32_t>(x) * 0x27D4EB2Fu ^ wy * 0x165667B1u);
    if (h % 199u == 1u) return kC2;
    const uint32_t patch = ihash((static_cast<uint32_t>(x / 12) * 0x5BD1E995u) ^ ((wy / 12u) * 0x1B873593u));
    const uint32_t density = 2u + (patch % 4u) * 4u; // 2..14 из 64
    return ((h >> 8) & 63u) < density ? kC4 : kC3;
}

// Трава: тёмная, с «галочками» пучков и редкими тёмными точками.
inline uint32_t grassPixel(int x, uint32_t wy) {
    const uint32_t cell = ihash((static_cast<uint32_t>(x / 6) * 0x8DA6B343u) ^ ((wy / 6u) * 0xD8163841u));
    if ((cell & 3u) == 0u) {
        const int lx = x % 6 - static_cast<int>((cell >> 4) % 3u);
        const int ly = static_cast<int>(wy % 6u) - static_cast<int>((cell >> 6) % 3u);
        // ly растёт вверх по экрану: сверху две травинки, снизу основание — «v».
        if ((ly == 1 && (lx == 0 || lx == 2)) || (ly == 0 && lx == 1)) return kC1;
    }
    const uint32_t h = ihash(static_cast<uint32_t>(x) * 0x9E3779B1u ^ wy * 0x85EBCA77u);
    return h % 37u == 0u ? kC1 : kC2;
}

bool isPickup(MiniObjectType t) {
    return t == MiniObjectType::Fuel || t == MiniObjectType::Camera || t == MiniObjectType::Battery ||
           t == MiniObjectType::Lock;
}

// Неподвижные (относительно дороги) помехи: камень, конус, масло.
bool isStaticHazard(MiniObjectType t) {
    return t == MiniObjectType::Rock || t == MiniObjectType::Cone || t == MiniObjectType::Oil;
}

const Sprite& spriteFor(MiniObjectType t, int variant) {
    switch (t) {
    case MiniObjectType::Rock: return kSprRock;
    case MiniObjectType::Cone: return kSprCone;
    case MiniObjectType::Oil: return kSprOil;
    case MiniObjectType::Car: return variant == 1 ? kSprTruck : kSprSedan;
    case MiniObjectType::Fuel: return kSprFuel;
    case MiniObjectType::Camera: return kSprCamera;
    case MiniObjectType::Battery: return kSprBattery;
    case MiniObjectType::Lock: return kSprLock;
    }
    return kSprRock;
}

// Размер хитбокса по типу (чуть меньше спрайта).
void hitboxFor(MiniObjectType t, int variant, float& w, float& h) {
    const Sprite& s = spriteFor(t, variant);
    w = static_cast<float>(s.w);
    h = static_cast<float>(s.h);
    if (t == MiniObjectType::Rock || t == MiniObjectType::Car || t == MiniObjectType::Cone) {
        w -= 1.0f;
        h -= 1.0f;
    } else if (t == MiniObjectType::Oil) {
        w -= 2.0f;
        h -= 2.0f;
    }
}

// Кадр отрисовки: целая часть пройденного пути и дробный сдвиг объектов,
// чтобы асфальт и предметы на нём двигались строго синхронно.
struct RenderCtx {
    long long fd = 0;
    float frac = 0.0f;
};

} // namespace

// ============================================================================
//  Состояние мини-игры
// ============================================================================
struct ConsoleMiniGame::State {
    // ---- Вспомогательные типы ------------------------------------------------
    enum class FxKind : uint8_t { Debris, Smoke, Dust, Spark };

    // Частица эффекта (обломок, клуб дыма или пыли, искра) — едет вместе с дорогой.
    struct Particle {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float age = 0.0f, life = 1.0f;
        FxKind kind = FxKind::Debris;
        uint32_t color = 0u;
        bool alive = false;
    };

    // Всплывающая надпись над подобранным предметом.
    struct Pop {
        const char* text = nullptr;
        float x = 0.0f, y = 0.0f;
        float age = 0.0f;
        bool alive = false;
    };

    // Точка следа шин на заносе.
    struct Mark {
        float x = 0.0f;
        float y = -1000.0f;
    };

    // Дополнительные данные объекта (параллельно MiniObject, тот же индекс).
    struct ObjExtra {
        float lane = 0.0f;        // машины: смещение от центра дороги
        float laneTarget = 0.0f;  // куда перестраивается
        float cruise = 0.0f;      // машины: крейсерская собственная скорость (vy)
        float signal = 0.0f;      // машины: поворотник мигает до перестроения
        float phase = 0.0f;       // фаза покачивания бонуса
        int variant = 0;          // вариант спрайта (седан / фургон)
        bool used = false;        // масло уже вызвало занос
    };

    // Занятый отрезок поперёк дороги (в координатах «полосы»).
    struct Span {
        float lo = 0.0f;
        float hi = 0.0f;
    };

    // ---- Генераторы: игровой (спавн) и для эффектов -------------------------
    Rng rng{1u};
    Rng fx{2u};
    // ---- Мир -----------------------------------------------------------------
    double dist = 0.0;      // пройденный путь, пиксели
    float time = 0.0f;      // с момента reset
    float speed = 0.0f;     // скорость прокрутки, пикс/с
    float halfWidth = 48.0f;
    // ---- Игрок ---------------------------------------------------------------
    float px = 80.0f;       // центр машины по горизонтали
    float vx = 0.0f;        // боковая скорость
    float tilt = 0.0f;      // визуальный крен -1..1
    float crashT = 0.0f;    // оглушение после аварии (с)
    float invulnT = 0.0f;   // неуязвимость (с)
    float skidT = 0.0f;     // занос (с)
    float dustClock = 0.0f;
    int jitterX = 0;        // тряска на траве/бордюре
    int jitterY = 0;
    bool offRoad = false;
    bool onKerb = false;
    // ---- Таймеры спавна (тикают пропорционально скорости) -------------------
    float obstacleClock = 1.4f;
    float fuelClock = 2.6f;
    float cameraClock = 3.0f;
    float batteryClock = 30.0f;
    float lockClock = 25.0f;
    bool cameraOwned = false;
    // ---- Счёт ----------------------------------------------------------------
    int fuelCount = 0;
    int crashCount = 0;
    // ---- Эффекты -------------------------------------------------------------
    float flashT = 0.0f;
    float itemFlashT = 0.0f;
    MiniObjectType itemFlashType = MiniObjectType::Fuel;
    float boomT = 0.0f;
    float boomX = 0.0f;
    float boomY = 0.0f;
    std::array<Particle, 96> parts{};
    std::array<Pop, 6> pops{};
    std::array<Mark, 192> marks{};
    int markHead = 0;
    // ---- Объекты и события ---------------------------------------------------
    std::vector<MiniObject> objs;
    std::vector<ObjExtra> extra;
    std::vector<MiniEvent> events;
    std::vector<Span> spans; // рабочий буфер проверки проезда

    // ---- Логика --------------------------------------------------------------
    void step(float dt, int steer, bool attended, float difficulty, bool hasCamera);
    float centerAt(float screenY) const {
        return roadCenterWorld(dist + static_cast<double>(kH) - static_cast<double>(screenY));
    }
    float laneOf(size_t i) const;
    bool gapOk(int self, float lo, float hi, float y, float h, bool movingCar);
    bool pickupsClear(float y, float h) const;
    bool laneBlockedAhead(size_t i, float lane, float look) const;
    bool laneFreeForCar(size_t i, float lane);
    void addObject(MiniObjectType type, int variant, float x, float y, float vy, const ObjExtra& ex);
    bool spawnObstacle(float difficulty);
    bool spawnCones();
    bool spawnCar();
    bool spawnPickup(MiniObjectType type);
    void updateCars(float dt);
    void collide(bool hasCamera);
    void crash(size_t i);
    void collect(size_t i, bool hasCamera);
    void startSkid(size_t i);
    void emit(FxKind kind, float x, float y, float vx, float vy, float life, uint32_t color);
    void addPop(const char* text, float x, float y);
    void updateFx(float dt, float move);
    void removeDead();

    // ---- Отрисовка -----------------------------------------------------------
    void render(Canvas& c) const;
    void drawBoot(Canvas& c) const;
    void drawRoad(Canvas& c, const RenderCtx& rc) const;
    void drawDecor(Canvas& c, const RenderCtx& rc) const;
    void drawMarks(Canvas& c, const RenderCtx& rc) const;
    void drawObjects(Canvas& c, const RenderCtx& rc) const;
    void drawParticles(Canvas& c, const RenderCtx& rc) const;
    void drawPlayer(Canvas& c) const;
    void drawBoom(Canvas& c, const RenderCtx& rc) const;
    void drawPops(Canvas& c) const;
    void drawHud(Canvas& c) const;
    void drawGo(Canvas& c) const;
};

// ============================================================================
//  Логика: один подшаг (dt <= 1/60)
// ============================================================================
void ConsoleMiniGame::State::step(float dt, int steer, bool attended, float difficulty, bool hasCamera) {
    time += dt;
    flashT = std::max(0.0f, flashT - dt);
    itemFlashT = std::max(0.0f, itemFlashT - dt);
    boomT = std::max(0.0f, boomT - dt);
    const bool intro = time < kIntroTime;

    // Ширина дороги плавно сужается с ростом сложности.
    halfWidth = approach(halfWidth, lerpf(48.0f, 38.0f, difficulty), 4.0f * dt);

    // Камера уже есть: камеры на трассе превращаются в батарейки.
    if (hasCamera) {
        for (size_t i = 0; i < objs.size(); ++i) {
            if (objs[i].type != MiniObjectType::Camera) continue;
            objs[i].type = MiniObjectType::Battery;
            hitboxFor(MiniObjectType::Battery, 0, objs[i].w, objs[i].h);
        }
        if (!cameraOwned) {
            cameraOwned = true;
            batteryClock = rng.range(25.0f, 35.0f);
        }
    }

    // ---- Скорость прокрутки --------------------------------------------------
    const float baseSpeed = lerpf(kSpeedMin, kSpeedMax, difficulty);
    const float target = offRoad ? baseSpeed * kOffRoadSpeed : baseSpeed;
    if (intro) speed = 0.0f;
    else if (crashT > 0.0f) speed = approach(speed, 0.0f, kCrashDecel * dt);
    else speed = approach(speed, target, (speed < target ? kAccel : kDecel) * dt);
    const float move = speed * dt;
    dist += static_cast<double>(move);

    // ---- Таймеры аварии и неуязвимости ---------------------------------------
    if (crashT > 0.0f) {
        crashT -= dt;
        if (crashT <= 0.0f) {
            crashT = 0.0f;
            invulnT = kInvulnTime;
        }
    } else {
        invulnT = std::max(0.0f, invulnT - dt);
    }
    skidT = std::max(0.0f, skidT - dt);

    // ---- Руление -------------------------------------------------------------
    // Без внимания игрока руль отпущен: машина едет прямо, а дорога петляет.
    const int input = (attended && !intro) ? clampi(steer, -1, 1) : 0;
    if (crashT > 0.0f) {
        vx = approach(vx, 0.0f, 600.0f * dt);
    } else {
        const float control = skidT > 0.0f ? 0.3f : 1.0f;
        if (input != 0) {
            float acc = kSteerAccel * control;
            if (vx * static_cast<float>(input) < 0.0f) acc *= 1.6f; // контррулёжка резче
            vx = approach(vx, static_cast<float>(input) * kSteerSpeed, acc * dt);
        } else {
            vx = approach(vx, 0.0f, kFriction * (skidT > 0.0f ? 0.2f : 1.0f) * dt);
        }
    }
    px += vx * dt;
    if (px < 6.0f || px > static_cast<float>(kW) - 6.0f) {
        px = clampf(px, 6.0f, static_cast<float>(kW) - 6.0f);
        vx = 0.0f;
    }
    float tiltTarget = clampf(vx / 70.0f, -1.0f, 1.0f);
    if (skidT > 0.0f) tiltTarget = std::sin(time * 26.0f);
    tilt = approach(tilt, tiltTarget, 8.0f * dt);

    // ---- Обочина: трава замедляет и трясёт, бордюр — лёгкая дрожь ----------
    const float off = std::fabs(px - centerAt(kPlayerY));
    offRoad = off > halfWidth + 1.0f;
    onKerb = !offRoad && off > halfWidth - kKerbW - kPlayerHalfW;
    jitterX = 0;
    jitterY = 0;
    if (speed > 5.0f && crashT <= 0.0f) {
        if (offRoad) {
            jitterX = fx.rangeInt(-1, 1);
            jitterY = fx.rangeInt(-1, 1);
        } else if (onKerb) {
            jitterY = (static_cast<int>(dist / 3.0) & 1);
        }
    }

    // ---- Объекты едут вниз вместе с дорогой ---------------------------------
    for (size_t i = 0; i < objs.size(); ++i) {
        MiniObject& o = objs[i];
        if (o.type == MiniObjectType::Car) continue; // машины двигаются сами
        o.y += move;
        extra[i].phase += dt;
    }
    updateCars(dt);

    // ---- Столкновения --------------------------------------------------------
    if (!intro) collide(hasCamera);

    // ---- Спавн: «часы» идут пропорционально скорости (стоим — ничего нет) --
    if (!intro) {
        const float k = clampf(speed / std::max(1.0f, baseSpeed), 0.0f, 1.2f) * dt;
        obstacleClock -= k;
        if (obstacleClock <= 0.0f) {
            const float interval = lerpf(1.35f, 0.75f, difficulty);
            obstacleClock = spawnObstacle(difficulty) ? interval * rng.range(0.7f, 1.3f) : kRetry;
        }
        fuelClock -= k;
        if (fuelClock <= 0.0f) fuelClock = spawnPickup(MiniObjectType::Fuel) ? rng.range(4.5f, 7.0f) : kRetry;
        if (!hasCamera) {
            cameraClock -= k;
            if (cameraClock <= 0.0f)
                cameraClock = spawnPickup(MiniObjectType::Camera) ? rng.range(9.0f, 11.0f) : kRetry;
        } else {
            batteryClock -= k;
            if (batteryClock <= 0.0f)
                batteryClock = spawnPickup(MiniObjectType::Battery) ? rng.range(25.0f, 35.0f) : kRetry;
        }
        lockClock -= k;
        if (lockClock <= 0.0f) lockClock = spawnPickup(MiniObjectType::Lock) ? rng.range(30.0f, 45.0f) : kRetry;
    }

    updateFx(dt, move);
    removeDead();
}

// Смещение объекта от центра дороги («полоса»).
float ConsoleMiniGame::State::laneOf(size_t i) const {
    if (objs[i].type == MiniObjectType::Car) return extra[i].lane;
    return objs[i].x - centerAt(objs[i].y);
}

// Останется ли проезд шириной kGapNeed, если занять полосы [lo, hi] на высоте y.
// self — индекс объекта, который не учитывать (машина при перестроении) или -1.
// movingCar — проверяем машину: ей мешают неподвижные помехи впереди (она их
// догоняет) и рядом в пределах длины машины игрока, а также все машины перед
// игроком (скорости разные — встретятся).
bool ConsoleMiniGame::State::gapOk(int self, float lo, float hi, float y, float h, bool movingCar) {
    spans.clear();
    for (size_t j = 0; j < objs.size(); ++j) {
        if (static_cast<int>(j) == self) continue;
        const MiniObject& o = objs[j];
        if (!o.alive || isPickup(o.type)) continue;
        float a = 0.0f, b = 0.0f;
        bool relevant = false;
        if (o.type == MiniObjectType::Car) {
            relevant = o.y < kPlayerY;
            const ObjExtra& e = extra[j];
            a = std::min(e.lane, e.laneTarget) - o.w * 0.5f;
            b = std::max(e.lane, e.laneTarget) + o.w * 0.5f;
        } else {
            if (movingCar) relevant = o.y < y + (o.h + h) * 0.5f + kCarLen + 2.0f && o.y > y - 110.0f;
            else relevant = std::fabs(o.y - y) < (o.h + h) * 0.5f + kCarLen + 2.0f;
            const float l = laneOf(j);
            a = l - o.w * 0.5f;
            b = l + o.w * 0.5f;
        }
        if (relevant) spans.push_back(Span{a - 1.0f, b + 1.0f});
    }
    spans.push_back(Span{lo - 1.0f, hi + 1.0f});
    std::sort(spans.begin(), spans.end(), [](const Span& p, const Span& q) { return p.lo < q.lo; });
    const float left = -halfWidth + kKerbW, right = halfWidth - kKerbW;
    float cursor = left, best = 0.0f;
    for (const Span& sp : spans) {
        if (sp.lo > cursor) best = std::max(best, std::min(sp.lo, right) - cursor);
        cursor = std::max(cursor, sp.hi);
        if (cursor >= right) break;
    }
    best = std::max(best, right - cursor);
    return best >= kGapNeed;
}

// Нет ли бонусов слишком близко по вертикали от новой помехи.
bool ConsoleMiniGame::State::pickupsClear(float y, float h) const {
    for (const MiniObject& o : objs) {
        if (!isPickup(o.type)) continue;
        if (std::fabs(o.y - y) < kPickupGapY + (o.h + h) * 0.25f) return false;
    }
    return true;
}

// Есть ли неподвижная помеха впереди машины i в полосе lane (в пределах look).
bool ConsoleMiniGame::State::laneBlockedAhead(size_t i, float lane, float look) const {
    const MiniObject& car = objs[i];
    for (size_t j = 0; j < objs.size(); ++j) {
        if (j == i || !isStaticHazard(objs[j].type)) continue;
        const MiniObject& o = objs[j];
        const float ahead = car.y - o.y;
        if (ahead <= 0.0f || ahead > look + (car.h + o.h) * 0.5f) continue;
        if (std::fabs(laneOf(j) - lane) < (car.w + o.w) * 0.5f + 3.0f) return true;
    }
    return false;
}

// Можно ли машине i перестроиться в полосу lane.
bool ConsoleMiniGame::State::laneFreeForCar(size_t i, float lane) {
    const MiniObject& car = objs[i];
    if (laneBlockedAhead(i, lane, 60.0f)) return false;
    for (size_t j = 0; j < objs.size(); ++j) {
        if (j == i || objs[j].type != MiniObjectType::Car) continue;
        if (std::fabs(objs[j].y - car.y) > (objs[j].h + car.h) * 0.5f + 14.0f) continue;
        const ObjExtra& e = extra[j];
        if (std::fabs(e.laneTarget - lane) < car.w + 2.0f || std::fabs(e.lane - lane) < car.w + 2.0f) return false;
    }
    // Во время перестроения машина заметает всю полосу между старой и новой позицией.
    const float lo = std::min(extra[i].lane, lane), hi = std::max(extra[i].lane, lane);
    return gapOk(static_cast<int>(i), lo - car.w * 0.5f, hi + car.w * 0.5f, car.y, car.h, true);
}

void ConsoleMiniGame::State::addObject(MiniObjectType type, int variant, float x, float y, float vy,
                                       const ObjExtra& ex) {
    MiniObject o;
    o.type = type;
    o.x = x;
    o.y = y;
    o.vy = vy;
    hitboxFor(type, variant, o.w, o.h);
    objs.push_back(o);
    ObjExtra e = ex;
    e.variant = variant;
    extra.push_back(e);
}

// ---------------------------------------------------------------------------
//  Спавн
// ---------------------------------------------------------------------------
bool ConsoleMiniGame::State::spawnObstacle(float difficulty) {
    if (objs.size() >= kMaxObjects) return false;
    const float r = rng.next01();
    // Доли: машины ~20% (16% -> 24%: к концу поездки трафик плотнее), конусы 25%,
    // масло 15%, камни — остальное (~40%).
    const float carShare = lerpf(0.16f, 0.24f, difficulty);
    if (r < carShare) return spawnCar();
    if (r < carShare + 0.25f) return spawnCones();
    const MiniObjectType type = r < carShare + 0.25f + 0.15f ? MiniObjectType::Oil : MiniObjectType::Rock;
    float w = 0.0f, h = 0.0f;
    hitboxFor(type, 0, w, h);
    const float y = 2.0f - h * 0.5f;
    if (!pickupsClear(y, h)) return false;
    const float lim = halfWidth - kKerbW - w * 0.5f - 1.0f;
    for (int attempt = 0; attempt < 6; ++attempt) {
        const float lane = rng.range(-lim, lim);
        if (!gapOk(-1, lane - w * 0.5f, lane + w * 0.5f, y, h, false)) continue;
        ObjExtra ex;
        ex.phase = rng.range(0.0f, kTau);
        addObject(type, 0, centerAt(y) + lane, y, 0.0f, ex);
        return true;
    }
    return false;
}

// Конусы: одиночный или ряд из 2–3 штук («ремонт полосы»).
bool ConsoleMiniGame::State::spawnCones() {
    const float pick = rng.next01();
    const int count = pick < 0.5f ? 1 : (pick < 0.8f ? 2 : 3);
    if (objs.size() + static_cast<size_t>(count) > kMaxObjects) return false;
    float w = 0.0f, h = 0.0f;
    hitboxFor(MiniObjectType::Cone, 0, w, h);
    const float spacing = 9.0f;
    const float groupW = spacing * static_cast<float>(count - 1) + w;
    const float y = 2.0f - h * 0.5f;
    if (!pickupsClear(y, h)) return false;
    const float lim = halfWidth - kKerbW - groupW * 0.5f - 1.0f;
    if (lim <= 0.0f) return false;
    for (int attempt = 0; attempt < 6; ++attempt) {
        const float lane = rng.range(-lim, lim);
        if (!gapOk(-1, lane - groupW * 0.5f, lane + groupW * 0.5f, y, h, false)) continue;
        const float c = centerAt(y);
        for (int k = 0; k < count; ++k) {
            const float off = (static_cast<float>(k) - static_cast<float>(count - 1) * 0.5f) * spacing;
            addObject(MiniObjectType::Cone, 0, c + lane + off, y, 0.0f, ObjExtra{});
        }
        return true;
    }
    return false;
}

// Попутная машина в одной из трёх полос.
bool ConsoleMiniGame::State::spawnCar() {
    if (objs.size() >= kMaxObjects) return false;
    const int variant = rng.chance(0.3f) ? 1 : 0;
    float w = 0.0f, h = 0.0f;
    hitboxFor(MiniObjectType::Car, variant, w, h);
    const float y = 2.0f - h * 0.5f;
    const float laneStep = halfWidth * 2.0f / 3.0f;
    const int first = rng.rangeInt(0, 2);
    for (int k = 0; k < 3; ++k) {
        const float lane = static_cast<float>((first + k) % 3 - 1) * laneStep;
        if (!gapOk(-1, lane - w * 0.5f, lane + w * 0.5f, y, h, true)) continue;
        // Не ставим машину вплотную к другой в той же полосе.
        bool crowded = false;
        for (size_t j = 0; j < objs.size(); ++j) {
            if (objs[j].type != MiniObjectType::Car) continue;
            if (objs[j].y < 30.0f && std::fabs(extra[j].laneTarget - lane) < w + 2.0f) crowded = true;
        }
        if (crowded) continue;
        ObjExtra ex;
        ex.lane = lane;
        ex.laneTarget = lane;
        ex.cruise = -rng.range(22.0f, 42.0f);
        // На малой скорости машина не должна сразу уехать вверх за экран.
        const float vy = std::min(0.0f, std::max(ex.cruise, 12.0f - speed));
        addObject(MiniObjectType::Car, variant, centerAt(y) + lane, y, vy, ex);
        return true;
    }
    return false;
}

bool ConsoleMiniGame::State::spawnPickup(MiniObjectType type) {
    if (objs.size() >= kMaxObjects) return false;
    float w = 0.0f, h = 0.0f;
    hitboxFor(type, 0, w, h);
    const float y = 2.0f - h * 0.5f;
    // По вертикали — подальше от помех и других бонусов.
    for (const MiniObject& o : objs) {
        const float dy = std::fabs(o.y - y);
        if (isPickup(o.type)) {
            if (dy < 18.0f) return false;
        } else if (o.type != MiniObjectType::Car) {
            if (dy < kPickupGapY + (o.h + h) * 0.25f) return false;
        }
    }
    const float lim = halfWidth - kKerbW - w * 0.5f - 3.0f;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const float lane = rng.range(-lim, lim);
        // Не кладём бонус в полосу попутной машины — его бы закрыло кузовом.
        bool ok = true;
        for (size_t j = 0; j < objs.size() && ok; ++j) {
            if (objs[j].type != MiniObjectType::Car || objs[j].y > kPlayerY) continue;
            if (std::fabs(extra[j].laneTarget - lane) < (w + objs[j].w) * 0.5f + 4.0f) ok = false;
        }
        if (!ok) continue;
        ObjExtra ex;
        ex.phase = rng.range(0.0f, kTau);
        addObject(type, 0, centerAt(y) + lane, y, 0.0f, ex);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Попутные машины: следуют изгибам, объезжают помехи, пропускают игрока
// ---------------------------------------------------------------------------
void ConsoleMiniGame::State::updateCars(float dt) {
    const float laneStep = halfWidth * 2.0f / 3.0f;
    for (size_t i = 0; i < objs.size(); ++i) {
        if (objs[i].type != MiniObjectType::Car) continue;
        ObjExtra& e = extra[i];
        if (e.signal > 0.0f) e.signal = std::max(0.0f, e.signal - dt);
        else e.lane = approach(e.lane, e.laneTarget, kLaneChangeSpeed * dt);

        const float carY = objs[i].y;
        float want = e.cruise;
        if (carY > kPlayerY - 12.0f) {
            // Рядом или позади игрока: притормаживает и отстаёт (не таранит сзади).
            want = std::max(e.cruise, 18.0f - speed);
        } else {
            const bool settled = e.signal <= 0.0f && e.lane == e.laneTarget;
            const bool farAhead = carY < kPlayerY - 40.0f;
            if (laneBlockedAhead(i, e.laneTarget, 46.0f)) {
                bool moved = false;
                if (settled && farAhead) {
                    // Ближайшая свободная полоса (сначала соседние).
                    for (int d = 1; d <= 2 && !moved; ++d) {
                        for (int sgn = -1; sgn <= 1 && !moved; sgn += 2) {
                            const float cand = e.laneTarget + static_cast<float>(sgn * d) * laneStep;
                            if (std::fabs(cand) > laneStep + 0.5f) continue;
                            if (laneFreeForCar(i, cand)) {
                                e.laneTarget = cand;
                                e.signal = 0.3f;
                                moved = true;
                            }
                        }
                    }
                }
                // Объехать нельзя — сбрасывает скорость и едет за помехой.
                if (!moved && e.lane == e.laneTarget) want = 0.0f;
            } else if (settled && farAhead && carY > 6.0f && rng.chance(0.05f * dt)) {
                // Изредка перестраивается просто так (только далеко впереди).
                const float cand = e.laneTarget + (rng.chance(0.5f) ? -laneStep : laneStep);
                if (std::fabs(cand) <= laneStep + 0.5f && laneFreeForCar(i, cand)) {
                    e.laneTarget = cand;
                    e.signal = 0.5f;
                }
            }
            // Не наезжает на более медленную машину впереди в своей полосе.
            for (size_t j = 0; j < objs.size(); ++j) {
                if (j == i || objs[j].type != MiniObjectType::Car) continue;
                const float ahead = carY - objs[j].y;
                if (ahead <= 0.0f || ahead > (objs[i].h + objs[j].h) * 0.5f + 8.0f) continue;
                if (std::fabs(extra[j].lane - e.lane) < objs[i].w + 1.0f) want = std::max(want, objs[j].vy);
            }
        }
        MiniObject& o = objs[i];
        o.vy = approach(o.vy, want, 40.0f * dt);
        o.y += (speed + o.vy) * dt;
        o.x = centerAt(o.y) + e.lane;
    }
}

// ---------------------------------------------------------------------------
//  Столкновения (AABB, у помех чуть ужатые, у бонусов — с запасом)
// ---------------------------------------------------------------------------
void ConsoleMiniGame::State::collide(bool hasCamera) {
    for (size_t i = 0; i < objs.size(); ++i) {
        MiniObject& o = objs[i];
        if (!o.alive) continue;
        const float dx = std::fabs(o.x - px), dy = std::fabs(o.y - kPlayerY);
        if (isPickup(o.type)) {
            if (dx < kPlayerHalfW + o.w * 0.5f + 2.0f && dy < kPlayerHalfH + o.h * 0.5f + 2.0f) collect(i, hasCamera);
            continue;
        }
        if (dx >= kPlayerHalfW + o.w * 0.5f - 1.0f || dy >= kPlayerHalfH + o.h * 0.5f - 1.0f) continue;
        if (o.type == MiniObjectType::Oil) {
            if (!extra[i].used && crashT <= 0.0f && skidT <= 0.0f) startSkid(i);
            continue;
        }
        if (crashT > 0.0f || invulnT > 0.0f) continue;
        if (o.type == MiniObjectType::Car && o.y > kPlayerY + 2.0f) continue; // машина сзади — не авария
        crash(i);
    }
}

void ConsoleMiniGame::State::crash(size_t i) {
    MiniObject& o = objs[i];
    crashT = kCrashTime;
    invulnT = 0.0f;
    skidT = 0.0f;
    ++crashCount;
    events.push_back(MiniEvent::Crashed);
    flashT = kFlashTime;
    boomT = kBoomTime;
    boomX = (o.x + px) * 0.5f;
    boomY = std::min(o.y + o.h * 0.5f, kPlayerY - 4.0f);
    vx *= 0.25f;
    // Обломки (цвет — как у того, во что врезались) и клубы дыма.
    const uint32_t debris = o.type == MiniObjectType::Cone ? kC4 : kC2;
    for (int k = 0; k < 12; ++k) {
        const float a = fx.range(0.0f, kTau), v = fx.range(35.0f, 105.0f);
        emit(FxKind::Debris, boomX, boomY, std::cos(a) * v, std::sin(a) * v, fx.range(0.45f, 0.9f),
             (k & 1) ? kC1 : debris);
    }
    for (int k = 0; k < 4; ++k)
        emit(FxKind::Smoke, boomX + fx.range(-4.0f, 4.0f), boomY + fx.range(-3.0f, 3.0f), fx.range(-8.0f, 8.0f),
             fx.range(-22.0f, -10.0f), fx.range(0.8f, 1.3f), kC2);
    o.alive = false;
}

void ConsoleMiniGame::State::collect(size_t i, bool hasCamera) {
    MiniObject& o = objs[i];
    o.alive = false;
    MiniObjectType shown = o.type;
    switch (o.type) {
    case MiniObjectType::Fuel:
        ++fuelCount;
        events.push_back(MiniEvent::PickedFuel);
        addPop(T8("+БЕНЗИН"), o.x, o.y);
        break;
    case MiniObjectType::Camera:
        if (!hasCamera) {
            events.push_back(MiniEvent::PickedCamera);
            addPop(T8("+КАМЕРА"), o.x, o.y);
        } else {
            shown = MiniObjectType::Battery;
            events.push_back(MiniEvent::PickedBattery);
            addPop(T8("+ЗАРЯД"), o.x, o.y);
        }
        break;
    case MiniObjectType::Battery:
        events.push_back(MiniEvent::PickedBattery);
        addPop(T8("+ЗАРЯД"), o.x, o.y);
        break;
    case MiniObjectType::Lock:
        events.push_back(MiniEvent::PickedLock);
        addPop(T8("+ЗАМОК"), o.x, o.y);
        break;
    default: break;
    }
    itemFlashT = kItemFlashTime;
    itemFlashType = shown;
    for (int k = 0; k < 8; ++k) {
        const float a = static_cast<float>(k) * (kTau / 8.0f) + 0.2f;
        emit(FxKind::Spark, o.x, o.y, std::cos(a) * 55.0f, std::sin(a) * 55.0f, 0.35f, (k & 1) ? kC1 : kC4);
    }
}

void ConsoleMiniGame::State::startSkid(size_t i) {
    extra[i].used = true;
    skidT = kSkidTime;
    const float dir = rng.chance(0.5f) ? -1.0f : 1.0f;
    vx += dir * rng.range(50.0f, 70.0f);
    events.push_back(MiniEvent::Skid);
}

// ---------------------------------------------------------------------------
//  Эффекты
// ---------------------------------------------------------------------------
void ConsoleMiniGame::State::emit(FxKind kind, float x, float y, float pvx, float pvy, float life,
                                  uint32_t color) {
    for (Particle& p : parts) {
        if (p.alive) continue;
        p.x = x;
        p.y = y;
        p.vx = pvx;
        p.vy = pvy;
        p.age = 0.0f;
        p.life = life;
        p.kind = kind;
        p.color = color;
        p.alive = true;
        return;
    }
}

void ConsoleMiniGame::State::addPop(const char* text, float x, float y) {
    Pop* slot = &pops[0];
    for (Pop& p : pops) {
        if (!p.alive) { slot = &p; break; }
        if (p.age > slot->age) slot = &p; // все заняты — вытесняем самую старую
    }
    slot->text = text;
    slot->x = x;
    slot->y = std::min(y, kPlayerY - 16.0f);
    slot->age = 0.0f;
    slot->alive = true;
}

void ConsoleMiniGame::State::updateFx(float dt, float move) {
    for (Particle& p : parts) {
        if (!p.alive) continue;
        p.age += dt;
        if (p.age >= p.life) { p.alive = false; continue; }
        const float drag = p.kind == FxKind::Smoke ? 1.5f : 3.5f;
        p.vx -= p.vx * std::min(1.0f, drag * dt);
        p.vy -= p.vy * std::min(1.0f, drag * dt);
        p.x += p.vx * dt;
        p.y += p.vy * dt + move;
    }
    for (Pop& p : pops) {
        if (!p.alive) continue;
        p.age += dt;
        p.y -= 13.0f * dt;
        if (p.age >= kPopTime) p.alive = false;
    }
    for (Mark& m : marks) m.y += move;
    // Следы шин на заносе.
    if (skidT > 0.0f && speed > 10.0f) {
        const float wobble = std::sin(time * 26.0f) * 1.5f;
        marks[static_cast<size_t>(markHead)] = Mark{px - 3.0f + wobble, kPlayerY + 6.0f};
        markHead = (markHead + 1) % static_cast<int>(marks.size());
        marks[static_cast<size_t>(markHead)] = Mark{px + 3.0f + wobble, kPlayerY + 6.0f};
        markHead = (markHead + 1) % static_cast<int>(marks.size());
    }
    // Пыль из-под колёс на траве.
    if (offRoad && speed > 15.0f && crashT <= 0.0f) {
        dustClock -= dt;
        if (dustClock <= 0.0f) {
            dustClock = 0.06f;
            emit(FxKind::Dust, px + fx.range(-4.0f, 4.0f), kPlayerY + 7.0f, fx.range(-12.0f, 12.0f),
                 fx.range(5.0f, 20.0f), fx.range(0.3f, 0.5f), fx.chance(0.5f) ? kC3 : kC4);
        }
    }
}

// Убирает подобранные/разбитые и уехавшие за экран объекты (порядок сохраняется).
void ConsoleMiniGame::State::removeDead() {
    size_t w = 0;
    for (size_t i = 0; i < objs.size(); ++i) {
        const MiniObject& o = objs[i];
        bool keep = o.alive && o.y - o.h * 0.5f < static_cast<float>(kH) + 2.0f;
        if (o.type == MiniObjectType::Car && o.y + o.h * 0.5f < -30.0f) keep = false;
        if (!keep) continue;
        if (w != i) {
            objs[w] = objs[i];
            extra[w] = extra[i];
        }
        ++w;
    }
    objs.resize(w);
    extra.resize(w);
}

// ============================================================================
//  Отрисовка экрана мини-игры
// ============================================================================
void ConsoleMiniGame::State::render(Canvas& c) const {
    c.resetClip();
    c.setOffset(0, 0);
    c.resetStencilModes();
    if (time < kBootTime) {
        drawBoot(c);
        return;
    }
    RenderCtx rc;
    rc.fd = static_cast<long long>(std::floor(dist));
    rc.frac = static_cast<float>(dist - static_cast<double>(rc.fd));

    drawRoad(c, rc);
    drawDecor(c, rc);
    drawMarks(c, rc);
    drawObjects(c, rc);
    drawParticles(c, rc);
    drawPlayer(c);
    drawBoom(c, rc);
    drawPops(c);
    drawHud(c);
    drawGo(c);

    // «Шторка»: заставка уходит горизонтальными жалюзи с каскадом сверху вниз.
    if (time < kBootTime + kWipeTime) {
        const float w = (time - kBootTime) / kWipeTime;
        for (int band = 0; band * 8 < kH; ++band) {
            const float local = w * 1.6f - static_cast<float>(band) / static_cast<float>(kH / 8) * 0.6f;
            const int shown = clampi(static_cast<int>(local * 8.0f), 0, 8);
            if (shown >= 8) continue;
            c.setClip(0, band * 8 + shown, kW, 8 - shown);
            drawBoot(c);
        }
        c.resetClip();
    }

    // Вспышка при аварии: негатив палитры (1–2 кадра).
    if (flashT > 0.0f) {
        uint32_t* p = c.data();
        const size_t n = static_cast<size_t>(c.width()) * static_cast<size_t>(c.height());
        for (size_t i = 0; i < n; ++i) {
            const uint32_t v = p[i];
            p[i] = v == kC1 ? kC4 : v == kC4 ? kC1 : v == kC2 ? kC3 : v == kC3 ? kC2 : v;
        }
    }
}

// Заставка: логотип «падает» сверху, затем название картриджа.
void ConsoleMiniGame::State::drawBoot(Canvas& c) const {
    c.fillRect(0, 0, kW, kH, kC4);
    const float t = std::min(time, kBootTime);
    // Падение с затухающим отскоком.
    const float u = saturate(t / 0.6f);
    float drop = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
    if (u >= 1.0f) drop = 1.0f;
    const float bounce = u > 0.7f ? std::sin((u - 0.7f) / 0.3f * kPi) * 0.06f : 0.0f;
    const float logoY = lerpf(-18.0f, 40.0f, drop) - bounce * 40.0f;
    const float logoX = 80.0f - static_cast<float>(kSprLogo.w);
    drawLogo(c, logoX + 2.0f, logoY + 2.0f, 2.0f, kC3);
    drawLogo(c, logoX, logoY, 2.0f, kC1);
    if (t > 0.6f) {
        // Полоска под логотипом «заливается» слева направо.
        const int len = clampi(static_cast<int>((t - 0.6f) / 0.2f * 90.0f), 0, 90);
        c.fillRect(35, 59, len, 2, kC2);
    }
    if (t > 0.68f) {
        // Название картриджа печатается по буквам; жирность — двойным проходом.
        const int chars = static_cast<int>((t - 0.68f) / 0.04f);
        const int tx = 80 - font::textWidth("FUEL RUN", 2) / 2 - 1;
        font::drawText(c, tx + 1, 73, "FUEL RUN", kC3, 2, chars);
        font::drawText(c, tx, 72, "FUEL RUN", kC1, 2, chars);
        font::drawText(c, tx + 1, 72, "FUEL RUN", kC1, 2, chars);
    }
    if (t > 0.9f) {
        // Маленькая машинка проезжает под названием.
        const int carX = static_cast<int>(lerpf(-8.0f, 168.0f, saturate((t - 0.9f) / 0.5f)));
        drawSpr(c, kSprCar, carX, 108, kPal, 1);
        c.hline(0, kW - 1, 116, kC3);
    }
}

void ConsoleMiniGame::State::drawRoad(Canvas& c, const RenderCtx& rc) const {
    uint32_t* pixels = c.data();
    const float hw = halfWidth;
    for (int y = kHudH; y < kH; ++y) {
        const long long wyl = rc.fd + (kH - y);
        const uint32_t wy = static_cast<uint32_t>(wyl);
        const float cen = roadCenterWorld(static_cast<double>(wyl));
        const int left = roundi(cen - hw), right = roundi(cen + hw) - 1;
        const int kerbL = left + static_cast<int>(kKerbW), kerbR = right - static_cast<int>(kKerbW);
        const uint32_t kerb = ((wy / 4u) & 1u) ? kC1 : kC4;
        const bool dash = (wy % 18u) < 8u;
        const int laneA = roundi(cen - hw / 3.0f), laneB = roundi(cen + hw / 3.0f);
        uint32_t* row = pixels + static_cast<size_t>(y) * static_cast<size_t>(kW);
        for (int x = 0; x < kW; ++x) {
            uint32_t col;
            if (x < left || x > right) col = grassPixel(x, wy);
            else if (x < kerbL || x > kerbR) col = kerb;
            else if (dash && (x == laneA || x == laneB)) col = kC2;
            else col = roadPixel(x, wy);
            row[x] = col;
        }
    }
}

// Придорожные кусты, деревья, цветы и столбики. Положение — хеш от номера
// «слота» в мире, поэтому декор едет вместе с дорогой без хранения состояния.
void ConsoleMiniGame::State::drawDecor(Canvas& c, const RenderCtx& rc) const {
    constexpr int kSlot = 22;
    c.setClip(0, kHudH, kW, kH - kHudH);
    const long long wLow = rc.fd - 16, wHigh = rc.fd + (kH - kHudH) + 16;
    // Светоотражающие столбики вдоль бордюра.
    for (long long k = floorDiv(wLow, 40); k <= floorDiv(wHigh, 40); ++k) {
        const long long wy = k * 40;
        const int y = static_cast<int>(rc.fd + kH - wy);
        const float cen = roadCenterWorld(static_cast<double>(wy));
        for (int side = -1; side <= 1; side += 2) {
            const int x = roundi(cen + static_cast<float>(side) * (halfWidth + 3.0f)) - (side < 0 ? 1 : 0);
            c.fillRect(x, y, 2, 3, kC1);
            c.fillRect(x, y, 2, 1, kC4);
        }
    }
    for (long long k = floorDiv(wLow, kSlot); k <= floorDiv(wHigh, kSlot); ++k) {
        for (int side = -1; side <= 1; side += 2) {
            const uint32_t h = ihash(static_cast<uint32_t>(k) * 0x9E3779B1u + (side > 0 ? 0x51ED27u : 0x2C1B3Cu));
            const long long wy = k * kSlot + static_cast<long long>((h >> 8) % 9u);
            const int y = static_cast<int>(rc.fd + kH - wy);
            const float edge = roadCenterWorld(static_cast<double>(wy)) + static_cast<float>(side) * halfWidth;
            const int kind = static_cast<int>(h % 100u);
            const int spread = static_cast<int>((h >> 12) % 23u);
            const float sd = static_cast<float>(side);
            if (kind < 26) {
                continue;
            } else if (kind < 50) {
                // Куст: два-три тёмных комка.
                const int x = roundi(edge + sd * static_cast<float>(6 + spread));
                c.fillCircle(x, y, 2, kC1);
                c.fillCircle(x + 3, y + 1, 2, kC1);
                if (h & 0x10000u) c.fillCircle(x + 1, y - 2, 2, kC1);
                c.plot(x - 1, y - 1, kC3);
                c.plot(x + 2, y, kC2);
            } else if (kind < 62) {
                // Ель сверху: тёмная многолучевая «звезда» лап со светлой макушкой.
                const float r = 5.0f + static_cast<float>((h >> 20) % 3u);
                const float x = edge + sd * (r + 5.0f + static_cast<float>(spread));
                const float yc = static_cast<float>(y) + 0.5f;
                Vec2 star[16];
                for (int layer = 0; layer < 2; ++layer) {
                    const float ro = layer == 0 ? r : r * 0.6f, ri = layer == 0 ? r * 0.55f : r * 0.3f;
                    const float rot = static_cast<float>(h % 7u) * 0.3f + static_cast<float>(layer) * 0.4f;
                    for (int p = 0; p < 16; ++p) {
                        const float ang = static_cast<float>(p) * (kTau / 16.0f) + rot;
                        const float rad = (p & 1) ? ri : ro;
                        star[p] = Vec2{x + 0.5f + std::cos(ang) * rad, yc + std::sin(ang) * rad};
                    }
                    if (layer == 0) {
                        Vec2 shadow[16];
                        for (int p = 0; p < 16; ++p) shadow[p] = Vec2{star[p].x + 2.0f, star[p].y + 2.0f};
                        c.fillPolygon(shadow, 16, kC1);
                    }
                    c.fillPolygon(star, 16, layer == 0 ? kC1 : kC2);
                }
                c.plot(roundi(x), y, kC3);
                c.plot(roundi(x) - 1, y - 1, kC3);
            } else if (kind < 78) {
                // Дерево: шар кроны, освещённый сверху-слева, с текстурой листвы.
                const int r = 4 + static_cast<int>((h >> 20) % 3u);
                const int x = roundi(edge + sd * static_cast<float>(r + 5 + spread));
                c.fillCircle(x + 2, y + 2, r, kC1);
                c.fillCircle(x, y, r, kC1);
                const int lr = r - 1;
                for (int dy = -lr; dy <= lr; ++dy) {
                    for (int dx = -lr; dx <= lr; ++dx) {
                        // Освещённая часть кроны — смещённый круг; внутри — тёмные «листья».
                        const int ex = dx + 1, ey = dy + 1;
                        if (dx * dx + dy * dy > lr * lr || ex * ex + ey * ey > (lr - 1) * (lr - 1)) continue;
                        const uint32_t leaf = ihash(h + static_cast<uint32_t>((dy + 8) * 17 + dx + 8));
                        c.plot(x + dx - 1, y + dy - 1, (leaf & 3u) == 0u ? kC1 : kC2);
                    }
                }
                c.plot(x - r / 2 - 1, y - r / 2, kC3);
                c.plot(x - r / 2, y - r / 2 - 1, kC3);
                c.plot(x - r / 2 + 2, y - r / 2, kC3);
            } else if (kind < 90) {
                // Цветы: светлые точки.
                const int x = roundi(edge + sd * static_cast<float>(4 + spread));
                c.plot(x, y, kC4);
                c.plot(x + 3, y + 1, kC4);
                c.plot(x + 1, y + 3, kC4);
                c.plot(x + 1, y + 4, kC1);
                c.plot(x + 3, y + 2, kC1);
            } else {
                // Высокая трава.
                const int x = roundi(edge + sd * static_cast<float>(5 + spread));
                for (int i = 0; i < 3; ++i) {
                    c.plot(x + i * 2, y - (i == 1 ? 1 : 0), kC1);
                    c.plot(x + i * 2, y + 1, kC1);
                    c.plot(x + i * 2 + 1, y + 2, kC1);
                }
            }
        }
    }
    c.resetClip();
}

void ConsoleMiniGame::State::drawMarks(Canvas& c, const RenderCtx& rc) const {
    for (const Mark& m : marks) {
        if (m.y < static_cast<float>(kHudH) - 2.0f || m.y > static_cast<float>(kH) + 2.0f) continue;
        const int x = roundi(m.x), y = roundi(m.y - rc.frac);
        c.plot(x, y, kC2);
        c.plot(x, y + 1, kC2);
    }
}

void ConsoleMiniGame::State::drawObjects(Canvas& c, const RenderCtx& rc) const {
    c.setClip(0, kHudH, kW, kH - kHudH);
    // Слой 1: масляные пятна (лежат на асфальте).
    for (size_t i = 0; i < objs.size(); ++i) {
        const MiniObject& o = objs[i];
        if (o.type != MiniObjectType::Oil) continue;
        const int x = roundi(o.x), y = roundi(o.y - rc.frac);
        drawSpr(c, kSprOil, x, y, kPal);
        // Блик на свежем пятне медленно «переливается».
        if (!extra[i].used && std::fmod(time + extra[i].phase, 1.4f) < 0.7f) c.plot(x + 2, y - 2, kC3);
    }
    // Слой 2: камни и конусы (с тенью).
    for (size_t i = 0; i < objs.size(); ++i) {
        const MiniObject& o = objs[i];
        if (o.type != MiniObjectType::Rock && o.type != MiniObjectType::Cone) continue;
        const Sprite& s = spriteFor(o.type, 0);
        const int x = roundi(o.x), y = roundi(o.y - rc.frac);
        drawSpr(c, s, x + 1, y + 1, kPalShadow);
        drawSpr(c, s, x, y, kPal);
    }
    // Слой 3: бонусы — тень на земле, сам предмет покачивается и мигает.
    for (size_t i = 0; i < objs.size(); ++i) {
        const MiniObject& o = objs[i];
        if (!isPickup(o.type)) continue;
        const Sprite& s = spriteFor(o.type, 0);
        const float ph = time + extra[i].phase;
        const int bob = roundi(std::sin(ph * 6.0f) * 1.4f);
        const int x = roundi(o.x), y = roundi(o.y - rc.frac);
        c.fillRect(x - s.w / 2 + 1, y + s.h / 2 + 1, s.w - 2, 1, kC2);
        const bool blink = std::fmod(ph, 0.6f) < 0.1f;
        drawSpr(c, s, x, y - 1 + bob, blink ? kPalBlink : kPal);
    }
    // Слой 4: попутные машины (с тенью и поворотником).
    for (size_t i = 0; i < objs.size(); ++i) {
        const MiniObject& o = objs[i];
        if (o.type != MiniObjectType::Car) continue;
        const ObjExtra& e = extra[i];
        const Sprite& s = spriteFor(o.type, e.variant);
        const int x = roundi(o.x), y = roundi(o.y - rc.frac);
        const float dl = e.laneTarget - e.lane;
        const int shear = std::fabs(dl) > 2.0f && e.signal <= 0.0f ? (dl < 0.0f ? -1 : 1) : 0;
        drawSpr(c, s, x + 1, y + 1, kPalShadow, 0, shear);
        drawSpr(c, s, x, y, kPal, 0, shear);
        if (std::fabs(dl) > 0.5f && std::fmod(time, 0.24f) < 0.12f) {
            const int sx = dl < 0.0f ? x - s.w / 2 : x + s.w / 2 - 1;
            c.plot(sx, y - s.h / 2 + 1, kC4);
            c.plot(sx, y + s.h / 2 - 2, kC4);
        }
    }
    c.resetClip();
}

void ConsoleMiniGame::State::drawParticles(Canvas& c, const RenderCtx& rc) const {
    c.setClip(0, kHudH, kW, kH - kHudH);
    for (const Particle& p : parts) {
        if (!p.alive) continue;
        const float t = p.age / p.life;
        const int x = roundi(p.x), y = roundi(p.y - rc.frac);
        switch (p.kind) {
        case FxKind::Debris:
            c.plot(x, y, p.color);
            if (t < 0.5f) c.plot(x + 1, y, p.color);
            break;
        case FxKind::Smoke:
        case FxKind::Dust: {
            // Клуб растёт и «растворяется» по матрице Байера.
            const float maxR = p.kind == FxKind::Smoke ? 5.0f : 2.0f;
            const int r = std::max(1, roundi(lerpf(1.0f, maxR, std::sqrt(t))));
            const int level = clampi(static_cast<int>((1.0f - t) * 17.0f), 0, 16);
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy > r * r) continue;
                    const int sx = x + dx, sy = y + dy;
                    if (kBayer4[sy & 3][sx & 3] >= level) continue;
                    c.plot(sx, sy, p.color);
                }
            }
            break;
        }
        case FxKind::Spark:
            if (t < 0.5f) {
                c.plot(x - 1, y, p.color);
                c.plot(x + 1, y, p.color);
                c.plot(x, y - 1, p.color);
                c.plot(x, y + 1, p.color);
            }
            c.plot(x, y, p.color);
            break;
        }
    }
    c.resetClip();
}

void ConsoleMiniGame::State::drawPlayer(Canvas& c) const {
    // Во время неуязвимости машина мигает.
    if (invulnT > 0.0f && std::fmod(time, 0.18f) >= 0.11f) return;
    const int x = roundi(px) + jitterX, y = roundi(kPlayerY) + jitterY;
    int orient = 0;
    int shear = tilt < -0.45f ? -1 : (tilt > 0.45f ? 1 : 0);
    if (crashT > 0.0f) {
        // Авария: машину крутит два оборота с замедлением, потом она замирает.
        const float t = saturate((kCrashTime - crashT) / 0.65f);
        const float turns = 8.0f * (1.0f - (1.0f - t) * (1.0f - t));
        orient = static_cast<int>(turns) & 3;
        shear = 0;
    }
    c.setClip(0, kHudH, kW, kH - kHudH);
    if (!offRoad) drawSpr(c, kSprCar, x + 1, y + 1, kPalShadow, orient, shear);
    drawSpr(c, kSprCar, x, y, kPal, orient, shear);
    c.resetClip();
}

// «Бах!»: звезда-вспышка (тёмный контур, светлая сердцевина), затем разлетающиеся искры.
void ConsoleMiniGame::State::drawBoom(Canvas& c, const RenderCtx& rc) const {
    if (boomT <= 0.0f) return;
    const float a = 1.0f - boomT / kBoomTime;
    const float bx = boomX + 0.5f, by = boomY - rc.frac + 0.5f;
    c.setClip(0, kHudH, kW, kH - kHudH);
    if (a < 0.42f) {
        const float grow = std::min(1.0f, a / 0.12f);
        const float rot = a * 2.0f;
        for (int layer = 0; layer < 2; ++layer) {
            const float rOut = (layer == 0 ? 10.0f : 7.0f) * (0.55f + 0.45f * grow);
            const float rIn = (layer == 0 ? 4.5f : 2.8f) * (0.6f + 0.4f * grow);
            Vec2 pts[16];
            for (int k = 0; k < 16; ++k) {
                const float ang = static_cast<float>(k) * (kTau / 16.0f) + rot;
                const float rad = (k & 1) ? rIn : rOut * ((k & 2) ? 0.8f : 1.0f);
                pts[k] = Vec2{bx + std::cos(ang) * rad, by + std::sin(ang) * rad};
            }
            const bool flick = std::fmod(a, 0.1f) < 0.05f;
            c.fillPolygon(pts, 16, layer == 0 ? kC1 : (flick ? kC4 : kC3));
        }
        c.fillCircle(roundi(bx - 0.5f), roundi(by - 0.5f), 1, kC4);
    } else {
        // Искры разлетаются кольцом и гаснут.
        const float t = (a - 0.42f) / 0.58f;
        const float r = 9.0f + t * 9.0f;
        for (int k = 0; k < 8; ++k) {
            if (t > 0.6f && ((k + static_cast<int>(t * 20.0f)) & 1)) continue;
            const float ang = static_cast<float>(k) * (kTau / 8.0f) + 0.39f;
            const int x = roundi(bx - 0.5f + std::cos(ang) * r), y = roundi(by - 0.5f + std::sin(ang) * r);
            c.plot(x, y, kC4);
            c.plot(x - 1, y, kC1);
            c.plot(x + 1, y, kC1);
            c.plot(x, y - 1, kC1);
            c.plot(x, y + 1, kC1);
        }
    }
    c.resetClip();
}

void ConsoleMiniGame::State::drawPops(Canvas& c) const {
    for (const Pop& p : pops) {
        if (!p.alive || !p.text) continue;
        if (p.age > kPopTime - 0.3f && std::fmod(p.age, 0.1f) < 0.05f) continue; // гаснет миганием
        const int tw = font::textWidth(p.text);
        const int x = clampi(roundi(p.x) - tw / 2, 2, kW - 2 - tw);
        const int y = std::max(kHudH + 2, roundi(p.y) - 12);
        drawOutlinedText(c, x, y, p.text, kC1, kC4);
    }
}

void ConsoleMiniGame::State::drawHud(Canvas& c) const {
    c.fillRect(0, 0, kW, kHudH, kC1);
    c.hline(0, kW - 1, kHudH - 1, kC2);
    char buf[16];
    // Слева — дистанция (очки).
    drawSpr(c, kSprHudFlag, 2 + kSprHudFlag.w / 2, 2 + kSprHudFlag.h / 2, kPal);
    const int sc = std::min(99999, static_cast<int>(dist / 10.0));
    std::snprintf(buf, sizeof(buf), "%05d", sc);
    font::drawText(c, 10, 2, buf, kC4);
    // Справа — канистры.
    std::snprintf(buf, sizeof(buf), "x%02d", std::min(99, fuelCount));
    const int tw = font::textWidth(buf);
    font::drawText(c, kW - 2 - tw, 2, buf, kC4);
    drawSpr(c, kSprHudCan, kW - 2 - tw - 3 - kSprHudCan.w / 2, 1 + kSprHudCan.h / 2, kPal);
    // В центре — значок подобранного предмета или спидометр.
    if (itemFlashT > 0.0f) {
        const float age = kItemFlashTime - itemFlashT;
        if (age > 0.6f || std::fmod(age, 0.16f) < 0.1f) {
            if (itemFlashT > 0.25f || std::fmod(itemFlashT, 0.1f) < 0.05f) {
                c.fillRect(67, 0, 26, kHudH - 1, kC3);
                drawSpr(c, spriteFor(itemFlashType, 0), 80, 5, kPal);
            }
        }
    } else {
        const int filled = clampi(roundi(speed / kSpeedMax * 8.0f), 0, 8);
        for (int i = 0; i < 8; ++i) {
            const int bh = 2 + i * 5 / 7;
            c.fillRect(57 + i * 6, 8 - bh, 4, bh, i < filled ? kC4 : kC2);
        }
    }
}

// «GO!» на старте (крупно, с обводкой; после старта мигает и гаснет).
void ConsoleMiniGame::State::drawGo(Canvas& c) const {
    if (time < kBootTime + kWipeTime * 0.5f || time >= kGoEnd) return;
    if (time > kIntroTime + 0.25f && std::fmod(time, 0.16f) < 0.07f) return;
    const char* text = time < kIntroTime - 0.15f ? "READY" : "GO!";
    const int tw = font::textWidth(text, 2);
    const int x = 80 - tw / 2;
    font::drawText(c, x + 3, 53, text, kC2, 2); // падающая тень
    drawOutlinedText(c, x, 50, text, kC1, kC4, 2);
}

// ============================================================================
//  Публичный интерфейс
// ============================================================================
ConsoleMiniGame::ConsoleMiniGame() : st_(std::make_unique<State>()) {
    st_->objs.reserve(kMaxObjects + 8);
    st_->extra.reserve(kMaxObjects + 8);
    st_->events.reserve(16);
    st_->spans.reserve(kMaxObjects + 8);
    reset(1u);
}

ConsoleMiniGame::~ConsoleMiniGame() = default;

void ConsoleMiniGame::reset(uint32_t seed) {
    State& s = *st_;
    // Сохраняем выделенную память векторов, всё остальное — по умолчанию.
    auto objs = std::move(s.objs);
    auto extra = std::move(s.extra);
    auto events = std::move(s.events);
    auto spans = std::move(s.spans);
    s = State();
    s.objs = std::move(objs);
    s.extra = std::move(extra);
    s.events = std::move(events);
    s.spans = std::move(spans);
    s.objs.clear();
    s.extra.clear();
    s.events.clear();
    s.spans.clear();
    s.rng.reseed(seed);
    s.fx.reseed(ihash(seed ^ 0xA5A5A5A5u));
    s.px = s.centerAt(kPlayerY);
}

void ConsoleMiniGame::update(float dt, int steer, bool attended, float difficulty, bool hasCamera) {
    if (!(dt > 0.0f)) return; // отсекает и NaN
    dt = std::min(dt, 0.25f);
    difficulty = std::isfinite(difficulty) ? saturate(difficulty) : 0.0f;
    // Подшаги не длиннее 1/60 с: столкновения не «проскакивают» на больших dt.
    const int n = std::max(1, static_cast<int>(std::ceil(dt / kSubStep - 1e-3f)));
    const float h = dt / static_cast<float>(n);
    for (int i = 0; i < n; ++i) st_->step(h, steer, attended, difficulty, hasCamera);
}

void ConsoleMiniGame::render(Canvas& screen) const {
    if (screen.width() != kW || screen.height() != kH) screen.resize(kW, kH);
    st_->render(screen);
}

std::vector<MiniEvent> ConsoleMiniGame::takeEvents() {
    if (st_->events.empty()) return {};
    std::vector<MiniEvent> out(st_->events.begin(), st_->events.end());
    st_->events.clear();
    return out;
}

float ConsoleMiniGame::playerX() const { return st_->px; }
float ConsoleMiniGame::playerY() const { return kPlayerY; }
float ConsoleMiniGame::roadCenterAt(float screenY) const { return st_->centerAt(screenY); }
float ConsoleMiniGame::roadHalfWidth() const { return st_->halfWidth; }
float ConsoleMiniGame::speed() const { return st_->speed; }
bool ConsoleMiniGame::crashed() const { return st_->crashT > 0.0f; }
int ConsoleMiniGame::score() const { return static_cast<int>(st_->dist / 10.0); }
int ConsoleMiniGame::fuelCollected() const { return st_->fuelCount; }
int ConsoleMiniGame::crashes() const { return st_->crashCount; }
const std::vector<MiniObject>& ConsoleMiniGame::objects() const { return st_->objs; }

// ============================================================================
//  Корпус консоли «GAMEKID»
//
//  Всё задано в «единицах устройства» (пиксели при scale = 1) относительно
//  центра экрана: корпус 268x176, экран 160x144, стекло вокруг экрана с
//  логотипом снизу, крестовина слева, кнопки A/B справа. Корпус и свечение
//  считаются попиксельно (скруглённые формы, фаска, блики, зелёный отсвет
//  экрана) и кэшируются для масштаба; мелкие детали — примитивами в масштабе.
// ============================================================================
namespace {

constexpr float kBodyHX = 134.0f;   // половина ширины корпуса
constexpr float kBodyHY = 88.0f;    // половина высоты
constexpr float kBodyR = 42.0f;     // радиус скругления «крыльев»
constexpr float kGlassCY = 2.5f;    // стекло опущено: внизу на нём логотип
constexpr float kGlassHX = 89.0f;
constexpr float kGlassHY = 80.5f;
constexpr float kGlassR = 5.0f;
constexpr float kScreenHX = 80.0f;
constexpr float kScreenHY = 72.0f;
constexpr float kHalo = 30.0f;      // свечение вокруг корпуса

constexpr uint32_t kPlastic = rgb(47, 47, 56);
constexpr uint32_t kGlass = rgb(16, 17, 21);
constexpr uint32_t kLcdFrame = rgb(6, 7, 8);
constexpr uint32_t kGlowCol = rgb(100, 190, 100);
constexpr uint32_t kDpadCol = rgb(24, 24, 29);
constexpr uint32_t kDpadHi = rgb(84, 84, 98);
constexpr uint32_t kDpadPressed = rgb(11, 11, 13);
constexpr uint32_t kRecess = rgb(30, 30, 37);
constexpr uint32_t kRecessRim = rgb(70, 70, 82);
constexpr uint32_t kBtnCol = rgb(128, 34, 70);
constexpr uint32_t kBtnHi = rgb(200, 96, 134);
constexpr uint32_t kBtnShadow = rgb(18, 10, 14);
constexpr uint32_t kPillCol = rgb(28, 28, 34);
constexpr uint32_t kPillHi = rgb(86, 86, 100);
constexpr uint32_t kSlotCol = rgb(13, 13, 16);
constexpr uint32_t kLabelCol = rgb(96, 96, 112);
constexpr uint32_t kLogoCol = rgb(128, 130, 146);
constexpr uint32_t kLedCol = rgb(255, 52, 40);

// Знаковое расстояние до скруглённого прямоугольника (отрицательно внутри).
// Корень нужен только в зонах скругления углов.
inline float sdRoundBox(float u, float v, float hx, float hy, float r) {
    const float qx = std::fabs(u) - (hx - r);
    const float qy = std::fabs(v) - (hy - r);
    if (qx > 0.0f && qy > 0.0f) return std::sqrt(qx * qx + qy * qy) - r;
    return std::max(qx, qy) - r;
}

// exp(-x) по таблице с линейной интерполяцией (x >= 0) — для мягких спадов
// свечения в попиксельном цикле, где настоящая экспонента слишком дорога.
float expNeg(float x) {
    static const std::array<float, 257> table = [] {
        std::array<float, 257> t{};
        for (size_t i = 0; i < t.size(); ++i) t[i] = std::exp(-static_cast<float>(i) / 16.0f);
        return t;
    }();
    if (!(x > 0.0f)) return 1.0f;
    const float f = x * 16.0f;
    if (f >= 256.0f) return 0.0f;
    const int i = static_cast<int>(f);
    const float fr = f - static_cast<float>(i);
    const size_t k = static_cast<size_t>(i);
    return table[k] + (table[k + 1] - table[k]) * fr;
}

// Параметры одного вызова drawHandheld: перевод единиц устройства в пиксели
// и освещение деталей.
struct Device {
    Canvas* out = nullptr;
    float cx = 0.0f;
    float cy = 0.0f;
    float s = 1.0f;
    float lit = 1.0f;
    float glow = 0.0f;

    float X(float u) const { return cx + u * s; }
    float Y(float v) const { return cy + v * s; }
    // Отсвет экрана на точке корпуса (убывает от края экрана).
    float glowAt(float u, float v) const {
        const float d = std::max(0.0f, sdRoundBox(u, v, kScreenHX, kScreenHY, 0.0f));
        return glow * 0.4f * std::exp(-d / 16.0f);
    }
    // Цвет детали с учётом освещённости и отсвета экрана.
    uint32_t shade(uint32_t c, float u, float v) const {
        return addColor(scaleColor(c, lit), scaleColor(kGlowCol, glowAt(u, v) * 0.5f));
    }
    void rect(float u0, float v0, float u1, float v1, uint32_t c) const {
        const int x0 = roundi(X(u0)), y0 = roundi(Y(v0));
        const int x1 = std::max(x0 + 1, roundi(X(u1))), y1 = std::max(y0 + 1, roundi(Y(v1)));
        out->fillRect(x0, y0, x1 - x0, y1 - y0, c);
    }
    void disc(float u, float v, float r, uint32_t c) const {
        const int rr = std::max(1, roundi(r * s));
        out->fillCircle(roundi(X(u)), roundi(Y(v)), rr, c);
    }
    // Капсула (кнопки START/SELECT) — прямоугольник со скруглёнными торцами.
    void pill(float u, float v, float hw, float hh, uint32_t c) const {
        rect(u - hw + hh, v - hh, u + hw - hh, v + hh, c);
        disc(u - hw + hh, v, hh, c);
        disc(u + hw - hh, v, hh, c);
    }
};

// Попиксельная «карта» корпуса для одного масштаба: геометрия (скругления,
// фаска, шов, стекло, рамка ЖК) и доля отсвета экрана не зависят от
// освещения, поэтому считаются один раз и кэшируются; каждый кадр остаётся
// только дешёвое смешивание с текущими brightness и screenGlow.
// Координаты карты — относительно центра экрана (cx, cy), так что смещение
// холста (тряска машины) и перенос консоли кэш не сбрасывают.
struct ShellMap {
    float scale = -1.0f;
    int x0 = 0, y0 = 0;          // левый верхний угол карты относительно (cx, cy)
    int w = 0, h = 0;
    int sx0 = 0, sy0 = 0;        // экран относительно (cx, cy) — его пиксели пропускаются
    int sw = 0, sh = 0;
    std::vector<uint8_t> kind;   // 0 — пусто, 1 — корпус/стекло, 2 — свечение вокруг
    std::vector<uint32_t> base;  // цвет пластика/стекла с затенением (при освещённости 1)
    std::vector<uint8_t> glow;   // доля зелёного отсвета экрана, 0..255
    unsigned lastUse = 0;
};

void buildShellMap(ShellMap& m, float s) {
    const float inv = 1.0f / s;
    m.scale = s;
    m.x0 = static_cast<int>(std::floor((-kBodyHX - kHalo) * s));
    m.y0 = static_cast<int>(std::floor((-kBodyHY - kHalo) * s));
    m.w = static_cast<int>(std::ceil((kBodyHX + kHalo) * s)) - m.x0 + 1;
    m.h = static_cast<int>(std::ceil((kBodyHY + kHalo) * s)) - m.y0 + 1;
    m.sw = std::max(1, roundi(static_cast<float>(cfg::kConsoleW) * s));
    m.sh = std::max(1, roundi(static_cast<float>(cfg::kConsoleH) * s));
    m.sx0 = -m.sw / 2;
    m.sy0 = -m.sh / 2;
    const size_t n = static_cast<size_t>(m.w) * static_cast<size_t>(m.h);
    m.kind.assign(n, 0u);
    m.base.assign(n, 0u);
    m.glow.assign(n, 0u);
    // Свет падает сверху-слева (салон), фаска шириной ~2.6 единицы.
    const float lx = -0.32f, ly = -0.95f;
    const float bevelW = 2.6f;
    auto g8 = [](float g) { return static_cast<uint8_t>(clampi(static_cast<int>(g * 255.0f), 0, 255)); };
    for (int j = 0; j < m.h; ++j) {
        const int ry = m.y0 + j;
        const float v = (static_cast<float>(ry) + 0.5f) * inv;
        const bool rowHasScreen = ry >= m.sy0 && ry < m.sy0 + m.sh;
        const float vGrad = 1.0f + 0.14f * (-v / kBodyHY);         // верх корпуса светлее
        const float glassGrad = 1.0f + 0.25f * (-v / kBodyHY);
        const float vv = v / kBodyHY;
        const float grooveU = 96.0f + 6.0f * vv * vv;               // шов «крыла»
        const bool grooveRow = std::fabs(v) < kBodyHY - 5.0f;
        for (int i = 0; i < m.w; ++i) {
            const int rx = m.x0 + i;
            if (rowHasScreen && rx >= m.sx0 && rx < m.sx0 + m.sw) continue; // экран рисуется отдельно
            const size_t idx = static_cast<size_t>(j) * static_cast<size_t>(m.w) + static_cast<size_t>(i);
            const float u = (static_cast<float>(rx) + 0.5f) * inv;
            const float sd = sdRoundBox(u, v, kBodyHX, kBodyHY, kBodyR);
            if (sd > kHalo) continue;
            const float dScreen = sdRoundBox(u, v, kScreenHX, kScreenHY, 0.0f);
            if (sd > 0.0f) {
                // Мягкое свечение экрана на окружающее.
                const float f = 1.0f - sd / kHalo;
                const uint8_t g = g8(0.26f * f * f * expNeg((dScreen - 60.0f) / 60.0f));
                if (g > 0u) {
                    m.kind[idx] = 2u;
                    m.glow[idx] = g;
                }
                continue;
            }
            m.kind[idx] = 1u;
            const float dGlass = sdRoundBox(u, v - kGlassCY, kGlassHX, kGlassHY, kGlassR);
            if (dGlass <= 0.0f) {
                if (dScreen * s < 1.0f) {
                    m.base[idx] = kLcdFrame; // тонкая чёрная рамка ЖК-матрицы
                } else {
                    // Стекло: чуть светлее к верху и по самой кромке; у экрана — сильный отсвет.
                    float k = glassGrad;
                    if (-dGlass * s < 1.0f) k *= 1.5f;
                    m.base[idx] = scaleColor(kGlass, k);
                    m.glow[idx] = g8(0.75f * expNeg(dScreen / 3.5f) + 0.22f * expNeg(dScreen / 20.0f));
                }
                continue;
            }
            const float depth = -sd;
            const float t = std::min(1.0f, depth / 12.0f);
            float k = vGrad * (0.78f + 0.22f * t * t * (3.0f - 2.0f * t)); // скругление к краю
            if (depth < bevelW) {
                // Нормаль к краю (численно) -> блик сверху, тень снизу.
                const float e = 0.5f;
                float nx = sdRoundBox(u + e, v, kBodyHX, kBodyHY, kBodyR) - sdRoundBox(u - e, v, kBodyHX, kBodyHY, kBodyR);
                float ny = sdRoundBox(u, v + e, kBodyHX, kBodyHY, kBodyR) - sdRoundBox(u, v - e, kBodyHX, kBodyHY, kBodyR);
                const float len = std::sqrt(nx * nx + ny * ny);
                if (len > 1e-5f) { nx /= len; ny /= len; }
                const float ndl = nx * lx + ny * ly;
                k += (ndl > 0.0f ? 0.95f : 0.45f) * ndl * (1.0f - depth / bevelW);
            }
            if (grooveRow) {
                const float groove = (std::fabs(u) - grooveU) * s;
                if (groove >= -0.5f && groove < 0.5f) k *= 0.62f;
                else if (groove >= 0.5f && groove < 1.5f) k *= 1.14f;
            }
            m.base[idx] = scaleColor(kPlastic, std::max(0.0f, k));
            m.glow[idx] = g8(0.38f * expNeg(std::max(0.0f, dGlass) / 14.0f));
        }
    }
}

// Карта для масштаба s: две записи (консоль на коленях и у лица могут
// рисоваться в одном кадре), при промахе перестраивается давно не нужная.
const ShellMap& shellMapFor(float s) {
    static std::array<ShellMap, 2> cache;
    static unsigned tick = 0;
    ++tick;
    for (ShellMap& m : cache) {
        if (m.scale == s) {
            m.lastUse = tick;
            return m;
        }
    }
    ShellMap& victim = cache[0].lastUse <= cache[1].lastUse ? cache[0] : cache[1];
    buildShellMap(victim, s);
    victim.lastUse = tick;
    return victim;
}

// Корпус, стекло и свечение вокруг: смешивание карты с освещением кадра.
void drawShell(const Device& d, int cx, int cy) {
    Canvas& out = *d.out;
    const ShellMap& m = shellMapFor(d.s);
    const int ox = out.offsetX(), oy = out.offsetY();
    const int lit = static_cast<int>(d.lit * 256.0f);
    const int glow = static_cast<int>(d.glow * 256.0f);
    const int gr = colR(kGlowCol), gg = colG(kGlowCol), gb = colB(kGlowCol);
    // Видимая часть карты с учётом смещения холста.
    const int j0 = std::max(0, -oy - (cy + m.y0)), j1 = std::min(m.h, out.height() - oy - (cy + m.y0));
    const int i0 = std::max(0, -ox - (cx + m.x0)), i1 = std::min(m.w, out.width() - ox - (cx + m.x0));
    for (int j = j0; j < j1; ++j) {
        const int y = cy + m.y0 + j;
        const size_t row = static_cast<size_t>(j) * static_cast<size_t>(m.w);
        for (int i = i0; i < i1; ++i) {
            const size_t idx = row + static_cast<size_t>(i);
            const uint8_t kind = m.kind[idx];
            if (kind == 0u) continue;
            const int x = cx + m.x0 + i;
            const int g = (static_cast<int>(m.glow[idx]) * glow) >> 8;
            if (kind == 2u) {
                if (g == 0) continue;
                const uint32_t dst = out.get(x + ox, y + oy);
                out.plot(x, y, rgb(std::min(255, colR(dst) + ((gr * g) >> 8)),
                                   std::min(255, colG(dst) + ((gg * g) >> 8)),
                                   std::min(255, colB(dst) + ((gb * g) >> 8))));
                continue;
            }
            const uint32_t b = m.base[idx];
            out.plot(x, y, rgb(std::min(255, (colR(b) * lit + gr * g) >> 8),
                               std::min(255, (colG(b) * lit + gg * g) >> 8),
                               std::min(255, (colB(b) * lit + gb * g) >> 8)));
        }
    }
}

// Крестовина: углубление, крест, нажатая сторона темнее и «утоплена».
void drawDpad(const Device& d, int dpad) {
    const float cu = -106.0f, cv = -12.0f;
    d.disc(cu, cv + 1.0f, 19.0f, d.shade(kRecessRim, cu, cv + 19.0f));
    d.disc(cu, cv, 19.0f, d.shade(kRecess, cu, cv));
    const uint32_t base = d.shade(kDpadCol, cu, cv);
    const uint32_t hi = d.shade(kDpadHi, cu, cv);
    const uint32_t pressed = d.shade(kDpadPressed, cu, cv);
    const float a = 14.0f, w = 5.0f;
    // Тень креста.
    const uint32_t shadow = d.shade(rgb(14, 14, 17), cu, cv);
    d.rect(cu - a + 1.0f, cv - w + 2.0f, cu + a + 1.0f, cv + w + 2.0f, shadow);
    d.rect(cu - w + 1.0f, cv - a + 2.0f, cu + w + 1.0f, cv + a + 2.0f, shadow);
    d.rect(cu - a, cv - w, cu + a, cv + w, base);
    d.rect(cu - w, cv - a, cu + w, cv + a, base);
    // Блики по верхним граням.
    d.rect(cu - a, cv - w, cu - w, cv - w + 1.0f, hi);
    d.rect(cu + w, cv - w, cu + a, cv - w + 1.0f, hi);
    d.rect(cu - w, cv - a, cu + w, cv - a + 1.0f, hi);
    d.rect(cu - a, cv - w, cu - a + 1.0f, cv + w, hi);
    if (dpad != 0) {
        const float u0 = dpad < 0 ? cu - a : cu + w;
        const float u1 = dpad < 0 ? cu - w : cu + a;
        d.rect(u0, cv - w, u1, cv + w, pressed);
        d.rect(u0, cv + w - 1.0f, u1, cv + w, base);
    }
    // Стрелки-выемки на концах.
    if (d.s >= 0.7f) {
        const uint32_t arrow = d.shade(rgb(40, 40, 48), cu, cv);
        d.rect(cu - a + 3.0f, cv - 1.0f, cu - a + 4.0f, cv + 1.0f, arrow);
        d.rect(cu + a - 4.0f, cv - 1.0f, cu + a - 3.0f, cv + 1.0f, arrow);
        d.rect(cu - 1.0f, cv - a + 3.0f, cu + 1.0f, cv - a + 4.0f, arrow);
        d.rect(cu - 1.0f, cv + a - 4.0f, cu + 1.0f, cv + a - 3.0f, arrow);
    }
    d.disc(cu, cv, 2.5f, d.shade(rgb(18, 18, 22), cu, cv));
}

void drawButtons(const Device& d) {
    const float bu[2] = {98.0f, 119.0f};
    const float bv[2] = {-4.0f, -22.0f};
    const char* const labels[2] = {"B", "A"};
    for (int i = 0; i < 2; ++i) {
        const float u = bu[i], v = bv[i];
        d.disc(u + 0.5f, v + 1.5f, 9.0f, d.shade(kBtnShadow, u, v));
        d.disc(u, v, 8.0f, d.shade(kBtnCol, u, v));
        d.disc(u - 2.0f, v - 2.5f, 3.0f, d.shade(lerpColor(kBtnCol, kBtnHi, 0.55f), u, v));
        d.disc(u - 2.5f, v - 3.5f, 1.0f, d.shade(kBtnHi, u, v));
        if (d.s >= 0.9f)
            font::drawText(*d.out, roundi(d.X(u + 7.0f)), roundi(d.Y(v + 8.0f)), labels[i], d.shade(kLabelCol, u, v));
    }
}

void drawPillsAndGrille(const Device& d) {
    // START / SELECT — под крестовиной.
    for (int i = 0; i < 2; ++i) {
        const float u = -117.0f + static_cast<float>(i) * 21.0f, v = 40.0f;
        d.pill(u, v + 1.0f, 7.0f, 2.5f, d.shade(rgb(16, 16, 20), u, v));
        d.pill(u, v, 7.0f, 2.5f, d.shade(kPillCol, u, v));
        d.rect(u - 4.0f, v - 2.5f, u + 4.0f, v - 1.5f, d.shade(kPillHi, u, v));
    }
    // Решётка динамика: наклонные прорези справа внизу.
    const uint32_t slot = d.shade(kSlotCol, 108.0f, 46.0f);
    const uint32_t lip = d.shade(rgb(70, 70, 82), 108.0f, 46.0f);
    for (int i = 0; i < 6; ++i) {
        const float u0 = 93.0f + static_cast<float>(i) * 5.5f, v0 = 60.0f;
        const float u1 = u0 + 8.0f, v1 = 36.0f;
        const int x0 = roundi(d.X(u0)), y0 = roundi(d.Y(v0));
        const int x1 = roundi(d.X(u1)), y1 = roundi(d.Y(v1));
        d.out->drawLine(x0 + 1, y0, x1 + 1, y1, lip);
        d.out->drawLine(x0, y0, x1, y1, slot);
        if (d.s >= 0.75f) d.out->drawLine(x0 - 1, y0, x1 - 1, y1, slot);
    }
}

} // namespace

void ConsoleMiniGame::drawHandheld(Canvas& out, const Canvas& screen, int cx, int cy, float scale, int dpad,
                                   float brightness, float screenGlow) {
    if (!(scale > 0.05f) || !std::isfinite(scale)) return;
    Device d;
    d.out = &out;
    d.cx = static_cast<float>(cx);
    d.cy = static_cast<float>(cy);
    d.s = std::min(scale, 4.0f);
    d.lit = lerpf(0.09f, 1.0f, std::isfinite(brightness) ? saturate(brightness) : 0.0f);
    d.glow = std::isfinite(screenGlow) ? saturate(screenGlow) : 0.0f;

    // Прямоугольник экрана в пикселях (точно 160x144 при scale = 1).
    const int sw = std::max(1, roundi(static_cast<float>(cfg::kConsoleW) * d.s));
    const int sh = std::max(1, roundi(static_cast<float>(cfg::kConsoleH) * d.s));
    const int sx0 = cx - sw / 2, sy0 = cy - sh / 2;

    drawShell(d, cx, cy);
    drawDpad(d, clampi(dpad, -1, 1));
    drawButtons(d);
    drawPillsAndGrille(d);

    // Индикатор питания на стекле слева (светится сам, не зависит от освещения).
    const float ledU = -85.0f, ledV = -44.0f;
    const int lx = roundi(d.X(ledU)), ly = roundi(d.Y(ledV));
    const int ls = std::max(1, roundi(2.0f * d.s));
    out.glow(lx, ly, std::max(2, roundi(6.0f * d.s)), kLedCol, 0.35f);
    out.fillRect(lx - ls / 2, ly - ls / 2, ls, ls, kLedCol);

    // Логотип на стекле под экраном.
    const float logoW = static_cast<float>(kSprLogo.w);
    drawLogo(out, d.X(-logoW * 0.5f), d.Y(kScreenHY + 2.5f), d.s, d.shade(kLogoCol, 0.0f, kScreenHY + 6.0f));

    // Сам экран.
    out.blitScaled(screen, sx0, sy0, sw, sh);
}
