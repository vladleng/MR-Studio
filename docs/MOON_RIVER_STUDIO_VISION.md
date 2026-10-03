# Moon River Studio (MRS) — Product Vision

## 1. Что такое MRS

**Moon River Studio (MRS)** — собственная DAW Moon River Studio, ориентированная на создание музыки, запись, MIDI, аранжировку, микширование и живое исполнение.

**Moon River Live (MRL)** — не отдельный аудиодвижок и не отдельная DAW. Это **Live / Performance workspace внутри MRS**, по смыслу близкий к Show Page в Studio Pro.

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live (MRL)
```

Один проект, один Audio Engine, один Project Model и один набор плагинов используются во всех режимах.

## 2. Основной принцип

```text
Production Mode <-> Live Mode
      |                 |
      +---- same project+
             |
        same engine
```

Переход в MRL не требует экспортировать песню в отдельное приложение.

Chord Track, Arranger Track, tempo map, markers, MIDI, audio tracks, plugins, routing и automation уже являются частью проекта MRS и напрямую используются Live workspace.

## 3. MRS как performance-first DAW

MRS не должен быть копией существующей DAW один-в-один. Ключевая особенность — live-performance является частью архитектуры проекта с самого начала.

Критические свойства:

- стабильный native low-latency audio;
- прямой vendor ASIO на Windows;
- realtime-safe Audio Engine;
- разделение live path и тяжёлого process/playback path;
- один Project Model для production и performance;
- быстрое переключение Arrange / Mix / Live;
- Chord Track и Arranger Track как базовые объекты проекта;
- безопасный preload и patch switching;
- offline-first live workflow.

## 4. Основные рабочие пространства

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

### Live / MRL

- setlist;
- moving Chord Track strip;
- current/next section;
- cues/markers;
- transport;
- live patches;
- MIDI automation;
- preflight;
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
                    Shared Project Model
                           |
        +------------------+------------------+
        |                  |                  |
   Audio Engine           MIDI             Plugins
        |                  |                  |
       ASIO            MIDI I/O             VST3
```

MRL не должен самостоятельно реализовывать ASIO, transport, mixer или plugin hosting. Он использует сервисы общего MRS Core.

## 6. Studio Pro

Fender Studio Pro перестаёт быть обязательным источником данных.

Интеграцию со Studio Pro можно сохранить как:

- import/migration path;
- Bridge для переноса старых проектов;
- reference implementation для workflow;
- compatibility layer на переходном этапе.

Но новая архитектура должна быть самодостаточной:

```text
Moon River Studio Project -> Arrange / Mix / Live
```

а не:

```text
Studio Pro -> export -> Moon River Live
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
- создать section/marker/clip через подтверждаемые команды.

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

Factory content и импортируемый пользователем content должны быть лицензированно разделены.

Подробнее: `DSP_MODELING.md`.

## 9. Главный критерий развития

Количество функций не должно иметь приоритет над стабильностью.

Особенно для MRL:

> если проект не может надёжно работать на тех же ASIO-настройках, на которых стабильно работает Studio Pro, performance problem считается blocking issue.

## 10. Сокращения

- **MRS** — Moon River Studio, вся DAW и общий продукт.
- **MRL** — Moon River Live, Live / Performance workspace внутри MRS.
- **MRS Core** — Audio Engine, Project Model, transport, MIDI, plugin hosting и другие общие подсистемы.
