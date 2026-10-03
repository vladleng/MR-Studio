# Moon River Studio — Product Vision

## 1. Что такое Moon River Studio

**Moon River Studio** (`MRS`, рабочее сокращение `MR Studio`) — собственная DAW, ориентированная на создание музыки, запись, MIDI, аранжировку, микширование и живое исполнение.

**Live Mode** — встроенный Performance / Show режим внутри этой DAW, по смыслу близкий к Show Page в Studio Pro.

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live
```

Один проект, один Audio Engine, один Project Model и один набор processing services используются во всех режимах.

## 2. Основной принцип

```text
Production Workspaces <-> Live Mode
          |                 |
          +---- same project+
                 |
            same engine
```

Переход в Live Mode не требует экспортировать песню в отдельное приложение.

Chord Track, Arranger Track, tempo map, markers, MIDI, audio tracks, plugins, routing и automation уже являются частью проекта MRS и напрямую используются performance-режимом.

## 3. Performance-first DAW

MRS не должен быть копией существующей DAW один-в-один. Ключевая особенность — live-performance является частью архитектуры проекта с самого начала.

Критические свойства:

- стабильный native low-latency audio;
- прямой vendor ASIO на Windows;
- realtime-safe Audio Engine;
- разделение low-latency path и тяжёлого process/playback path;
- один Project Model для production и performance;
- быстрое переключение Arrange / Mix / Live;
- Chord Track и Arranger Track как базовые объекты проекта;
- безопасный preload и patch switching;
- offline-first live workflow.

## 4. Рабочие пространства

### Arrange

- audio/MIDI tracks;
- clips/events;
- folders;
- Chord Track;
- Arranger Track;
- markers;
- tempo/meter;
- automation.

### Edit

- waveform editing;
- piano roll;
- MIDI editing;
- clip/event editing;
- automation editing.

### Mix

- channel strips;
- buses;
- routing;
- plugins;
- sends;
- metering.

### Live Mode

- setlist;
- moving Chord Track strip;
- current/next section;
- cues/markers;
- transport;
- live patches;
- MIDI automation;
- preflight/recovery;
- remote view/control;
- performance-safe UI.

## 5. Общая архитектурная идея

```text
                    Moon River Studio
                           |
        +------------------+------------------+
        |                  |                  |
     Arrange              Mix               Live
        |                  |                  |
        +------------------+------------------+
                           |
                       SHARED CORE
                           |
        +------------------+------------------+
        |                  |                  |
   Audio Engine           MIDI             Plugins
        |                  |                  |
       ASIO            MIDI I/O             VST3
```

Live Mode не должен самостоятельно реализовывать ASIO, Transport, Mixer backend или Plugin Hosting. Он использует сервисы общего Core.

## 6. Fender Studio Pro

Studio Pro перестаёт быть обязательным источником данных.

Интеграцию можно сохранить как:

- import/migration path;
- Bridge для переноса существующих проектов;
- reference implementation для workflow/performance;
- compatibility layer.

Новая архитектура самодостаточна:

```text
Moon River Studio Project -> Arrange / Edit / Mix / Live
```

а не:

```text
Studio Pro -> export -> separate Live app
```

## 7. AI как системная возможность

MRS проектируется так, чтобы AI мог понимать Project Model и выполнять разрешённые операции через структурированный Tool API.

AI не управляет DAW через пиксели/мышь и не получает доступ к realtime audio thread.

Примеры будущих запросов:

- проанализировать MIDI-бас в тактах 17–24;
- предложить менее плотную партию;
- написать MIDI drums под текущую секцию и гармонию;
- создать фортепианную партию по Chord Track;
- объяснить перегруженность аранжировки;
- предложить mixer/plugin changes;
- создать section/marker/clip через подтверждаемые commands.

Подробнее: `AI_INTEGRATION.md`.

## 8. Native DSP / Amp / Cab ecosystem

MRS может иметь собственные native processors:

- EQ;
- compressor;
- saturation;
- reverb/delay;
- Cab IR loader;
- amp/preamp/pedal models;
- neural capture player.

Factory Content и импортируемый пользователем content должны быть лицензированно разделены.

Подробнее: `DSP_MODELING.md`.

## 9. Критерий производительности

Количество функций не должно иметь приоритет над стабильностью.

Особенно перед активным использованием Live Mode:

> если Moon River Studio не может надёжно работать на тех же ASIO-настройках, на которых стабильно работает Studio Pro, performance problem считается blocking issue.

## 10. Разработка Live Mode

Live Mode появляется не как отдельный продукт, а как feature-track внутри roadmap MRS.

Рекомендуемая последовательность:

- Core contracts и mock services;
- ранний Live UI prototype;
- продолжение основной разработки DAW;
- real Live integration после появления нативного Musical Structure;
- параллельное развитие Live workflow и production-функций на одном engine.

Подробнее: `ROADMAP.md` и `DEVELOPMENT_TRACKS.md`.

## 11. Рабочее название

Текущее название всей DAW:

- `Moon River Studio`;
- `MRS` / `MR Studio`.

На заключительных стадиях название может быть заменено на более ёмкое. Архитектура не должна зависеть от брендинга.
