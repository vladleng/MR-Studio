# Moon River Studio / Moon River Live — Project Vision

## 1. Назначение

**Moon River Studio (MRS)** — собственная DAW Moon River Studio.

**Moon River Live (MRL)** — встроенный Live / Performance workspace внутри MRS, по смыслу близкий к Show Page в Studio Pro, но работающий на том же Project Model и Audio Engine, что и production-режимы DAW.

Цель проекта — создать performance-first DAW, в которой создание музыки и живое исполнение являются двумя режимами одного проекта, а не двумя разными приложениями с экспортом между ними.

## 2. Базовая модель продукта

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live (MRL)
```

Все рабочие пространства используют:

- один Project Model;
- один transport;
- один Audio Engine;
- один MIDI Engine;
- один plugin graph;
- один Chord Track;
- один Arranger Track;
- одну систему markers/automation;
- одну систему сохранения проекта.

## 3. Почему MRL является режимом MRS

В отдельной live-программе пришлось бы экспортировать:

- chords;
- arranger sections;
- markers;
- tempo;
- audio;
- MIDI;
- plugin/preset state.

В MRS эти данные уже являются частью проекта.

Поэтому переход:

```text
Arrange -> Live
```

должен менять представление и performance-state, но не создавать вторую копию песни.

## 4. MRS — DAW

Долгосрочно MRS должна включать:

- audio recording/playback;
- audio tracks/clips/events;
- MIDI tracks/editor;
- mixer/buses/routing;
- VST3 hosting;
- automation;
- tempo/meter map;
- Chord Track;
- Arranger Track;
- markers;
- native processors;
- project save/load;
- AI integration;
- Live workspace.

## 5. MRL — Live / Performance workspace

MRL использует тот же открытый проект и показывает только информацию/управление, нужные на сцене:

- setlist;
- moving Chord Track strip;
- current/next section;
- bar/beat/time;
- cues/markers;
- transport;
- live patches;
- MIDI actions;
- click/cue state;
- audio/MIDI/device status;
- preflight/recovery;
- remote/mobile state.

MRL не владеет отдельным ASIO engine или отдельным plugin host.

## 6. Performance-first принцип

Надёжность audio path важнее количества функций.

Критические требования:

- direct vendor ASIO support на Windows;
- realtime-safe callback;
- audio thread отделён от UI/network/AI/file I/O;
- preload/read-ahead;
- predictable patch switching;
- no systematic xruns/dropouts;
- performance benchmark относительно Studio Pro на одинаковом hardware/setup.

Подробнее: `AUDIO_ENGINE.md`.

## 7. AI как часть будущей архитектуры

MRS должна иметь структурированный Project Context API и Command/Tool API, через которые ChatGPT/OpenAI сможет:

- понимать tracks/clips/MIDI/chords/sections;
- анализировать аранжировку и mixer state;
- генерировать MIDI;
- предлагать изменения;
- после подтверждения выполнять разрешённые project commands.

AI не должен работать в realtime thread и не должен быть обязательным для работы DAW/Live.

Подробнее: `AI_INTEGRATION.md`.

## 8. Native DSP / Amp / Cab

MRS может включать собственные:

- utility DSP;
- Cab/Room IR convolution;
- amp/preamp/pedal models;
- neural model player;
- собственные Moon River captures/models.

Factory content и User Library должны иметь раздельную licensing policy.

Подробнее: `DSP_MODELING.md`.

## 9. Studio Pro

Studio Pro больше не является обязательным authoring environment.

Он остаётся полезным как:

- референс UX/performance;
- источник идей для Live workflow;
- возможный import/migration source;
- Bridge/compatibility target для старых проектов.

Но целевая схема:

```text
Moon River Studio Project
        |
  +-----+-----+
  |           |
Production   Live
```

## 10. Разработка двумя потоками

MRS и MRL развиваются параллельно:

- `MRS-*` — DAW/core;
- `MRL-*` — Live workspace;
- `[SHARED]` — общие интерфейсы и модели.

MRL может разрабатываться на mock/fixture services до готовности реального MRS backend.

Подробнее: `DEVELOPMENT_TRACKS.md`.

## 11. Главный принцип разработки

Каждый этап должен завершаться проверяемым пользовательским сценарием, но инфраструктура проектируется так, чтобы новые workspaces не дублировали Core.

Главная архитектурная формула:

> **MRS и MRL — разные development tracks, но один Project Model и один realtime engine.**
