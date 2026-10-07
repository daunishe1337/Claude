// ============================================================================
//  Audio.cpp — вывод звука через WinAPI waveOut (см. Audio.h).
//
//  Звуковой поток владеет синтезатором и кольцом из kBufferCount буферов.
//  Устройство открыто с CALLBACK_EVENT: доиграв буфер, waveOut взводит
//  событие; поток просыпается, забирает накопленные команды игры (под
//  мьютексом), применяет их к синтезатору и заново заполняет и отправляет
//  освободившиеся буферы. Callback-функции нет вовсе, поэтому функции
//  waveOut никогда не вызываются из контекста драйвера.
//
//  Игровой поток только кладёт команды в очередь (play/stopAllSfx) и
//  запоминает последние параметры фона (setAmbient) — это дёшево и не
//  блокирует его дольше короткой критической секции.
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include "Audio.h"
#include "Common.h"
#include "Synth.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <pmmintrin.h>
#include <xmmintrin.h>
#define BACKSEAT_AUDIO_X86 1
#else
#define BACKSEAT_AUDIO_X86 0
#endif

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace {

// Кольцо буферов: 6 x 512 кадров (по 11.6 мс) — около 70 мс в очереди.
// Этого хватает, чтобы не было провалов даже в отладочной сборке, а
// задержка между событием в игре и звуком остаётся незаметной.
constexpr int kBufferCount = 6;
constexpr int kBufferFrames = 512;
constexpr int kChannels = 2;
constexpr size_t kMaxQueue = 256;      // команд между двумя заполнениями (с запасом)
constexpr float kMuteRamp = 0.03f;     // плавность включения/выключения звука, с
constexpr DWORD kWaitMs = 50;          // страховочный таймаут ожидания события
constexpr ULONGLONG kDrainMs = 400;    // сколько ждать доигрывания очереди при остановке

struct Command {
    enum class Kind { Play, StopAll };
    Kind kind = Kind::Play;
    Sfx sfx = Sfx::MenuMove;
    float volume = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
};

} // namespace

struct Audio::Impl {
    // --- Общие данные игрового и звукового потоков ---
    std::mutex mutex;
    std::vector<Command> queue;    // новые команды (пишет игра, под mutex)
    AmbientParams pendingAmbient;  // последние параметры фона (под mutex)
    bool ambientDirty = false;     // параметры фона обновились (под mutex)
    std::atomic<bool> running{false};
    std::atomic<bool> muted{false};

    // --- Только игровой поток ---
    bool open = false; // устройство открыто, поток запущен
    std::thread thread;

    // --- Только звуковой поток (после запуска) ---
    Synth synth{cfg::kSampleRate};
    std::vector<Command> work;  // забранные из очереди команды
    std::vector<float> mix;     // float-стерео одного буфера
    std::vector<int16_t> pcm;   // память всех буферов устройства
    float gain = 1.0f;          // текущий множитель приглушения
    int next = 0;               // следующий буфер по кругу

    // --- Устройство ---
    HWAVEOUT device = nullptr;
    HANDLE event = nullptr;
    WAVEHDR headers[kBufferCount] = {};
    bool prepared[kBufferCount] = {};
    bool submitted[kBufferCount] = {};

    bool bufferFree(int i) const;
    void pullCommands();
    void renderBuffer(int i, bool fadeOut);
    bool submit(int i);
    void refill();
    void drain();
    void threadMain();
    void releaseDevice();
};

// ---------------------------------------------------------------------------
//  Звуковой поток
// ---------------------------------------------------------------------------

// Буфер можно заполнять: ещё не отправлялся или драйвер его уже вернул.
bool Audio::Impl::bufferFree(int i) const {
    if (!submitted[i]) return true;
    // Флаг выставляет драйвер из своего потока — читаем без кэширования.
    const volatile DWORD& flags = headers[i].dwFlags;
    return (flags & WHDR_DONE) != 0;
}

// Забрать команды игры: короткая критическая секция, только обмен векторами
// (ёмкость обоих зарезервирована заранее — выделений памяти нет).
void Audio::Impl::pullCommands() {
    AmbientParams amb;
    bool haveAmbient = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        work.swap(queue);
        if (ambientDirty) {
            amb = pendingAmbient;
            ambientDirty = false;
            haveAmbient = true;
        }
    }
    if (haveAmbient) synth.setAmbient(amb);
    for (const Command& c : work) {
        if (c.kind == Command::Kind::StopAll) synth.stopAllSfx();
        else synth.trigger(c.sfx, c.volume, c.pan, c.pitch);
    }
    work.clear();
}

// Синтез одного буфера и перевод в 16 бит. fadeOut — финальный буфер перед
// остановкой: громкость плавно уходит в ноль, чтобы не было щелчка.
void Audio::Impl::renderBuffer(int i, bool fadeOut) {
    pullCommands();
    synth.render(mix.data(), kBufferFrames);
    const float target = (fadeOut || muted.load(std::memory_order_relaxed)) ? 0.0f : 1.0f;
    const float step = fadeOut ? 1.0f / static_cast<float>(kBufferFrames)
                               : 1.0f / (kMuteRamp * static_cast<float>(cfg::kSampleRate));
    int16_t* dst = pcm.data() + static_cast<size_t>(i) * kBufferFrames * kChannels;
    const float* src = mix.data();
    for (int f = 0; f < kBufferFrames; ++f) {
        gain = approach(gain, target, step);
        for (int c = 0; c < kChannels; ++c) {
            float x = src[f * kChannels + c] * gain;
            if (!(x == x)) x = 0.0f; // NaN -> тишина
            x = clampf(x, -1.0f, 1.0f);
            dst[f * kChannels + c] = static_cast<int16_t>(std::lrint(x * 32767.0f));
        }
    }
}

bool Audio::Impl::submit(int i) {
    if (waveOutWrite(device, &headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        submitted[i] = false;
        return false;
    }
    submitted[i] = true;
    return true;
}

// Заполнить все освободившиеся буферы. Драйвер возвращает их строго в
// порядке отправки, поэтому идём по кругу от next и останавливаемся на
// первом занятом.
void Audio::Impl::refill() {
    for (int n = 0; n < kBufferCount; ++n) {
        if (!bufferFree(next)) break;
        renderBuffer(next, false);
        if (!submit(next)) break; // устройство отказало — попробуем позже
        next = (next + 1) % kBufferCount;
    }
}

// Остановка: дописать один буфер с затуханием и дождаться, пока очередь
// доиграет (с ограничением по времени — завис драйвер, не зависнем и мы).
void Audio::Impl::drain() {
    const ULONGLONG deadline = GetTickCount64() + kDrainMs;
    while (!bufferFree(next) && GetTickCount64() < deadline) WaitForSingleObject(event, 10);
    if (bufferFree(next)) {
        renderBuffer(next, true);
        if (submit(next)) next = (next + 1) % kBufferCount;
    }
    for (;;) {
        bool busy = false;
        for (int i = 0; i < kBufferCount; ++i) busy = busy || !bufferFree(i);
        if (!busy || GetTickCount64() >= deadline) break;
        WaitForSingleObject(event, 10);
    }
}

void Audio::Impl::threadMain() {
#if BACKSEAT_AUDIO_X86
    // Денормализованные числа в хвостах фильтров считаются очень медленно —
    // в звуковом потоке обнуляем их аппаратно.
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    while (running.load(std::memory_order_acquire)) {
        refill();
        WaitForSingleObject(event, kWaitMs);
    }
    drain();
}

// Освобождение устройства (поток уже остановлен или не запускался).
void Audio::Impl::releaseDevice() {
    if (device) {
        waveOutReset(device); // вернуть все буферы драйвера
        for (int i = 0; i < kBufferCount; ++i) {
            if (prepared[i]) waveOutUnprepareHeader(device, &headers[i], sizeof(WAVEHDR));
            prepared[i] = false;
            submitted[i] = false;
        }
        waveOutClose(device);
        device = nullptr;
    }
    if (event) {
        CloseHandle(event);
        event = nullptr;
    }
}

// ---------------------------------------------------------------------------
//  Интерфейс для игры
// ---------------------------------------------------------------------------
Audio::Audio() : impl_(std::make_unique<Impl>()) {}

Audio::~Audio() { shutdown(); }

bool Audio::init() {
    Impl& m = *impl_;
    if (m.open) return true;

    m.event = CreateEventW(nullptr, FALSE, FALSE, nullptr); // авто-сброс
    if (!m.event) return false;

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = static_cast<WORD>(kChannels);
    fmt.nSamplesPerSec = static_cast<DWORD>(cfg::kSampleRate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = static_cast<WORD>(kChannels * sizeof(int16_t));
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;
    if (waveOutOpen(&m.device, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(m.event), 0, CALLBACK_EVENT) !=
        MMSYSERR_NOERROR) {
        m.device = nullptr;
        m.releaseDevice();
        return false;
    }

    // Память буферов и рабочие векторы выделяются один раз здесь.
    m.pcm.assign(static_cast<size_t>(kBufferCount) * kBufferFrames * kChannels, 0);
    m.mix.assign(static_cast<size_t>(kBufferFrames) * kChannels, 0.0f);
    m.queue.reserve(kMaxQueue);
    m.work.reserve(kMaxQueue);
    for (int i = 0; i < kBufferCount; ++i) {
        WAVEHDR& h = m.headers[i];
        h = WAVEHDR{};
        h.lpData = reinterpret_cast<LPSTR>(m.pcm.data() + static_cast<size_t>(i) * kBufferFrames * kChannels);
        h.dwBufferLength = static_cast<DWORD>(kBufferFrames * kChannels * sizeof(int16_t));
        if (waveOutPrepareHeader(m.device, &h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            m.releaseDevice();
            return false;
        }
        m.prepared[i] = true;
        m.submitted[i] = false;
    }
    m.next = 0;
    m.gain = m.muted.load() ? 0.0f : 1.0f;

    m.running.store(true, std::memory_order_release);
    try {
        m.thread = std::thread(&Impl::threadMain, &m);
    } catch (const std::system_error&) {
        m.running.store(false);
        m.releaseDevice();
        return false;
    }
    m.open = true;
    return true;
}

void Audio::shutdown() {
    Impl& m = *impl_;
    if (!m.open) return;
    m.running.store(false, std::memory_order_release);
    SetEvent(m.event); // разбудить поток немедленно
    if (m.thread.joinable()) m.thread.join();
    m.releaseDevice();
    m.open = false;
    // Невыполненные команды теряют смысл: следующий init() начнёт с чистого листа.
    std::lock_guard<std::mutex> lock(m.mutex);
    m.queue.clear();
    m.ambientDirty = false;
}

void Audio::play(Sfx s, float volume, float pan, float pitch) {
    Impl& m = *impl_;
    if (!m.open) return;
    Command c;
    c.kind = Command::Kind::Play;
    c.sfx = s;
    c.volume = volume;
    c.pan = pan;
    c.pitch = pitch;
    std::lock_guard<std::mutex> lock(m.mutex);
    if (m.queue.size() >= kMaxQueue) return; // звуковой поток не успевает — лишнее отбрасываем
    m.queue.push_back(c);
}

void Audio::setAmbient(const AmbientParams& p) {
    Impl& m = *impl_;
    if (!m.open) return;
    std::lock_guard<std::mutex> lock(m.mutex);
    m.pendingAmbient = p;
    m.ambientDirty = true;
}

void Audio::stopAllSfx() {
    Impl& m = *impl_;
    if (!m.open) return;
    Command c;
    c.kind = Command::Kind::StopAll;
    std::lock_guard<std::mutex> lock(m.mutex);
    // Остановка важнее ожидающих запусков: при переполнении они отбрасываются.
    if (m.queue.size() >= kMaxQueue) m.queue.clear();
    m.queue.push_back(c);
}

void Audio::setMuted(bool m) { impl_->muted.store(m, std::memory_order_relaxed); }

bool Audio::muted() const { return impl_->muted.load(std::memory_order_relaxed); }
