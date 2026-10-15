# MR Studio — индекс документации

**Stage5/#25 завершён2026-10-13 — 9/9**, #87 accepted/closed completed.
Пользователь: «#87 пройдено, все сохраняется, undo/redo работает.». Все редакторы и #80 приняты ранее.
Последняя принятая сборка0.3h upd2/source803774d, schema21/archive1; package/manifest/
shortcut/source прежние. Использованы прежние full130/finalfocused3/persistence/GUI
результаты; новых build/tests/аппаратных измерений не заявляем. [Контекст](PROJECT_CONTEXT.md).
Следующий этап — только по выбору пользователя; AI Foundation пока backlog.

Принято2026-10-13: **0.3h upd2/#80**, «Все работает - закрывай!».
#80 closed completed; parent#25 актуально8/9, остаётся#87 (не начат).
Приёмка docs/issues-only, source/пакет/manifest/ярлык и прежние проверки неизменны;
новая ASIO/physical DPI матрица не предоставлена. [Контекст](PROJECT_CONTEXT.md).

0.3h upd2/#80, 2026-10-13: uniform marker/chord group gestures, chord color palette/radius3.
Schema21 (legacy1–20), archive1. Full130 PASS148.13s, packaged diagnostics/J3 exit0;
GUI100/150 inspected. Upd2 accepted2026-10-13 («Все работает - закрывай!»); #80 closed completed.
[Контракт/проверки](MUSICAL_LANE_GESTURES_UPD2.md). #87 не начат; code/build/package local.

0.3h upd1/#80 готов2026-10-13: compact headers, Alt-copy shadow/touch, group selection/operations; full130 PASS136.47s, packaged diagnostics/J3 exit0. Приёмка ожидается; #87 не начат. [Контракт](ARRANGER_EDITOR_80.md).

GitHub — главный источник информации проекта (решение пользователя2026-10-12).
Документы описывают локальные результаты; исходники main/release могут отставать.
Не путать реализовано, автоматические проверки и пользовательскую приёмку.

## Единый GUI-стандарт (утверждён 2026-10-15)

[MR Studio Unified Design System](MR_UNIFIED_DESIGN_SYSTEM.md) — единый визуальный стиль всей DAW, Live и native plugins; [пример SVG](design/MR_UNIFIED_STYLE_REFERENCE.svg), [палитра JSON](design/MR_UNIFIED_TOKENS.json), [GUI skill для Codex](../.codex/skills/mr-unified-gui/SKILL.md). Общая инфраструктура компонентов остаётся в [#189 MR UI Kit](https://github.com/vladleng/MR-Studio/issues/189), а отдельное внедрение — [#205 DAW/Live](https://github.com/vladleng/MR-Studio/issues/205), [#206 native plugins](https://github.com/vladleng/MR-Studio/issues/206), [#207 Reverb](https://github.com/vladleng/MR-Studio/issues/207), [#208 SVG](https://github.com/vladleng/MR-Studio/issues/208). Стиль утверждён, код GUI ещё не обновлён; принятые экраны сохраняются до отдельных этапов.

## Начать здесь

- [Текущий контекст](PROJECT_CONTEXT.md): версия, source, пакет, подтверждённая приёмка и следующий шаг.
- [Правила](../AGENTS.md), [локальная разработка](LOCAL_DEVELOPMENT.md), [сборки/проверки](LOCAL_BUILD_POLICY.md).
- [Roadmap](ROADMAP.md), [Stage5](STAGE_5_PLAN.md), [риски](STAGE_5_RISK_REVIEW.md), [версии](VERSIONING.md).
- [GitHub issues](https://github.com/vladleng/MR-Studio/issues): задачи, требования и статусы.

## Текущая музыкальная структура

0.3h/#80 Arranger editor готов2026-10-13, full130 PASS98.38s, packaged diagnostics/J3 exit0; приёмка ожидается. Последняя принятая0.3g/#84.

- [Arranger editor/#80](ARRANGER_EDITOR_80.md): copy/duplicate, lane visibility, контракт и проверки.

- [Chord Track/#79](CHORD_TRACK_79.md), [Markers/#82](MARKER_TRACK_82.md).
- [Sections/colors/bounds/#81](SECTION_COLORS_BOUNDS_81.md), [context/#85](MUSICAL_CONTEXT_85.md).
- [Navigation/#86](NAVIGATION_86.md), [Tempo/#83](TEMPO_MAP_83.md), [Meter/#84](METER_MAP_84.md), [shared timeline](MUSICAL_TIMELINE.md).
- [Persistence](PERSISTENCE.md): текущая локальная schema20/archive1, legacy migration.

## Движок, плагины, интерфейс

- **MR UI Kit / UI Gallery (plan #189):** [компоненты/дизайн-система](MR_UI_KIT.md), [интерактивная Gallery](MR_UI_GALLERY.md), [этапы](MR_UI_KIT_ROADMAP.md), [handoff для Codex](MR_UI_KIT_CODEX_HANDOFF.md). Общее JUCE UI-направление, не реализовано; принятый [UI reference](UI_REFERENCE_UPD9.md) сохраняется.

- [Архитектура](ARCHITECTURE.md), [контракты Core](CORE_CONTRACTS.md), [модель](DATA_MODEL.md).
- [Engine plan](ENGINE_PERFORMANCE_PLAN.md), [reliability](PROCESSING_RELIABILITY.md), [профайлер](ENGINE_PROFILING.md), [приёмка P4](ENGINE_P4_ACCEPTANCE.md).
- [VST3](VST3_HOSTING.md), [MIDI](MIDI_CLIPS.md), [native inserts](NATIVE_INSERTS.md), [sends](MIXER_SENDS_95.md).
- [MR Reverb дизайн](MR_REVERB.md), [реализация/приёмка](REVERB_VALIDATION.md), [gate/Mix](REVERB_GATE_MIX.md).
- [UI reference](UI_REFERENCE_UPD9.md), [plugin loading/send bypass](INSERT_LOADING_SEND_BYPASS_UPD8.md).

- **MR Cab Full Combo IR [#196](https://github.com/vladleng/MR-Studio/issues/196):** [Full Combo IR](MR_CAB_FULL_COMBO_IR.md) — будущее аддитивное расширение принятого #185: WAV записанной цепочки Polytone/Fender без второго кабинета; neural amp DSP отдельно.

- **MR Rig / [#197](https://github.com/vladleng/MR-Studio/issues/197) (planned):** [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md), [MR_RIG_GUI.md](MR_RIG_GUI.md), [MR_RIG_LIBRARY_PRESETS.md](MR_RIG_LIBRARY_PRESETS.md), [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md), [MR_RIG_ROADMAP.md](MR_RIG_ROADMAP.md), [MR_RIG_CODEX_HANDOFF.md](MR_RIG_CODEX_HANDOFF.md) — визуальные готовые модели IR/NAM с изображениями, My Rigs, Pre/Core/Post chain, педали, NAM отдельным DSP. Планы и задачи #198–204, без реализации; #185 Cab принят, #196 Full Combo готов локально и ждёт приёмки.

## Будущие концепции — не реализованы в текущей сборке

- [Spatial](ACOUSTIC_SPACE_PROTOTYPE.md), [FX routing](PRE_SPATIAL_ROUTING.md), [MR Strip Link](MR_STRIP_LINK.md).
- [Harma Waves](HARMA_WAVES_NATIVE.md), [roadmap](HARMA_WAVES_ROADMAP.md), [handoff](HARMA_WAVES_CODEX_HANDOFF.md).
- [Parallel VSTi](ENGINE_INSTRUMENT_PARALLELISM.md), [MR Saturator](MR_SATURATOR.md).
- **MR Cab #182 — архив принятого WAV backend:** [архитектура/история](MR_CAB_COLLECTIONS.md), [архивный roadmap](MR_CAB_ROADMAP.md). #183/#184/#185/#196 приняты, #182/#186–188 закрыты как superseded/not_planned. **Новых кабинетных этапов нет; дальнейшая работа только MR Rig #197.**
- [ARA2 Hosting](ARA2_HOSTING.md) — внешний ARA2 editor host (#88; stages #177–#181), **план / не реализовано**.

## История

Датированные MRS_STAGE_*_CHECKLIST/SHARED_STAGE_*_CHECKLIST и старые delivery docs —
свидетельства своего этапа. Их старые current/pending/not-started не меняют текущий контекст.
[Локальный архив](history/PROJECT_CONTEXT_LOCAL_BEFORE_SYNC_20261012.md),
[предыдущий GitHub контекст](history/PROJECT_CONTEXT_GITHUB_BEFORE_SYNC_20261012.md).
