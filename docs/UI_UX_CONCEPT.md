# Moon River Studio — Live Mode UI / UX Concept

## 1. Назначение

Live Mode — встроенный performance/show workspace Moon River Studio.

Он должен быстро считываться на ноутбуке, держать ключевую musical/performance information на виду и не ощущаться как перегруженная production DAW.

Ключевой принцип: **performance-first UI**.

## 2. Базовый визуальный референс

Зафиксированное направление:

- flat UI;
- без выпуклых/глянцевых/3D-card эффектов;
- deep graphite / dark navy base;
- restrained blue-violet accent;
- amber for cues/playback;
- green for ready/connected;
- red only for danger/error;
- крупная типографика;
- тонкие разделители и спокойные flat panels;
- минимум decorative glow;
- readable на 14–16" laptop.

Второй созданный концепт интерфейса считается базовым визуальным reference для дальнейшего wireframing и реализации.

## 3. Главная особенность — Moving Chord Track

Chord Track не должен состоять из больших статичных карточек current/next chord.

Основная форма — **горизонтальная движущаяся полоса**, по принципу Show Page:

```text
... | Dm7 | G7 | [ Gmaj7 ] | Em7 | Am7 | D7 | ...
                    ^
                 playhead
```

Во время playback:

- playhead может оставаться в фиксированной зоне;
- chord strip движется относительно playhead;
- current chord выделяется;
- previous/next chords остаются видимыми;
- длина visual segment может отражать duration chord event;
- bar/beat/time находятся рядом;
- движение должно быть плавным, но не влиять на realtime audio.

Источник chord data — SHARED Project Model, позиция — SHARED Transport.

## 4. Основной Performance layout

```text
┌────────────────────────────────────────────────────────────────────┐
│ Top Bar                                                           │
│ Song | Setlist | Tempo | Meter | Audio | MIDI | Remote | Settings │
├───────────────┬───────────────────────────────────────┬────────────┤
│ Left Rail     │ Main Performance Area                 │ Right Rail │
│               │                                       │            │
│ Setlist       │ Bar / Beat / Time                     │ Cue        │
│ Current/Next  │                                       │ Notes      │
│ Songs         │ Moving Chord Track                    │ Patch      │
│               │                                       │ Warnings   │
│               │ Current / Next Section                │ Actions    │
├───────────────┴───────────────────────────────────────┴────────────┤
│ Arranger Timeline / Transport / Loop / Volume                     │
└────────────────────────────────────────────────────────────────────┘
```

## 5. Top Bar

Глобальный context/status:

- current project/song;
- setlist/show;
- tempo;
- meter;
- audio device status;
- MIDI status;
- remote status;
- save/preflight state;
- settings.

Служебная информация не должна конкурировать с musical context.

## 6. Left Rail

Show navigation:

- setlist;
- current song;
- next song;
- quick song selection;
- optional notes.

В concert use панель может иметь compact state.

## 7. Main Performance Area

Приоритет:

1. moving Chord Track;
2. current musical position;
3. current section;
4. next section;
5. playback state.

Current chord должен быть самым заметным элементом **внутри chord strip**, а не отдельной декоративной card.

Section blocks могут быть flat labels/panels под chord strip.

## 8. Right Rail

Контекстные live-данные:

- upcoming cue;
- marker;
- section notes;
- patch;
- upcoming MIDI action;
- warnings.

Right Rail вторичен по отношению к chord/section context.

## 9. Bottom Arranger / Transport Bar

Всегда доступны:

- arranger timeline;
- sections;
- markers;
- playhead;
- Play/Pause/Stop;
- Previous/Next Section;
- Loop;
- volume/status.

В rehearsal/edit state могут появляться дополнительные preparation controls.

## 10. Информационная иерархия

### Level 1

- current chord at chord-strip playhead;
- nearby next chord(s);
- current section;
- bar/beat;
- playback state.

### Level 2

- project/song title;
- next section;
- elapsed/remaining;
- upcoming cue;
- next song.

### Level 3

- BPM/meter;
- patch state;
- MIDI status;
- audio status;
- routing indicators.

### Level 4

- diagnostics;
- internal IDs/schema/source information;
- detailed device/plugin metadata.

## 11. Live Mode states

Live Mode может иметь несколько states без превращения их в отдельные приложения.

### Performance

Read-mostly concert state:

- chord strip;
- sections;
- cues;
- setlist;
- safe transport;
- device status.

### Rehearsal

Дополнительно:

- free seek;
- section loop;
- mute/solo helpers;
- expanded timeline;
- quick notes.

### Preparation / Inspector

Для live-specific settings:

- patch assignments;
- MIDI actions;
- cue properties;
- playback preparation;
- preflight/routing checks.

General DAW editing остаётся в Arrange/Edit/Mix workspaces MRS.

## 12. Playback Preparation UX

Для Live-specific non-destructive preparation:

- master playback gain;
- Trim Start;
- Trim End;
- Reset;
- Preview/Audition.

Требования:

- исходные clips/audio не переписываются;
- trim boundaries clearly visible;
- exact numeric input available;
- controls скрыты в normal Performance state;
- global trim synchronously affects project playback/stems.

## 13. Визуальный стиль

### Base

- deep graphite/dark navy;
- flat surfaces;
- subtle separators;
- moderate rounding only where useful;
- no bevels;
- no glassy/skeuomorphic controls;
- no heavy neon glow.

### Accent

- blue-violet — main MRS accent;
- amber — cue/playback attention;
- green — ready/connected;
- red — danger/error.

Arranger section colors are musical data and must remain distinguishable from system-state colors.

## 14. Typography

- modern neutral sans-serif;
- large chord symbols;
- clear section names;
- minimum tiny text in Performance state;
- short labels;
- stable numerical position display.

## 15. Laptop target

Primary target:

- 14" FHD;
- 15–16" FHD/2K;
- 100/125/150% scale;
- fullscreen Live Mode;
- viewing distance greater than normal DAW editing.

Everything critical must be readable without leaning toward the display.

## 16. Safety

Basic transport like Play/Stop must remain immediate and predictable.

Extra protection is appropriate for:

- Stop All / Panic;
- routing/device change during playback;
- destructive reset;
- dangerous MIDI command;
- project/song switching where current playback could be lost unexpectedly.

Avoid modal dialogs during performance. Prefer drawers/side panels/non-blocking warnings.

## 17. UI Components

Shared MRS design system should provide reusable:

- app/top bar;
- rails/inspectors;
- transport controls;
- moving chord strip;
- arranger timeline;
- section labels;
- cue banner;
- status pills;
- device indicators;
- trim handles;
- meters/sliders;
- warning/danger states.

Live Mode should reuse the same design language as the rest of MRS, but with larger spacing/type and lower information density.

## 18. Implementation isolation

Moving chord animation and all Live UI rendering run outside the realtime audio callback.

Conceptually:

```text
Audio Engine -> atomic/state snapshot -> UI model -> chord strip rendering
```

A UI stall must not interrupt audio.

## 19. Roadmap mapping

Current Issues:

- #28 — LIVE Stage 0: UX Foundation;
- #29 — LIVE Stage 1: real Project/Transport integration;
- #30+ — show workflow and advanced Live capabilities.

Live Mode has no separate product version; UI readiness is part of MRS builds.

## 20. Branding

The application is currently called **Moon River Studio / MR Studio**.

The Live screen should be branded as a mode of that application, e.g.:

```text
Moon River Studio
LIVE
```

rather than presenting `Moon River Live` as a separate installed product.

Final DAW name may change later; UI architecture should not depend on the current working brand.
