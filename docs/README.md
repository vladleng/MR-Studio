# MR Studio — индекс документации

GitHub — главный источник информации проекта (решение пользователя2026-10-12).
Документы описывают локальные результаты; исходники main/release могут отставать.
Не путать реализовано, автоматические проверки и пользовательскую приёмку.

## Начать здесь

- [Текущий контекст](PROJECT_CONTEXT.md): версия, source, пакет, подтверждённая приёмка и следующий шаг.
- [Правила](../AGENTS.md), [локальная разработка](LOCAL_DEVELOPMENT.md), [сборки/проверки](LOCAL_BUILD_POLICY.md).
- [Roadmap](ROADMAP.md), [Stage5](STAGE_5_PLAN.md), [риски](STAGE_5_RISK_REVIEW.md), [версии](VERSIONING.md).
- [GitHub issues](https://github.com/vladleng/MR-Studio/issues): задачи, требования и статусы.

## Текущая музыкальная структура

0.3g/#84 Meter map принят2026-10-13, full130 PASS; далее рекомендован #80/0.3h, пока не начат.

- [Chord Track/#79](CHORD_TRACK_79.md), [Markers/#82](MARKER_TRACK_82.md).
- [Sections/colors/bounds/#81](SECTION_COLORS_BOUNDS_81.md), [context/#85](MUSICAL_CONTEXT_85.md).
- [Navigation/#86](NAVIGATION_86.md), [Tempo/#83](TEMPO_MAP_83.md), [Meter/#84](METER_MAP_84.md), [shared timeline](MUSICAL_TIMELINE.md).
- [Persistence](PERSISTENCE.md): текущая локальная schema20/archive1, legacy migration.

## Движок, плагины, интерфейс

- [Архитектура](ARCHITECTURE.md), [контракты Core](CORE_CONTRACTS.md), [модель](DATA_MODEL.md).
- [Engine plan](ENGINE_PERFORMANCE_PLAN.md), [reliability](PROCESSING_RELIABILITY.md), [профайлер](ENGINE_PROFILING.md), [приёмка P4](ENGINE_P4_ACCEPTANCE.md).
- [VST3](VST3_HOSTING.md), [MIDI](MIDI_CLIPS.md), [native inserts](NATIVE_INSERTS.md), [sends](MIXER_SENDS_95.md).
- [MR Reverb дизайн](MR_REVERB.md), [реализация/приёмка](REVERB_VALIDATION.md), [gate/Mix](REVERB_GATE_MIX.md).
- [UI reference](UI_REFERENCE_UPD9.md), [plugin loading/send bypass](INSERT_LOADING_SEND_BYPASS_UPD8.md).

## Будущие концепции — не реализованы в текущей сборке

- [Spatial](ACOUSTIC_SPACE_PROTOTYPE.md), [FX routing](PRE_SPATIAL_ROUTING.md), [MR Strip Link](MR_STRIP_LINK.md).
- [Harma Waves](HARMA_WAVES_NATIVE.md), [roadmap](HARMA_WAVES_ROADMAP.md), [handoff](HARMA_WAVES_CODEX_HANDOFF.md).
- [Parallel VSTi](ENGINE_INSTRUMENT_PARALLELISM.md), [MR Saturator](MR_SATURATOR.md).

## История

Датированные MRS_STAGE_*_CHECKLIST/SHARED_STAGE_*_CHECKLIST и старые delivery docs —
свидетельства своего этапа. Их старые current/pending/not-started не меняют текущий контекст.
[Локальный архив](history/PROJECT_CONTEXT_LOCAL_BEFORE_SYNC_20261012.md),
[предыдущий GitHub контекст](history/PROJECT_CONTEXT_GITHUB_BEFORE_SYNC_20261012.md).
