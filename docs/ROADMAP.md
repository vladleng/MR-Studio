# Moon River Studio / Moon River Live — Roadmap

## 1. Принцип roadmap

Проект развивается двумя параллельными продуктовыми потоками и одним общим core-потоком:

```text
MRS    = DAW / production / core implementation
MRL    = Live / Performance workspace
SHARED = contracts, models and services used by both
```

MRS и MRL не используют одну последовательную нумерацию версий.

```text
MRS 0.1 -> MRS 0.2 -> MRS 0.3 ...
MRL 0.1 -> MRL 0.2 -> MRL 0.3 ...
```

Общий build Moon River Studio может содержать разные версии готовности подсистем.

Подробнее: `DEVELOPMENT_TRACKS.md`.

---

# TRACK MRS — Moon River Studio DAW

## MRS Stage 0 -> MRS 0.1 — Core Foundation

Цель: доказать жизнеспособность realtime audio architecture и базовой модели DAW до наращивания функций.

### MRS 0.1a — Project bootstrap

- monorepo structure;
- desktop shell;
- Windows CI;
- logging/config;
- tests infrastructure;
- basic app launch.

### MRS 0.1b — Technology spike

Проверить/зафиксировать:

- desktop framework;
- native C/C++ realtime core approach;
- ASIO integration;
- MIDI I/O;
- VST3 hosting path;
- high-DPI UI;
- build/packaging;
- module boundaries.

### MRS 0.1c — Project Model v1

- stable IDs;
- tracks/folders;
- clips/events abstraction;
- tempo/meter map;
- markers;
- serialization/versioning;
- migration placeholder.

### MRS 0.1d — Command / Undo + Transport contracts

- command architecture;
- Undo/Redo;
- transport interface;
- sample position;
- musical position;
- loop/seek;
- mock implementation for UI/MRL.

### MRS 0.1e — ASIO Audio Performance Gate

- enumerate vendor ASIO drivers;
- open selected native device;
- sample rate/buffer handling;
- input/output channels and vendor names;
- open vendor control panel;
- stereo playback prototype;
- low-latency input passthrough;
- realtime thread isolation;
- xrun/dropout metrics;
- benchmark against Studio Pro on identical setup.

**Blocking acceptance:** если MRS заметно уступает Studio Pro по live-stability на той же машине/interface/driver/sample-rate/buffer configuration, Stage 0 не считается закрытым до анализа причины.

---

## MRS Stage 1 -> MRS 0.2 — Audio Arrangement

Цель: первый рабочий audio project.

### MRS 0.2a — Audio tracks

- create/delete/reorder tracks;
- channel format;
- arm/monitor state;
- basic track metadata.

### MRS 0.2b — Audio clips/events

- import WAV;
- clip position/length;
- move;
- trim start/end;
- split;
- non-destructive edits.

### MRS 0.2c — Timeline / waveform

- horizontal timeline;
- waveform cache/render;
- zoom/scroll;
- playhead;
- selection.

### MRS 0.2d — Disk streaming

- read-ahead;
- preload;
- long-file playback;
- stress test.

Acceptance: открыть проект, импортировать audio, расположить clips и стабильно проиграть их через native ASIO.

---

## MRS Stage 2 -> MRS 0.3 — Mixer / Routing

- gain/pan;
- meters;
- buses;
- sends;
- input/output routing;
- mute/solo;
- master bus;
- multi-output support;
- save/restore mixer state.

Acceptance: полноценный небольшой audio project можно свести и маршрутизировать на несколько outputs.

---

## MRS Stage 3 -> MRS 0.4 — Plugins / Native DSP

### VST3

- scanning/cache;
- load/bypass;
- state save/restore;
- latency reporting;
- plugin parameter access;
- crash/isolation strategy.

### Native processors

- utility gain/filter/EQ foundation;
- processor graph integration;
- preset/state model;
- later Cab IR convolution.

Подробнее: `DSP_MODELING.md`.

---

## MRS Stage 4 -> MRS 0.5 — MIDI

- MIDI devices;
- MIDI tracks/clips;
- note events;
- velocity/duration;
- CC/program data;
- MIDI playback;
- recording;
- piano roll foundation;
- quantize/transpose/basic editing.

Acceptance: записать/создать MIDI clip, отредактировать и воспроизвести его через instrument/plugin or MIDI output.

---

## MRS Stage 5 -> MRS 0.6 — Musical Structure

- Chord Track;
- Arranger Track;
- section colors/bounds;
- markers;
- tempo/meter map editing;
- current/next musical context services;
- stable APIs for MRL.

Acceptance: проект MRS сам содержит всю структуру, ранее импортируемую из Studio Pro.

---

## MRS Stage 6 -> MRS 0.7 — AI Foundation

Цель: подготовить DAW к глубокой ChatGPT/OpenAI integration без связи с realtime thread.

- Project Context API;
- query API;
- Command/Tool API;
- permissions: READ/SUGGEST/EDIT/AUTO;
- AI operation preview/diff;
- Undo/Redo integration;
- MIDI generation contract;
- project snapshot serialization for AI;
- optional OpenAI integration prototype.

Подробнее: `AI_INTEGRATION.md`.

---

## MRS Stage 7 -> MRS 0.8+ — Advanced DAW / Reliability

Дальнейшее развитие:

- audio recording refinement;
- automation editor;
- comping/takes;
- advanced MIDI;
- native DSP expansion;
- neural amp/model player;
- project recovery/autosave;
- plugin sandboxing/refinement;
- compatibility matrix;
- installer/signing;
- long-session stress testing.

---

# TRACK MRL — Moon River Live workspace

MRL разрабатывается параллельно и использует SHARED interfaces. До готовности реального backend допустимы mocks/fixtures.

## MRL Stage 0 -> MRL 0.1 — Live UX Foundation

Цель: получить рабочий Performance UI ещё до полного DAW backend.

### MRL 0.1a — Workspace shell

- fullscreen/performance layout;
- setlist rail;
- central performance area;
- cue/notes panel;
- transport/timeline area.

### MRL 0.1b — Moving Chord Track

- горизонтальная движущаяся chord strip;
- current chord at playhead;
- previous/next chord visibility;
- bar/beat/time;
- fixture/mock chord data.

### MRL 0.1c — Section context

- current section;
- next section;
- section colors;
- progress.

### MRL 0.1d — UI/UX design system

- flat visual language;
- typography;
- laptop readability;
- high-DPI;
- safe/danger states;
- keyboard navigation.

Acceptance: MRL prototype полностью работает на mock Project/Transport API и соответствует `UI_UX_CONCEPT.md`.

---

## MRL Stage 1 -> MRL 0.2 — Real Project / Transport Integration

- connect real MRS Transport;
- connect real Chord Track;
- connect Arranger Track;
- markers/cues;
- elapsed/remaining;
- section navigation;
- loop section.

Acceptance: MRL показывает live context из настоящего открытого MRS project без экспорта.

---

## MRL Stage 2 -> MRL 0.3 — Setlists / Show Workflow

- setlist model/editor;
- previous/current/next project/song;
- preload next;
- per-song start/end behavior;
- notes;
- preflight;
- recovery state.

Acceptance: можно провести репетицию из нескольких MRS projects через один Live workspace.

---

## MRL Stage 3 -> MRL 0.4 — Playback / Click / Cue

Использует общий MRS Audio Engine:

- multitrack/stems where needed;
- sample-locked playback;
- click;
- cue;
- dedicated output routing;
- global trim/gain live preparation;
- next-song preload;
- no dropout on transitions.

Acceptance: stems + click/cue надёжно работают на разных outputs в полном setlist.

---

## MRL Stage 4 -> MRL 0.5 — Live Inputs / Patches

- low-latency input monitoring;
- live-safe plugin/native processor chains;
- guitar/keys/vocal patch model;
- patch per song/section;
- preload/warm next patch;
- safe switching/crossfade where appropriate;
- plugin latency warnings.

Acceptance: live instruments обрабатываются общим MRS engine и безопасно меняют patches по секциям.

---

## MRL Stage 5 -> MRL 0.6 — MIDI Automation / Hardware Control

- timeline MIDI actions;
- Program Change;
- Control Change;
- Note triggers;
- foot controller mapping;
- learn mode;
- no duplicate actions;
- panic/all-notes-off;
- reconnect state.

---

## MRL Stage 6 -> MRL 0.7 — Remote / Mobile Companion

- local state API;
- pairing;
- WebSocket/live state;
- read-only phone/tablet view first;
- current/next chord;
- section;
- bar;
- cue;
- setlist;
- later limited transport controls.

---

## MRL Stage 7 -> MRL 1.0 — Concert Reliability

- multi-hour playback;
- repeated project/song switching;
- audio device recovery;
- MIDI reconnect;
- network/remote disconnect;
- problematic plugin handling;
- autosave show state;
- crash recovery;
- full preflight;
- performance regression suite;
- real rehearsal/full-show acceptance.

Acceptance: полный концертный set выполняется внутри MRS Live workspace без необходимости переходить в production UI.

---

# SHARED — общие блоки

Общие components оформляются отдельными `[SHARED]` issues и не принадлежат только MRS или MRL:

- Project Model contracts;
- Transport API;
- Chord Track model;
- Arranger model;
- Command/Undo;
- device abstraction;
- plugin state model;
- MIDI event model;
- serialization/versioning;
- state/event bus;
- test fixtures.

---

# Integration Milestones

## Milestone A — MRS Core Prototype

- app shell;
- Project Model;
- Transport contract;
- ASIO prototype;
- performance benchmark.

## Milestone B — MRL Interactive Prototype

- Live workspace;
- moving Chord Track;
- sections/cues;
- mock transport/project data.

MRS and MRL work on A/B can proceed in parallel.

## Milestone C — First MRS + MRL Integration

- real transport;
- real Chord/Arranger data;
- MRL driven by open MRS project.

## Milestone D — First Rehearsal Build

- audio project;
- setlist;
- click/cue;
- stable ASIO;
- basic patches/MIDI where needed.

## Milestone E — First Full Show Build

- complete setlist workflow;
- preflight;
- recovery;
- long-duration stress-test passed.

---

# Studio Pro compatibility track

Studio Pro Bridge больше не является обязательным Stage основной архитектуры.

Его можно вести отдельным optional issue track:

```text
[COMPAT:StudioPro] Import/Bridge research
[COMPAT:StudioPro] Chord/Arranger migration
[COMPAT:StudioPro] Project conversion tools
```

Это позволяет сохранить ценность существующих Studio Pro проектов, не связывая развитие MRS/MRL с закрытым форматом другой DAW.
