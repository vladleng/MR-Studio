# Moon River Studio / Moon River Live — Architecture

## 1. Общая схема

```text
┌───────────────────────────────────────────────┐
│              Moon River Studio               │
│                                               │
│ Arrange | Edit | Mix | Project | Live (MRL) │
└───────────────────────┬───────────────────────┘
                        │
                        ▼
┌───────────────────────────────────────────────┐
│                Shared Project Model           │
│ tracks / clips / MIDI / chords / arranger    │
│ markers / tempo / automation / mixer state   │
└───────────────────────┬───────────────────────┘
                        │
        ┌───────────────┼────────────────┐
        ▼               ▼                ▼
   Audio Engine      MIDI Engine     Plugin Engine
        │               │                │
   ASIO/WASAPI        MIDI I/O           VST3
```

Moon River Live (MRL) — это workspace внутри Moon River Studio (MRS), а не отдельный realtime engine.

## 2. Главная архитектурная граница

UI/workspace не должен владеть низкоуровневыми engine-компонентами.

Правильная зависимость:

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
Live UI -> ASIO directly
Mixer UI -> own transport
Arrange UI -> separate project state
```

## 3. Core modules

### 3.1 Project Model

Единый versioned model проекта:

```text
Project
├── tracks
├── folders
├── clips/events
├── MIDI
├── tempo/meter map
├── chord track
├── arranger track
├── markers
├── automation
├── mixer state
├── plugin state
└── live metadata
```

Project Model должен иметь stable IDs, serialization/versioning и не зависеть от конкретного workspace.

### 3.2 Command / Undo System

Все изменения проекта выполняются через команды:

```text
Command
├── validate
├── execute
├── undo
└── serialize/audit where useful
```

Это используется UI, MIDI editing, AI layer и будущими remote/control interfaces.

### 3.3 Transport

Единый transport для всех workspaces:

- play/pause/stop;
- seek;
- sample position;
- musical position;
- loop range;
- tempo/meter sync;
- section navigation.

MRL использует тот же transport, что Arrange/Edit/Mix.

## 4. Audio Engine

Audio Engine — общий MRS Core.

Критические правила:

```text
Audio thread != UI thread != file/network/AI thread
```

На Windows основным live/performance backend должен быть vendor ASIO driver аудиоинтерфейса.

В Audio Engine входят:

- device layer;
- ASIO/WASAPI backend;
- realtime graph;
- mixer/buses;
- disk streaming;
- monitoring;
- plugin processing;
- metering;
- latency accounting;
- preload/read-ahead;
- xrun/dropout diagnostics.

Подробнее: `AUDIO_ENGINE.md`.

## 5. Live path и process path

Архитектура должна позволять отделять low-latency monitoring от тяжёлой playback/mix обработки.

Концептуально:

```text
                  Audio Engine
                       |
          +------------+------------+
          |                         |
   Low-latency path            Process path
   live inputs                 playback/mix
   small buffer                larger processing window
          |                         |
          +------------+------------+
                       |
                     Mixer
```

Фактическая реализация определяется Stage MRS Core и performance tests.

## 6. MIDI Engine

Отвечает за:

- MIDI input/output;
- timestamped events;
- MIDI clips;
- note/CC/program data;
- recording;
- playback;
- hardware mappings;
- Live actions;
- panic/all-notes-off.

MIDI editing и MRL automation используют один engine/model.

## 7. Plugin Engine

Первый внешний plugin target — VST3.

Функции:

- scan/cache;
- load/unload;
- state save/restore;
- latency reporting;
- parameter access;
- safe bypass;
- crash/isolation strategy;
- live-safe classification.

Native MRS DSP также подключается к общему processing graph.

## 8. Native DSP / modeling

MRS может включать:

- utility processors;
- convolution/Cab IR;
- amp/preamp/pedal DSP;
- neural model player;
- Moon River factory models/captures.

Подробнее: `DSP_MODELING.md`.

## 9. Workspaces

### Arrange

Timeline-oriented production workspace.

### Edit

Audio/MIDI detailed editing.

### Mix

Mixer/routing/plugin workspace.

### Live / MRL

Performance-oriented projection того же Project Model:

```text
LiveState
├── currentSong/project
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

MRL не дублирует project entities, а строит runtime LiveState из них.

## 10. Moving Chord Track

Live UI использует горизонтальную движущуюся полосу Chord Track:

```text
Dm7 | G7 | [ Gmaj7 ] | Em7 | Am7 | D7
             ^ playhead
```

Источник данных — общий Chord Track Project Model, позиция — общий Transport.

Подробнее по UI: `UI_UX_CONCEPT.md`.

## 11. Setlist Engine

Setlist является MRL/application-level model и ссылается на проекты/песни MRS.

Отвечает за:

- порядок песен;
- preload следующего проекта/song state;
- next/previous;
- inter-song policy;
- preflight;
- recovery.

## 12. AI Layer

AI работает только через подготовленный Context/Tool API.

```text
OpenAI / ChatGPT
      |
AI Controller
      |
Context + Command API
      |
Project Model / Application Services
```

AI может читать/анализировать проект и формировать commands, но не обращается к ASIO callback напрямую.

Подробнее: `AI_INTEGRATION.md`.

## 13. Remote API

Remote/mobile получает подготовленный application/live state:

```text
MRS Application Core
      |
local API / WebSocket
      |
Remote Client
```

Remote failure не должен влиять на Audio Engine.

## 14. Studio Pro compatibility

Studio Pro Bridge становится опциональным import/migration adapter, а не фундаментом MRS.

```text
Studio Pro project
      |
optional Bridge/import
      |
MRS Project Model
```

Это позволяет переносить старые workflow, не создавая runtime dependency от Studio Pro.

## 15. AI/remote/UI isolation from realtime

Ни один из этих компонентов не должен блокировать audio callback:

- UI;
- waveform generation;
- project serialization;
- network;
- remote;
- AI;
- plugin scanning/loading;
- logging;
- metadata parsing.

Для realtime state используются bounded/lock-free/atomic mechanisms там, где это требуется.

## 16. Возможная структура monorepo

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

## 17. Development tracks

Внутри одного monorepo разработка ведётся тремя потоками:

```text
[MRS]    DAW/core
[MRL]    Live workspace
[SHARED] общие contracts/services
```

MRL может использовать mock implementations общих API, пока MRS backend ещё разрабатывается.

Подробнее: `DEVELOPMENT_TRACKS.md`.

## 18. Fail-safe модель

Особенно в Live mode:

- UI failure не останавливает звук;
- AI/network failure не влияет на playback;
- remote disconnect безопасен;
- audio device errors диагностируются явно;
- MIDI actions не дублируются;
- project/show state сохраняется для recovery;
- performance-blocking regressions блокируют релиз.

## 19. Технологический выбор

Конкретный framework окончательно выбирается после spike, но критерии уже фиксированы:

- Windows-first;
- native low-latency ASIO;
- realtime-safe C/C++-подход для core audio;
- VST3 hosting;
- MIDI;
- современный high-DPI UI;
- тестируемые module boundaries;
- возможность remote/mobile клиента;
- CI/packaging;
- отсутствие зависимости realtime engine от UI technology.
