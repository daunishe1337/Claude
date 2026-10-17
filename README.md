# Optimod

Клиентский мод оптимизации производительности для **Minecraft Java Edition 26.3** на **Fabric**.
Поднимает FPS и снижает нагрузку на CPU и память, не меняя геймплей и (с настройками по умолчанию)
внешний вид. Работает только на клиенте: не добавляет блоков, предметов и пакетов, поэтому
подключается к любым серверам, в том числе без Optimod.

| № | Модуль | Что делает | По умолчанию |
|---|--------|------------|--------------|
| 1 | Отсечение скрытых | Не рисует сущности и блок-сущности (сундуки, таблички, баннеры…), полностью закрытые стенами. Видимость считается лучами в **отдельном потоке** | вкл. |
| 2 | Дальность отрисовки | Свои лимиты для сущностей / блок-сущностей / частиц, отдельно от дальности чанков | 128 / 64 / 128 блоков (как в ванилле или дальше) |
| 3 | Частицы | Лимит одновременно живых частиц; по желанию — не создавать частицы вне обзора | лимит 8000, «вне обзора» выкл. |
| 4 | Фон | Лимит FPS, когда окно не в фокусе / свёрнуто; по желанию — приглушение звука | 10 / 5 FPS, звук не трогаем |
| 5 | Память и запуск | Общие объекты для одинаковых кубов моделей, сборка мусора после загрузки, замер времени запуска | вкл. |
| 6 | Анимации | Новые кадры анимированных текстур загружаются на видеокарту, только когда текстура видна | вкл. |
| 7 | Отладочный оверлей | Сколько объектов отсечено, частиц пропущено, загрузок анимаций сэкономлено и т.д. | вкл., виден вместе с F3 |

Каждый модуль выключается в настройках, большинство — на лету, без перезапуска.

---

## Окружение

| Компонент | Версия | Откуда взята |
|-----------|--------|--------------|
| Minecraft | 26.3 | — |
| Java | 25 | требование Minecraft 26.x |
| Fabric Loader | 0.19.5 | официальный fabric-example-mod (ветка 26.3) |
| Fabric API | 0.161.0+26.3 (подойдёт и новее, например 0.162.0+26.3) | Mod Menu и Dynamic FPS для 26.3 |
| Fabric Loom | 1.18-SNAPSHOT, плагин `net.fabricmc.fabric-loom` | fabric-example-mod |
| Gradle | 9.7.1 (через wrapper, ставить не нужно) | fabric-example-mod |
| Mod Menu (необязательно) | 20.0.3 | Dynamic FPS для 26.3 |

Minecraft с 26.x поставляется **без обфускации**: в коде используются официальные имена Mojang,
маппинги (Yarn) не подключаются вовсе — в `build.gradle` нет строки `mappings`, а плагин Loom
называется `net.fabricmc.fabric-loom` (без ремаппинга). Актуальные версии всегда можно сверить
на <https://fabricmc.net/develop>.

---

## Структура проекта

```
optimod/
├── gradle/wrapper/                      Gradle Wrapper (9.7.1)
├── src/main/
│   ├── java/dev/optimod/
│   │   ├── Optimod.java                 константы, логгер (без ссылок на классы игры)
│   │   ├── OptimodClient.java           точка входа клиента, события тиков/мира
│   │   ├── config/
│   │   │   ├── OptimodConfig.java       схема config/optimod.json + проверка диапазонов
│   │   │   └── ConfigManager.java       загрузка/атомарное сохранение (Gson)
│   │   ├── compat/
│   │   │   ├── OptModule.java           список модулей
│   │   │   ├── CompatManager.java       Sodium/Iris/Lithium/Dynamic FPS/EntityCulling → что выключить
│   │   │   └── Modules.java             быстрые флаги «модуль активен»
│   │   ├── stats/OptimodStats.java      счётчики для оверлея
│   │   ├── render/
│   │   │   ├── FrameContext.java        камера и frustum текущего кадра
│   │   │   └── RenderCullingHooks.java  решения «рисовать/нет» (модули 1–2)
│   │   ├── culling/                     модуль 1
│   │   │   ├── OptimodCullable.java     поля результата в Entity/BlockEntity
│   │   │   ├── CullingManager.java      связь рендер-поток ↔ фоновый поток
│   │   │   └── OcclusionCullingThread.java  лучи 3D-DDA по сетке блоков
│   │   ├── particle/                    модули 2–3
│   │   │   ├── ParticleLimiter.java
│   │   │   └── ParticleTracker.java     учёт живых частиц
│   │   ├── background/                  модуль 4
│   │   │   ├── BackgroundManager.java   состояние окна (SDL), лимит FPS, громкость
│   │   │   └── OptimodSoundEngine.java
│   │   ├── memory/                      модуль 5
│   │   │   ├── CubeCache.java
│   │   │   └── MemoryManager.java
│   │   ├── animation/                   модуль 6
│   │   │   ├── AnimationTracker.java
│   │   │   ├── OptimodAnimationState.java
│   │   │   └── OptimodSpriteActivity.java
│   │   ├── overlay/DebugOverlay.java    модуль 7
│   │   ├── gui/
│   │   │   ├── OptimodConfigScreen.java экран настроек (ванильные OptionInstance)
│   │   │   ├── OptimodKeyMappings.java  клавиша открытия настроек
│   │   │   └── ModMenuIntegration.java  кнопка в Mod Menu
│   │   └── mixin/
│   │       ├── OptimodMixinPlugin.java  какие миксины применять (конфликты с Sodium/ModernFix)
│   │       ├── culling/CullableMixin.java
│   │       ├── render/                  EntityRenderer, BlockEntityRenderDispatcher, LevelExtractor
│   │       ├── particle/                ParticleEngine, Particle, ClientLevel
│   │       ├── background/              FramerateLimitTracker, Options, SoundEngine, SoundManager
│   │       ├── memory/CubeDefinitionMixin.java
│   │       └── animation/               SpriteContents, AnimationState, TextureAtlas(Sprite), GUI, частицы,
│   │                                    ModelBlockRenderer, FluidStateModelSet, BufferBuilder
│   └── resources/
│       ├── fabric.mod.json
│       ├── optimod.mixins.json          обязательные миксины (сверены с модами для 26.3)
│       ├── optimod.optional.mixins.json необязательные: не применились — игра не падает
│       └── assets/optimod/lang/         ru_ru.json, en_us.json
├── build.gradle
├── gradle.properties
├── settings.gradle
├── gradlew, gradlew.bat
└── LICENSE
```

---

## Как открыть проект в IntelliJ IDEA

1. **Установите JDK 25** (например, Eclipse Temurin 25 или Microsoft Build of OpenJDK 25).
2. **File → Open…** → выберите папку проекта (где лежит `build.gradle`) → **Open as Project** → **Trust Project**.
3. **File → Settings → Build, Execution, Deployment → Build Tools → Gradle** → *Gradle JVM*: JDK 25.
   **File → Project Structure → Project → SDK**: тоже 25.
4. Дождитесь синхронизации Gradle (справа внизу). В первый раз Loom скачивает Minecraft,
   библиотеки и ассеты — это несколько сотен мегабайт и несколько минут.
5. **Исходники игры для чтения и поиска.** Откройте окно *Gradle* (справа) →
   `optimod → Tasks → fabric → genSources` (или в терминале `./gradlew genSources`).
   После этого `Ctrl+N` (класс) → например `EntityRenderer` открывает декомпилированный исходник,
   а `Alt+F7` ищет все использования метода. Если IDEA предлагает *Choose Sources…* — выберите
   файл `…-sources.jar`.
6. Плагин **Minecraft Development** для IDEA не обязателен, но подсвечивает ошибки в миксинах.

## Запуск игры в dev-режиме (runClient)

- В IDEA: при синхронизации Gradle Loom создаёт конфигурацию запуска **Minecraft Client** —
  выберите её вверху и нажмите ▶ (или 🐞 Debug — тогда работает горячая замена кода).
  Если конфигурации нет, нажмите «Reload All Gradle Projects» (🔄) в окне Gradle.
- Или в терминале:
  ```bash
  ./gradlew runClient          # Linux/macOS
  gradlew.bat runClient        # Windows
  ```
- Игра запускается из папки `run/` (там же `run/config/optimod.json` и `run/logs/latest.log`).
- В логе должны появиться строки `[Optimod]`: отчёт о совместимости, «Optimod инициализирован…»
  и «Игра загрузилась за N с».
- Чтобы проверить интеграцию с Mod Menu в dev-режиме, скачайте `modmenu-20.0.3.jar` и
  `placeholder-api` для 26.3 и положите их в `run/mods/`.

## Сборка .jar (gradlew build)

```bash
./gradlew build          # Linux/macOS
gradlew.bat build        # Windows
```

Готовый мод: `build/libs/optimod-1.0.0.jar` (файл `…-sources.jar` — это исходники, его ставить не нужно).
Положите jar в папку `.minecraft/mods` вместе с **Fabric API**. Mod Menu — по желанию.

---

## Настройки

- **С Mod Menu:** Моды → Optimod → кнопка настроек.
- **Без Mod Menu:** клавиша **O** в игре (переназначается в *Настройки → Управление → Optimod*).
- Файл `config/optimod.json` создаётся при первом запуске; можно править вручную (значения вне
  допустимых диапазонов поправятся автоматически, повреждённый файл сохраняется как `.broken`).

```jsonc
{
  "configVersion": 1,
  "entityCulling": {
    "enabled": true,
    "cullEntities": true,            // мобы, предметы, рамки (не игроки и не сущности с именем)
    "cullBlockEntities": true,       // сундуки, таблички, баннеры...
    "maxCheckDistance": 128,         // дальше — без проверки (считается видимым)
    "updateIntervalMs": 15           // пауза фонового потока между проходами
  },
  "renderDistance": {
    "enabled": true,
    "entityDistance": 128,
    "blockEntityDistance": 64,       // в ванилле большинство блок-сущностей видны до 64
    "particleDistance": 128,
    "applyToPlayers": false
  },
  "particles": {
    "enabled": true,
    "maxParticles": 8000,
    "skipOutOfViewSpawns": false,    // экономнее, но дым может «пропасть» при повороте
    "outOfViewMinDistance": 16
  },
  "background": {
    "enabled": true,
    "unfocusedFpsLimit": 10,
    "minimizedFpsLimit": 5,
    "reduceVolumeInBackground": false,
    "backgroundVolumePercent": 20
  },
  "memory": {
    "enabled": true,
    "deduplicateModelCubes": true,   // применяется при загрузке ресурсов (нужен перезапуск)
    "gcAfterLoading": true,
    "gcOnWorldLeave": false
  },
  "animations": {
    "enabled": true,
    "alwaysAnimate": ["minecraft:block/fire_0", "minecraft:block/fire_1", "..."]
  },
  "debugOverlay": {
    "enabled": true,
    "onlyWithF3": true,
    "position": "BOTTOM_RIGHT"       // TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT
  },
  "compatibility": {
    "autoDisableOverlappingModules": true
  }
}
```

(Комментарии `//` здесь только для пояснения — в настоящем файле их нет, JSON их не поддерживает.)

---

## Совместимость

При запуске мод проверяет установленные моды и пишет итог в лог. Если
`compatibility.autoDisableOverlappingModules = true` (по умолчанию), пересекающиеся модули выключаются:

| Мод | Что отключается в Optimod | Почему |
|-----|---------------------------|--------|
| **Sodium** | модуль 1 (отсечение), модуль 6 (анимации) | Sodium сам отсекает сущности в невидимых секциях и анимирует только видимые текстуры. Модуль 6 при Sodium не применяется **никогда** (оба мода добавляют метод `BufferBuilder#putBakedQuad`). Модули 2–5 и 7 работают вместе с Sodium: хуки `tryExtractRenderState`/`shouldRender` вызывает и сам Sodium |
| **Iris** | модуль 1 | проход теней рисует сущности с точки зрения солнца — отсечение по камере ломало бы тени |
| **Lithium** | ничего | Lithium ускоряет игровую логику (тики, физику), Optimod — только клиентский рендер; пересечений нет (это тоже пишется в лог) |
| Dynamic FPS | модуль 4 | делает то же самое |
| EntityCulling | модуль 1 | делает то же самое |
| ModernFix | дедупликация кубов (часть модуля 5) | у ModernFix есть та же оптимизация `compact_entity_models` |

Пример строки в логе: `[Optimod] Обнаружен Sodium: модуль «анимации только видимых текстур» отключён — …`.
В экране настроек такие модули помечены «отключено (Sodium)».

Все хуки — `@Inject` (плюс `@Shadow`/`@Accessor` для доступа к полям). `@Overwrite` не используется.
Исключения, где `@Inject` невозможен: подмена результата `new ModelPart.Cube(...)` сделана через
`@WrapOperation` (MixinExtras — уживается с такими же обёртками других модов), а в `BufferBuilder`
добавлено переопределение default-метода, которое вызывает исходную реализацию.

---

## Как проверить прирост производительности

### Подготовка (иначе цифры будут шуметь)
1. *Настройки → Графика*: **VSync выкл.**, **Макс. частота кадров — без ограничения**; одинаковые
   разрешение, дальность прорисовки и пакеты ресурсов во всех замерах.
2. Одинаковая сцена: тот же мир/сид, те же координаты и направление взгляда (удобно `/tp` с углами).
3. После входа в мир подождите 30–60 секунд: догрузятся чанки и «прогреется» JIT Java.
4. Закройте фоновые программы; ноутбук — от сети.

### Где смотреть
- **F3** — FPS в верхней строке. В 26.x набор строк F3 настраивается; список сочетаний клавиш
  отладки показывает **F3 + Q**. Полезны строки рендера сущностей/частиц и график времени кадра.
- **Оверлей Optimod** (правый нижний угол вместе с F3): сколько сущностей и блок-сущностей
  скрыто стенами и дальностью за кадр, время прохода фонового потока, живые/пропущенные частицы,
  состояние окна, память, пропущенные загрузки анимаций.
- **Лог** (`logs/latest.log`): время запуска игры, результат сборки мусора после загрузки
  («… 1450 МБ -> 620 МБ»), сводка дедупликации кубов.

### Замер «до/после» (без перезапуска)
Модули переключаются на лету (клавиша **O**), поэтому сравнивать удобно в одной сессии:
1. В настройках выключите модули 1–3 и 6 → постойте 60 с на точке, запишите средний и минимальный FPS.
2. Включите их обратно → повторите замер на той же точке.
3. Повторите оба замера 2–3 раза и возьмите среднее.

Для точных цифр (средний FPS, 1% low) используйте внешний счётчик: **MangoHud** (Linux, с записью
лога), **PresentMon** или **CapFrameX** (Windows). Для нагрузки на CPU — профайлер (например, мод
spark, если вышел для 26.3) или вкладка «Производительность» диспетчера задач.

### Сцены, где эффект заметен
| Модуль | Сцена | Что ожидать |
|--------|-------|-------------|
| 1 | Деревня/склад: много сундуков и мобов за стенами; ферма мобов под землёй | в оверлее «Скрыто стенами» > 0, рост FPS тем больше, чем больше скрытых объектов |
| 2 | Поставьте дальность сущностей 32–48 в открытом мире с большим числом мобов | в оверлее «За дальностью» > 0 |
| 3 | `/particle minecraft:flame ~ ~1 ~ 3 3 3 0 20000 force` несколько раз, взрывы ТНТ | «живых» упирается в лимит, нет просадки до однозначных FPS |
| 4 | Сверните окно / переключитесь в другое приложение | загрузка GPU/CPU в диспетчере задач падает в разы, в оверлее «свёрнуто» |
| 5 | Запуск игры | строка лога о сборке мусора и меньшее потребление памяти в F3 |
| 6 | Океан с ламинарией, Незер с магмой — отвернитесь от них | «пропущено загрузок N из M» в оверлее |

Шаблон для записи результатов:

| Сцена | Модули выкл.: средн. / 1% low | Модули вкл.: средн. / 1% low | Прирост |
|-------|-------------------------------|------------------------------|---------|
| …     |                               |                              |         |

---

## Что сверено с исходниками 26.3, а что нужно проверить через genSources

Сетевой доступ к серверам Mojang и Fabric при подготовке проекта был закрыт, поэтому имена классов и
методов 26.3 сверялись не по декомпилированной игре, а по исходному коду модов, уже собранных под 26.3:
Fabric API (ветка 26.3), Sodium, Sodium Extra, Iris, EntityCulling, Dynamic FPS и Mod Menu. Код мода проверен компиляцией против заглушек с этими сигнатурами, но в
реальной игре не запускался — первый `./gradlew build` и `runClient` и есть настоящая проверка.

**Сверено (обязательный конфиг `optimod.mixins.json`):**

| Хук | Сверено по |
|-----|------------|
| `EntityRenderer#shouldRender(T, Frustum, double, double, double, float)`, `affectedByCulling(T)` | Sodium, EntityCulling |
| `BlockEntityRenderDispatcher#tryExtractRenderState(BlockEntity, float, CrumblingOverlay, boolean)`, `getRenderer` | EntityCulling |
| `LevelExtractor#extractVisibleEntities(Camera, Frustum, DeltaTracker, LevelRenderState)` | EntityCulling |
| `ParticleEngine#createParticle(ParticleOptions, 6×double)` | Sodium Extra |
| `ClientLevel#addDestroyBlockEffect`, `#addBreakingParticles` | Sodium Extra |
| `FramerateLimitTracker#getFramerateLimit`, `Options#getSoundSourceVolume`, поля `SoundEngine`, `SoundManager#soundEngine` | Dynamic FPS |
| `SpriteContents.AnimationState#tick/needsToDraw/frame`, `TextureAtlas#getSprite`, `TextureAtlasSprite#createAnimationState`, `GuiGraphicsExtractor#blitSprite`, `SingleQuadParticle#extractRotatedQuad`, `ModelBlockRenderer#putQuadWithTint`, `FluidStateModelSet#get` | Sodium, Sodium Extra |
| HUD API, клавиши, события тиков/чанков, `OptionsSubScreen`/`OptionInstance`, `gui.setScreen`, `debugEntries.isOverlayVisible()`, SDL-флаги окна | Fabric API, Mod Menu, Dynamic FPS, Sodium |

**Не удалось сверить — проверьте после `genSources`:**

1. **`Particle#isAlive()`** (используется в `ParticleTracker`). Если метод переименован, `gradlew build`
   сразу укажет строку. Откройте `net.minecraft.client.particle.Particle` и найдите метод, который
   возвращает признак «частица жива» (обычно `return !this.removed;`), подставьте его имя.
2. **Дескриптор конструктора `ModelPart.Cube`** в `CubeDefinitionMixin` взят из ModernFix для 26.1.2.
   Миксин в необязательном конфиге: если не совпал — игра не падает, а в логе будет
   «Дедупликация кубов моделей не сработала». Как исправить: откройте
   `net.minecraft.client.model.geom.builders.CubeDefinition#bake`, посмотрите аргументы
   `new ModelPart.Cube(...)` и обновите строку `target` (I=int, F=float, Z=boolean).
3. **Проходит ли сборка мешей чанков через `ModelBlockRenderer#putQuadWithTint`.** Если нет, модуль 6
   ничего не ломает, просто не экономит (в оверлее «спрайтов в мире 0»). Проверка: `Alt+F7` по
   `putQuadWithTint` — среди вызывающих должен быть путь из сборки секций (`SectionCompiler`).
4. **`BufferBuilder` не переопределяет `putBakedQuad` сам** (так в 26.3 по коду Sodium). Если в будущей
   версии переопределит — миксин из необязательного конфига просто не применится.

**Если игра падает при запуске из-за миксина**, в отчёте будет строка вида
`@Inject annotation on optimod$… could not find any targets matching 'имяМетода'` с именем класса.
Откройте этот класс (Ctrl+N), найдите новое имя метода и поправьте `method = "…"` в соответствующем
миксине. Подробный лог миксинов: добавьте в VM options запуска `-Dmixin.debug.verbose=true`,
выгрузка преобразованных классов — `-Dmixin.debug.export=true` (появятся в `run/.mixin.out/`).

---

## Как это устроено (кратко)

- **Модуль 1.** Рендер (главный поток) для каждой сущности спрашивает `CullingManager.isCulled()` —
  это чтение одного volatile-поля, объект заодно ставится в очередь. Фоновый поток
  `Optimod Occlusion Culling` по кругу пускает лучи от камеры к центру и 8 углам рамки цели по
  сетке блоков (3D-DDA); непрозрачность — `BlockState#isSolidRender`. Чтобы не мерцало, объект
  скрывается только после двух «невидимых» проходов подряд, а показывается сразу. Не трогаются:
  игроки, сущности с видимым именем, светящиеся, `Display`-сущности, транспорт игрока, «глобальные»
  блок-сущности и те, что рисуются вне экрана (лучи маяков).
- **Модуль 2.** Простая проверка расстояния в тех же хуках (`shouldRender`, `tryExtractRenderState`,
  `createParticle`).
- **Модуль 3.** Учёт живых частиц через конструктор `Particle` с очисткой раз в тик; сверх лимита
  `createParticle` возвращает `null`, а облака частиц разрушения блоков не создаются.
- **Модуль 4.** Флаги окна SDL (в 26.x окно на SDL, не на GLFW) → лимит FPS в
  `FramerateLimitTracker#getFramerateLimit` (только уменьшаем ванильный). Громкость — множитель
  в `Options#getSoundSourceVolume` без изменения настроек игрока.
- **Модуль 5.** Кэш одинаковых `ModelPart.Cube`, одна сборка мусора после загрузки, замер времени
  запуска. В «горячих» местах самого мода нет аллокаций: переиспользуемые `MutableBlockPos`,
  кэш `Long2ByteOpenHashMap`, списки, координаты в примитивах, лямбды без захвата.
- **Модуль 6.** Отметки «спрайт использован» (GUI, частицы, предметы, мгновенная отрисовка блоков) и
  карта «спрайт → секции мира» из сборки мешей; раз в тик секции проверяются по frustum.
  Для невидимых `needsToDraw()` отвечает `false` — фаза анимации продолжает идти.
- **Модуль 7.** HUD-элемент Fabric (`HudElementRegistry`).

---

## История этапов (git)

Проект собирался поэтапно, каждый этап — отдельный коммит: можно откатиться на любой
(`git checkout <хеш>`) и проверить его отдельно.

1. Скелет: Gradle/Loom, `fabric.mod.json`, конфиги миксинов с плагином, JSON-конфиг, совместимость, точка входа.
2. Модуль 1 — отсечение скрытых в фоновом потоке.
3. Модуль 2 — дальность отрисовки.
4. Модуль 3 — частицы.
5. Модуль 4 — фон.
6. Модуль 5 — память и запуск.
7. Модуль 6 — анимации.
8. Модуль 7 — отладочный оверлей.
9. Экран настроек, Mod Menu, клавиша.

## Лицензия

MIT — см. [LICENSE](LICENSE).
