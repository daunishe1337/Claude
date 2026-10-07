// ============================================================================
//  Parents.cpp — родители на передних сиденьях (см. Parents.h).
//
//  Ссора записана заранее и разбита на «перепалки» по 2–5 реплик. Перепалки
//  сгруппированы в акты, которые раскрывают историю постепенно:
//    1) поздно выехали от бабушки (огурцы, «чайку на дорожку», папа уснул);
//    2) папа свернул на старую лесную дорогу, нет связи, не заправился;
//    3) деньги, работа, «не при ребёнке»;
//    4) признание (папу сокращают — вот почему он экономил на бензине);
//    5) усталое затишье после признания; у самого дома — примирение.
//  Акт выбирается по времени поездки, перепалка внутри акта — случайно (по
//  seed). На внешние события родители отвечают короткими реакциями: реакция
//  дожидается, пока говорящий договорит, затем ссора продолжается с того же
//  места. Срочные события (двигатель глохнет, приехали) обрывают реплику на
//  полуслове; оборванную в начале реплику потом повторяют. Монстра родители
//  не замечают никогда — всему находится будничное объяснение.
//
//  Модуль ничего не рисует; в update() нет выделений памяти.
// ============================================================================
#include "Parents.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace {

// ============================================================================
//  Сценарий
// ============================================================================
// Кому адресована реплика (определяет, куда повёрнуты головы).
enum class To : uint8_t {
    Partner, // друг другу (ссора)
    Kid,     // ребёнку: мама оборачивается назад, папа смотрит в зеркало
    Ahead,   // в пустоту / себе под нос: взгляд на дорогу
};

struct Line {
    Speaker who = Speaker::Dad;
    To to = To::Partner;
    const char* text = nullptr; // nullptr — конец списка реплик
};

Line dad(const char* text, To to = To::Partner) { return Line{Speaker::Dad, to, text}; }
Line mom(const char* text, To to = To::Partner) { return Line{Speaker::Mom, to, text}; }

// Акты истории. Reconcile — примирение, звучит только после «почти приехали».
enum class Act : uint8_t { Opening, Road, Pressure, Reveal, Aftermath, Reconcile };

constexpr int kMaxLines = 5;

struct Exchange {
    Act act;
    float heat; // накал перепалки 0..1 (жестикуляция, тон)
    Line lines[kMaxLines];
};

template <typename T, size_t N>
constexpr int countOf(const T (&)[N]) {
    return static_cast<int>(N);
}

// clang-format off
const Exchange kExchanges[] = {
    // ---- Акт 1: поздно выехали ----------------------------------------------
    {Act::Opening, 0.30f, {
        mom(T8("Говорила же: выезжаем в шесть, пока светло.")),
        dad(T8("А кто полтора часа банки по сумкам раскладывал?")),
        mom(T8("Мама старалась. Огурцы на всю зиму, между прочим.")),
        dad(T8("Огурцы. Едем ночью из-за огурцов.")),
    }},
    {Act::Opening, 0.25f, {
        mom(T8("Солнышко, не сиди всю дорогу в своей игре."), To::Kid),
        mom(T8("Глаза испортишь. Посмотри лучше в окно."), To::Kid),
        dad(T8("А что там смотреть? Темень одна.")),
        mom(T8("Спасибо. Очень помог.")),
    }},
    {Act::Opening, 0.35f, {
        dad(T8("Выехали бы раньше, если б не «чайку на дорожку».")),
        mom(T8("А кто после обеда уснул на диване?")),
        dad(T8("Я отдыхал перед дорогой. Это разные вещи.")),
        mom(T8("Ты храпел так, что кот ушёл из комнаты.")),
    }},
    {Act::Opening, 0.20f, {
        dad(T8("Эй, на заднем сиденье, не спишь там?"), To::Kid),
        mom(T8("Спи, солнышко. Ещё далеко."), To::Kid),
        dad(T8("Не так уж и далеко. Час, от силы полтора.")),
        mom(T8("Ты это говорил два часа назад.")),
    }},
    // ---- Акт 2: старая дорога через лес --------------------------------------
    {Act::Road, 0.45f, {
        mom(T8("Зачем ты свернул с трассы? Нормально же ехали.")),
        dad(T8("По трассе крюк в сорок километров. А тут напрямик.")),
        mom(T8("Напрямик. Через лес. Ночью. С ребёнком.")),
        dad(T8("Я тут с отцом ездил, когда был маленький.")),
        mom(T8("Вот именно. Сто лет назад.")),
    }},
    {Act::Road, 0.40f, {
        mom(T8("Обещала маме написать, как выедем. А связи нет.")),
        dad(T8("Утром напишешь. Она уже десятый сон видит.")),
        mom(T8("Она не ляжет, пока я не напишу. Ты же знаешь.")),
        dad(T8("Знаю. Вся область знает.")),
    }},
    {Act::Road, 0.35f, {
        mom(T8("Навигатор показывает, что мы едем по полю.")),
        dad(T8("Он эту дорогу просто не знает. Её нет на картах.")),
        mom(T8("А почему её нет на картах?")),
        dad(T8("Потому что карты рисуют в городе. Не нагнетай.")),
    }},
    {Act::Road, 0.35f, {
        mom(T8("За час ни одной машины навстречу. Тебе не странно?")),
        dad(T8("Ночь. Нормальные люди спят.")),
        mom(T8("А мы, значит, ненормальные.")),
        dad(T8("А мы едем домой. Короткой дорогой.")),
    }},
    {Act::Road, 0.40f, {
        mom(T8("По-моему, здесь давно никто не ездит.")),
        dad(T8("Раньше тут автобус ходил. До Ольховки.")),
        mom(T8("А сейчас?")),
        dad(T8("А сейчас и Ольховки нет. Все разъехались.")),
        mom(T8("Прекрасно. Едем через деревню, которой нет.")),
    }},
    {Act::Road, 0.50f, {
        mom(T8("Мы же проезжали заправку. Почему не остановился?")),
        dad(T8("Там цены как в аэропорту. До дома хватит.")),
        mom(T8("А если не хватит?")),
        dad(T8("Хватит. Я всё посчитал.")),
    }},
    {Act::Road, 0.30f, {
        mom(T8("Включи хоть радио. Тишина давит.")),
        dad(T8("Тут ничего не ловит. Одно шипение.")),
        mom(T8("Куда ты нас завёз, что даже радио не ловит?")),
        dad(T8("В лес. Природа. Ты же любишь природу.")),
    }},
    // ---- Акт 3: деньги и работа ----------------------------------------------
    {Act::Pressure, 0.60f, {
        dad(T8("Мне завтра к восьми на работу. Поспать бы часок.")),
        mom(T8("Мог бы отпроситься. Один раз в жизни.")),
        dad(T8("Сейчас не то время, чтобы отпрашиваться.")),
        mom(T8("А когда оно было, то время?")),
    }},
    {Act::Pressure, 0.60f, {
        mom(T8("Мама опять сунула мне в сумку деньги.")),
        dad(T8("Верни. Мы не нищие.")),
        mom(T8("Она от души. И нам сейчас правда не помешает.")),
        dad(T8("Нам сейчас нормально. Я разберусь.")),
    }},
    {Act::Pressure, 0.70f, {
        mom(T8("Кто тебе весь вечер звонил? Ты трижды выходил.")),
        dad(T8("С работы. Ничего особенного.")),
        mom(T8("В субботу вечером? Ничего особенного?")),
        dad(T8("Давай не сейчас. Не при ребёнке.")),
    }},
    {Act::Pressure, 0.85f, {
        dad(T8("Сколько можно меня пилить? Я пятый час за рулём!")),
        mom(T8("Не кричи, пожалуйста. Ребёнок не спит.")),
        mom(T8("Солнышко, всё хорошо. Мы просто устали."), To::Kid),
        dad(T8("Да. Просто устали. Играй."), To::Kid),
    }},
    {Act::Pressure, 0.65f, {
        mom(T8("Пора продавать эту машину. Вечно что-то стучит.")),
        dad(T8("А новую на какие деньги? Ипотека ещё восемь лет.")),
        mom(T8("Я просто сказала.")),
        dad(T8("Ты всегда «просто говоришь».")),
    }},
    // ---- Акт 4: признание ------------------------------------------------------
    {Act::Reveal, 0.90f, {
        mom(T8("Что у тебя на работе? Только честно.")),
        dad(T8("...Сокращение. С первого числа я там не работаю.")),
        mom(T8("И ты молчал? Всё лето молчал?")),
        dad(T8("Не хотел портить поездку. Маме не говори.")),
        mom(T8("Так вот почему ты не стал заправляться.")),
    }},
    // ---- Акт 5: после признания ------------------------------------------------
    {Act::Aftermath, 0.45f, {
        mom(T8("Почему ты мне сразу не сказал?")),
        dad(T8("Думал, найду что-нибудь, пока ты не узнала.")),
        mom(T8("Мы бы вместе что-нибудь придумали.")),
        dad(T8("Я знаю. Прости.")),
    }},
    {Act::Aftermath, 0.40f, {
        dad(T8("Брат звал к себе в автосервис. Наверное, пойду.")),
        mom(T8("Ты же терпеть не можешь работать с братом.")),
        dad(T8("Я много чего терпеть не могу. Пойду.")),
    }},
    {Act::Aftermath, 0.20f, {
        mom(T8("Солнышко, ты не волнуйся. Всё будет хорошо."), To::Kid),
        dad(T8("Слушай маму. Всё будет хорошо."), To::Kid),
        mom(T8("Сначала доедем, а там разберёмся.")),
        dad(T8("Доедем. Куда мы денемся.")),
    }},
    {Act::Aftermath, 0.25f, {
        mom(T8("Зато мамины огурцы теперь очень кстати.")),
        dad(T8("Будем жить на огурцах.")),
        mom(T8("И на варенье. Там ещё четыре банки.")),
        dad(T8("Значит, до весны продержимся.")),
    }},
    {Act::Aftermath, 0.25f, {
        mom(T8("Знаешь, что смешно? Мама тебя сегодня хвалила.")),
        dad(T8("Меня? Твоя мама?")),
        mom(T8("Сказала: «Хоть машину водит хорошо».")),
        dad(T8("Ну, хоть что-то.")),
    }},
    // ---- Примирение (после «почти приехали») -----------------------------------
    {Act::Reconcile, 0.10f, {
        dad(T8("Слушай... Прости, что я сегодня так.")),
        mom(T8("И ты меня прости. Дома поговорим спокойно.")),
        dad(T8("Договорились.")),
    }},
};
// clang-format on

constexpr int kExchangeCount = countOf(kExchanges);

// ============================================================================
//  Реакции на события
// ============================================================================
constexpr int kMaxReactionLines = 3;
constexpr int kMaxVariants = 8;

struct Reaction {
    Line lines[kMaxReactionLines];
};

// clang-format off
// Удар по крыше — «ветка», «яма», «шишка»…
const Reaction kRoofThud[] = {
    {{mom(T8("Ой. Это что было?"), To::Ahead), dad(T8("Ветка, наверное. Лес же кругом."))}},
    {{dad(T8("Ух ты, яма. Дорога тут ужасная."), To::Ahead)}},
    {{mom(T8("Что-то по крыше стукнуло."), To::Ahead), dad(T8("Шишка упала. Тут одни сосны."))}},
    {{mom(T8("Ты слышал?")), dad(T8("Подвеска стучит. Давно пора в сервис."))}},
    {{dad(T8("Опять банки в багажнике гремят."), To::Ahead),
      mom(T8("Только попробуй разбить мамины огурцы."))}},
    {{mom(T8("Господи, что это?"), To::Ahead), dad(T8("Птица, наверное. Не пугай ребёнка."))}},
};
// Удар по стеклу — ветер, гравий, ребёнок балуется.
const Reaction kWindowBang[] = {
    {{mom(T8("Ветер какой..."), To::Ahead)}},
    {{dad(T8("Гравий из-под колёс. Тут же не асфальт."), To::Ahead)}},
    {{mom(T8("Что это в стекло ударило?")), dad(T8("Ветка хлестнула. Лес у самой дороги."))}},
    {{mom(T8("Солнышко, не стучи по стеклу."), To::Kid)}},
    {{dad(T8("Бабочка в стекло. Здоровая какая."), To::Ahead), mom(T8("Фу. Не рассказывай."))}},
    {{mom(T8("Солнышко, не облокачивайся на дверь."), To::Kid)}},
};
// Вспышка камеры в салоне.
const Reaction kCameraFlash[] = {
    {{mom(T8("Не слепи папу, он за рулём!"), To::Kid)}},
    {{dad(T8("Убери вспышку, ты что! Я чуть в кювет не съехал."), To::Kid)}},
    {{mom(T8("Откуда у тебя фотоаппарат? Убери, пожалуйста."), To::Kid)}},
    {{dad(T8("Так. Ещё одна вспышка, и игра едет в багажник."), To::Kid)}},
    {{mom(T8("Что ты там фотографируешь? Темно же."), To::Kid),
      dad(T8("Лес фотографирует. Талант растёт."))}},
    {{dad(T8("Ай! Глаза! Предупреждать же надо!"), To::Kid)}},
};
// Мало бензина — папа бормочет про заправку, которую проехал.
const Reaction kLowFuel[] = {
    {{dad(T8("Так. Лампочка бензина загорелась."), To::Ahead), mom(T8("Ты же говорил, что хватит.")),
      dad(T8("Должно хватить. Должно."), To::Ahead)}},
    {{dad(T8("Надо было всё-таки на той заправке остановиться..."), To::Ahead)}},
    {{dad(T8("Ну давай, родная, дотяни до дома."), To::Ahead),
      mom(T8("Ты что, с машиной разговариваешь?"))}},
    {{mom(T8("Что там мигает? Бензин?")), dad(T8("Мигает. Там ещё литров пять, не паникуй."))}},
    {{dad(T8("Сэкономил на бензине. Молодец. Гений."), To::Ahead)}},
};
// Писк консоли.
const Reaction kConsoleCrash[] = {
    {{dad(T8("Сделай потише свою игру."), To::Kid)}},
    {{mom(T8("Солнышко, выключи звук, папа нервничает."), To::Kid), dad(T8("Я не нервничаю."))}},
    {{dad(T8("Что там у тебя пищит? Убавь звук."), To::Kid)}},
    {{mom(T8("Что, опять машинка разбилась? Ничего, бывает."), To::Kid)}},
    {{mom(T8("Можно без этого писка? Голова и так болит."), To::Kid)}},
};
// Двигатель чихает.
const Reaction kStalling[] = {
    {{dad(T8("Нет-нет-нет, только не сейчас..."), To::Ahead)}},
    {{dad(T8("Давай, давай, не глохни!"), To::Ahead), mom(T8("Что с машиной?!"))}},
    {{mom(T8("Почему она дёргается?")), dad(T8("Не знаю! Ну давай же..."), To::Ahead)}},
    {{dad(T8("Только не здесь. Только не посреди леса..."), To::Ahead)}},
    {{mom(T8("Мы же тут не встанем, правда?")), dad(T8("Не встанем. Не встанем..."), To::Ahead)}},
};
// Скоро дом (после реакции звучит примирение).
const Reaction kNearHome[] = {
    {{dad(T8("Вон огни за полем. Почти приехали.")), mom(T8("Наконец-то."))}},
    {{mom(T8("Это наш поворот? Неужели.")), dad(T8("Он самый. Почти приехали."))}},
    {{dad(T8("Вон водонапорная башня. Почти дома.")), mom(T8("Слава богу."))}},
};
// Приехали.
const Reaction kArrived[] = {
    {{dad(T8("Ну вот. Дома."), To::Ahead), mom(T8("Солнышко, просыпайся. Приехали."), To::Kid)}},
    {{mom(T8("Приехали. Неужели."), To::Ahead), dad(T8("Эй, на заднем. Вылезаем, мы дома."), To::Kid)}},
    {{dad(T8("Всё. Дома. Глушу мотор."), To::Ahead),
      mom(T8("Собирай свою игру, солнышко. Мы дома."), To::Kid)}},
};
// clang-format on

struct CueInfo {
    const Reaction* variants;
    int count;
    float cooldown; // перезарядка после начала реакции, с
    float expiry;   // сколько реакция может ждать своей очереди, с
    float heat;     // накал реакции (для tension)
    int priority;   // при конфликте ожидающих реакций побеждает более важная
    bool once;      // один раз за поездку
    bool urgent;    // обрывает текущую реплику, не дожидаясь конца фразы
};

constexpr float kNever = 1.0e9f;

// Порядок — как в enum ParentCue.
const CueInfo kCues[] = {
    {kRoofThud, countOf(kRoofThud), 28.0f, 6.0f, 0.35f, 5, false, false},
    {kWindowBang, countOf(kWindowBang), 26.0f, 5.0f, 0.25f, 3, false, false},
    {kCameraFlash, countOf(kCameraFlash), 24.0f, 5.0f, 0.45f, 4, false, false},
    {kLowFuel, countOf(kLowFuel), 35.0f, 20.0f, 0.50f, 6, false, false},
    {kConsoleCrash, countOf(kConsoleCrash), 30.0f, 5.0f, 0.30f, 2, false, false},
    {kStalling, countOf(kStalling), 12.0f, 4.0f, 0.75f, 8, false, true},
    {kNearHome, countOf(kNearHome), kNever, 15.0f, 0.10f, 7, true, false},
    {kArrived, countOf(kArrived), kNever, kNever, 0.05f, 10, true, true},
};

constexpr int kCueCount = static_cast<int>(ParentCue::Arrived) + 1;
static_assert(countOf(kCues) == kCueCount, "kCues must follow the ParentCue order");
static_assert(countOf(kRoofThud) <= kMaxVariants && countOf(kWindowBang) <= kMaxVariants &&
                  countOf(kCameraFlash) <= kMaxVariants && countOf(kLowFuel) <= kMaxVariants &&
                  countOf(kConsoleCrash) <= kMaxVariants && countOf(kStalling) <= kMaxVariants &&
                  countOf(kNearHome) <= kMaxVariants && countOf(kArrived) <= kMaxVariants,
              "too many reaction variants");

// ============================================================================
//  Темп речи и паузы
// ============================================================================
constexpr float kTypeSpeed = 28.0f;      // символов в секунду (печатная машинка)
constexpr float kFadeIn = 0.15f;         // появление субтитра
constexpr float kFadeOut = 0.35f;        // исчезание субтитра
constexpr float kFirstExchangeAt = 3.0f; // первая перепалка после reset()
constexpr float kSilenceMin = 6.0f;      // тишина между перепалками
constexpr float kSilenceMax = 14.0f;
constexpr float kLineGapMin = 0.35f;     // пауза между репликами перепалки
constexpr float kLineGapMax = 0.90f;
constexpr float kReactionGap = 0.2f;     // пауза перед реакцией на событие
constexpr float kAfterReaction = 3.0f;   // минимум тишины после реакции
constexpr float kReconcileDelay = 1.5f;  // примирение после «почти приехали»
constexpr float kAfterReconcile = 40.0f; // дальше молчат (если дом всё не приходит)
constexpr float kAnticipate = 0.3f;      // голова поворачивается чуть раньше реплики
// Начало актов по времени поездки (Opening, Road, Pressure, Reveal).
constexpr float kActStart[] = {0.0f, 50.0f, 135.0f, 200.0f};

// Длительность субтитра по числу символов.
float lineDuration(int chars) {
    return clampf(1.6f + 0.065f * static_cast<float>(chars), 2.2f, 6.5f);
}
// Сколько человек реально говорит (голова качается, звучит «бубнёж»);
// остаток длительности — время дочитать субтитр.
float speechDuration(int chars, float duration) {
    return clampf(0.35f + static_cast<float>(chars) / 13.0f, 0.8f, duration - 0.3f);
}

// Число символов UTF-8 (кодовых точек): считаем все байты, кроме продолжений.
int utf8Length(const char* s) {
    int n = 0;
    for (; *s; ++s)
        if ((static_cast<unsigned char>(*s) & 0xC0u) != 0x80u) ++n;
    return n;
}

int lineCount(const Line* lines, int maxCount) {
    int n = 0;
    while (n < maxCount && lines[n].text) ++n;
    return n;
}

int exchangeLength(int idx) { return lineCount(kExchanges[idx].lines, kMaxLines); }
int reactionLength(int cue, int variant) {
    return lineCount(kCues[cue].variants[variant].lines, kMaxReactionLines);
}

int speakerIndex(Speaker s) { return s == Speaker::Dad ? 0 : 1; }

int findAct(Act a) {
    for (int i = 0; i < kExchangeCount; ++i)
        if (kExchanges[i].act == a) return i;
    return -1;
}

// Взгляд в сторону: включается и выключается по таймеру.
struct Glance {
    bool on = false;
    float timer = 0.0f;
};

enum class Phase : uint8_t {
    Silence,  // между перепалками
    Gap,      // пауза между репликами внутри перепалки/реакции
    Speaking, // звучит реплика
};

} // namespace

// ============================================================================
//  Состояние
// ============================================================================
struct Parents::State {
    Rng rng{1u};        // пересевается в reset()
    float clock = 0.0f; // время с reset()

    Phase phase = Phase::Silence;
    float phaseT = 0.0f;         // время в текущей фазе (в Speaking — от начала реплики)
    float gapLeft = 0.0f;        // Gap: сколько ещё до следующей реплики
    float nextExchangeAt = 0.0f; // Silence: когда начать следующую перепалку

    // Текущая перепалка: индекс и номер звучащей (в паузе — следующей) реплики.
    int exIdx = -1;
    int exPos = 0;
    float exHeat = 0.0f;
    bool momSulks = false; // слушая папу, мама отворачивается к окну

    // Текущая реакция на событие.
    int reCue = -1;
    int reVariant = 0;
    int rePos = 0;

    // Реакция, ждущая своей очереди (не больше одной).
    int pending = -1;
    float pendingAge = 0.0f;

    // Реплика (после её окончания указатель хранится для позы в паузе).
    const Line* line = nullptr;
    int lineChars = 0;
    float lineDur = 0.0f;
    float speechDur = 0.0f;
    float lineHeat = 0.0f;
    float cutAt = -1.0f; // когда реплику оборвали (-1 — договорили)
    uint32_t lineSeed = 0;
    Speaker lastSpeaker = Speaker::Dad;

    // Событие «началась реплика» для звука.
    bool started = false;
    Speaker startedWho = Speaker::Dad;
    float startedDur = 0.0f;

    // Сюжет.
    std::array<bool, kExchangeCount> played{};
    std::array<float, kExchangeCount> lastPlayed{};
    int revealIdx = -1;
    int reconcileIdx = -1;
    bool freeMode = false; // весь сценарий уже прозвучал: повторяем самые давние
    bool reconcileQueued = false;
    bool finished = false; // приехали: больше не спорят

    // Реакции: перезарядка, «один раз», порядок вариантов (перемешан в reset).
    std::array<float, kCueCount> cooldown{};
    std::array<bool, kCueCount> used{};
    std::array<std::array<uint8_t, kMaxVariants>, kCueCount> order{};
    std::array<int, kCueCount> cursor{};

    // Анимация: [0] — папа, [1] — мама. Свой генератор, чтобы настройка
    // анимации не меняла выбор реплик.
    Rng animRng{2u};
    std::array<float, 2> talk{};
    std::array<float, 2> turn{};
    float tension = 0.0f;
    // Взгляды: папа отрывается от дороги (на маму / в зеркало),
    // мама в тишине подолгу смотрит в своё окно.
    std::array<Glance, 2> glance{};
    uint32_t murmurSeed = 0;

    void step(float dt);
    // Реплики и паузы.
    void startLine(const Line& l, float baseHeat);
    void startExchangeLine();
    void startReactionLine();
    void enterSilence(float delay);
    void enterGap(float gap);
    void finishLine();
    const Line* upcomingLine() const;
    // Сюжет.
    int pickUnplayed(Act act);
    int pickExchange();
    void startExchange(int idx);
    void startReaction();
    // Анимация.
    void animate(float dt);
    void updateGlances(float dt);
    float speechLevel() const;
    float listenerMurmur() const;
    void headTargets(const Line& l, float& dadTurn, float& momTurn) const;
};

// ---- Шаг логики ------------------------------------------------------------------
void Parents::State::step(float dt) {
    clock += dt;
    phaseT += dt;
    for (float& c : cooldown) c = std::max(0.0f, c - dt);
    if (pending >= 0) {
        pendingAge += dt;
        if (pendingAge > kCues[pending].expiry) pending = -1; // момент упущен
    }

    // Реакция может начаться, когда текущая последовательность её пропускает:
    // посреди реакции ждут её конца (кроме срочных событий).
    const bool canReact = pending >= 0 && (reCue < 0 || kCues[pending].urgent);
    switch (phase) {
    case Phase::Speaking:
        // Ждущая реакция укорачивает «хвост» реплики: срочная — сразу,
        // обычная — как только говорящий договорит фразу.
        if (canReact && (kCues[pending].urgent || phaseT >= speechDur) && phaseT + kFadeOut < lineDur) {
            lineDur = phaseT + kFadeOut;
            if (cutAt < 0.0f) cutAt = phaseT;
        }
        if (phaseT >= lineDur) finishLine();
        break;
    case Phase::Gap:
        gapLeft -= dt;
        if (canReact && phaseT >= kReactionGap) {
            startReaction();
        } else if (gapLeft <= 0.0f) {
            if (reCue >= 0) startReactionLine();
            else if (exIdx >= 0) startExchangeLine();
            else enterSilence(kAfterReaction); // защита: продолжать нечего
        }
        break;
    case Phase::Silence:
        if (pending >= 0 && phaseT >= kReactionGap) {
            startReaction();
        } else if (!finished && clock >= nextExchangeAt) {
            if (reconcileQueued) {
                reconcileQueued = false;
                startExchange(reconcileIdx);
            } else {
                startExchange(pickExchange());
            }
        }
        break;
    }
    animate(dt);
}

// ---- Реплики -------------------------------------------------------------------------
void Parents::State::startLine(const Line& l, float baseHeat) {
    line = &l;
    phase = Phase::Speaking;
    phaseT = 0.0f;
    lineChars = utf8Length(l.text);
    lineDur = lineDuration(lineChars);
    speechDur = speechDuration(lineChars, lineDur);
    cutAt = -1.0f;
    lineSeed = rng.nextU32();
    lastSpeaker = l.who;
    float heat = baseHeat + (std::strchr(l.text, '!') ? 0.12f : 0.0f);
    if (l.to == To::Kid) heat *= 0.6f; // с ребёнком говорят мягче
    lineHeat = saturate(heat);
    // К ребёнку обращаются — папа сразу глядит в зеркало; в запале
    // он поворачивается к маме в начале своей реплики.
    Glance& g = glance[0];
    if (l.to == To::Kid || (l.who == Speaker::Dad && l.to == To::Partner && lineHeat > 0.55f)) {
        g.on = true;
        g.timer = animRng.range(0.9f, 1.4f);
    }
    started = true;
    startedWho = l.who;
    startedDur = speechDur;
}

void Parents::State::startExchangeLine() {
    startLine(kExchanges[exIdx].lines[exPos], exHeat + 0.04f * static_cast<float>(exPos));
}

void Parents::State::startReactionLine() {
    // Реакция посреди ссоры не гасит её накал полностью.
    const float heat = exIdx >= 0 ? std::max(kCues[reCue].heat, 0.85f * exHeat) : kCues[reCue].heat;
    startLine(kCues[reCue].variants[reVariant].lines[rePos], heat);
}

void Parents::State::enterSilence(float delay) {
    phase = Phase::Silence;
    phaseT = 0.0f;
    nextExchangeAt = clock + delay;
}

void Parents::State::enterGap(float gap) {
    phase = Phase::Gap;
    phaseT = 0.0f;
    gapLeft = gap;
}

// Реплика отзвучала: что дальше.
void Parents::State::finishLine() {
    if (reCue >= 0) {
        ++rePos;
        if (rePos < reactionLength(reCue, reVariant)) {
            enterGap(rng.range(kLineGapMin, 0.6f));
            return;
        }
        reCue = -1;
        if (exIdx >= 0) { // ссора продолжается с прерванного места
            enterGap(rng.range(0.6f, kLineGapMax));
            return;
        }
        if (reconcileQueued) enterSilence(kReconcileDelay);
        else enterSilence(std::max(nextExchangeAt - clock, kAfterReaction));
        return;
    }
    if (exIdx < 0) { // защита: реплика вне перепалки и реакции — просто замолкают
        enterSilence(kAfterReaction);
        return;
    }
    // Реплику, которую перебили в самом начале, после реакции повторят заново.
    const bool interrupted = cutAt >= 0.0f && cutAt < 0.6f * speechDur;
    if (!interrupted) ++exPos;
    if (exPos < exchangeLength(exIdx)) {
        // Чем жарче спор, тем быстрее перебивают друг друга.
        const float gap = lerpf(kLineGapMax, kLineGapMin, exHeat) + rng.range(-0.1f, 0.1f);
        enterGap(clampf(gap, kLineGapMin, kLineGapMax));
        return;
    }
    const bool wasReconcile = exIdx == reconcileIdx;
    exIdx = -1;
    enterSilence(wasReconcile ? kAfterReconcile : rng.range(kSilenceMin, kSilenceMax));
}

// Следующая реплика текущей последовательности (для упреждающего поворота головы).
const Line* Parents::State::upcomingLine() const {
    if (reCue >= 0) {
        if (rePos < reactionLength(reCue, reVariant)) return &kCues[reCue].variants[reVariant].lines[rePos];
        return nullptr;
    }
    if (exIdx >= 0 && exPos < exchangeLength(exIdx)) return &kExchanges[exIdx].lines[exPos];
    return nullptr;
}

// ---- Выбор перепалки -------------------------------------------------------------
// Случайная ещё не звучавшая перепалка акта или -1.
int Parents::State::pickUnplayed(Act act) {
    int n = 0;
    for (int i = 0; i < kExchangeCount; ++i)
        if (kExchanges[i].act == act && !played[static_cast<size_t>(i)]) ++n;
    if (n == 0) return -1;
    int k = rng.rangeInt(0, n - 1);
    for (int i = 0; i < kExchangeCount; ++i) {
        if (kExchanges[i].act != act || played[static_cast<size_t>(i)]) continue;
        if (k-- == 0) return i;
    }
    return -1;
}

int Parents::State::pickExchange() {
    if (!freeMode) {
        // Акт по времени; признание — один раз, после него только «затишье».
        int want = 0;
        for (int a = 0; a < countOf(kActStart); ++a)
            if (clock >= kActStart[a]) want = a;
        if (revealIdx >= 0 && played[static_cast<size_t>(revealIdx)]) want = static_cast<int>(Act::Aftermath);
        // Сначала текущий акт, потом недосказанное из прошлых, потом — вперёд по сюжету.
        for (int a = want; a >= 0; --a) {
            const int i = pickUnplayed(static_cast<Act>(a));
            if (i >= 0) return i;
        }
        for (int a = want + 1; a <= static_cast<int>(Act::Aftermath); ++a) {
            const int i = pickUnplayed(static_cast<Act>(a));
            if (i >= 0) return i;
        }
        freeMode = true;
    }
    // Всё уже сказано: случайная из трёх самых давних (кроме признания).
    int oldest[3] = {-1, -1, -1};
    for (int i = 0; i < kExchangeCount; ++i) {
        const Act a = kExchanges[i].act;
        if (a == Act::Reveal || a == Act::Reconcile) continue;
        // Вставка в отсортированный по давности список из трёх.
        int c = i;
        for (int& slot : oldest) {
            if (slot < 0 || lastPlayed[static_cast<size_t>(c)] < lastPlayed[static_cast<size_t>(slot)]) {
                std::swap(slot, c);
                if (c < 0) break;
            }
        }
    }
    int n = 0;
    while (n < 3 && oldest[n] >= 0) ++n;
    return oldest[rng.rangeInt(0, n - 1)];
}

void Parents::State::startExchange(int idx) {
    exIdx = idx;
    exPos = 0;
    exHeat = kExchanges[idx].heat;
    momSulks = exHeat > 0.55f && rng.chance(0.5f);
    played[static_cast<size_t>(idx)] = true;
    lastPlayed[static_cast<size_t>(idx)] = clock;
    startExchangeLine();
}

// ---- Реакции ----------------------------------------------------------------------
void Parents::State::startReaction() {
    const int c = pending;
    pending = -1;
    const CueInfo& info = kCues[c];
    if (c == static_cast<int>(ParentCue::Arrived)) {
        finished = true; // приехали: ссора окончена
        exIdx = -1;
    } else if (c == static_cast<int>(ParentCue::NearHome)) {
        exIdx = -1; // «почти приехали» гасит спор, дальше — примирение
        reconcileQueued = reconcileIdx >= 0;
    }
    const size_t ci = static_cast<size_t>(c);
    reCue = c;
    rePos = 0;
    reVariant = order[ci][static_cast<size_t>(cursor[ci] % info.count)];
    ++cursor[ci];
    cooldown[ci] = info.cooldown;
    used[ci] = true;
    startReactionLine();
}

// ---- Анимация ------------------------------------------------------------------------
// Активность речи 0..1: огибающая фразы × неровные «слоги» × паузы между словами.
float Parents::State::speechLevel() const {
    const float t = phaseT;
    if (t >= speechDur) return 0.0f;
    const float env = smoothstep(0.0f, 0.08f, t) * (1.0f - smoothstep(speechDur - 0.15f, speechDur, t));
    const float ph = static_cast<float>(lineSeed & 1023u) * (kTau / 1024.0f);
    // Темп слогов «плавает» около 4.6 в секунду, ударные слоги громче.
    const float beat = 4.6f * t + 0.8f * valueNoise(t * 1.5f, lineSeed + 3u);
    const float syllable = 0.5f + 0.5f * std::sin(kTau * beat + ph);
    const float stress = 0.65f + 0.35f * valueNoise(t * 4.0f, lineSeed + 5u);
    const float word = smoothstep(0.12f, 0.3f, valueNoise(t * 3.2f, lineSeed));
    return env * (0.6f + 0.4f * syllable * stress * word);
}

// Слушающий иногда вставляет «угу» / «ну-ну».
float Parents::State::listenerMurmur() const {
    return 0.2f * smoothstep(0.7f, 0.76f, valueNoise(clock * 1.8f, murmurSeed));
}

// Таймеры взглядов: короткие у папы-водителя (чаще, когда спор жарче),
// долгие у мамы. Вне ссоры папа лишь изредка проверяет ребёнка в зеркале.
void Parents::State::updateGlances(float dt) {
    const bool quiet = phase == Phase::Silence;
    Glance& d = glance[0];
    d.timer -= dt;
    if (d.timer <= 0.0f) {
        d.on = !d.on;
        if (d.on) d.timer = animRng.range(0.7f, 1.4f);
        else if (quiet) d.timer = animRng.range(5.0f, 12.0f);
        else d.timer = animRng.range(1.5f, 4.5f) * (1.0f - 0.4f * tension);
    }
    Glance& m = glance[1];
    m.timer -= dt;
    if (m.timer <= 0.0f) {
        m.on = !m.on;
        m.timer = m.on ? animRng.range(3.0f, 8.0f) : animRng.range(4.0f, 10.0f);
    }
}

// Куда повёрнуты головы, пока звучит (или вот-вот прозвучит) реплика l.
void Parents::State::headTargets(const Line& l, float& dadTurn, float& momTurn) const {
    const bool dadLooks = glance[0].on;
    if (l.who == Speaker::Dad) {
        switch (l.to) {
        case To::Partner:
            dadTurn = dadLooks ? 0.3f : 0.0f;
            momTurn = momSulks ? 0.35f : -0.35f;
            break;
        case To::Kid:
            dadTurn = dadLooks ? 0.2f : 0.0f; // в зеркало заднего вида
            momTurn = 0.45f;                  // мама тоже оглядывается
            break;
        case To::Ahead:
            dadTurn = 0.0f;
            momTurn = -0.3f; // мама косится на папу
            break;
        }
    } else {
        switch (l.to) {
        case To::Partner:
            momTurn = -0.5f - 0.1f * lineHeat;
            dadTurn = dadLooks ? 0.3f : 0.0f;
            break;
        case To::Kid:
            momTurn = 1.0f;
            dadTurn = dadLooks ? 0.2f : 0.0f;
            break;
        case To::Ahead:
            momTurn = 0.0f;
            dadTurn = 0.0f;
            break;
        }
    }
}

void Parents::State::animate(float dt) {
    float talkTarget[2] = {0.0f, 0.0f};
    float dadTurn = 0.0f, momTurn = 0.0f;
    updateGlances(dt);
    if (phase == Phase::Speaking && line) {
        const int who = speakerIndex(line->who);
        talkTarget[who] = speechLevel();
        if (line->to == To::Partner) talkTarget[1 - who] = listenerMurmur();
        headTargets(*line, dadTurn, momTurn);
    } else if (phase == Phase::Gap && line) {
        // В паузе держим позу прошлой реплики, перед новой — поворачиваемся заранее.
        const Line* next = gapLeft < kAnticipate && pending < 0 ? upcomingLine() : nullptr;
        headTargets(next ? *next : *line, dadTurn, momTurn);
    } else {
        // Тишина: мама смотрит в своё окно или на дорогу, папа изредка — в зеркало.
        momTurn = glance[1].on ? 0.4f : 0.0f;
        dadTurn = glance[0].on ? 0.2f : 0.0f;
    }

    for (size_t i = 0; i < 2; ++i) talk[i] = saturate(approach(talk[i], talkTarget[i], 10.0f * dt));
    turn[0] = clampf(damp(turn[0], dadTurn, 5.0f, dt), -1.0f, 1.0f);
    turn[1] = clampf(damp(turn[1], momTurn, 4.5f, dt), -1.0f, 1.0f);

    // Напряжение растёт за время перепалки и медленно спадает в тишине.
    const bool inSequence = phase != Phase::Silence;
    const float target = inSequence ? lineHeat : 0.0f;
    const float speed = target > tension ? 0.9f : (inSequence ? 0.6f : 0.15f);
    tension = saturate(damp(tension, target, speed, dt));
}

// ============================================================================
//  Parents
// ============================================================================
Parents::Parents() : st_(std::make_unique<State>()) { reset(1u); }

Parents::~Parents() = default;

void Parents::reset(uint32_t seed) {
    State& s = *st_;
    s = State{};
    s.rng.reseed(seed * 2654435761u + 0x5A17u);
    s.revealIdx = findAct(Act::Reveal);
    s.reconcileIdx = findAct(Act::Reconcile);
    s.lastPlayed.fill(-kNever);
    s.nextExchangeAt = kFirstExchangeAt + s.rng.range(0.0f, 0.6f);
    // Варианты реакций идут по кругу в перемешанном порядке: один и тот же
    // вариант повторится не раньше, чем прозвучат все остальные.
    for (int c = 0; c < kCueCount; ++c) {
        auto& ord = s.order[static_cast<size_t>(c)];
        const int n = kCues[c].count;
        for (int i = 0; i < n; ++i) ord[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
        for (int i = n - 1; i > 0; --i)
            std::swap(ord[static_cast<size_t>(i)], ord[static_cast<size_t>(s.rng.rangeInt(0, i))]);
    }
    s.animRng.reseed(s.rng.nextU32());
    s.murmurSeed = s.rng.nextU32();
    s.glance[0].timer = s.animRng.range(2.0f, 6.0f);
    s.glance[1].timer = s.animRng.range(1.0f, 4.0f);
}

void Parents::update(float dt) {
    if (!(dt > 0.0f)) return;
    st_->step(std::min(dt, 0.1f)); // защита от огромного шага после паузы отладчика
}

void Parents::cue(ParentCue c) {
    State& s = *st_;
    const int ci = static_cast<int>(c);
    if (ci < 0 || ci >= kCueCount || s.finished) return;
    const CueInfo& info = kCues[ci];
    const size_t i = static_cast<size_t>(ci);
    if ((info.once && s.used[i]) || s.cooldown[i] > 0.0f) return;
    if (s.pending == ci) {
        s.pendingAge = 0.0f; // событие повторилось — реакция снова актуальна
        return;
    }
    // Ждать может только одна реакция: новая вытесняет старую, если не менее важна.
    if (s.pending >= 0 && info.priority < kCues[s.pending].priority) return;
    s.pending = ci;
    s.pendingAge = 0.0f;
}

// ---- Текущая реплика ---------------------------------------------------------------
bool Parents::hasLine() const { return st_->phase == Phase::Speaking && st_->line; }

Speaker Parents::speaker() const { return st_->lastSpeaker; }

const char* Parents::speakerName() const {
    return st_->lastSpeaker == Speaker::Dad ? T8("ПАПА") : T8("МАМА");
}

const char* Parents::text() const { return hasLine() ? st_->line->text : ""; }

int Parents::visibleChars() const {
    if (!hasLine()) return 0;
    const State& s = *st_;
    return std::min(s.lineChars, static_cast<int>(s.phaseT * kTypeSpeed) + 1);
}

float Parents::lineAlpha() const {
    if (!hasLine()) return 0.0f;
    const State& s = *st_;
    return saturate(std::min(s.phaseT / kFadeIn, (s.lineDur - s.phaseT) / kFadeOut));
}

// ---- Анимация ------------------------------------------------------------------------
float Parents::talk(Speaker s) const { return st_->talk[static_cast<size_t>(speakerIndex(s))]; }

float Parents::headTurn(Speaker s) const { return st_->turn[static_cast<size_t>(speakerIndex(s))]; }

float Parents::tension() const { return st_->tension; }

// ---- Звук ------------------------------------------------------------------------------
bool Parents::takeLineStarted(Speaker& who, float& duration) {
    State& s = *st_;
    if (!s.started) return false;
    s.started = false;
    who = s.startedWho;
    duration = s.startedDur;
    return true;
}
