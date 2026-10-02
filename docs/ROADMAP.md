# Moon River Live — Roadmap

## Общий принцип версий

Разработка идёт по Stage. Каждый Stage должен завершаться проверяемым пользовательским результатом.

Рекомендуемая схема:

```text
Stage 0 → 0.1
Stage 1 → 0.2
Stage 2 → 0.3
...
Stage 9 → 1.0
```

Внутри этапа можно использовать промежуточные версии `0.1a`, `0.1b`, `0.1c` и т. д.

---

# Stage 0 — Foundation / технический фундамент

Цель: подтвердить архитектуру и технические риски до разработки большого приложения.

## 0.1a — Project bootstrap

- структура репозитория;
- базовый desktop shell;
- CI для Windows;
- логирование;
- конфигурация;
- тестовый запуск приложения.

## 0.1b — Technology spike

Сравнить и зафиксировать стек по критериям:

- Windows desktop;
- audio I/O;
- MIDI I/O;
- VST3 hosting;
- современный UI;
- packaging;
- будущий remote/mobile.

## 0.1c — Song Metadata prototype

- реализовать schema v1;
- JSON validation;
- тестовый song fixture;
- loader;
- migration placeholder.

## 0.1d — Timeline prototype

На искусственном JSON:

- tempo;
- bars/beats;
- chords;
- sections;
- markers;
- вычисление current/next.

### Acceptance Stage 0

Приложение открывает тестовую song metadata и корректно показывает текущий bar, section и chord по движущемуся timeline.

---

# Stage 1 — Studio Pro Bridge / импорт metadata

Цель: получить реальные данные из Fender Studio Pro.

## 0.2a — Integration research spike

Проверить доступность:

- project identity;
- tempo / tempo map;
- meter;
- Chord Track;
- Arranger Track;
- markers;
- tracks/folders;
- clips/events;
- stable IDs.

Результат — документированная compatibility matrix.

## 0.2b — Bridge export v1

- read-only export;
- mapping Studio Pro → Moon River schema;
- export report;
- warnings вместо падения при неподдерживаемых данных.

## 0.2c — `.moonlive` package

- manifest;
- metadata;
- source information;
- integrity validation.

## 0.2d — Re-sync

- стабильные IDs;
- source-owned/live-owned fields;
- повторный импорт без дубликатов;
- сохранение live metadata.

### Acceptance Stage 1

Реальный проект Studio Pro экспортируется в `.moonlive`, повторная синхронизация обновляет музыкальную структуру без потери live-owned данных.

---

# Stage 2 — Live Timeline / musical context

Цель: получить первый действительно полезный live-интерфейс.

## 0.3a — Song screen

- название;
- tempo;
- meter;
- bar/beat;
- elapsed/remaining time.

## 0.3b — Chord view

- current chord;
- next chord;
- крупное live-отображение;
- корректная работа при пустых участках.

## 0.3c — Arranger timeline

- sections;
- colors;
- current section;
- next section;
- progress inside section.

## 0.3d — Markers / cues

- marker list;
- upcoming cue;
- визуальное предупреждение;
- marker filtering.

## 0.3e — Performance UI

- полноэкранный режим;
- крупная типографика;
- high-DPI;
- keyboard shortcuts;
- защита от случайных кликов.

### Acceptance Stage 2

Пользователь может открыть реальную экспортированную песню и использовать Moon River Live как экран музыкального контекста во время репетиции.

---

# Stage 3 — Playback / transport / navigation

Цель: Moon River Live становится самостоятельным live-player.

## 0.4a — Stereo audio playback

- WAV playback;
- audio device selection;
- play/pause/stop;
- seek;
- position sync с Timeline Engine.

## 0.4b — Section navigation

- jump to section;
- previous/next section;
- restart song;
- quantized переходы как отдельная опция.

## 0.4c — Section loop

- loop current section;
- rehearsal loop;
- clear visual indication.

## 0.4d — End behavior

- stop at end;
- ready next song;
- optional continue policy.

### Acceptance Stage 3

Полноценную песню можно сыграть с stereo backing track, видеть синхронные chords/sections и переходить между секциями.

---

# Stage 4 — Multitrack / click / cue

Цель: перейти от stereo backing к профессиональному концертному playback.

## 0.5a — Multi-stem engine

- несколько синхронных audio files;
- sample-locked playback;
- per-stem gain;
- mute/solo.

## 0.5b — Output routing

- audio output buses;
- main out;
- click out;
- cue out;
- сохранение routing preset.

## 0.5c — Click engine

- click from tempo/meter map;
- accents;
- count-in;
- independent output.

## 0.5d — Cue track

- prerecorded cues;
- future generated cues;
- independent level/routing.

## 0.5e — Preload

- preload current/next song;
- отсутствие dropout при переключении.

### Acceptance Stage 4

Приложение способно надёжно проигрывать stems + click/cue на разных выходах в течение полного setlist.

---

# Stage 5 — Live Inputs / VST3 / Patches

Цель: перенести обработку живых инструментов в live-среду.

## 0.6a — Live audio inputs

- input device/channel selection;
- monitoring;
- low latency;
- input meter.

## 0.6b — VST3 hosting

- scan;
- load;
- bypass;
- save state;
- crash isolation strategy.

## 0.6c — Patch model

- guitar patch;
- keys patch;
- vocal patch;
- patch per song/section.

## 0.6d — Patch switching

- section-triggered patches;
- manual override;
- safe transitions.

### Acceptance Stage 5

Во время playback можно обрабатывать live-input через VST3 и автоматически переключать patch при смене секции.

---

# Stage 6 — MIDI Automation / Hardware Control

Цель: автоматизировать внешнее оборудование и foot control.

## 0.7a — MIDI I/O

- enumerate ports;
- input/output selection;
- reconnect;
- diagnostics.

## 0.7b — Timeline actions

- Program Change;
- Control Change;
- Note;
- triggers by bar/section/marker.

## 0.7c — Foot controller mapping

- learn mode;
- configurable commands;
- per-device profile.

## 0.7d — Safety

- no accidental duplicate actions;
- reset policy;
- reconnect state;
- panic/all-notes-off.

### Acceptance Stage 6

Песня может автоматически управлять внешним MIDI-оборудованием, а основные live-команды доступны с педального контроллера.

---

# Stage 7 — Setlists / Show Workflow

Цель: управлять не отдельной песней, а полноценным выступлением.

## 0.8a — Setlist editor

- создать setlist;
- reorder;
- add/remove song;
- notes.

## 0.8b — Show mode

- previous/current/next song;
- preload next;
- per-song start behavior;
- inter-song pause.

## 0.8c — Preflight

- missing files;
- audio device;
- MIDI devices;
- output routing;
- incompatible song schema.

## 0.8d — Recovery

- save current show state;
- recover setlist/current song;
- last-known position;
- safe restart workflow.

### Acceptance Stage 7

Можно провести полный концертный setlist без открытия Studio Pro.

---

# Stage 8 — Remote / Mobile Companion

Цель: дать второму экрану доступ к live-state.

## 0.9a — Local Remote API

- local network discovery;
- session pairing;
- WebSocket/live state;
- read-only first.

## 0.9b — Mobile/tablet view

- current/next chord;
- section;
- bar;
- cue;
- setlist status.

## 0.9c — Remote controls

После стабилизации read-only:

- play/pause;
- section navigation;
- next song;
- permission model.

### Acceptance Stage 8

Телефон или планшет работает как дополнительный концертный экран и при разрешении может управлять ограниченным набором live-команд.

---

# Stage 9 — Reliability / Release 1.0

Цель: превратить функциональный прототип в концертно надёжное приложение.

## 1.0a — Stress testing

- multi-hour playback;
- repeated song switching;
- device reconnect;
- network disconnect;
- corrupted song package.

## 1.0b — Crash recovery

- autosave state;
- crash logs;
- safe mode;
- problematic plugin isolation.

## 1.0c — Compatibility matrix

- Windows versions;
- audio drivers;
- Studio Pro versions;
- Bridge versions;
- schema versions.

## 1.0d — Release packaging

- installer;
- signed build if possible;
- migration policy;
- release notes;
- backup/restore.

### Acceptance Stage 9 / 1.0

Moon River Live выдерживает полный концертный сценарий, длительный stress-test и имеет понятный recovery workflow при отказе второстепенных компонентов.

---

# После 1.0 — возможные направления

Не входят в текущий обязательный roadmap:

- lighting / DMX;
- lyrics/prompter;
- generated voice cues;
- deeper Arranger Manager integration;
- shared metadata service между приложениями Moon River Studio;
- write-back в Studio Pro;
- macOS;
- полноценный mobile app;
- cloud backup/setlist sync;
- collaboration.
