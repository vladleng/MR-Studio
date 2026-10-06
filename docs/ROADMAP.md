# Moon River Studio — Roadmap

## 1. Главный принцип

**Moon River Studio** (рабочее сокращение `MRS` / `MR Studio`) — единственная DAW и единственный продукт проекта.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Studio Pro. Это не отдельное приложение и не отдельная продуктовая версия.

Разработка организована тремя техническими потоками:

```text
MRS    = пользовательские DAW-функции Arrange / Edit / Mix
SHARED = единый Core / Engine всей DAW
LIVE   = встроенный Live Mode
```

При этом version line существует только у Moon River Studio:

```text
MRS 0.1 -> 0.2 -> 0.3 -> ...
```

Live Mode отслеживается через `LIVE Stage 0..7` и входит в соответствующие сборки MRS.

Главный tracking issue: #1.

---

# SHARED — единое ядро Moon River Studio

Parent: #13

SHARED не является отдельным продуктом. Это backend, которым одновременно пользуются Arrange/Edit/Mix и Live Mode.

## SHARED Stage 0 — Core Contracts — #15

- Project Model v1;
- stable IDs;
- Command / Undo;
- `ITransport`;
- sample/musical position;
- event/state API;
- serialization/versioning foundation;
- mocks/fixtures.

## SHARED Stage 1 — Audio Engine / ASIO — #16

- vendor ASIO drivers;
- device/sample-rate/buffer/channel management;
- realtime-safe callback;
- playback graph;
- low-latency input path;
- streaming/preload foundation;
- routing contracts;
- xrun/dropout metrics;
- performance benchmark against Studio Pro.

**Performance gate:** если Moon River Studio заметно уступает Studio Pro по live-stability на одинаковом hardware/driver/buffer setup, расширение тяжёлых функций не имеет приоритета над анализом и оптимизацией engine.

## SHARED Stage 2 — Musical Timeline — #17

- tempo/meter map;
- bar/beat/sample conversion;
- Chord Track model;
- Arranger Section model;
- markers/cues;
- current/next chord/section services;
- navigation contracts.

## SHARED Stage 3 — MIDI / Plugin Graph — #18

- MIDI event/device/routing model;
- processor/plugin graph;
- VST3 state model;
- latency reporting;
- native processor interface;
- preset/patch state;
- preload/warm hooks.

## SHARED Stage 4 — Persistence / State / Recovery — #19

- project serialization/migrations;
- mixer/plugin/MIDI state;
- live metadata inside project;
- show-level state references;
- autosave/recovery foundation;
- compatibility tests.

## SHARED Audio capability — Time Stretch / Pitch Shift — #56

Post-stage capability общего Audio Engine. Первый backend — **Signalsmith Stretch** (MIT), но Core остаётся backend-neutral.

Child issues:

- #57 — shared contract/state;
- #58 — pinned dependency + adapter + licensing;
- #59 — realtime playback/latency/RT safety;
- #60 — offline/HQ render/cache;
- #61 — clip controls/tempo mapping/persistence;
- #62 — quality/CPU/regression benchmark.

Полный план: `TIME_STRETCH.md`.

Это не часть Plugin/Native DSP #23 и не отдельный Live engine. Arrange/Edit/Mix/Live используют один SHARED stretch path.

---

### Core rule

```text
                  SHARED CORE
                       |
       +---------------+---------------+
       |               |               |
   Arrange/Edit        Mix          Live Mode
```

Никаких отдельных Audio Engine, Transport, MIDI Engine или Plugin Host для Live Mode.

---

# MRS — основная линия развития DAW

Parent: #12

## MRS Stage 0 -> MRS 0.1 — DAW Foundation — #20

- monorepo/application structure;
- desktop shell;
- Windows CI;
- logging/config/tests;
- technology spike;
- workspace navigation: Arrange / Edit / Mix / Live;
- SHARED Project Model/Transport connection;
- basic timeline/playhead;
- audio settings over SHARED Audio Engine;
- high-DPI foundation.

### Acceptance
MRS запускается как DAW shell, открывает project model, управляет общим Transport и воспроизводит SHARED audio prototype.

### Параллельный Live старт
После фиксации SHARED contracts #15/#17 можно параллельно начинать LIVE Stage 0 #28 на mock backend. Это UI/prototyping work, а не отдельная DAW.

---

## MRS Stage 1 -> MRS 0.2 — Audio Arrangement — #21

- audio tracks;
- WAV import;
- clips/events;
- non-destructive move/trim/split;
- waveform;
- zoom/scroll/selection;
- disk read-ahead;
- recording/monitor foundation;
- project save/load.

### Acceptance
Создать проект, импортировать и отредактировать audio clips и стабильно воспроизвести их через native ASIO.

---

## MRS Stage 2 -> MRS 0.3 — Mixer / Routing — #22

- gain/pan;
- mute/solo;
- meters;
- buses/subgroups;
- sends/returns;
- input/output routing;
- multi-output;
- routing/device profiles;
- mixer state persistence.

Routing graph принадлежит SHARED Core и позднее напрямую используется Live Mode.

---

## MRS Stage 3 -> MRS 0.4 — Plugins / Native DSP — #23

- VST3 scan/cache/load/bypass;
- plugin state;
- latency reporting;
- parameter access;
- insert-chain UI;
- native processor API;
- utility DSP;
- Cab IR convolution;
- preset/state model;
- neural model foundation;
- plugin isolation strategy.

Подробнее: `DSP_MODELING.md`.

---

## MRS Stage 4 -> MRS 0.2a–0.2h — MIDI / метроном — #24

Актуальное разбиение от 2026-10-06: [MRS_STAGE_4_PLAN](MRS_STAGE_4_PLAN.md).
4a реализован локально: [MIDI input/VST3](MIDI_LIVE_INPUT.md); Release/111 CTest
и packaged GUI smoke прошли. [Физическая приёмка](MRS_STAGE_4A_CHECKLIST.md)
ожидается; #63 и весь #24 открыты. 4b–4h ещё не реализованы.
4a / 0.2a — MIDI-вход и VST3-инструменты; 4b / 0.2b — клипы/playback;
4c / 0.2c — запись; 4d / 0.2d — piano roll; 4e / 0.2e — quantize/transpose;
4f / 0.2f — CC/Program Change; 4g / 0.2g — внешний MIDI;
4h / 0.2h — метроном и precount для записи,
затем итоговая совместная проверка Stage 4.
Доработки получают updN, исправления fixN в пределах базовой версии.
Прежний MRS 0.5 был ориентиром milestone; пользовательская схема выше имеет приоритет.

- MIDI devices;
- MIDI tracks/clips;
- notes/velocity/duration;
- CC/program data;
- playback/recording;
- piano roll;
- quantize/transpose;
- plugin/external routing;
- метроном и precount для audio/MIDI записи.

Тот же MIDI backend используется Live Mode для automation и hardware control.

---

## MRS Stage 5 -> MRS 0.6 — Musical Structure — #25

- Chord Track editor;
- Arranger Track editor;
- section colors/bounds;
- markers;
- tempo/meter editing;
- section/marker navigation;
- current/next musical context.

### Acceptance
Проект MRS сам содержит музыкальную структуру, необходимую Live Mode.

### Live integration point
На этом этапе LIVE Stage 1 #29 получает реальный Chord/Arranger/Transport context открытого проекта. С этого момента Live Mode становится не только UI prototype, а нативным режимом реального MRS project.

---

## MRS Stage 6 -> MRS 0.7 — AI Foundation — #26

- Project Context API;
- query API;
- DAW Tool / Command API;
- READ / SUGGEST / EDIT / AUTO permissions;
- preview/diff;
- Undo/Redo integration;
- MIDI generation/edit contract;
- project snapshot;
- optional OpenAI prototype;
- AI worker isolation from realtime audio.

Подробнее: `AI_INTEGRATION.md`.

---

## MRS Stage 7 -> MRS 0.8+ — Advanced DAW / Reliability — #27

- automation editor;
- advanced recording/takes/comping;
- advanced MIDI;
- project autosave/recovery;
- plugin isolation refinement;
- native DSP/neural expansion;
- compatibility matrix;
- installer/signing;
- long-session stress tests;
- performance regression suite.

---

# LIVE — встроенный Live Mode

Parent: #14

Live Mode не имеет отдельного version line. Его Stage показывает степень готовности performance-режима внутри текущей Moon River Studio build.

## LIVE Stage 0 — UX Foundation — #28

Можно вести параллельно после фиксации #15/#17 contracts.

- fullscreen/performance workspace;
- flat UI;
- setlist rail placeholder;
- moving horizontal Chord Track;
- current/next section;
- cue/notes/patch panel;
- bar/beat/time;
- transport controls;
- mock/fixture backend;
- laptop/high-DPI readability.

## LIVE Stage 1 — Real Project / Transport Integration — #29

- real Project Model;
- real Transport;
- real Chord/Arranger data;
- markers/cues;
- elapsed/remaining;
- section navigation/loop;
- no-export integration with open MRS project.

## LIVE Stage 2 — Setlists / Show Workflow — #30

- setlists referencing MRS projects;
- previous/current/next song;
- preload next project;
- start/end policies;
- show notes/state;
- preflight;
- recovery;
- safe project switching.

## LIVE Stage 3 — Playback / Click / Cue — #31

Uses SHARED Audio Engine:

- stems/multitrack where needed;
- sample-locked playback;
- click/cue;
- dedicated outputs;
- live playback gain;
- non-destructive Trim Start/End;
- next-song preload;
- transition stress tests.

## LIVE Stage 4 — Live Inputs / Patches — #32

Uses SHARED Audio/Plugin Graph:

- low-latency monitoring;
- guitar/keys/vocal patches;
- patch per song/section;
- preload/warm next patch;
- safe switching;
- plugin latency/live-safe warnings.

## LIVE Stage 5 — MIDI Automation / Hardware Control — #33

Uses SHARED MIDI backend:

- PC/CC/Note actions;
- bar/beat/section/marker triggers;
- foot-controller mapping;
- learn mode;
- reconnect/safety;
- panic/all-notes-off.

## LIVE Stage 6 — Remote / Mobile Companion — #34

- local live-state API;
- discovery/pairing;
- read-only mobile/tablet first;
- chord/section/bar/cue/setlist state;
- later limited controls;
- network completely isolated from realtime audio.

## LIVE Stage 7 — Concert Reliability — #35

- multi-hour playback;
- repeated song/project switching;
- device recovery;
- MIDI reconnect;
- remote disconnect;
- plugin failure handling;
- full preflight;
- show-state recovery;
- performance regression;
- rehearsal/full-show acceptance.

Any backend performance fix belongs to SHARED Core, not to a separate Live engine.

---

# Integration milestones

## A — Core Prototype
#15 + #16 + #20

## B — Live UI Prototype
#28 in parallel on mocks.

## C — First native Live integration
#17 + #25 + #29: Live Mode reads the same open MRS project.

## D — First Rehearsal Build
#30 + #31 plus #32/#33 as required.

## E — First Full Show Build
#35 passes rehearsal/full-show stress tests.

---

# Studio Pro compatibility

Studio Pro is an optional import/migration source, not a runtime dependency.

Issue: #3

```text
Studio Pro project
      |
Bridge / Import adapter
      v
MRS Project Model
      |
Arrange / Edit / Mix / Live Mode
```

This preserves existing Studio Pro projects while keeping Moon River Studio self-contained.

## Версии сборок — уточнение 2026-10-03
Пользовательские версии подэтапов и суффиксы определяются VERSIONING.md:
0.1b, 0.1c; updN для небольших обновлений, fixN для ошибок.
Номерные MRS version targets выше — прежние ориентиры функциональных milestones;
они не задают имя текущего артефакта. Текущий tracks/import/waveform — 0.1b,
clip editing — следующая 0.1c. Stage IDs не перенумеровываются.
