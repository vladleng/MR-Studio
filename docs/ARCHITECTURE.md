# Moon River Live — Architecture

## 1. Общая схема

```text
┌──────────────────────┐
│ Fender Studio Pro    │
│ authoring environment│
└──────────┬───────────┘
           │
           │ export / sync
           ▼
┌──────────────────────┐
│ Moon River Bridge    │
│ extraction + mapping │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ Song Metadata Model  │
│ versioned contract   │
└──────────┬───────────┘
           │
           ▼
┌─────────────────────────────────────────┐
│ Moon River Live                         │
│                                         │
│  Song Loader                            │
│  Timeline Engine                        │
│  Playback Engine                        │
│  Live State Engine                      │
│  MIDI / Patch Engine                    │
│  Setlist Engine                         │
│  Remote API                             │
└─────────────────────────────────────────┘
```

## 2. Главная архитектурная граница

Moon River Live не должен напрямую строить внутреннюю логику UI на объектах Studio Pro.

Все данные Studio Pro сначала приводятся к внутренней универсальной модели.

```text
Studio Pro object
       ↓
Bridge adapter
       ↓
Moon River Song Metadata
       ↓
Live modules
```

Это позволит менять способ интеграции со Studio Pro, не переписывая live-приложение.

## 3. Moon River Bridge

Bridge отвечает только за получение и нормализацию данных.

Предполагаемые адаптеры:

### Adapter A — Studio Pro Extension / Script

Предпочтительный путь.

Задачи:

- прочитать доступные metadata;
- преобразовать их в Moon River schema;
- экспортировать или синхронизировать song package.

### Adapter B — прямой parser `.song`

Дополнительный/fallback путь.

Использовать только там, где это достаточно устойчиво и покрыто тестами.

### Adapter C — manual/import adapter

Резервный способ для данных, которые Studio Pro не позволяет получить автоматически.

Например:

- JSON import;
- MIDI-derived markers;
- ручное редактирование дополнительных live metadata.

## 4. Song Loader

Отвечает за:

- открытие `.moonlive`;
- проверку версии schema;
- валидацию обязательных полей;
- проверку наличия audio assets;
- миграцию старых форматов;
- построение runtime-представления песни.

## 5. Timeline Engine

Центральный модуль синхронизации музыкальной структуры.

На вход получает:

- tempo map;
- meter map;
- chords;
- arranger sections;
- markers;
- automation events.

На выходе формирует live state:

```text
currentTime
currentBar
currentBeat
currentSection
nextSection
currentChord
nextChord
activeMarkers
pendingActions
```

Timeline Engine не должен зависеть от конкретного UI.

## 6. Playback Engine

Этапы развития:

### Phase A

- stereo playback;
- play / pause / stop;
- seek;
- sample-accurate position source.

### Phase B

- multiple stems;
- per-stem mute / solo / gain;
- click;
- cue;
- multiple audio outputs.

### Phase C

- live inputs;
- plugin processing;
- low-latency monitoring.

Playback Engine должен быть отделён от визуального интерфейса и metadata-парсинга.

## 7. Live State Engine

Единое runtime-состояние текущего шоу.

Пример:

```text
ShowState
├── currentSetlist
├── currentSong
├── transportState
├── currentPosition
├── currentSection
├── currentChord
├── activePatch
├── activeMidiState
└── connectedRemotes
```

Все UI-компоненты подписываются на это состояние, а не вычисляют собственную логику позиции.

## 8. MIDI / Patch Engine

Должен уметь запускать действия по:

- старту песни;
- времени;
- bar/beat;
- section enter;
- marker;
- manual trigger.

Типы действий:

- MIDI Program Change;
- MIDI Control Change;
- MIDI Note;
- patch change;
- plugin state change;
- внешние команды будущих интеграций.

## 9. Setlist Engine

Отвечает за:

- порядок песен;
- быстрый переход next/previous;
- preload следующей песни;
- stop/continue policy;
- восстановление текущей позиции шоу;
- метаданные выступления.

## 10. Remote API

Remote/mobile не должен напрямую читать song package.

Он получает подготовленный live state от desktop-приложения.

```text
Desktop Live Engine
        │
        ├── WebSocket / local network
        │
        ▼
Remote Client
```

Первый remote-клиент может быть read-only.

## 11. Разделение потоков

Критически важное правило для live-системы:

```text
Audio thread ≠ UI thread ≠ metadata thread
```

Ни загрузка JSON, ни сетевой remote, ни перерисовка интерфейса не должны блокировать real-time audio.

## 12. Fail-safe модель

При сбое второстепенного компонента:

- playback должен продолжаться;
- MIDI actions не должны повторно отправляться без необходимости;
- remote может отключиться без влияния на звук;
- UI должен уметь восстановить state;
- последняя стабильная позиция шоу должна сохраняться.

## 13. Возможная структура репозитория

```text
Moon-River-Live/
├── app/
├── bridge/
│   ├── studio-pro/
│   └── importers/
├── core/
│   ├── song-model/
│   ├── timeline/
│   ├── transport/
│   └── live-state/
├── audio/
├── midi/
├── setlist/
├── remote/
├── ui/
├── tests/
├── docs/
└── examples/
```

Фактическая структура будет выбрана после технического spike Stage 0.

## 14. Технологический выбор

Фреймворк приложения и audio engine пока намеренно не зафиксированы.

На Stage 0 необходимо сравнить варианты по критериям:

- Windows-first desktop;
- стабильный low-latency audio;
- VST3 hosting;
- MIDI I/O;
- удобство построения современного UI;
- возможность будущего mobile/remote клиента;
- простота CI и packaging.

Архитектурные документы не должны преждевременно привязывать проект к одному стеку до завершения spike.
