// Медиаплеер Windows 11: главная, музыкальная библиотека, видеотека, очередь, режим «Сейчас
// воспроизводится» и нижняя панель управления (перемешивание, повтор, перемотка, громкость).
// Звук настоящий: музыка синтезируется по «семени» файла (аккорды, бас, мелодия, ударные) и
// выводится через waveOut; для видео показывается кадр-фотография с плавным панорамированием.

#include "../Shell.h"
#include "AppFactories.h"
#include "ImmUi.h"
#include "Photos.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <mmsystem.h>
#ifdef _MSC_VER
#pragma warning(pop)
#pragma comment(lib, "winmm.lib")
#endif

namespace
{
constexpr float kTitleH = 48.0f;
constexpr float kNavFull = 280.0f;
constexpr float kNavCompact = 48.0f;
constexpr float kBarH = 96.0f;
constexpr int kRate = 22050;
constexpr int kBufFrames = 4096;
constexpr int kBufCount = 12;

// ============================================================================ синтезатор
uint32_t Hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

struct Style
{
    float tempo = 96.0f;
    int root = 0;
    bool minor = false;
    bool drums = false;
    bool rain = false;
    float melody = 0.6f;
};

class Synth
{
public:
    void Setup(const FsNode* n, double duration)
    {
        m_seed = n ? static_cast<uint32_t>(n->id) * 2654435761u + 17u : 1u;
        m_dur = duration;
        const std::wstring name = n ? ToLower(n->name) : L"";
        if (name.find(L"джаз") != std::wstring::npos)
            m_st = {112.0f, 5, false, true, false, 0.85f};
        else if (name.find(L"дожд") != std::wstring::npos)
            m_st = {70.0f, 2, true, false, true, 0.45f};
        else if (name.find(L"тишин") != std::wstring::npos)
            m_st = {58.0f, 0, false, false, false, 0.2f};
        else
        {
            const uint32_t h = Hash32(m_seed);
            m_st = {80.0f + static_cast<float>(h % 50), static_cast<int>(h / 7 % 12), (h & 8u) != 0, (h & 16u) != 0, false, 0.6f};
        }
        m_rain = 0.0f;
    }

    // Отсчёт времени t (секунды) — детерминированная функция, поэтому перемотка работает без состояния.
    float Sample(double t, uint32_t index)
    {
        static const int majorScale[7] = {0, 2, 4, 5, 7, 9, 11};
        static const int minorScale[7] = {0, 2, 3, 5, 7, 8, 10};
        static const int progMajor[4] = {0, 4, 5, 3};
        static const int progMinor[4] = {0, 5, 2, 6};
        const int* scale = m_st.minor ? minorScale : majorScale;
        auto note = [&](int degree) {
            const int oct = degree >= 0 ? degree / 7 : (degree - 6) / 7;
            const int d = degree - oct * 7;
            return 48 + m_st.root + 12 * oct + scale[d];
        };
        auto freq = [](int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); };
        const double b = 60.0 / m_st.tempo;
        const double beat = t / b;
        const int beatInt = static_cast<int>(std::floor(beat));
        const int bar = beatInt / 4;
        const int chord = (m_st.minor ? progMinor : progMajor)[bar % 4];
        const double inBar = (beat - bar * 4.0) * b;
        constexpr double tau = 6.283185307179586;
        double s = 0.0;
        // Пэд: тоны аккорда
        const double padEnv = (1.0 - std::exp(-inBar / 0.35)) * (0.85 + 0.15 * std::sin(tau * 0.25 * t));
        for (int k = 0; k < 3; ++k)
        {
            const double f = freq(note(chord + 2 * k) + 12);
            s += 0.055 * padEnv * (std::sin(tau * f * t) + 0.35 * std::sin(tau * f * 1.003 * t));
        }
        // Бас на каждую долю
        const double bt = (beat - beatInt) * b;
        {
            const double f = freq(note(chord) - 12);
            const double ph = std::fmod(f * t, 1.0);
            const double tri = 4.0 * std::fabs(ph - 0.5) - 1.0;
            s += 0.16 * tri * std::exp(-bt * 2.5) * (1.0 - std::exp(-bt * 60.0));
        }
        // Мелодия восьмыми
        const double eighthPos = beat * 2.0;
        const int eighth = static_cast<int>(std::floor(eighthPos));
        const double et = (eighthPos - eighth) * b * 0.5;
        const uint32_t h = Hash32(m_seed ^ static_cast<uint32_t>(eighth) * 0x9E3779B9u);
        if (static_cast<float>(h % 1000) < m_st.melody * 620.0f)
        {
            const int deg = chord + static_cast<int>((h >> 10) % 8);
            const double f = freq(note(deg) + 12);
            const double env = std::exp(-et * 3.5) * (1.0 - std::exp(-et * 90.0));
            m_lastMelody = static_cast<float>(env);
            s += 0.085 * env * (std::sin(tau * f * t) + 0.25 * std::sin(tau * 2.0 * f * t));
        }
        else
            m_lastMelody *= 0.999f;
        // Ударные
        if (m_st.drums)
        {
            if (beatInt % 2 == 0)
            {
                const double f0 = 48.0 + 90.0 * std::exp(-bt * 25.0);
                s += 0.3 * std::sin(tau * f0 * bt) * std::exp(-bt * 9.0);
            }
            if (eighth % 2 == 1)
            {
                const double noise = static_cast<double>(Hash32(index * 747796405u + 1u) & 0xFFFF) / 32768.0 - 1.0;
                s += 0.035 * noise * std::exp(-et * 45.0);
            }
        }
        if (m_st.rain)
        {
            const float noise = static_cast<float>(Hash32(index * 2891336453u + 7u) & 0xFFFF) / 32768.0f - 1.0f;
            m_rain += (noise - m_rain) * 0.18f;
            s += 0.09 * m_rain;
        }
        m_lastBass = static_cast<float>(std::exp(-bt * 2.5));
        const double fade = std::min({1.0, t / 2.0, std::max(0.0, (m_dur - t) / 2.5)});
        return static_cast<float>(s * fade);
    }

    float Bass() const { return m_lastBass; }
    float Melody() const { return m_lastMelody; }
    const Style& St() const { return m_st; }

private:
    uint32_t m_seed = 1;
    double m_dur = 60.0;
    Style m_st;
    float m_rain = 0.0f;
    float m_lastBass = 0.0f;
    float m_lastMelody = 0.0f;
};

// Вывод через waveOut с опросом готовых буферов (без потоков и обратных вызовов).
class AudioOut
{
public:
    ~AudioOut() { Close(); }

    bool Open()
    {
        if (m_h)
            return true;
        if (m_failed)
            return false;
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 1;
        wf.nSamplesPerSec = kRate;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = 2;
        wf.nAvgBytesPerSec = kRate * 2;
        if (waveOutOpen(&m_h, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            m_h = nullptr;
            m_failed = true;
            return false;
        }
        m_data.assign(static_cast<size_t>(kBufFrames) * kBufCount, 0);
        for (int i = 0; i < kBufCount; ++i)
        {
            WAVEHDR& w = m_hdr[i];
            w = WAVEHDR{};
            w.lpData = reinterpret_cast<LPSTR>(m_data.data() + static_cast<size_t>(i) * kBufFrames);
            w.dwBufferLength = kBufFrames * 2;
            waveOutPrepareHeader(m_h, &w, sizeof(WAVEHDR));
            w.dwFlags |= WHDR_DONE;
        }
        return true;
    }

    void Close()
    {
        if (!m_h)
            return;
        waveOutReset(m_h);
        for (WAVEHDR& w : m_hdr)
            waveOutUnprepareHeader(m_h, &w, sizeof(WAVEHDR));
        waveOutClose(m_h);
        m_h = nullptr;
    }

    void Reset()
    {
        if (m_h)
            waveOutReset(m_h);
    }

    // Заполнить все освободившиеся буферы через fill(int16_t*, frames).
    template <typename F>
    void Feed(F&& fill)
    {
        if (!m_h)
            return;
        for (WAVEHDR& w : m_hdr)
        {
            if (!(w.dwFlags & WHDR_DONE))
                continue;
            fill(reinterpret_cast<int16_t*>(w.lpData), kBufFrames);
            w.dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
            waveOutWrite(m_h, &w, sizeof(WAVEHDR));
        }
    }

    bool Ok() const { return m_h != nullptr; }

private:
    HWAVEOUT m_h = nullptr;
    bool m_failed = false;
    WAVEHDR m_hdr[kBufCount]{};
    std::vector<int16_t> m_data;
};

// ============================================================================ библиотека
struct Track
{
    FsNode* node = nullptr;
    std::wstring title;
    std::wstring artist;
    std::wstring album;
    std::wstring genre;
    double duration = 0.0;
    bool video = false;
    int year = 2024;
};

const wchar_t* const kArtists[] = {L"Городской квартет", L"Тихие волны", L"Ансамбль «Осень»", L"Ночной трамвай", L"Северный ветер"};
const wchar_t* const kAlbums[] = {L"Утренние истории", L"Звуки города", L"Тёплый вечер", L"Полёт", L"Акварель"};
const wchar_t* const kGenres[] = {L"Джаз", L"Эмбиент", L"Лоу-фай", L"Электроника", L"Классика"};

std::wstring BaseName(const std::wstring& name)
{
    const size_t dot = name.rfind(L'.');
    return dot == std::wstring::npos ? name : name.substr(0, dot);
}

Track MakeTrack(FsNode* n)
{
    Track t;
    t.node = n;
    t.title = BaseName(n->name);
    t.video = n->kind == FileKind::Video;
    const uint32_t h = Hash32(n->id * 31u + 7u);
    t.artist = t.video ? L"Видео" : kArtists[h % 5];
    t.album = t.video ? L"Видео" : kAlbums[(h / 5) % 5];
    t.genre = t.video ? L"" : kGenres[(h / 25) % 5];
    if (ToLower(n->name).find(L"джаз") != std::wstring::npos)
        t.genre = L"Джаз";
    t.year = 2019 + static_cast<int>(h % 7);
    const double bytes = static_cast<double>(std::max<unsigned long long>(n->size, 200000ULL));
    t.duration = t.video ? std::min(5400.0, bytes / 500000.0) : std::min(900.0, bytes * 8.0 / 192000.0);
    return t;
}

void CollectMedia(FsNode* d, std::vector<Track>& out)
{
    for (auto& c : d->children)
    {
        if (c->IsFolder())
            CollectMedia(c.get(), out);
        else if (c->kind == FileKind::Audio || c->kind == FileKind::Video)
            out.push_back(MakeTrack(c.get()));
    }
}

std::wstring TimeText(double secs)
{
    const int s = static_cast<int>(std::max(0.0, secs));
    if (s >= 3600)
        return std::to_wstring(s / 3600) + L":" + Pad2(s / 60 % 60) + L":" + Pad2(s % 60);
    return std::to_wstring(s / 60) + L":" + Pad2(s % 60);
}

// Обложка: градиент и фигуры по «семени» трека.
void DrawArt(Renderer& r, const Rect& rc, const Track& t, float radius)
{
    const uint32_t h = Hash32(t.node ? t.node->id * 977u : 3u);
    if (t.video)
    {
        if (std::shared_ptr<Gdiplus::Bitmap> bmp = PhotoBitmap(static_cast<int>(h % 8) + 1))
        {
            r.ImageRounded(bmp.get(), rc, radius);
            r.FillCircle(rc.CenterX(), rc.CenterY(), std::min(rc.w, rc.h) * 0.16f, Color(0, 0, 0, 0x60));
            const float s = std::min(rc.w, rc.h) * 0.12f;
            r.FillPolygon({{rc.CenterX() - s * 0.4f, rc.CenterY() - s * 0.6f}, {rc.CenterX() + s * 0.7f, rc.CenterY()}, {rc.CenterX() - s * 0.4f, rc.CenterY() + s * 0.6f}},
                          Color(255, 255, 255));
            return;
        }
    }
    const Color c1 = Theme::HslToRgb(static_cast<float>(h % 360) / 360.0f, 0.6f, 0.5f);
    const Color c2 = Theme::HslToRgb(static_cast<float>((h / 360) % 360) / 360.0f, 0.65f, 0.3f);
    r.FillGradientV(rc, radius, c1, c2);
    r.PushClip(rc);
    for (int i = 0; i < 5; ++i)
    {
        const uint32_t k = Hash32(h + static_cast<uint32_t>(i) * 101u);
        const float cx = rc.x + rc.w * static_cast<float>(k % 100) / 100.0f, cy = rc.y + rc.h * static_cast<float>((k / 100) % 100) / 100.0f;
        const float rad = rc.w * (0.15f + static_cast<float>((k / 10000) % 30) / 100.0f);
        r.FillCircle(cx, cy, rad, Color(255, 255, 255, static_cast<uint8_t>(20 + (k >> 24) % 40)));
    }
    r.PopClip();
    const float gs = std::min(rc.w, rc.h) * 0.36f;
    r.Glyph(Icon::Music, {rc.CenterX() - gs * 0.5f, rc.CenterY() - gs * 0.5f, gs, gs}, Color(255, 255, 255, 0xD8), 1.3f);
}

enum class Page
{
    Home,
    Music,
    Video,
    Queue,
    Playlists,
    Settings,
    NowPlaying,
};

enum Hk : int
{
    kHkNone = 0,
    kHkNav,
    kHkHamburger,
    kHkPlay,
    kHkPrev,
    kHkNext,
    kHkShuffle,
    kHkRepeat,
    kHkSeek,
    kHkVolume,
    kHkVolSlider,
    kHkArt,
    kHkFull,
    kHkTrack, // строка/карточка трека (index — в библиотеке)
    kHkQueueRow,
    kHkPlayAll,
    kHkBack,
};

class MediaWindow : public Window
{
public:
    explicit MediaWindow(const std::wstring& arg) : Window(AppId::MediaPlayer, L"Медиаплеер")
    {
        SetInitialSize(1180.0f, 760.0f);
        m_minSize = {640.0f, 520.0f};
        CollectMedia(GetShell().FS().Root(), m_lib);
        OnLaunchArgs(arg);
    }

    ~MediaWindow() override { m_audio.Close(); }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnLaunchArgs(const std::wstring& arg) override
    {
        FsNode* n = GetShell().FS().FromArg(arg);
        if (!n || (n->kind != FileKind::Audio && n->kind != FileKind::Video))
            return;
        int idx = -1;
        for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
        {
            if (m_lib[static_cast<size_t>(i)].node == n)
                idx = i;
        }
        if (idx < 0)
        {
            m_lib.push_back(MakeTrack(n));
            idx = static_cast<int>(m_lib.size()) - 1;
        }
        // Очередь — файлы из той же папки.
        m_queue.clear();
        for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
        {
            if (m_lib[static_cast<size_t>(i)].node->parent == n->parent)
                m_queue.push_back(i);
        }
        PlayIndex(idx);
        m_page = Page::NowPlaying;
    }

    bool OnClose() override
    {
        m_audio.Close();
        return true;
    }

    void OnTimer1s() override { Pump(); }

private:
    // ================================================================== воспроизведение
    const Track* Current() const { return m_cur >= 0 && m_cur < static_cast<int>(m_lib.size()) ? &m_lib[static_cast<size_t>(m_cur)] : nullptr; }

    double Position() const
    {
        if (!m_playing)
            return m_pos;
        return std::min(m_pos + (NowSeconds() - m_startClock), Current() ? Current()->duration : 0.0);
    }

    void PlayIndex(int idx)
    {
        if (idx < 0 || idx >= static_cast<int>(m_lib.size()))
            return;
        if (std::find(m_queue.begin(), m_queue.end(), idx) == m_queue.end())
            m_queue.push_back(idx);
        m_cur = idx;
        m_synth.Setup(m_lib[static_cast<size_t>(idx)].node, m_lib[static_cast<size_t>(idx)].duration);
        SeekTo(0.0);
        Play();
        SetTitle(m_lib[static_cast<size_t>(idx)].title + L" — Медиаплеер");
    }

    void Play()
    {
        if (!Current())
        {
            if (!m_queue.empty())
                PlayIndex(m_queue.front());
            else if (!m_lib.empty())
            {
                for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
                    m_queue.push_back(i);
                PlayIndex(0);
            }
            return;
        }
        if (m_playing)
            return;
        m_playing = true;
        m_startClock = NowSeconds();
        m_genFrame = static_cast<long long>(m_pos * kRate);
        m_audio.Open();
        m_audio.Reset();
        Pump();
    }

    void Pause()
    {
        if (!m_playing)
            return;
        m_pos = Position();
        m_playing = false;
        m_audio.Reset();
    }

    void SeekTo(double t)
    {
        const bool was = m_playing;
        if (was)
            Pause();
        m_pos = Clamp(t, 0.0, Current() ? Current()->duration : 0.0);
        if (was)
            Play();
    }

    int QueuePos() const
    {
        for (int i = 0; i < static_cast<int>(m_queue.size()); ++i)
        {
            if (m_queue[static_cast<size_t>(i)] == m_cur)
                return i;
        }
        return -1;
    }

    void Next(bool user)
    {
        if (m_queue.empty())
            return;
        if (m_repeat == 2 && !user)
        {
            SeekTo(0.0);
            Play();
            return;
        }
        int qp = QueuePos();
        if (m_shuffle && m_queue.size() > 1)
        {
            int n = qp;
            while (n == qp)
                n = RandomInt(0, static_cast<int>(m_queue.size()) - 1);
            qp = n;
        }
        else
            ++qp;
        if (qp >= static_cast<int>(m_queue.size()))
        {
            if (m_repeat == 0 && !user)
            {
                Pause();
                m_pos = 0.0;
                return;
            }
            qp = 0;
        }
        PlayIndex(m_queue[static_cast<size_t>(qp)]);
    }

    void Prev()
    {
        if (Position() > 3.0 || m_queue.empty())
        {
            SeekTo(0.0);
            return;
        }
        const int qp = std::max(0, QueuePos() - 1);
        PlayIndex(m_queue[static_cast<size_t>(qp)]);
    }

    // Досинтезировать звук в освободившиеся буферы и проверить конец трека.
    void Pump()
    {
        if (!m_playing || !Current())
            return;
        if (Position() >= Current()->duration - 0.05)
        {
            Next(false);
            return;
        }
        const ShellSettings& st = GetShell().Settings();
        const float vol = st.muted ? 0.0f : st.volume * m_volume;
        m_audio.Feed([&](int16_t* out, int frames) {
            for (int i = 0; i < frames; ++i)
            {
                const long long f = m_genFrame++;
                const float v = m_synth.Sample(static_cast<double>(f) / kRate, static_cast<uint32_t>(f)) * vol;
                out[i] = static_cast<int16_t>(Clamp(v, -1.0f, 1.0f) * 30000.0f);
            }
        });
    }

    // ================================================================== отрисовка
    bool Wide() const { return Width() >= 900.0f; }
    float NavW() const { return Wide() && m_navPref ? kNavFull : kNavCompact; }
    Rect ContentRect() const { return {NavW(), kTitleH, Width() - NavW(), Height() - kTitleH - kBarH}; }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        Pump();
        m_imm.Begin({0.0f, 0.0f, Width(), Height()});
        r.DrawAppIcon(AppIcon::MediaPlayer, {16.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        r.Text(L"Медиаплеер", {48.0f, 0.0f, 200.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        const bool full = m_page == Page::NowPlaying;
        const Rect c = full ? Rect{0.0f, kTitleH, Width(), Height() - kTitleH - kBarH} : ContentRect();
        if (!full)
        {
            Imm::ContentLayer(r, c);
            DrawNav(r);
        }
        r.PushClip(c);
        m_imm.SetClip(c);
        m_contentH = 0.0f;
        switch (m_page)
        {
        case Page::Home:
            DrawHome(r, c);
            break;
        case Page::Music:
            DrawSongs(r, c);
            break;
        case Page::Video:
            DrawVideos(r, c);
            break;
        case Page::Queue:
            DrawQueue(r, c);
            break;
        case Page::Playlists:
            DrawPlaylists(r, c);
            break;
        case Page::Settings:
            DrawSettings(r, c);
            break;
        case Page::NowPlaying:
            DrawNowPlaying(r, c);
            break;
        }
        Imm::Scrollbar(r, c, m_scroll, m_contentH);
        r.PopClip();
        m_imm.ResetClip();
        DrawTransport(r);
        if (m_playing)
            RequestFrame();
    }

    void DrawNav(Renderer& r)
    {
        const bool expanded = Wide() && m_navPref;
        const float w = expanded ? kNavFull : kNavCompact;
        Imm::SubtleButton(r, m_imm, {4.0f, kTitleH, 40.0f, 36.0f}, Icon::Hamburger, kHkHamburger, 0, expanded ? L"Свернуть" : L"Развернуть");
        struct Item
        {
            Page page;
            Icon icon;
            const wchar_t* text;
        };
        static const Item items[] = {{Page::Home, Icon::Home, L"Главная"},
                                     {Page::Music, Icon::Music, L"Музыкальная библиотека"},
                                     {Page::Video, Icon::Videos, L"Видеотека"},
                                     {Page::Queue, Icon::List, L"Очередь воспроизведения"},
                                     {Page::Playlists, Icon::Tiles, L"Списки воспроизведения"},
                                     {Page::Settings, Icon::Settings, L"Параметры"}};
        for (int i = 0; i < 6; ++i)
        {
            const bool settings = i == 5;
            float y = kTitleH + 44.0f + static_cast<float>(i) * 40.0f + (i >= 3 ? 17.0f : 0.0f);
            if (settings)
                y = Height() - kBarH - 44.0f;
            if (i == 3)
                r.FillRect({12.0f, y - 10.0f, w - 24.0f, 1.0f}, Theme::P().divider);
            Imm::NavRow(r, m_imm, {4.0f, y, w - 8.0f, 36.0f}, items[i].icon, items[i].text, m_page == items[i].page, expanded, kHkNav, i);
        }
    }

    void Header(Renderer& r, const Rect& c, float& y, const wchar_t* title)
    {
        r.Text(title, {c.x + 40.0f, y, c.w - 80.0f, 44.0f}, Theme::P().text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 60.0f;
    }

    void DrawHome(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Главная");
        Imm::TextButton(r, m_imm, {c.x + 40.0f, y, 220.0f, 36.0f}, L"Воспроизвести всё", Icon::Play, true, kHkPlayAll);
        y += 60.0f;
        r.Text(L"Недавние файлы мультимедиа", {c.x + 40.0f, y, c.w - 80.0f, 26.0f}, p.text, 18.0f, FontWeight::Semibold, TextFlags::VCenter);
        y += 40.0f;
        y = DrawCards(r, c, y, [](const Track&) { return true; });
        m_contentH = y + m_scroll - c.y + 24.0f;
    }

    template <typename Pred>
    float DrawCards(Renderer& r, const Rect& c, float y, Pred pred)
    {
        const Palette& p = Theme::P();
        const float cw = 168.0f, ch = 228.0f, gap = 20.0f;
        const int cols = std::max(1, static_cast<int>((c.w - 80.0f + gap) / (cw + gap)));
        int k = 0;
        for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
        {
            const Track& t = m_lib[static_cast<size_t>(i)];
            if (!pred(t))
                continue;
            const Rect rc{c.x + 40.0f + static_cast<float>(k % cols) * (cw + gap), y + static_cast<float>(k / cols) * (ch + gap), cw, ch};
            const bool hot = m_imm.Hot(kHkTrack, i);
            if (hot)
                r.FillRoundRect(rc.Inflated(6.0f), 8.0f, p.subtleHover);
            const Rect art{rc.x, rc.y, cw, t.video ? cw * 0.5625f : cw};
            DrawArt(r, t.video ? Rect{art.x, art.y + (cw - art.h) * 0.5f, art.w, art.h} : art, t, 6.0f);
            if (hot && !t.video)
            {
                r.FillCircle(art.Right() - 26.0f, art.Bottom() - 26.0f, 18.0f, p.accent);
                r.Glyph(Icon::Play, {art.Right() - 33.0f, art.Bottom() - 33.0f, 14.0f, 14.0f}, p.textOnAccent);
            }
            r.Text(t.title, {rc.x, rc.y + cw + 8.0f, cw, 20.0f}, i == m_cur ? p.accentText : p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(t.video ? TimeText(t.duration) : t.artist, {rc.x, rc.y + cw + 30.0f, cw, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter);
            m_imm.Add(rc, kHkTrack, i, L"");
            ++k;
        }
        if (k == 0)
        {
            r.Text(L"Файлы не найдены. Поместите музыку в папку «Музыка», а видео — в «Видео».", {c.x + 40.0f, y, c.w - 80.0f, 24.0f}, p.textSecondary, 14.0f);
            return y + 30.0f;
        }
        return y + static_cast<float>((k + cols - 1) / cols) * (ch + gap);
    }

    void DrawSongs(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Музыкальная библиотека");
        Imm::TextButton(r, m_imm, {c.x + 40.0f, y, 220.0f, 36.0f}, L"Воспроизвести всё", Icon::Play, true, kHkPlayAll);
        y += 56.0f;
        const float x = c.x + 40.0f, w = c.w - 80.0f;
        const float cTitle = x + 56.0f, cArtist = x + w * 0.38f, cAlbum = x + w * 0.58f, cGenre = x + w * 0.76f, cDur = x + w - 70.0f;
        r.Text(L"Название", {cTitle, y, 200.0f, 20.0f}, p.textSecondary, 12.0f);
        r.Text(L"Исполнитель", {cArtist, y, 200.0f, 20.0f}, p.textSecondary, 12.0f);
        r.Text(L"Альбом", {cAlbum, y, 200.0f, 20.0f}, p.textSecondary, 12.0f);
        if (w > 700.0f)
            r.Text(L"Жанр", {cGenre, y, 200.0f, 20.0f}, p.textSecondary, 12.0f);
        r.Text(L"Длительность", {cDur - 30.0f, y, 100.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Right);
        y += 28.0f;
        int row = 0;
        for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
        {
            const Track& t = m_lib[static_cast<size_t>(i)];
            if (t.video)
                continue;
            const Rect rr{x, y, w, 48.0f};
            const bool hot = m_imm.Hot(kHkTrack, i), cur = i == m_cur;
            r.FillRoundRect(rr, 4.0f, hot ? p.subtleHover : (row % 2 == 0 ? p.card : Color(0, 0, 0, 0)));
            DrawArt(r, {rr.x + 8.0f, rr.y + 6.0f, 36.0f, 36.0f}, t, 4.0f);
            if (cur && m_playing)
            {
                // Анимированный эквалайзер у текущего трека
                for (int k = 0; k < 3; ++k)
                {
                    const float hgt = 6.0f + 10.0f * (0.5f + 0.5f * std::sin(static_cast<float>(NowSeconds()) * (6.0f + static_cast<float>(k) * 2.3f)));
                    r.FillRect({rr.x + 16.0f + static_cast<float>(k) * 7.0f, rr.CenterY() + 8.0f - hgt, 4.0f, hgt}, Color(255, 255, 255));
                }
            }
            const Color fg = cur ? p.accentText : p.text;
            r.Text(t.title, {cTitle, rr.y, cArtist - cTitle - 12.0f, rr.h}, fg, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(t.artist, {cArtist, rr.y, cAlbum - cArtist - 12.0f, rr.h}, cur ? fg : p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(t.album, {cAlbum, rr.y, cGenre - cAlbum - 12.0f, rr.h}, cur ? fg : p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            if (w > 700.0f)
                r.Text(t.genre, {cGenre, rr.y, cDur - cGenre - 40.0f, rr.h}, cur ? fg : p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            r.Text(TimeText(t.duration), {cDur - 30.0f, rr.y, 100.0f, rr.h}, cur ? fg : p.textSecondary, 14.0f, FontWeight::Regular,
                   TextFlags::VCenter | TextFlags::Right);
            m_imm.Add(rr, kHkTrack, i, L"");
            y += 50.0f;
            ++row;
        }
        if (row == 0)
            r.Text(L"В папке «Музыка» нет файлов.", {x, y, w, 24.0f}, p.textSecondary, 14.0f);
        m_contentH = y + m_scroll - c.y + 24.0f;
    }

    void DrawVideos(Renderer& r, const Rect& c)
    {
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Видеотека");
        y = DrawCards(r, c, y, [](const Track& t) { return t.video; });
        m_contentH = y + m_scroll - c.y + 24.0f;
    }

    void DrawQueue(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Очередь воспроизведения");
        const float x = c.x + 40.0f, w = c.w - 80.0f;
        if (m_queue.empty())
            r.Text(L"Очередь пуста. Выберите файл в библиотеке, чтобы начать воспроизведение.", {x, y, w, 24.0f}, p.textSecondary, 14.0f);
        for (int q = 0; q < static_cast<int>(m_queue.size()); ++q)
        {
            const Track& t = m_lib[static_cast<size_t>(m_queue[static_cast<size_t>(q)])];
            const Rect rr{x, y, w, 56.0f};
            const bool cur = m_queue[static_cast<size_t>(q)] == m_cur;
            r.FillRoundRect(rr, 6.0f, m_imm.Hot(kHkQueueRow, q) ? p.subtleHover : (cur ? p.card : Color(0, 0, 0, 0)));
            if (cur)
                r.FillRoundRect({rr.x, rr.CenterY() - 10.0f, 3.0f, 20.0f}, 1.5f, p.accent);
            DrawArt(r, {rr.x + 12.0f, rr.y + 8.0f, 40.0f, 40.0f}, t, 4.0f);
            r.Text(t.title, {rr.x + 64.0f, rr.y + 8.0f, w - 200.0f, 22.0f}, cur ? p.accentText : p.text, 14.0f, FontWeight::Semibold);
            r.Text(t.artist, {rr.x + 64.0f, rr.y + 30.0f, w - 200.0f, 18.0f}, p.textSecondary, 12.0f);
            r.Text(TimeText(t.duration), {rr.Right() - 100.0f, rr.y, 88.0f, rr.h}, p.textSecondary, 13.0f, FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
            m_imm.Add(rr, kHkQueueRow, q, L"");
            y += 60.0f;
        }
        m_contentH = y + m_scroll - c.y + 24.0f;
    }

    void DrawPlaylists(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Списки воспроизведения");
        const Rect card{c.x + 40.0f, y, 200.0f, 260.0f};
        const Track fav = m_lib.empty() ? Track{} : m_lib.front();
        if (!m_lib.empty())
            DrawArt(r, {card.x, card.y, 200.0f, 200.0f}, fav, 8.0f);
        r.Text(L"Избранное", {card.x, card.y + 210.0f, 200.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(Plural(static_cast<int>(m_lib.size())), {card.x, card.y + 232.0f, 200.0f, 18.0f}, p.textSecondary, 12.0f);
        m_imm.Add(card, kHkPlayAll);
        m_contentH = 0.0f;
    }

    static std::wstring Plural(int n)
    {
        const int m10 = n % 10, m100 = n % 100;
        const wchar_t* w = (m10 == 1 && m100 != 11) ? L"элемент" : ((m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) ? L"элемента" : L"элементов");
        return std::to_wstring(n) + L" " + w;
    }

    void DrawSettings(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        float y = c.y + 28.0f - m_scroll;
        Header(r, c, y, L"Параметры");
        const float x = c.x + 40.0f, w = std::min(900.0f, c.w - 80.0f);
        const Rect a{x, y, w, 72.0f};
        Imm::Card(r, a);
        r.Glyph(Icon::Folder, {a.x + 20.0f, a.CenterY() - 10.0f, 20.0f, 20.0f}, p.text);
        r.Text(L"Расположения музыкальной библиотеки", {a.x + 60.0f, a.y + 14.0f, w - 80.0f, 22.0f}, p.text, 14.0f);
        r.Text(GetShell().FS().PathOf(GetShell().FS().Music()) + L";  " + GetShell().FS().PathOf(GetShell().FS().Videos()), {a.x + 60.0f, a.y + 36.0f, w - 80.0f, 20.0f},
               p.textSecondary, 12.0f);
        y += 88.0f;
        const Rect b{x, y, w, 72.0f};
        Imm::Card(r, b);
        r.Glyph(Icon::Volume3, {b.x + 20.0f, b.CenterY() - 10.0f, 20.0f, 20.0f}, p.text);
        r.Text(L"Аудиоустройство", {b.x + 60.0f, b.y + 14.0f, w - 80.0f, 22.0f}, p.text, 14.0f);
        r.Text(m_audio.Ok() ? L"Устройство вывода по умолчанию (waveOut, 22 кГц, 16 бит)"
                            : L"Звук синтезируется, но устройство вывода не найдено — воспроизведение идёт без звука",
               {b.x + 60.0f, b.y + 36.0f, w - 80.0f, 20.0f}, p.textSecondary, 12.0f);
        y += 88.0f;
        const Rect d{x, y, w, 96.0f};
        Imm::Card(r, d);
        r.DrawAppIcon(AppIcon::MediaPlayer, {d.x + 20.0f, d.y + 20.0f, 32.0f, 32.0f});
        r.Text(L"Медиаплеер", {d.x + 68.0f, d.y + 16.0f, w - 90.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(L"Версия 11.2409.4.0", {d.x + 68.0f, d.y + 38.0f, w - 90.0f, 20.0f}, p.textSecondary, 12.0f);
        r.Text(L"Музыка синтезируется в реальном времени по содержимому файла; обложки рисуются программно.", {d.x + 68.0f, d.y + 60.0f, w - 90.0f, 20.0f},
               p.textSecondary, 12.0f);
        m_contentH = 0.0f;
    }

    void DrawNowPlaying(Renderer& r, const Rect& c)
    {
        const Palette& p = Theme::P();
        const Track* t = Current();
        if (!t)
        {
            r.Text(L"Ничего не воспроизводится", c, p.textSecondary, 16.0f, FontWeight::Regular, TextFlags::Middle);
            Imm::SubtleButton(r, m_imm, {c.x + 12.0f, c.y + 8.0f, 40.0f, 36.0f}, Icon::ArrowLeft, kHkBack, 0, L"Назад");
            return;
        }
        const double pos = Position();
        if (t->video)
        {
            r.FillRect(c, Color(0, 0, 0));
            if (std::shared_ptr<Gdiplus::Bitmap> bmp = PhotoBitmap(static_cast<int>(Hash32(t->node->id * 977u) % 8) + 1))
            {
                // Медленное панорамирование и масштаб («эффект Кена Бёрнса»).
                const float iw = static_cast<float>(bmp->GetWidth()), ih = static_cast<float>(bmp->GetHeight());
                const float k = static_cast<float>(std::fmod(pos, 20.0) / 20.0);
                const float zoom = 1.05f + 0.12f * (0.5f - 0.5f * std::cos(k * 2.0f * kPi));
                const float s = std::max(c.w / iw, c.h / ih) * zoom;
                const float ox = (iw * s - c.w) * (0.5f + 0.4f * std::sin(static_cast<float>(pos) * 0.11f));
                const float oy = (ih * s - c.h) * (0.5f + 0.4f * std::cos(static_cast<float>(pos) * 0.07f));
                r.Image(bmp.get(), {c.x - ox, c.y - oy, iw * s, ih * s});
            }
            r.FillGradientV({c.x, c.y, c.w, 80.0f}, 0.0f, Color(0, 0, 0, 0x90), Color(0, 0, 0, 0));
            r.Text(t->title, {c.x + 60.0f, c.y + 12.0f, c.w - 120.0f, 28.0f}, Color(255, 255, 255), 16.0f, FontWeight::Semibold, TextFlags::VCenter);
            Imm::SubtleButton(r, m_imm, {c.x + 12.0f, c.y + 8.0f, 40.0f, 36.0f}, Icon::ArrowLeft, kHkBack, 0, L"Назад", false, Color(255, 255, 255));
            return;
        }
        // Музыка: размытый фон цвета обложки, крупная обложка и визуализация.
        const uint32_t h = Hash32(t->node->id * 977u);
        const Color c1 = Theme::HslToRgb(static_cast<float>(h % 360) / 360.0f, 0.45f, p.dark ? 0.22f : 0.55f);
        const Color c2 = Theme::HslToRgb(static_cast<float>((h / 360) % 360) / 360.0f, 0.5f, p.dark ? 0.1f : 0.35f);
        r.FillGradientV(c, 0.0f, c1, c2);
        Imm::SubtleButton(r, m_imm, {c.x + 12.0f, c.y + 8.0f, 40.0f, 36.0f}, Icon::ArrowLeft, kHkBack, 0, L"Назад", false, Color(255, 255, 255));
        const float art = std::min(320.0f, std::min(c.w * 0.4f, c.h - 120.0f));
        const Rect ar{c.x + std::max(60.0f, c.w * 0.12f), c.y + (c.h - art) * 0.5f - 10.0f, art, art};
        r.Shadow(ar, 8.0f, 32.0f, 12.0f, 0.45f);
        DrawArt(r, ar, *t, 8.0f);
        const float tx = ar.Right() + 48.0f, tw = c.Right() - tx - 40.0f;
        const Color white(255, 255, 255), soft(255, 255, 255, 0xC8);
        r.Text(t->title, {tx, ar.y + 20.0f, tw, 44.0f}, white, 32.0f, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(t->artist + L" · " + t->album, {tx, ar.y + 68.0f, tw, 24.0f}, soft, 16.0f);
        r.Text(t->genre + L" · " + std::to_wstring(t->year) + L" · " + TimeText(t->duration), {tx, ar.y + 96.0f, tw, 20.0f}, soft, 13.0f);
        // Визуализация
        const int bars = 32;
        const float bw = std::min(14.0f, tw / static_cast<float>(bars) - 4.0f);
        const float vy = ar.Bottom() - 20.0f;
        const float time = static_cast<float>(pos);
        for (int i = 0; i < bars; ++i)
        {
            float v = 0.15f;
            if (m_playing)
            {
                const float band = static_cast<float>(i) / static_cast<float>(bars);
                v = (1.0f - band) * m_synth.Bass() * 0.8f + band * m_synth.Melody() * 0.7f +
                    0.25f * (0.5f + 0.5f * std::sin(time * (3.0f + static_cast<float>(i) * 0.7f) + static_cast<float>(i)));
            }
            const float bh = 8.0f + Saturate(v) * 120.0f;
            r.FillRoundRect({tx + static_cast<float>(i) * (bw + 4.0f), vy - bh, bw, bh}, 2.0f, Color(255, 255, 255, 0x90));
        }
    }

    void DrawTransport(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect bar{0.0f, Height() - kBarH, Width(), kBarH};
        r.FillRect(bar, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
        r.FillRect({0.0f, bar.y, Width(), 1.0f}, p.divider);
        const Track* t = Current();
        // Слева: обложка и название
        if (t)
        {
            const Rect art{16.0f, bar.y + 20.0f, 56.0f, 56.0f};
            DrawArt(r, art, *t, 4.0f);
            m_imm.Add(art, kHkArt, 0, L"Сейчас воспроизводится");
            const float lw = std::min(260.0f, Width() * 0.25f);
            r.Text(t->title, {84.0f, bar.y + 26.0f, lw, 22.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
            r.Text(t->artist, {84.0f, bar.y + 48.0f, lw, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        // Центр: кнопки
        const float cx = Width() * 0.5f, by = bar.y + 10.0f;
        Imm::SubtleButton(r, m_imm, {cx - 136.0f, by, 36.0f, 36.0f}, Icon::Shuffle, kHkShuffle, 0, m_shuffle ? L"Перемешивание: вкл." : L"Перемешивание: выкл.",
                          false, m_shuffle ? p.accentText : Color(0, 0, 0, 0));
        Imm::SubtleButton(r, m_imm, {cx - 88.0f, by, 36.0f, 36.0f}, Icon::Previous, kHkPrev, 0, L"Назад (Ctrl+B)");
        Imm::RoundButton(r, m_imm, {cx - 20.0f, by - 2.0f, 40.0f, 40.0f}, m_playing ? Icon::Pause : Icon::Play, true, !m_lib.empty(), kHkPlay, 0,
                         m_playing ? L"Пауза (Ctrl+P)" : L"Воспроизвести (Ctrl+P)");
        Imm::SubtleButton(r, m_imm, {cx + 52.0f, by, 36.0f, 36.0f}, Icon::Next, kHkNext, 0, L"Вперёд (Ctrl+F)");
        Imm::SubtleButton(r, m_imm, {cx + 100.0f, by, 36.0f, 36.0f}, Icon::Repeat, kHkRepeat, 0,
                          m_repeat == 0 ? L"Повтор: выкл." : (m_repeat == 1 ? L"Повторять все" : L"Повторять один"), false,
                          m_repeat ? p.accentText : Color(0, 0, 0, 0));
        if (m_repeat == 2)
            r.Text(L"1", {cx + 124.0f, by + 18.0f, 10.0f, 12.0f}, p.accentText, 9.0f, FontWeight::Bold);
        // Перемотка
        const double dur = t ? t->duration : 0.0, pos = Position();
        const float sw = std::min(560.0f, Width() * 0.42f);
        const Rect seek{cx - sw * 0.5f, bar.y + 62.0f, sw, 4.0f};
        m_seekRect = seek;
        r.Text(TimeText(m_dragSeek ? m_dragPos : pos), {seek.x - 64.0f, seek.CenterY() - 9.0f, 56.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::Right | TextFlags::VCenter);
        r.Text(TimeText(dur), {seek.Right() + 8.0f, seek.CenterY() - 9.0f, 56.0f, 18.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        r.FillRoundRect(seek, 2.0f, p.controlStrongStroke.MulAlpha(0.6f));
        const float frac = dur > 0.0 ? static_cast<float>((m_dragSeek ? m_dragPos : pos) / dur) : 0.0f;
        r.FillRoundRect({seek.x, seek.y, seek.w * Saturate(frac), seek.h}, 2.0f, p.accent);
        const bool hot = m_imm.Hot(kHkSeek) || m_dragSeek;
        const float kx = seek.x + seek.w * Saturate(frac);
        r.FillCircle(kx, seek.CenterY(), hot ? 9.0f : 8.0f, p.controlSolid);
        r.StrokeCircle(kx, seek.CenterY(), hot ? 9.0f : 8.0f, p.controlStroke);
        r.FillCircle(kx, seek.CenterY(), hot ? 6.0f : 5.0f, p.accent);
        m_imm.Add(seek.Inflated(4.0f, 10.0f), kHkSeek);
        // Справа: громкость и полноэкранный режим
        const ShellSettings& st = GetShell().Settings();
        const float vx = Width() - 16.0f - 36.0f;
        Imm::SubtleButton(r, m_imm, {vx, bar.y + 30.0f, 36.0f, 36.0f}, Icon::Maximize, kHkFull, 0, L"Сейчас воспроизводится");
        const Rect vs{vx - 12.0f - 100.0f, bar.y + 46.0f, 100.0f, 4.0f};
        m_volRect = vs;
        if (Width() > 760.0f)
        {
            r.FillRoundRect(vs, 2.0f, p.controlStrongStroke.MulAlpha(0.6f));
            r.FillRoundRect({vs.x, vs.y, vs.w * m_volume, vs.h}, 2.0f, p.accent);
            r.FillCircle(vs.x + vs.w * m_volume, vs.CenterY(), 7.0f, p.controlSolid);
            r.StrokeCircle(vs.x + vs.w * m_volume, vs.CenterY(), 7.0f, p.controlStroke);
            r.FillCircle(vs.x + vs.w * m_volume, vs.CenterY(), 4.0f, p.accent);
            m_imm.Add(vs.Inflated(6.0f, 10.0f), kHkVolSlider);
            const Icon vi = st.muted || m_volume <= 0.0f ? Icon::VolumeMute : (m_volume < 0.34f ? Icon::Volume1 : (m_volume < 0.67f ? Icon::Volume2 : Icon::Volume3));
            Imm::SubtleButton(r, m_imm, {vs.x - 44.0f, bar.y + 30.0f, 36.0f, 36.0f}, vi, kHkVolume, 0, m_volume > 0.0f ? L"Отключить звук" : L"Включить звук");
        }
    }

    // ================================================================== ввод
    void Activate(int kind, int index)
    {
        switch (kind)
        {
        case kHkHamburger:
            m_navPref = !m_navPref;
            break;
        case kHkNav:
        {
            static const Page pages[] = {Page::Home, Page::Music, Page::Video, Page::Queue, Page::Playlists, Page::Settings};
            m_page = pages[Clamp(index, 0, 5)];
            m_scroll = 0.0f;
            break;
        }
        case kHkPlay:
            if (m_playing)
                Pause();
            else
                Play();
            break;
        case kHkPrev:
            Prev();
            break;
        case kHkNext:
            Next(true);
            break;
        case kHkShuffle:
            m_shuffle = !m_shuffle;
            break;
        case kHkRepeat:
            m_repeat = (m_repeat + 1) % 3;
            break;
        case kHkVolume:
            m_volume = m_volume > 0.0f ? 0.0f : 0.8f;
            break;
        case kHkArt:
        case kHkFull:
            if (m_page == Page::NowPlaying)
                m_page = m_backPage;
            else
            {
                m_backPage = m_page;
                m_page = Page::NowPlaying;
            }
            break;
        case kHkBack:
            m_page = m_backPage == Page::NowPlaying ? Page::Home : m_backPage;
            break;
        case kHkTrack:
            m_queue.clear();
            for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
            {
                if (m_lib[static_cast<size_t>(i)].video == m_lib[static_cast<size_t>(index)].video)
                    m_queue.push_back(i);
            }
            PlayIndex(index);
            if (m_lib[static_cast<size_t>(index)].video)
            {
                m_backPage = m_page;
                m_page = Page::NowPlaying;
            }
            break;
        case kHkQueueRow:
            PlayIndex(m_queue[static_cast<size_t>(index)]);
            break;
        case kHkPlayAll:
            m_queue.clear();
            for (int i = 0; i < static_cast<int>(m_lib.size()); ++i)
            {
                if (!m_lib[static_cast<size_t>(i)].video)
                    m_queue.push_back(i);
            }
            if (!m_queue.empty())
                PlayIndex(m_shuffle ? m_queue[static_cast<size_t>(RandomInt(0, static_cast<int>(m_queue.size()) - 1))] : m_queue.front());
            break;
        default:
            break;
        }
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        m_imm.Move(e.pos, {Frame().x, Frame().y});
        if (m_dragSeek && Current())
            m_dragPos = Saturate((e.pos.x - m_seekRect.x) / m_seekRect.w) * Current()->duration;
        if (m_dragVol)
            m_volume = Saturate((e.pos.x - m_volRect.x) / m_volRect.w);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_imm.Leave();
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        Window::OnMouseDown(e);
        if (HasDialog() || m_ui.HasCapture())
            return;
        m_imm.Down(e.pos, e.clicks);
        const ImmHot* h = m_imm.Hit(e.pos);
        if (h && h->kind == kHkSeek && Current() && e.button == MouseButton::Left)
        {
            m_dragSeek = true;
            m_dragPos = Saturate((e.pos.x - m_seekRect.x) / m_seekRect.w) * Current()->duration;
        }
        if (h && h->kind == kHkVolSlider && e.button == MouseButton::Left)
        {
            m_dragVol = true;
            m_volume = Saturate((e.pos.x - m_volRect.x) / m_volRect.w);
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog())
            return;
        if (m_dragSeek)
        {
            m_dragSeek = false;
            SeekTo(m_dragPos);
        }
        m_dragVol = false;
        int kind = 0, index = 0;
        if (m_imm.Up(e.pos, kind, index) && e.button == MouseButton::Left && kind != kHkSeek && kind != kHkVolSlider)
            Activate(kind, index);
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (HasDialog())
            return true;
        const Rect c = ContentRect();
        if (c.Contains(e.pos) && m_page != Page::NowPlaying)
            m_scroll = Clamp(m_scroll - e.wheel * 90.0f, 0.0f, std::max(0.0f, m_contentH - c.h));
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return Window::OnKeyDown(vk, mods);
        if (vk == VK_SPACE || (mods.ctrl && vk == 'P'))
        {
            Activate(kHkPlay, 0);
            return true;
        }
        if (mods.ctrl && vk == 'F')
        {
            Next(true);
            return true;
        }
        if (mods.ctrl && vk == 'B')
        {
            Prev();
            return true;
        }
        if (vk == VK_RIGHT || vk == VK_LEFT)
        {
            SeekTo(Position() + (vk == VK_RIGHT ? 10.0 : -10.0));
            return true;
        }
        if (vk == VK_ESCAPE && m_page == Page::NowPlaying)
        {
            Activate(kHkBack, 0);
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

    // ================================================================== поля
    std::vector<Track> m_lib;
    std::vector<int> m_queue;
    int m_cur = -1;
    bool m_playing = false;
    double m_pos = 0.0;
    double m_startClock = 0.0;
    long long m_genFrame = 0;
    bool m_shuffle = false;
    int m_repeat = 1; // 0 — нет, 1 — все, 2 — один
    float m_volume = 0.8f;
    Synth m_synth;
    AudioOut m_audio;
    Page m_page = Page::Home;
    Page m_backPage = Page::Home;
    bool m_navPref = true;
    float m_scroll = 0.0f;
    float m_contentH = 0.0f;
    ImmUi m_imm;
    Rect m_seekRect;
    Rect m_volRect;
    bool m_dragSeek = false;
    double m_dragPos = 0.0;
    bool m_dragVol = false;
};
} // namespace

std::unique_ptr<Window> CreateMediaPlayer(const std::wstring& arg) { return std::make_unique<MediaWindow>(arg); }
