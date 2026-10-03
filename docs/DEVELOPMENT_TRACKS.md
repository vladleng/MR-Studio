# Moon River Studio / Moon River Live — Development Tracks

## 1. Зачем разделять MRS и MRL

Moon River Studio (MRS) и Moon River Live (MRL) находятся в одном продукте и используют общий Core, но имеют разные пользовательские цели и могут развиваться параллельно.

- **MRS** — DAW: audio/MIDI engine, arrange, edit, mixer, plugins, project model.
- **MRL** — Live / Performance workspace: setlists, moving Chord Track, cues, patches, show workflow.
- **SHARED** — общая инфраструктура, без которой оба направления зависят друг от друга.

MRL не является отдельным audio engine.

## 2. Три issue track

Рекомендуемые префиксы issues:

```text
[MRS]    DAW / production functionality
[MRL]    Live / performance functionality
[SHARED] общие core-компоненты
```

Примеры:

```text
[MRS 0.1a] ASIO device layer
[MRS 0.1b] Project Model v1
[MRS 0.2a] Audio track + clip model

[MRL 0.1a] Live workspace shell
[MRL 0.1b] Moving Chord Track prototype
[MRL 0.2a] Setlist model

[SHARED] Transport service
[SHARED] Chord Track data model
[SHARED] Command/Undo architecture
```

## 3. Не использовать одну общую нумерацию версий

Не рекомендуется делать:

```text
0.1 = MRS
0.2 = MRL
0.3 = MRS
```

Это создаёт искусственные блокировки.

Лучше:

```text
MRS 0.1
MRS 0.2
MRS 0.3

MRL 0.1
MRL 0.2
MRL 0.3
```

При этом общий релиз приложения может иметь собственную версию:

```text
Moon River Studio 0.5
├── MRS Core 0.5
└── MRL 0.3
```

## 4. Labels

Рекомендуемый минимальный набор labels.

### Track

```text
track:mrs
track:mrl
track:shared
```

### Area

```text
area:audio
area:midi
area:project-model
area:arrange
area:mixer
area:plugins
area:ui
area:live
area:setlist
area:ai
area:dsp
area:remote
area:testing
```

### Type

```text
type:feature
type:bug
type:research
type:refactor
type:docs
type:test
```

### Priority / blocking

```text
priority:critical
priority:high
priority:normal
blocked
performance-blocker
```

## 5. Parent issues

Для каждого крупного этапа создаётся parent issue.

Например:

```text
[MRS Stage 0] Core Foundation
  ├── MRS 0.1a ASIO layer
  ├── MRS 0.1b transport
  ├── MRS 0.1c Project Model
  └── MRS 0.1d Audio Performance Gate

[MRL Stage 0] Live UX Foundation
  ├── MRL 0.1a workspace shell
  ├── MRL 0.1b moving Chord Track
  ├── MRL 0.1c current/next section
  └── MRL 0.1d cue/notes panel
```

Parent issue содержит checklist и ссылки на дочерние issues.

## 6. Dependency rule

Каждый MRL issue должен явно указывать зависимости от Core.

Пример:

```text
Depends on:
- #123 [SHARED] Transport service
- #126 [SHARED] Chord Track model
```

Если backend ещё не готов, MRL может использовать mock/stub interface.

Это позволяет параллельную работу:

```text
MRS team/workstream -> real Transport
MRL workstream      -> ITransport mock
                       |
                       +-> later replace with real service
```

## 7. Interface-first development

Для общих компонентов сначала фиксируется контракт.

Пример:

```text
ITransport
- play()
- stop()
- seek(samples)
- getPosition()
- getState()
```

После этого:

- MRS реализует realtime backend;
- MRL строит UI на mock backend;
- интеграция происходит после стабилизации интерфейса.

Так MRL не ждёт завершения всей DAW.

## 8. Milestones

Milestones лучше использовать для конкретных интеграционных точек, а не для каждой мелкой версии.

Пример:

```text
Milestone: MRS Core Prototype
Milestone: First Audio Project
Milestone: MRL Interactive Prototype
Milestone: MRS + MRL Integration 0.1
Milestone: First Rehearsal Build
Milestone: First Full Show Build
```

В один milestone могут входить issues из `track:mrs`, `track:mrl` и `track:shared`.

## 9. Git branches

Не требуется постоянная ветка `mrs` и постоянная ветка `mrl`.

Предпочтительно короткоживущие feature branches:

```text
mrs/asio-device-layer
mrs/audio-clips
mrl/chord-strip
mrl/setlist-ui
shared/transport-api
```

`main` остаётся интеграционной веткой и должен быть buildable.

## 10. Пример параллельной работы

```text
TRACK MRS                  TRACK SHARED             TRACK MRL

ASIO Engine -----------+
                       +-> Transport API ---------> Live transport UI
Audio Project Model --->   Chord Model ----------> Moving Chord Strip
Mixer ----------------->   State API ------------> Patch/status panel
MIDI Engine ----------->   Command API ----------> Live MIDI actions
```

MRL может опережать MRS визуально, пока использует fixture/mock data. MRS может опережать MRL в engine-функциях без необходимости сразу создавать performance UI.

## 11. Definition of Done

### MRS issue

- unit/integration tests где применимо;
- не нарушает realtime constraints;
- API документирован;
- project state serializable, если функция хранится в проекте.

### MRL issue

- работает на mock/real service через один интерфейс;
- читаем с ноутбука;
- не блокирует audio thread;
- keyboard/MIDI control учитывается там, где применимо.

### SHARED issue

- контракт стабилен;
- имеет tests;
- не содержит UI-specific assumptions;
- учитывает serialization/versioning.

## 12. Главный принцип

**MRS и MRL — разные development tracks, но не разные архитектуры.**

Они должны сходиться через общий Core и стабильные интерфейсы, а не дублировать transport, audio, MIDI, project state или plugin hosting.
