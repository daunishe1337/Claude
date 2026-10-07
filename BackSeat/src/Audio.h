// ============================================================================
//  Audio.h — процедурный звук.
//
//  Два уровня:
//    * Synth  (Synth.h/.cpp) — переносимый синтезатор/микшер: все звуки
//      генерируются формулами (осцилляторы, шум, фильтры, огибающие).
//    * Audio  (Audio.cpp)    — вывод через WinAPI waveOut: отдельный поток
//      заполняет кольцо буферов, игра лишь отправляет команды.
//
//  Интерфейс Audio потокобезопасен: play()/setAmbient() можно вызывать из
//  игрового потока в любой момент. Если звуковое устройство недоступно,
//  init() вернёт false, а игра продолжит работу без звука.
// ============================================================================
#pragma once

#include <memory>

// Короткие звуковые эффекты (one-shot). Порядок не важен, Count — последний.
enum class Sfx : int {
    // --- Консоль (квадратные волны, «писк») ---
    ConsolePickupFuel,  // арпеджио вверх
    ConsolePickupItem,  // джингл подбора предмета
    ConsoleCrash,       // шумовой взрыв + нисходящий писк
    ConsoleSkid,        // «вжик» на масляном пятне
    ConsoleBoot,        // звук включения консоли
    ConsoleBlip,        // короткий тик (обратный отсчёт, меню)
    // --- Монстр и крыша ---
    RoofThud,           // тяжёлое приземление на крышу
    RoofStep,           // шаг/перехват по крыше
    RoofScrape,         // скрежет когтей по металлу
    GlassKnock,         // стук по стеклу
    GlassBang,          // сильный удар по стеклу
    GlassCrack,         // треск стекла
    GlassShatter,       // стекло разбито (проигрыш)
    MonsterGrowl,       // низкое рычание/дыхание
    MonsterScreech,     // визг при вспышке камеры
    // --- Камера ---
    CameraShutter,      // щелчок затвора + хлопок вспышки
    CameraReady,        // вспышка заряжена (тонкий писк)
    CameraEmpty,        // сухой щелчок: не заряжена
    // --- Машина ---
    EngineSputter,      // чихание двигателя (мало бензина / глохнет)
    EngineDie,          // двигатель заглох
    StreetlightWhoosh,  // проезд под фонарём (мягкий шум)
    // --- Люди ---
    MumbleDad,          // «бубнёж» низкого голоса (pitch задаёт длительность)
    MumbleMom,          // «бубнёж» высокого голоса
    // --- Сюжетные ---
    Jumpscare,          // скример: визг + удар
    LoseSting,          // мрачный аккорд проигрыша
    WinJingle,          // тёплый аккорд победы
    MenuMove,           // перемещение по меню
    MenuSelect,         // подтверждение
    Count
};

// Непрерывные слои звука. Игра выставляет их каждый тик целиком.
struct AmbientParams {
    float engine = 0.0f;        // громкость гула двигателя 0..1
    float engineRpm = 0.5f;     // обороты 0..1 (высота тона)
    float road = 0.0f;          // шум шин/дороги 0..1
    float wind = 0.0f;          // ветер 0..1
    float rain = 0.0f;          // дождь по крыше 0..1
    float heartbeat = 0.0f;     // сердцебиение 0..1 (и громкость, и частота)
    float dread = 0.0f;         // низкий тревожный дрон 0..1
    float consoleMusic = 0.0f;  // громкость мелодии консоли 0..1
    float consoleMuffle = 0.0f; // 0 — консоль у лица, 1 — опущена (глуше)
    float musicTempo = 1.0f;    // множитель темпа мелодии консоли
    float master = 1.0f;        // общая громкость 0..1
};

class Synth;

class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Открывает устройство и запускает поток. false — звука не будет.
    bool init();
    // Останавливает поток и освобождает устройство (вызывается и из деструктора).
    void shutdown();

    // volume 0..1, pan -1 (лево) .. 1 (право), pitch — множитель высоты/скорости.
    void play(Sfx s, float volume = 1.0f, float pan = 0.0f, float pitch = 1.0f);
    void setAmbient(const AmbientParams& p);
    void stopAllSfx();
    void setMuted(bool m);
    bool muted() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
