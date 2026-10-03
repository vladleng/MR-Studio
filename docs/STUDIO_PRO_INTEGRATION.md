# Moon River Studio — Fender Studio Pro Compatibility / Import

## 1. Роль Studio Pro

Fender Studio Pro больше не является обязательным authoring environment или runtime dependency.

Moon River Studio должна быть самодостаточной DAW.

Studio Pro остаётся полезным как:

- import/migration source для существующих проектов;
- compatibility target;
- performance/UX benchmark;
- reference для Live/Show workflow.

Issue: #3.

## 2. Целевая схема

```text
Fender Studio Pro project
          |
 Bridge / Export / Parser adapter
          v
Moon River Studio Project Model
          |
   +------+------+------+
   |             |      |
Arrange/Edit    Mix    Live Mode
```

После импорта Live Mode не зависит от Studio Pro и не требует отдельного `.moonlive` package.

## 3. Что желательно импортировать

- project identity;
- tempo / tempo map;
- time signatures;
- Chord Track;
- Arranger Track;
- section names/bounds/colors;
- markers;
- tracks/folders;
- clips/events;
- по возможности audio asset references;
- stable source IDs;
- позднее, если реалистично: mixer/plugin/routing metadata.

## 4. Adapter boundary

Studio Pro-specific код должен быть изолирован:

```text
Studio Pro internals
      ↓
Compatibility Adapter
      ↓
MRS Import DTO / Mapping
      ↓
Native Project Model
```

Изменение Studio Pro API/format не должно ломать Audio Engine, Live Mode или остальную DAW.

## 5. Возможные способы получения данных

### A. Extension / Script / Bridge

Предпочтительный вариант, если доступно достаточно metadata.

### B. Export intermediary

JSON/MIDI/other export, который затем импортирует MRS.

### C. Direct `.song` parser

Дополнительный путь, только если формат достаточно понятен и покрывается compatibility tests.

### D. Manual/fallback import

Для данных, недоступных автоматически.

## 6. Первый technical spike

- [ ] project identity;
- [ ] tempo events/map;
- [ ] time signatures;
- [ ] Chord Track events;
- [ ] Arranger Track sections;
- [ ] section name/start/end/color;
- [ ] markers;
- [ ] tracks;
- [ ] folder hierarchy;
- [ ] clips/events;
- [ ] stable source IDs;
- [ ] behavior after source project edits;
- [ ] compatibility matrix `supported / workaround / unavailable`.

## 7. Import semantics

Первый целевой workflow — **import into MRS**, а не синхронизация отдельного Live-приложения.

```text
Import Studio Pro Project...
        ↓
Create / Update MRS Project
```

После успешного импорта native MRS Project Model становится основой production и Live Mode.

## 8. Re-import / sync — optional

Если позднее понадобится повторный import из Studio Pro, нужны:

- stable source IDs;
- source mapping;
- conflict rules;
- protection MRS-owned data;
- preview/diff перед применением изменений.

Пример:

1. Project imported from Studio Pro.
2. В MRS назначены Live patches и сделан mixer state.
3. В Studio Pro изменён Arranger Track.
4. Re-import должен обновить структурные данные без молчаливой потери MRS-owned state.

Но persistent bidirectional sync **не является обязательным фундаментом проекта**.

## 9. Ownership

### Source/import fields

Могут обновляться при explicit re-import:

- imported tempo/meter;
- chords;
- arranger sections;
- markers;
- imported tracks/clips.

### MRS-owned fields

Не должны молча стираться importer-ом:

- native edits MRS;
- mixer/routing;
- plugin/native processor states;
- MIDI edits/actions;
- Live metadata/patches/notes;
- show/setlist state.

## 10. Graceful degradation

Недоступный тип metadata не должен ломать весь import.

```text
Chord Track: available
Arranger Track: available
Markers: available
Tempo map: unavailable -> fixed tempo fallback
Clips: unavailable -> warning / skip
```

Importer возвращает report.

## 11. Diagnostics

Import report должен содержать:

- Studio Pro version, если определяется;
- adapter/importer version;
- MRS schema version;
- sections/chords/markers/tracks/clips counts;
- warnings;
- skipped/unsupported data.

## 12. Safety

Первый importer должен быть read-only относительно Studio Pro project.

Write-back, если когда-либо понадобится, требует отдельного Stage/issue и не должен появляться неявно.

## 13. Live Mode

Live Mode не читает Studio Pro project напрямую.

Правильная граница:

```text
Studio Pro -> Import Adapter -> MRS Project Model -> Live Mode
```

Это гарантирует, что концертный режим работает на том же project/runtime state, что Arrange/Edit/Mix, и не получает второй engine/data model.
