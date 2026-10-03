# Moon River Studio — Project Vision

## 1. Назначение

**Moon River Studio** (`MRS`, рабочее сокращение также `MR Studio`) — собственная DAW, объединяющая production и live-performance workflow в одном приложении.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Studio Pro.

Это не отдельное приложение и не отдельный продукт.

Цель проекта — создать performance-first DAW, в которой создание музыки и живое исполнение являются разными режимами одного проекта.

## 2. Базовая модель продукта

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live
```

Все режимы используют:

- один Project Model;
- один Transport;
- один Audio Engine;
- один MIDI backend;
- один plugin/native processor graph;
- один Chord Track;
- один Arranger Track;
- одну систему markers/automation;
- одну систему сохранения проекта.

## 3. Live Mode как режим DAW

Переход:

```text
Arrange / Edit / Mix -> Live
```

должен менять представление, show-state и доступные performance-команды, но не создавать вторую копию песни и не запускать отдельный engine.

Live Mode читает тот же открытый проект:

- audio/MIDI tracks;
- tempo/meter;
- Chord Track;
- Arranger Track;
- markers;
- plugins;
- routing;
- automation;
- live metadata.

## 4. Основные возможности MRS

Долгосрочно DAW должна включать:

- audio recording/playback;
- audio tracks/clips/events;
- MIDI tracks/editor;
- mixer/buses/routing;
- VST3 hosting;
- native DSP;
- automation;
- tempo/meter map;
- Chord Track;
- Arranger Track;
- markers;
- project save/load/recovery;
- AI / ChatGPT integration;
- Live Mode.

## 5. Возможности Live Mode

Live Mode показывает и управляет только тем, что нужно на сцене:

- setlist;
- moving Chord Track strip;
- current/next section;
- bar/beat/time;
- cues/markers;
- transport;
- click/cue;
- live patches;
- MIDI actions;
- audio/MIDI/device status;
- preflight/recovery;
- remote/mobile state.

Live Mode не владеет отдельным ASIO engine, transport, MIDI engine или plugin host.

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

## 7. AI как системная возможность

MRS должна иметь структурированный Project Context API и Command/Tool API, через которые ChatGPT/OpenAI сможет:

- понимать tracks/clips/MIDI/chords/sections;
- анализировать аранжировку и mixer state;
- генерировать MIDI;
- предлагать изменения;
- после подтверждения выполнять разрешённые project commands.

AI не должен работать в realtime thread и не должен быть обязательным для работы DAW или Live Mode.

Подробнее: `AI_INTEGRATION.md`.

## 8. Native DSP / Amp / Cab

MRS может включать собственные:

- utility DSP;
- Cab/Room IR convolution;
- amp/preamp/pedal models;
- neural model player;
- собственные Moon River captures/models.

Factory Content и User Library должны иметь раздельную licensing policy.

Подробнее: `DSP_MODELING.md`.

## 9. Studio Pro

Fender Studio Pro больше не является обязательным authoring environment.

Он остаётся полезным как:

- UX/performance reference;
- источник идей для Live workflow;
- import/migration source;
- compatibility target для существующих проектов.

Целевая схема:

```text
Moon River Studio Project
        |
  +-----+------+------+
  |            |      |
Arrange/Mix   Edit   Live
```

## 10. Организация разработки

Для параллельной работы используются три issue track:

- `[MRS]` — пользовательские DAW-функции;
- `[SHARED]` — общий Core / Engine;
- `[LIVE]` — встроенный Live Mode.

Это не три продукта.

Live Mode может разрабатываться на mock/fixture services после фиксации нужных SHARED contracts, пока realtime backend и остальные функции MRS продолжают развиваться параллельно.

Версионируется только Moon River Studio. Live Mode имеет Stage readiness, но не отдельную product version line.

Подробнее: `DEVELOPMENT_TRACKS.md`.

## 11. Рабочее название

Текущее название DAW — **Moon River Studio / MR Studio**.

Это рабочий вариант. На поздней стадии проект может получить более короткое или ёмкое финальное имя без изменения архитектуры.

## 12. Главная архитектурная формула

> **Moon River Studio — одна DAW. Arrange/Edit/Mix/Live — разные режимы. Project Model и realtime engine — одни и те же.**
