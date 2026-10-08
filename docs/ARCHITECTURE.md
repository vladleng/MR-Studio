# Moon River Studio — Architecture

## 1. Продуктовая граница

**Moon River Studio** (`MRS`, рабочее имя `MR Studio`) — единственное desktop-приложение и единственная DAW проекта.

Live Mode — встроенный Performance / Show режим этой же DAW.

```text
┌───────────────────────────────────────────────┐
│              Moon River Studio               │
│                                               │
│   Arrange | Edit | Mix | Project | Live      │
└───────────────────────┬───────────────────────┘
                        │
                        ▼
┌───────────────────────────────────────────────┐
│                  SHARED CORE                  │
│                                               │
│ Project Model / Commands / Transport          │
│ Audio / MIDI / Plugins / Musical Timeline     │
│ Persistence / State / Recovery                │
└───────────────────────────────────────────────┘
```

Live Mode не является отдельным приложением, не открывает отдельный project format и не имеет собственного realtime engine.

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

Единая versioned-модель проекта:

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

Все режимы читают и изменяют одну модель через общие commands/services.

Live Mode строит runtime performance-state из Project Model, но не создаёт копию музыкальной структуры.

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

Переключение `Arrange -> Live` не создаёт новый transport и не перезапускает project engine.

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

Arrange playback, Mix и Live Mode используют **один и тот же engine**.

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

Это свойство полезно и в production mode, и в Live Mode: большая сессия не должна автоматически разрушать низкую latency живого входа.

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

MIDI editor и Live automation работают с одной event/device model.

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

Live patch — это state/snapshot общего processor graph, а не отдельная plugin-chain система.

## 10. Native DSP / modeling

MRS может включать:

- utility processors;
- EQ/compression/saturation;
- convolution/Cab/Room IR;
- amp/preamp/pedal DSP;
- neural model player;
- Moon River factory models/captures.

Подробнее: `DSP_MODELING.md`.

## 11. Workspaces

### Arrange

Timeline-oriented production workspace.

### Edit

Detailed audio/MIDI/automation editing.

### Mix

Mixer/routing/plugin workspace.

### Live Mode

Performance-oriented представление того же проекта:

```text
LiveState
├── currentProject/song
├── transportState
├── currentPosition
├── currentSection
├── nextSection
├── currentChord
├── nextChord
├── activePatch
├── pendingCue
├── setlistState
└── connectedRemotes
```

LiveState — runtime projection. Project entities остаются общими.

## 12. Moving Chord Track

Live Mode использует горизонтальную движущуюся Chord Track strip:

```text
Dm7 | G7 | [ Gmaj7 ] | Em7 | Am7 | D7
             ^ playhead
```

Источник аккордов — SHARED Chord Track model. Позиция — общий Transport.

Подробнее: `UI_UX_CONCEPT.md`.

## 13. Setlist / Show State

Setlist относится к Live Mode/application-level state и ссылается на MRS projects.

Он отвечает за:

- порядок песен/projects;
- preload следующего project state;
- next/previous;
- inter-song policy;
- preflight;
- recovery.

Show state не должен копировать полный Project Model. Он хранит ссылки и show-specific параметры.

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
MRS Application Core
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
Arrange / Edit / Mix / Live
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

## 18. Предлагаемая структура monorepo

```text
Moon-River-Studio/
├── apps/
│   └── studio-desktop/
├── core/
│   ├── audio/
│   ├── midi/
│   ├── plugins/
│   ├── transport/
│   ├── project-model/
│   ├── commands/
│   └── storage/
├── workspaces/
│   ├── arrange/
│   ├── edit/
│   ├── mix/
│   └── live/
├── native-dsp/
├── ai/
├── remote/
├── ui/
├── tests/
├── tools/
└── docs/
```

## 19. Development tracks

В одном monorepo Issues разделяются для удобства параллельной работы:

```text
[MRS]    DAW features/workspaces
[SHARED] common Core/Engine
[LIVE]   embedded Live Mode
```

Это организационные tracks, а не разные приложения.

Подробнее: `DEVELOPMENT_TRACKS.md`.

## 20. Fail-safe модель

Особенно для Live Mode:

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
