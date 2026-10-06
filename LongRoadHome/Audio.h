// Audio.h - процедурный звук. Весь звук синтезируется на лету и выводится через waveOut.
// Игровой поток пишет атомарные параметры и ставит одноразовые эффекты в очередь;
// аудио-поток микширует их в стерео 44.1 кГц / 16 бит.
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace lrh {

enum class Sfx {
    Step, Knock, Beep, BeepLow, Pickup, Crash, Flash, Hiss, Scream,
    Whoosh, Crack, Switch, Win, Click, Skid, Stall
};

class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();      // открыть устройство (только Windows). false -> игра идёт без звука
    void shutdown();

    // Одноразовый эффект. pan: -1 (слева) .. +1 (справа).
    void play(Sfx s, float vol = 1.0f, float pan = 0.0f);

    // Синтез блока (стерео, interleaved). Публично, чтобы можно было тестировать без устройства.
    void render(int16_t* out, int frames);

    // Непрерывные параметры (0..1, если не указано иное).
    std::atomic<float> engineRpm{0.0f};   // обороты
    std::atomic<float> engineVol{0.0f};
    std::atomic<float> stutter{0.0f};     // «чихание» мотора перед остановкой
    std::atomic<float> roadVol{0.0f};     // шум дороги
    std::atomic<float> scrapeVol{0.0f};   // скрежет по крыше
    std::atomic<float> scrapePan{0.0f};   // -1..1
    std::atomic<float> heartBpm{60.0f};
    std::atomic<float> heartVol{0.0f};
    std::atomic<float> voiceDad{0.0f};    // громкость голоса отца (ссора)
    std::atomic<float> voiceMom{0.0f};
    std::atomic<float> ambience{0.25f};   // ветер/ночь
    std::atomic<float> master{1.0f};

    void silenceAll();  // обнулить непрерывные звуки (меню, конец игры)

private:
    struct Synth;
    struct Platform;
    std::unique_ptr<Synth> syn_;
    std::unique_ptr<Platform> plat_;

    struct Event { Sfx type; float vol; float pan; };
    std::mutex qMutex_;
    std::vector<Event> queue_;
};

}  // namespace lrh
