# Moon River Live — Data Model

## 1. Зачем нужна собственная модель

Moon River Live не должен использовать внутренние объекты Fender Studio Pro как runtime-контракт.

Bridge преобразует данные Studio Pro в независимую, версионируемую модель **Moon River Song Metadata**.

Это даёт:

- устойчивость к изменениям Studio Pro;
- возможность импорта из других источников;
- независимость UI и audio engine от DAW;
- единый формат для desktop, remote и других инструментов Moon River Studio.

## 2. Основная сущность Song

Концептуальная структура:

```text
Song
├── identity
├── musicalTime
│   ├── tempoMap
│   └── meterMap
├── chords
├── arrangerSections
├── markers
├── tracks
├── clips
├── playback
├── liveActions
├── notes
└── source
```

## 3. Identity

Минимальный набор:

```json
{
  "id": "uuid",
  "title": "Moon River",
  "artist": "Duet Moon River",
  "version": "live-2026-10",
  "schemaVersion": 1
}
```

`id` должен сохраняться между повторными экспортами одной и той же песни.

## 4. Musical Time

### Tempo Map

```json
{
  "tempoMap": [
    { "position": { "bar": 1, "beat": 1 }, "bpm": 112.0 },
    { "position": { "bar": 33, "beat": 1 }, "bpm": 116.0 }
  ]
}
```

В будущем модель должна поддерживать не только ступенчатые, но и плавные изменения tempo, если источник их предоставляет.

### Meter Map

```json
{
  "meterMap": [
    { "position": { "bar": 1, "beat": 1 }, "numerator": 4, "denominator": 4 },
    { "position": { "bar": 49, "beat": 1 }, "numerator": 3, "denominator": 4 }
  ]
}
```

## 5. Chord Events

```json
{
  "chords": [
    {
      "id": "chord-001",
      "position": { "bar": 1, "beat": 1 },
      "durationBeats": 4,
      "symbol": "Gmaj7"
    },
    {
      "id": "chord-002",
      "position": { "bar": 2, "beat": 1 },
      "durationBeats": 4,
      "symbol": "Em7"
    }
  ]
}
```

На раннем этапе `symbol` считается авторитетным отображаемым значением.

Позже можно добавить структурный разбор:

- root;
- bass;
- quality;
- extensions;
- alterations.

## 6. Arranger Sections

```json
{
  "arrangerSections": [
    {
      "id": "section-intro",
      "name": "Intro",
      "start": { "bar": 1, "beat": 1 },
      "end": { "bar": 9, "beat": 1 },
      "color": "#6853A6"
    }
  ]
}
```

Секция является одной из ключевых сущностей live-приложения.

К ней в Moon River Live могут добавляться собственные данные:

```json
{
  "live": {
    "notes": "После второй фразы оставить пространство",
    "patch": "Guitar Clean",
    "stopAtEnd": false
  }
}
```

Важно: эти дополнительные данные не должны мешать повторной синхронизации базовой структуры из Studio Pro.

## 7. Markers

```json
{
  "markers": [
    {
      "id": "marker-001",
      "position": { "bar": 17, "beat": 1 },
      "name": "Solo",
      "type": "cue"
    }
  ]
}
```

Возможные будущие типы:

- cue;
- warning;
- lyric;
- action;
- navigation;
- generic.

## 8. Tracks

```json
{
  "tracks": [
    {
      "id": "track-drums",
      "name": "Drums",
      "type": "audio",
      "parentId": "folder-playback",
      "color": "#...",
      "sourceId": "studio-pro-id"
    }
  ]
}
```

Папки представлены через `parentId`, а не отдельной жёсткой структурой UI.

## 9. Clips / Events

```json
{
  "clips": [
    {
      "id": "clip-001",
      "trackId": "track-drums",
      "name": "Verse 1",
      "start": { "bar": 9, "beat": 1 },
      "end": { "bar": 17, "beat": 1 },
      "sourceId": "studio-pro-event-id"
    }
  ]
}
```

На первых этапах clips нужны прежде всего для контекста и будущей интеграции с Arranger Manager.

## 10. Playback Assets

```json
{
  "playback": {
    "mode": "stereo",
    "assets": [
      {
        "id": "backing",
        "role": "backing",
        "path": "audio/backing.wav",
        "gainDb": 0.0
      }
    ]
  }
}
```

Позже:

```text
role = drums | bass | keys | backingVocals | click | cue | custom
```

## 11. Live Actions

Все автоматизированные live-действия должны иметь общий формат trigger → action.

```json
{
  "liveActions": [
    {
      "id": "action-001",
      "trigger": {
        "type": "section-enter",
        "sectionId": "section-solo"
      },
      "action": {
        "type": "midi-program-change",
        "port": "Guitar Rig",
        "channel": 1,
        "program": 12
      }
    }
  ]
}
```

Будущие trigger-типы:

- song-start;
- song-stop;
- time;
- bar-beat;
- section-enter;
- section-exit;
- marker;
- manual.

Будущие action-типы:

- MIDI PC;
- MIDI CC;
- MIDI Note;
- patch change;
- mute/unmute;
- plugin parameter;
- external command.

## 12. Source Metadata

Для повторной синхронизации необходимо помнить происхождение данных:

```json
{
  "source": {
    "type": "fender-studio-pro",
    "projectPath": "...",
    "projectId": "...",
    "exportedAt": "2026-10-02T20:00:00+07:00",
    "bridgeVersion": "0.1.0"
  }
}
```

Не все поля обязательны: путь проекта может быть недоступен или не переносим между компьютерами.

## 13. `.moonlive` package

Рабочая концепция:

```text
Song Name.moonlive
├── manifest.json
├── metadata.json
├── audio/
├── click/
├── midi/
└── assets/
```

Технически контейнер может быть каталогом во время разработки и архивным форматом для распространения.

### manifest.json

Содержит минимум:

```json
{
  "format": "moon-river-live-song",
  "packageVersion": 1,
  "metadata": "metadata.json"
}
```

## 14. Версионирование

Нужны два независимых номера:

- `packageVersion` — формат контейнера;
- `schemaVersion` — структура metadata.

Moon River Live должен:

1. читать текущую schema;
2. мигрировать поддерживаемые старые schema;
3. явно сообщать о неподдерживаемой будущей schema;
4. никогда молча не терять неизвестные live-данные при сохранении.

## 15. Sync ownership

Для безопасной повторной синхронизации поля делятся на две категории.

### Source-owned

Приходят из Studio Pro и могут обновляться Bridge:

- tempo;
- meter;
- chords;
- arranger sections;
- markers;
- tracks;
- clips.

### Live-owned

Создаются в Moon River Live:

- MIDI actions;
- patch assignments;
- live notes;
- output routing;
- setlist-specific settings.

Bridge не должен стирать live-owned данные при повторном импорте.

## 16. ID strategy

Одна из ключевых задач Stage 1 — определить стабильные идентификаторы объектов.

Приоритет:

1. стабильный source ID Studio Pro, если доступен;
2. сохранённый mapping;
3. детерминированный fingerprint по типу/позиции/имени;
4. новый UUID как последний fallback.

Без устойчивых IDs повторная синхронизация будет создавать дубликаты.
