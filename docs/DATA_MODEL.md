# Moon River Studio — Project / Data Model

## 1. Главный принцип

Moon River Studio использует **один Project Model** для Arrange, Edit, Mix и Live Mode.

Live Mode не создаёт вторую копию песни и не требует отдельного `.moonlive` package для обычной работы внутри MRS.

```text
MRS Project
├── production data
├── musical structure
├── mixer/plugin/MIDI state
└── live metadata
```

Studio Pro и другие внешние источники могут импортироваться в эту модель через adapters.

## 2. Основная сущность Project

Концептуально:

```text
Project
├── identity
├── musicalTime
│   ├── tempoMap
│   └── meterMap
├── tracks
├── folders
├── clips/events
├── midi
├── chords
├── arrangerSections
├── markers
├── automation
├── mixer
├── processors/plugins
├── routing
├── live
└── importSources
```

## 3. Identity

```json
{
  "id": "uuid",
  "title": "Moon River",
  "artist": "Duet Moon River",
  "schemaVersion": 1
}
```

Project IDs должны быть стабильными между save/load и использоваться setlist/show state.

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

### Meter Map

```json
{
  "meterMap": [
    { "position": { "bar": 1, "beat": 1 }, "numerator": 4, "denominator": 4 },
    { "position": { "bar": 49, "beat": 1 }, "numerator": 3, "denominator": 4 }
  ]
}
```

Project Model должен поддерживать conversion между sample position и musical position через SHARED Musical Timeline.

## 5. Chord Track

```json
{
  "chords": [
    {
      "id": "chord-001",
      "position": { "bar": 1, "beat": 1 },
      "durationBeats": 4,
      "symbol": "Gmaj7"
    }
  ]
}
```

Позже структурный chord model может включать:

- root;
- bass;
- quality;
- extensions;
- alterations.

Arrange/Chord editor и Live moving Chord Track читают одну и ту же коллекцию `chords`.

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

Секция является общей сущностью проекта, а Live Mode может хранить связанные performance-настройки по её ID.

## 7. Markers / Cues

```json
{
  "markers": [
    {
      "id": "marker-001",
      "position": { "bar": 17, "beat": 1 },
      "name": "Vocal entry",
      "type": "cue"
    }
  ]
}
```

Типы могут включать:

- generic;
- cue;
- warning;
- lyric;
- action;
- navigation.

## 8. Tracks / Folders

```json
{
  "tracks": [
    {
      "id": "track-drums",
      "name": "Drums",
      "type": "audio",
      "parentId": "folder-rhythm",
      "color": "#667788"
    }
  ]
}
```

Папки и hierarchy являются частью Project Model и не зависят от конкретного workspace.

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
      "source": "audio/drums.wav"
    }
  ]
}
```

Editing должен быть неразрушающим там, где это возможно.

## 10. MIDI

Project Model должен хранить минимум:

- MIDI tracks;
- clips;
- note events;
- velocity;
- duration;
- CC;
- Program Change;
- routing;
- articulation/metadata extensions later.

Эта же MIDI model используется AI Tool API и Live automation.

## 11. Mixer / Routing / Processor State

Проект должен хранить:

```text
Mixer
├── channel gain/pan
├── mute/solo
├── buses
├── sends
├── input/output routing
└── master

Processor Graph
├── native processors
├── VST3 instances
├── parameters/state
├── latency metadata
└── presets/patch snapshots
```

Live patches являются states/snapshots общего processor graph.

## 12. Live Metadata

Live-specific metadata хранится рядом с project entities, но не копирует их.

Пример:

```json
{
  "live": {
    "sectionSettings": {
      "section-solo": {
        "notes": "После второй фразы оставить пространство",
        "patchId": "guitar-lead",
        "stopAtEnd": false
      }
    }
  }
}
```

Возможные Live metadata:

- section notes;
- patch assignments;
- MIDI actions;
- cue overrides;
- playback preparation params;
- performance warnings;
- show-related references.

## 13. Playback Preparation

Live Mode может иметь неразрушающие playback-настройки поверх project content:

```json
{
  "live": {
    "playback": {
      "masterGainDb": -2.0,
      "trimStartSeconds": 1.25,
      "trimEndSeconds": 243.40
    }
  }
}
```

Правила:

- исходные audio files не переписываются;
- trim/gain являются performance preparation state;
- global trim применяется синхронно ко всему project playback;
- Live elapsed/remaining/end behavior учитывают эти границы;
- Reset возвращает project playback к исходным границам.

## 14. Live Actions

Общий trigger -> action формат:

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

Triggers:

- project/song start;
- project/song stop;
- time;
- bar/beat;
- section enter/exit;
- marker;
- manual.

Actions:

- MIDI PC/CC/Note;
- patch change;
- mute/unmute;
- plugin/native parameter;
- future external command.

## 15. Show / Setlist State

Setlist не должен копировать проекты.

Концептуально:

```text
ShowState
├── id
├── title
├── projectRefs[]
├── order
├── per-song show overrides
├── currentProjectId
├── lastPosition
└── recovery state
```

Каждый item ссылается на стабильный `Project.id` и optional path/location.

## 16. Project vs Show ownership

### Project-owned

- tracks/clips/MIDI;
- tempo/meter;
- Chord Track;
- Arranger Track;
- markers;
- automation;
- mixer/routing;
- plugins/processors;
- reusable Live metadata tied to the song/project.

### Show-owned

- setlist order;
- current/next song;
- show-specific notes/overrides;
- last show position;
- temporary preflight/recovery state.

Это разделение нужно для того, чтобы один project можно было использовать в разных setlists.

## 17. Import Source Metadata

Imported projects may retain provenance:

```json
{
  "importSources": [
    {
      "type": "fender-studio-pro",
      "sourceProjectId": "...",
      "sourcePath": "...",
      "importedAt": "...",
      "adapterVersion": "..."
    }
  ]
}
```

После импорта нативный MRS Project Model становится runtime authority.

## 18. Studio Pro sync/import ownership

Для optional compatibility track #3 можно различать:

### Imported/source-owned during re-import

- tempo;
- meter;
- chords;
- arranger sections;
- markers;
- tracks;
- clips.

### MRS-owned

- edits, созданные уже внутри MRS;
- mixer/plugin/MIDI state;
- Live metadata;
- show/setlist state.

Точная conflict policy определяется отдельно, если persistent re-sync со Studio Pro действительно понадобится.

## 19. Project format

Финальное расширение MRS project пока не фиксируется.

На ранней разработке project может быть directory/JSON + assets; позже — versioned package/container.

Критические требования:

- schema versioning;
- migrations;
- unknown-data safety;
- stable IDs;
- relative asset references where practical;
- crash-safe save strategy.

Историческая идея `.moonlive` **не является основным project format**. При необходимости такой portable Live package может появиться позднее как export/deployment format, но обычный Live Mode работает напрямую с MRS projects.

## 20. ID strategy

Приоритет:

1. persistent native MRS UUID/stable ID;
2. stable importer source ID для imported objects;
3. preserved mapping;
4. deterministic fingerprint for migration fallback;
5. new UUID as last resort.

## 21. Главный принцип

> **Project Model один. Production и Live используют одни и те же musical/audio/MIDI entities; Live хранит только дополнительный performance state и policy.**
