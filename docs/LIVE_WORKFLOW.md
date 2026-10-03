# Moon River Studio — Live Mode Workflow

## 1. Что такое Live Mode

Live Mode — встроенный performance/show режим Moon River Studio.

Он работает с тем же project, который пользователь редактирует в Arrange/Edit/Mix.

Обычный workflow не требует экспорта песни в отдельное live-приложение.

```text
MRS Project
   |
Arrange / Edit / Mix
   |
Live Mode
```

## 2. Подготовка песни

В Moon River Studio пользователь подготавливает:

- tempo / tempo map;
- Chord Track;
- Arranger Track;
- markers/cues;
- audio/MIDI tracks;
- routing;
- plugins/native processors;
- live metadata/patch assignments.

Затем переключается в Live Mode.

Studio Pro import #3 может использоваться для переноса существующего проекта в нативный MRS Project Model, но не является частью обычного runtime workflow.

## 3. Подготовка к выступлению

Перед концертом пользователь:

- собирает setlist из MRS projects;
- проверяет audio interface/ASIO;
- проверяет sample rate/buffer;
- проверяет input/output routing;
- проверяет MIDI devices;
- проверяет click/cue outputs;
- проверяет plugins/patches;
- выполняет preload;
- запускает preflight.

Пример:

```text
✓ 14 projects available
✓ ASIO device connected
✓ Sample rate: 48 kHz
✓ Guitar input ready
✓ MIDI controller connected
✓ Click output available
! Song 07: cue asset missing
```

## 4. Live Mode UI

Основной performance экран должен показывать:

- current song/project;
- moving Chord Track;
- current/next chord;
- current/next section;
- bar/beat/time;
- upcoming cue/marker;
- patch state;
- transport;
- next song;
- audio/MIDI/device status.

Визуальный принцип подробно описан в `UI_UX_CONCEPT.md`.

## 5. Moving Chord Track

Вместо больших статичных chord cards используется горизонтальная moving strip:

```text
Dm7 | G7 | [ Gmaj7 ] | Em7 | Am7 | D7
             ^ playhead
```

Chord Track берётся из общего Project Model, позиция — из общего Transport.

Live Mode не вычисляет собственную timeline position.

## 6. Основные live-команды

Базовые:

- Play;
- Pause;
- Stop;
- Previous Song;
- Next Song;
- Jump to Section;
- Restart Song.

Позже:

- Loop Section;
- Stop at End;
- Continue to Next;
- Skip Section;
- Rehearsal Loop;
- emergency fade out.

Transport остаётся тем же SHARED Transport, которым пользуется вся DAW.

## 7. Секции и performance metadata

Arranger Section является основной единицей performance-навигации.

```text
Section: SOLO
├── guitar patch: Lead
├── keys patch: Rhodes
├── note: 8 bars
├── MIDI actions
├── loop policy
└── stop/continue policy
```

Live metadata привязываются к стабильным IDs sections проекта.

## 8. Playback

Live Mode использует общий Audio Engine MRS.

### Project playback

Обычные audio/MIDI tracks проекта продолжают воспроизводиться тем же engine.

### Stems / backing workflow

Если show использует подготовленные stems:

```text
Drums --------┐
Bass ---------+-> Main Out
Percussion ---+
Back Vocals --┘

Click -> IEM / dedicated out
Cue   -> IEM / dedicated out
```

Это routing общего mixer/audio graph, а не отдельный Live playback engine.

## 9. Playback preparation

Для концертной подготовки допускаются неразрушающие параметры Live Mode:

- overall playback gain;
- Trim Start;
- Trim End;
- End Behavior.

Они не должны переписывать исходные clips/audio files.

## 10. Live Inputs / Patches

Live guitar/vocal/keys проходят через общий low-latency Audio/Plugin graph.

Patch — state общего processor graph.

```text
Current Patch -> ACTIVE
Next Patch    -> PRELOADED / WARM
```

На section boundary выполняется быстрый state switch/crossfade, а не загрузка тяжёлой chain с нуля.

## 11. Song / Project transition

Следующий MRS project подготавливается заранее:

```text
Project A playing
      ↓
Project B preload
      ↓
Project A ends
      ↓
Ready / optional continue
```

Preload не должен вызывать dropout текущего audio.

## 12. Setlist

Setlist — show-level state, содержащий ссылки на MRS projects.

Он не копирует project content.

```text
Setlist
├── Project A ref
├── Project B ref
├── Project C ref
└── show-specific notes/overrides
```

## 13. Foot control

Пример:

```text
Button 1 -> Play / Pause
Button 2 -> Next Section
Button 3 -> Previous Section
Button 4 -> Next Song
Long press -> Stop
```

Mapping настраиваемый и работает через общий MIDI/control layer.

## 14. Rehearsal behavior

Внутри Live Mode можно предусмотреть rehearsal state, где доступны:

- free seek;
- section loop;
- repeated section;
- stems mute/solo;
- note editing;
- быстрые навигационные действия.

Concert/Performance state должен быть строже и защищать опасные действия.

## 15. Performance behavior

Принципы:

- крупные элементы;
- moving Chord Track;
- минимум модальных окон;
- основные controls всегда видимы;
- destructive/system actions отделены;
- отсутствие обязательного интернета;
- UI/AI/network не влияют на audio thread;
- сохранение last-known show state;
- predictable transport behavior.

## 16. Recovery

После перезапуска MRS может предложить:

```text
Resume last show?
Setlist: Kitchen Lab
Song: Moon River
Last position: bar 33
```

На раннем этапе безопаснее восстановить show/project selection и показать последнюю позицию, чем автоматически продолжать audio playback.

## 17. Remote workflow

Первый remote/client — дополнительный монитор:

```text
CURRENT: Gmaj7
NEXT: Em7
SECTION: Solo
NEXT SECTION: Chorus
BAR: 33
```

Transport control добавляется позже с permission/safety model.

Remote работает через prepared application state и не входит в realtime audio path.

## 18. Preflight

Перед show проверяются минимум:

- project files/assets;
- ASIO device;
- sample rate/buffer;
- output routing;
- live inputs;
- MIDI devices;
- plugin availability/state;
- click/cue routing;
- next-project preload readiness.

## 19. Концертный критерий качества

Live-функция считается готовой только после:

1. обычной репетиции;
2. long continuous playback;
3. stress-test transitions;
4. проверки failure/recovery paths;
5. отсутствия regression в SHARED Audio Engine.

## 20. Главный принцип

> **Live Mode — это performance-представление и show workflow того же Moon River Studio project, а не отдельное приложение.**
