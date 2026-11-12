# Moon River Studio — Architecture

## 1. Продуктовая граница (действующий контракт)

**MR Studio** (DAW) и **MR Live** (performance/show) — **два самостоятельных desktop-приложения** с независимыми линиями версий. Общий monorepo и Shared Core не означают единую программу, один файл проекта или Live workspace внутри Studio.

```text
 MR Studio (.mrsproject)      MR Live (.mrlive show)
 Arrange / Edit / Mix         Setlist / Item / Patch / Playback
             \                 /
              Shared Core / Services
     Audio / MIDI / Transport / Plugin Host
     Processing / Model APIs / Persistence
```

MR Live можно запустить с Home MR Studio, но оно функционирует независимо и хранит собственный show. Item **не открывает Studio project напрямую**; экспорт Studio → Live как Playback — отдельная будущая функция. MR Studio по умолчанию поддерживает ARA2; MR Live standalone не использует ARA2. Подробности — [MR Live native integration](MR_LIVE_NATIVE_INTEGRATION.md) и [AGENTS.md](../AGENTS.md).

## 2. Главная архитектурная граница

Workspace UI не должен владеть низкоуровневыми engine-компонентами.

Правильно:

```text
Workspace UI
     |
Application Services / Commands
     |
Project Model + Engine APIs
     |
Audio / MIDI / Plugins / Storage
```

Неправильно:

```text
Live UI -> own ASIO engine
Mix UI  -> own transport
Arrange -> separate project state
```

## 3. Shared Project Model

Shared Core определяет общие сущности и интерфейсы; versioned DAW Project Model используется MR Studio. MR Live отдельно сохраняет show-level данные в `.mrlive`:

```text
Project
├── identity
├── tracks
├── folders
├── clips/events
├── MIDI
├── tempo/meter map
├── chord track
├── arranger track
├── markers
├── automation
├── mixer/routing state
├── plugin/native processor state
└── live metadata
```

MR Studio использует проектные Commands/Undo и Persistence. MR Live использует совместимые общие сервисы, но владеет show/item state отдельно; идентичность DAW и show-документа не предполагается.

## 4. Command / Undo System

Изменения проекта должны проходить через общий Command layer:

```text
Command
├── validate
├── execute
├── undo
└── optional preview/diff/audit
```

Это позволяет одним механизмом обслуживать:

- Arrange/Edit/Mix UI;
- MIDI editing;
- Live actions там, где они изменяют project/show state;
- AI Tool API;
- будущие remote/control operations.

## 5. Transport

Один Transport для всей DAW:

- play / pause / stop;
- seek;
- sample position;
- musical position;
- loop range;
- tempo/meter sync;
- section/marker navigation.

В MR Studio и MR Live применяются общие транспортные контракты, но каждый запущенный документ владеет соответствующим state. Конкретный show/item workflow определяет [MR Live native integration](MR_LIVE_NATIVE_INTEGRATION.md), а текущая реализация проверяется по коду и Issues.

## 6. Audio Engine

Audio Engine — общий SHARED Core.

```text
Audio thread != UI thread != file/network/AI thread
```

На Windows основной professional/live backend должен поддерживать прямую работу через vendor ASIO driver.

Audio Engine включает:

- device layer;
- ASIO/WASAPI backends;
- realtime processing graph;
- mixer/buses/routing;
- disk streaming;
- monitoring;
- plugin/native processing;
- metering;
- latency accounting;
- preload/read-ahead;
- xrun/dropout diagnostics.

Studio и MR Live используют **общую реализацию Engine API/backend**; это не утверждение, что два запущенных процесса разделяют один runtime instance.

Подробнее: `AUDIO_ENGINE.md`.

## 6.1. Time Stretch / Pitch Shift

Time-stretch / pitch-shift является возможностью **SHARED Audio Engine**, а не функцией отдельного workspace.

Первый backend: **Signalsmith Stretch** (MIT), tracking #56.

```text
Clip / Timeline / Transport / Render
              |
       ITimeStretchEngine
              |
     +--------+---------+
     |                  |
Signalsmith          future backends
(first)             Rubber Band / élastique / ...
```

Signalsmith не должен протекать типами или preset names в Project Model, Transport, clip serialization или UI. Сохраняется пользовательское намерение — ratio/pitch/formant policy — а backend остаётся заменяемым.

Realtime path подчиняется общим правилам Audio Engine: prepare/reconfigure вне callback, bounded/preallocated buffers, no blocking locks/file I/O/heap growth в callback и явный latency accounting.

Подробнее: `TIME_STRETCH.md`.

## 7. Low-latency path и process path

Архитектура должна позволять разделять интерактивный live monitoring и тяжёлую обработку/playback.

```text
                    Audio Engine
                         |
            +------------+------------+
            |                         |
     Low-latency path            Process path
     live monitoring             playback / mix
            |                         |
            +------------+------------+
                         |
                       Mixer
```

Фактическая реализация определяется техническим spike и performance benchmark.

Это свойство полезно и в production mode, и в MR Live: большая сессия не должна автоматически разрушать низкую latency живого входа.

## 8. MIDI Engine

Общий MIDI backend обслуживает:

- MIDI input/output;
- timestamped events;
- MIDI tracks/clips;
- notes/CC/program data;
- recording/playback;
- instrument/external routing;
- hardware mappings;
- Live timeline actions;
- panic/all-notes-off.

Studio MIDI editor и MR Live automation используют общие контракты event/device model, сохраняя раздельное владение документами.

## 9. Plugin / Processor Graph

Один processing graph для production и performance.

External target:

- VST3.

Native processors также подключаются к этому graph.

Основные функции:

- scan/cache/load/unload;
- state save/restore;
- latency reporting;
- parameter access;
- automation hooks;
- safe bypass;
- plugin failure strategy;
- processor/preset state;
- preload/warm state hooks.

MR Live patch хранит ссылку/снимок show-owned канала поверх общего processor graph; не создавать отдельный plugin-host/DSP backend.

## 10. Native DSP / modeling

MRS может включать:

- utility processors;
- EQ/compression/saturation;
- convolution/Cab/Room IR;
- amp/preamp/pedal DSP;
- neural model player;
- Moon River factory models/captures.

Подробнее: `DSP_MODELING.md`.

## 11. UI / document boundaries

MR Studio содержит **Arrange**, **Edit** и **Mix** для `.mrsproject`. MR Live имеет собственные **Setlist**, **Item**, **Playback**, **Patch/Sub-patch** и performance-представления для `.mrlive`. Они могут переиспользовать shared controls и компоненты данных, но не копируют один и тот же workspace/document state.

```text
Studio UI -> Studio Application/Commands -> Shared Core
Live UI   -> Live Application/Show Commands -> Shared Core
```

LiveState — runtime-проекция текущего show/item (transport, section/chord, selected patch, cue, setlist, connected remotes), а не открытый Studio project.

## 12. Moving Chord Track

MR Live использует горизонтальную движущуюся Chord Track strip:

```text
Dm7 | G7 | [ Gmaj7 ] | Em7 | Am7 | D7
             ^ playhead
```

Источник аккордов — SHARED Chord Track model. Позиция — общий Transport.

Подробнее: `UI_UX_CONCEPT.md`.

## 13. Setlist / Show State

Setlist и Item принадлежат **MR Live show-документу** `.mrlive`, а не единой DAW Project Model. Show-state включает порядок items, собственные Playback/stems, performance channels, patch/sub-patch, cues, preflight и recovery. Переиспользуются Core APIs/Graph/Transport, но нельзя создавать прямую ссылку на открытый `.mrsproject` как на Live item по старому сценарию.

При изменении show-state соблюдать стабильность IDs, Undo/Redo, Save/Reopen и realtime safety. Действующие детали: [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md), [#289](https://github.com/vladleng/MR-Studio/issues/289).

## 14. AI Layer

AI работает через подготовленный Context / Tool API:

```text
OpenAI / ChatGPT
      |
AI Controller
      |
Context + Command API
      |
Project Model / Application Services
```

AI может читать/анализировать проект и формировать разрешённые commands, но не обращается к ASIO callback напрямую.

Подробнее: `AI_INTEGRATION.md`.

## 15. Remote API

Remote/mobile получает подготовленный application/live state:

```text
Shared Application Services
      |
local API / WebSocket
      |
Remote Client
```

Remote failure не должен влиять на Audio Engine.

## 16. Studio Pro compatibility

Fender Studio Pro — optional import/migration source, а не runtime dependency.

```text
Studio Pro project
      |
Bridge / Import adapter
      v
MRS Project Model
      |
Arrange / Edit / Mix
```

Подробнее: `STUDIO_PRO_INTEGRATION.md`.

## 17. Thread isolation

Ни один из этих компонентов не должен блокировать realtime callback:

- UI;
- waveform generation;
- serialization;
- network/remote;
- AI;
- plugin scanning/loading;
- logging;
- import/parsing.

Для realtime state используются подходящие bounded / lock-free / atomic механизмы там, где они действительно необходимы.

## 18. Репозиторий (логическая карта)

```text
apps/studio-desktop/   desktop application/services
apps/studio-juce/      JUCE frontend / Studio and Live integration
core/                 shared audio, processing, model APIs, persistence
tests/                contract / feature regression checks
assets/ui/mr-uds/     common SVG/GUI assets
.agents/skills/       agent skills and workflow guidance
tools/ci/             pinned SDK, package and validation scripts
docs/                 lasting technical contracts only
```

Структура не заменяет фактическое дерево Git. Изменения Shared Core требуют проверять влияние на **оба** приложения.

## 19. Development tracks

`[MRS]` — Studio DAW, `[LIVE]` — отдельное MR Live, `[SHARED]` — общий Core. Это **разные приложения**, но один общий engine/backend. Этапы и приёмка отслеживаются в GitHub Issues, а не в Markdown-журнале. См. [DEVELOPMENT_TRACKS.md](DEVELOPMENT_TRACKS.md).

## 20. Fail-safe модель

Особенно для MR Live:

- UI failure не останавливает звук;
- AI/network failure не влияет на playback;
- remote disconnect безопасен;
- audio device errors диагностируются явно;
- MIDI actions не дублируются;
- project/show state восстанавливается;
- performance-blocking regressions блокируют live release readiness.

## 21. Рабочее название

Текущее название всей DAW:

- `Moon River Studio`;
- сокращённо `MRS` или `MR Studio`.

Название считается рабочим и может быть заменено на более ёмкое ближе к зрелому релизу без изменения архитектурных границ.


## Stage 1d / 0.1e recording foundation
The existing SHARED AudioEngine now supports raw mono ASIO capture through a fixed
ring and background WAV writer, independent monitoring and one-step take attachment
through the existing ProjectStore/Undo. Same engine/device/transport, archive schema
unchanged. See [recording contracts](RECORDING.md) and
[Windows checklist](MRS_STAGE_1D_CHECKLIST.md). Hardware acceptance pending.

## 2026-10-10 — Revised architecture: MR Strip Link + Pre-Spatial FX Sends

**Authoritative contracts:** [PRE_SPATIAL_ROUTING.md](PRE_SPATIAL_ROUTING.md) and [MR_STRIP_LINK.md](MR_STRIP_LINK.md); tracking [#133](https://github.com/vladleng/MR-Studio/issues/133). **Future design only, not implemented.**

The earlier plan for a special Dry Group Bus that would process a sum and send the result **back to individual sources**, or general per-track/bus `Spatial / Direct` output switches, is **cancelled**. Traditional audio buses remain unchanged and available, but their sums become **one composite source** if spatialized; they cannot reconstruct individual positions after group saturation/compression.

**Preferred group processing/control:** each instrument owns a normal native **MR Strip** instance, and a project-owned **MR Strip Link Group** lets the editor of one member control linked EQ/Dynamics/Saturation parameters on other members. Link is **control-only**; no audio summing, no extra path, no duplicate dry. Each track retains its own Spatial source/position. Linked compressor settings do **not** imply a shared bus-compressor detector or processing of the summed audio.

**Only the FX Sends / Returns receive a special bypass around MR Spatial.** Send tap is before the spatial scene (while preserving existing Pre/Post-Fader semantics), FX Return comes to final mix independently of MR Spatial. For reverb send returns, wet-only processing prevents accidentally duplicating dry. Ordinary main paths go through shared spatial scene when enabled; bypassing the scene globally must retain FX returns/tails. No hidden second copy of instrument direct signal.

Preserve existing SHARED Audio Engine / #22 Mixer graph, legacy session compatibility, DAG validation, mute/solo, PDC/latency, host automation and Undo/Redo. MR Reverb #128–#132 is independent and should not be interrupted to implement this future routing plan.
