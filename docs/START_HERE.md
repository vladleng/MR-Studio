# Moon River Studio — START HERE

## Актуальное продолжение — 2026-10-03
Stage 0 принят, PR #36 слит. Stage 1 PR #37: базовые native ASIO playback и monitor
при 48k/128 проверены пользователем. Длительные hardware/reconnect тесты и сравнение
со Studio Pro отложены пользователем и не блокируют разработку; gate не считается пройденным.
Stage 2 issue #17 / PR #38: читать docs/MUSICAL_TIMELINE.md и
docs/SHARED_STAGE_2_CHECKLIST.md. Текущая snapshot schema 2 с чтением v1.
Stage 2 принят пользователем: все четыре PASS Windows-checker подтверждены.
PR #38 слит в #37; общий код Stage 1/2 интегрируется через PR #37 в main.
Stage 1 performance gate остаётся отложенным в #16. Следующие направления: #18 или #20/#28.


Этот документ — короткая точка входа для нового чата, разработчика или агента, который подключается к проекту без контекста предыдущих обсуждений.

## 1. Что мы строим

**Moon River Studio** (`MRS`, рабочее сокращение `MR Studio`) — собственная performance-first DAW.

Рабочее название не является окончательным и может быть изменено ближе к зрелой стадии проекта без изменения архитектуры.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Fender Studio Pro.

Live Mode:

- не является отдельным приложением;
- не является отдельной DAW;
- не имеет собственной продуктовой версии;
- не имеет отдельного Audio Engine;
- не имеет отдельного Transport;
- не имеет отдельного MIDI Engine;
- не имеет отдельного plugin host;
- не хранит отдельную копию проекта для обычного workflow.

Главная формула:

```text
Moon River Studio
│
├── Arrange
├── Edit
├── Mix
└── Live Mode
        │
        └── тот же Project Model
            тот же Transport
            тот же Audio Engine
            тот же MIDI backend
            тот же Plugin Graph
```

## 2. Главное архитектурное правило

В Moon River Studio существует **одно общее ядро / SHARED Core**.

```text
                    SHARED CORE
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
  Arrange / Edit        Mix            Live Mode
```

В SHARED Core входят:

- Project Model;
- Command / Undo;
- Transport;
- Audio Engine / ASIO;
- mixer/routing graph;
- MIDI model/backend;
- plugin/native processor graph;
- Musical Timeline;
- Tempo/Meter Map;
- Chord Track;
- Arranger Track;
- Markers/Cues;
- serialization/versioning;
- persistence/recovery foundation.

**Нельзя создавать отдельный backend специально для Live Mode**, даже если это кажется быстрее для конкретной задачи.

Если backend-функция нужна нескольким workspace, она относится к SHARED Core.

## 3. Performance-first

Стабильность аудио — blocking requirement проекта.

На Windows основной professional/live path должен использовать родной vendor ASIO driver аудиоинтерфейса.

Базовые требования:

- realtime audio thread отделён от UI/network/AI/file I/O;
- no blocking I/O и тяжёлых allocations в audio callback;
- playback не зависит от UI responsiveness;
- plugin latency учитывается;
- предусмотрены preload/read-ahead;
- low-latency live path не должен ломаться из-за тяжёлого playback/mix path;
- ведутся xrun/dropout/callback metrics;
- Fender Studio Pro используется как performance benchmark на одинаковом hardware/setup.

Перед активным наращиванием функций Audio Engine должен пройти performance gate.

Подробнее: `AUDIO_ENGINE.md`.

## 4. Как организована разработка

Используются три **issue track одного приложения**:

```text
[MRS]    функции DAW и production workspaces
[SHARED] общий Core / Engine
[LIVE]   встроенный Live Mode
```

Это не три продукта.

Версионируется **Moon River Studio**.

Live Mode имеет только Stage readiness:

```text
Moon River Studio 0.6
├── Arrange / Mix / MIDI
├── Musical Structure
└── Live Mode: Stage 1 complete
```

Live Mode может разрабатываться параллельно на mock/fixture implementations SHARED interfaces. После появления real backend mock должен заменяться без изменения архитектуры Live UI.

## 5. GitHub Issues — карта проекта

Главные tracking issues:

- `#1` — общий master roadmap Moon River Studio;
- `#12` — MRS roadmap;
- `#13` — SHARED Core / Engine roadmap;
- `#14` — Live Mode roadmap;
- `#3` — optional Fender Studio Pro compatibility/import.

SHARED Core:

- `#15` — Core contracts: Project Model, Command/Undo, Transport API;
- `#16` — Audio Engine / ASIO / performance gate;
- `#17` — Musical Timeline: tempo, meter, chords, arranger, markers;
- `#18` — MIDI / Plugin Graph;
- `#19` — Persistence / State / Recovery.

MRS stages:

- `#20` — DAW Foundation;
- `#21` — Audio Arrangement;
- `#22` — Mixer / Routing;
- `#23` — Plugins / Native DSP;
- `#24` — MIDI;
- `#25` — Musical Structure;
- `#26` — AI Foundation;
- `#27` — Advanced DAW / Reliability.

Live Mode stages:

- `#28` — UX Foundation / moving Chord Track;
- `#29` — Real Project / Transport Integration;
- `#30` — Setlists / Show Workflow;
- `#31` — Playback / Click / Cue;
- `#32` — Live Inputs / Patches;
- `#33` — MIDI Automation / Hardware Control;
- `#34` — Remote / Mobile Companion;
- `#35` — Concert Reliability.

## 6. С чего начинать новому чату

Минимальный порядок чтения:

1. `docs/START_HERE.md` — этот файл;
2. `README.md` — краткий обзор всего продукта;
3. `docs/ARCHITECTURE.md` — архитектурные границы;
4. `docs/ROADMAP.md` — этапы разработки;
5. `docs/DEVELOPMENT_TRACKS.md` — правила параллельной работы;
6. открыть master issue `#1`;
7. открыть parent issue нужного track: `#12`, `#13` или `#14`;
8. открыть конкретный Stage issue, над которым продолжается работа.

Для audio/ASIO обязательно дополнительно прочитать:

- `docs/AUDIO_ENGINE.md`.

Для AI:

- `docs/AI_INTEGRATION.md`.

Для встроенного DSP/amp/cab:

- `docs/DSP_MODELING.md`.

Для Live Mode:

- `docs/UI_UX_CONCEPT.md`;
- `docs/LIVE_WORKFLOW.md`.

Для Studio Pro import/compatibility:

- `docs/STUDIO_PRO_INTEGRATION.md`;
- issue `#3`.

## 7. Что важно не перепутать

### Не создавать Moon River Live как отдельное приложение

Историческое имя репозитория — `Moon-River-Live`, но целевой продукт теперь Moon River Studio.

`Live Mode` — встроенный workspace.

### Не создавать отдельный Live Audio Engine

Live использует тот же engine, что Arrange/Edit/Mix.

### Не создавать отдельный Live project format для обычной работы

Live-specific данные хранятся как часть MRS Project Model либо как show/setlist state, ссылающийся на project IDs.

### Не делать Studio Pro обязательной зависимостью

Studio Pro теперь:

- UX/performance reference;
- benchmark;
- optional import/migration source.

Moon River Studio должна быть самодостаточной DAW.

### Не связывать AI с realtime thread

AI работает через Project Context API + Command/Tool API и никогда не вызывается из ASIO callback.

## 8. Live Mode — визуальный ориентир

Базовое направление уже выбрано:

- flat dark UI;
- без выпуклых/glossy элементов;
- высокая читаемость на ноутбуке;
- горизонтальная движущаяся Chord Track strip по принципу Show Page;
- current chord читается относительно playhead;
- previous/next chords остаются видимыми;
- sections/cues/setlist/transport постоянно доступны;
- performance safety важнее декоративности.

Live UI должен быть performance-oriented представлением того же открытого MRS project.

## 9. AI — архитектурная цель

Будущий AI/ChatGPT layer должен получать структурированное состояние DAW через API, а не управлять интерфейсом мышью.

Планируемые возможности:

- читать tracks/clips/MIDI/chords/sections/mixer/plugins;
- анализировать аранжировку;
- генерировать и редактировать MIDI;
- предлагать mixer/plugin changes;
- создавать markers/sections/clips;
- выполнять разрешённые commands с Undo/Redo;
- работать в режимах READ / SUGGEST / EDIT / AUTO.

Даже до реализации AI Project Model должен иметь stable IDs и нормальный Command API.

## 10. Native DSP / guitar processing

В долгосрочном плане MRS предусматривает:

- utility DSP;
- EQ/compressor/saturation;
- convolution/Cab IR;
- amp/preamp/pedal models;
- neural model player;
- собственные Moon River captures/models.

Factory Content и User Library должны быть лицензированно разделены.

## 11. Текущая стартовая логика разработки

Основной фундамент строится через SHARED + ранние MRS stages.

Критическая последовательность:

```text
#15 Core contracts
      │
      ├── #16 Audio Engine / ASIO
      ├── #17 Musical Timeline
      └── #20 MRS DAW Foundation
```

После фиксации нужных contracts Live UI может идти параллельно:

```text
#15 / #17 interfaces
        │
        └── #28 Live UX prototype on mocks
                    │
                    └── #29 real Core integration
```

Новые чаты не должны ждать завершения всей DAW, если текущую задачу можно безопасно вести через зафиксированный SHARED interface.

## 12. Как передать задачу следующему чату

Достаточный стартовый запрос:

```text
Ознакомься с docs/START_HERE.md, docs/ARCHITECTURE.md,
docs/ROADMAP.md и GitHub Issues #1, #12, #13, #14.

После этого открой issue #XX и продолжай работу над ним.
Не создавай отдельный engine или отдельное приложение для Live Mode.
```

Если работа идёт над Audio Engine, нужно добавить:

```text
Обязательно прочитай docs/AUDIO_ENGINE.md и соблюдай performance gate.
```

## 13. Source of truth

При противоречии старых обсуждений и текущего репозитория приоритет имеют:

1. актуальный Stage issue;
2. `docs/START_HERE.md`;
3. `docs/ARCHITECTURE.md`;
4. `docs/ROADMAP.md`;
5. специализированный документ соответствующей подсистемы.

Если обнаружено противоречие между актуальными документами, сначала исправить документацию и только затем продолжать реализацию.

## 2026-10-03 handoff
Stage 3 accepted: PR #39 merged, #18 closed. Active Stage 4 #19 on
shared/stage-4-persistence-recovery. Read PERSISTENCE.md and its acceptance checklist.
Deferred Stage 1 hardware/performance checks stay in #16 and do not block progress.
