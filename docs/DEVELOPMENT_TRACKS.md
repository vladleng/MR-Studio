# Moon River Studio — Development Tracks

## Текущий порядок работы — 2026-10-04

По инструкции пользователя код, configure/build/tests и пакеты ведутся локально.
На GitHub обновляются только issues и документация. Отправка новых изменений кода,
создание PR и слияние — только по отдельной просьбе пользователя. Приёмка локальной
сборки сама по себе не разрешает слияние. GitHub Actions не используются.
Локальная 0.1g принята; уже открытый PR #49 остаётся без слияния.
Этот порядок имеет приоритет над общими рекомендациями workflow ниже.

## 1. Термины

**Moon River Studio** (`MRS`, рабочее имя `MR Studio`) — вся DAW и единственное приложение.

**Live Mode** — встроенный Performance / Show режим MRS, аналогичный по роли Show Page в Studio Pro. Это не отдельное приложение и не отдельная продуктовая версия.

**SHARED** — единый Core / Engine, который обслуживает все режимы DAW.

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
└── Live Mode
       |
       +---- all use the same SHARED Core
```

## 2. Зачем нужны отдельные issue tracks

Разделение Issues нужно не для создания отдельных продуктов, а чтобы можно было параллельно вести:

- развитие основной DAW;
- развитие общего backend;
- развитие Live Mode UI/show workflow.

Префиксы:

```text
[MRS]    пользовательские DAW-функции
[SHARED] общий Core / Engine
[LIVE]   встроенный Live Mode
```

Главные parent issues:

- #12 — MRS Roadmap;
- #13 — SHARED Core Roadmap;
- #14 — LIVE Roadmap;
- #1 — общий master roadmap.

## 3. Версионирование

Версионируется только Moon River Studio. Пользовательские версии подэтапов:

```text
0.1b -> 0.1c -> ...
0.1b upd1 / upd2 — небольшие обновления
0.1b fix1 / fix2 — исправления ошибок
```

Счётчики upd/fix независимы и относятся к базовой версии.
Stage IDs остаются идентификаторами плана, а не версиями сборки.
Актуальные правила и соответствие: [VERSIONING.md](VERSIONING.md).

Live Mode не получает отдельную линию `MRL 0.x`.

Его готовность обозначается Stage-ами:

```text
LIVE Stage 0
LIVE Stage 1
LIVE Stage 2
...
```

Пример состояния общей сборки:

```text
Moon River Studio <build version>
├── Audio/Mixer/Plugins/MIDI ready at current MRS level
├── Musical Structure ready
└── Live Mode: Stage 1 complete
```

## 4. Единый engine — обязательное правило

Внутри MRS не допускаются отдельные backend для Production и Live.

Общие:

- Project Model;
- Command / Undo;
- Transport;
- Audio Engine / ASIO;
- mixer/routing graph;
- MIDI backend;
- plugin/native processor graph;
- Chord/Arranger/Marker timeline;
- persistence/versioning/recovery.

```text
                    SHARED CORE
                         |
        +----------------+----------------+
        |                |                |
   Arrange/Edit          Mix          Live Mode
```

Если проблема Live Mode находится в audio/transport/MIDI/plugin backend, исправление вносится в SHARED Core, а не в отдельный Live fork.

## 5. Interface-first development

Общий интерфейс фиксируется раньше конкретного UI/backend implementation.

Пример:

```text
ITransport
- play()
- pause()
- stop()
- seek(samples)
- setLoop(range)
- getPosition()
- getState()
```

После этого параллельно:

```text
SHARED/MRS workstream -> real realtime backend
LIVE workstream       -> mock ITransport / fixture Project Model
                              |
                              +-> later switch to real backend
```

Live Mode не должен ждать завершения всей DAW, если нужный контракт уже определён.

## 6. Когда начинается разработка Live Mode

Live Mode не обязан ждать финала MRS.

Рекомендуемый порядок:

1. SHARED Stage 0 (#15) фиксирует Project/Transport contracts.
2. SHARED Stage 2 (#17) фиксирует musical timeline contracts.
3. Параллельно можно начинать LIVE Stage 0 (#28) на mocks/fixtures.
4. MRS продолжает Audio/Mixer/Plugins/MIDI.
5. После MRS Musical Structure (#25) LIVE Stage 1 (#29) переключается на настоящий открытый MRS project.
6. Дальше production и Live развиваются параллельно на одном engine.

Таким образом, UI Live Mode можно прорабатывать рано, но реальный Live workflow появляется как часть зрелого MRS project model.

## 7. Current issue tree

### SHARED Core — #13

```text
#15 Core Contracts
#16 Audio Engine / ASIO
#17 Musical Timeline
#18 MIDI / Plugin Graph
#19 Persistence / State / Recovery
```

### MRS — #12

```text
#20 DAW Foundation
#21 Audio Arrangement
#22 Mixer / Routing
#23 Plugins / Native DSP
#24 MIDI
#25 Musical Structure
#26 AI Foundation
#27 Advanced DAW / Reliability
```

### LIVE — #14

```text
#28 UX Foundation
#29 Real Project / Transport Integration
#30 Setlists / Show Workflow
#31 Playback / Click / Cue
#32 Live Inputs / Patches
#33 MIDI Automation / Hardware Control
#34 Remote / Mobile
#35 Concert Reliability
```

## 8. Labels

Рекомендуемая система:

### Track

```text
track:mrs
track:shared
track:live
```

### Area

```text
area:audio
area:midi
area:project-model
area:arrange
area:mixer
area:plugins
area:ui
area:live
area:setlist
area:ai
area:dsp
area:remote
area:testing
```

### Type

```text
type:feature
type:bug
type:research
type:refactor
type:docs
type:test
```

### Priority / blockers

```text
priority:critical
priority:high
priority:normal
blocked
performance-blocker
```

## 9. Dependency rule

Каждый LIVE issue должен явно указывать нужные SHARED services.

Пример:

```text
[LIVE] Moving Chord Track
Depends on:
- SHARED ITransport
- SHARED Chord Track model
```

Если backend ещё не готов, разрешён mock, но mock должен реализовывать тот же интерфейс.

MRS-specific UI не должен становиться зависимостью Live Mode. Live зависит от Core/API, а не от Arrange/Mixer UI.

## 10. Branch naming

Постоянные ветки `mrs` и `live` не нужны.

Предпочтительны короткоживущие feature branches:

```text
shared/transport-api
shared/asio-engine
mrs/audio-clips
mrs/mixer-routing
live/chord-strip
live/setlist-ui
```

`main` остаётся интеграционной и buildable веткой.

## 11. Параллельная работа

```text
TRACK MRS                TRACK SHARED              TRACK LIVE

Arrange UI -----------> Project Model <---------- Live UI
Mixer UI -------------> Audio Graph ------------> Live routing/status
MIDI Editor ----------> MIDI Model -------------> Live actions
Chord Editor ---------> Chord Model ------------> Moving Chord Track
Plugin UI ------------> Processor Graph --------> Live patches
```

Ключевая идея: MRS и Live Mode могут развиваться одновременно, но **backend появляется только один раз**.

## 12. Integration milestones

### Core Prototype

#15 + #16 + #20

### Live UI Prototype

#28 на mocks/fixtures.

### First native Live integration

#17 + #25 + #29.

### First Rehearsal Build

#30 + #31, затем нужные #32/#33.

### First Full Show Build

#35.

## 13. Definition of Done

### MRS issue

- использует SHARED APIs для backend;
- tests где применимо;
- realtime constraints не нарушены;
- state serializable, если функция хранится в project;
- не создаёт скрытый альтернативный Core.

### SHARED issue

- contract не зависит от UI;
- tests/mocks;
- realtime/threading rules задокументированы;
- serialization/versioning учтены;
- пригоден для Arrange/Edit/Mix и Live Mode.

### LIVE issue

- является частью MRS app/workspace system;
- работает через SHARED API;
- может переключаться mock/real implementation без смены UI architecture;
- readable/performance-safe UI;
- не блокирует audio thread;
- не создаёт отдельный transport/audio/MIDI/plugin backend.

## 14. Главный принцип

> **Moon River Studio — одна DAW. Live Mode — один из её режимов. Engine — один.**

Issue tracks нужны только для параллельной организации разработки, а не для разделения продукта на MRS и MRL.


## Локальная 0.1h / MRS Stage 2c готова к проверке — 2026-10-04
Посылы/возвраты (до 8 на канал, pre/post-fader, уровни, nested buses, cycle rejection),
вертикальные фейдеры/стереометры, Mix поверх аранжировки, мини-панели дорожек
(горизонтальные gain/meters, колесо pan, mono input selection), Files → Open recent project.
Core snapshot v5 читает v1–v4; desktop config v3 читает v1/v2. Undo и сохранение общие.
Локальные Windows x64 ASIO Release configure/build, 65/65 CTest и расширенный GUI smoke
пройдены; зависимости использованы из кеша, GitHub Actions не использовались.
Контракты: SENDS.md; пользовательская проверка: MRS_STAGE_2C_CHECKLIST.md.
Пользовательская/физическая ASIO приёмка 0.1h ожидается; последняя принятая версия — 0.1g.
Ветка mrs/0.1h-sends-ui-local остаётся локальной. На GitHub — только issues/docs;
код push/PR/merge исключительно по отдельной просьбе. #22 и отложенный #16 остаются открыты.
Далее в #22: hardware multi-output и device profiles. Multi-input recording — будущая работа.
