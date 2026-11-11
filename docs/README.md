# MR Studio — техническая документация

Этот индекс связывает **постоянные технические контракты**, а не повторяет историю разработки. Для статуса/приёмки смотрите [GitHub Issues](https://github.com/vladleng/MR-Studio/issues), PR и Actions; для поведения агента — [AGENTS.md](../AGENTS.md). Не загружайте всю папку `docs/` в контекст одной задачи.

## Архитектура и общие системы

- [Архитектура двух приложений и Shared Core](ARCHITECTURE.md), [контракты Core](CORE_CONTRACTS.md), [модель проекта](DATA_MODEL.md), [Persistence / Recovery](PERSISTENCE.md).
- [Audio Engine / ASIO](AUDIO_ENGINE.md), [Realtime и надёжность](PROCESSING_RELIABILITY.md), [параллельная обработка](ENGINE_PARALLEL_PROCESSING.md), [Time Stretch API](TIME_STRETCH_API.md).
- [VST3 Hosting](VST3_HOSTING.md), [ARA2 Hosting](ARA2_HOSTING.md), [ARA state](ARA2_STATE_180.md).
- [Музыкальный таймлайн](MUSICAL_TIMELINE.md), [MIDI clips](MIDI_CLIPS.md), [MIDI input](MIDI_LIVE_INPUT.md), [MIDI processing](MIDI_PROCESSOR_GRAPH.md).

## Приложения и AI

- [MR Live: native integration](MR_LIVE_NATIVE_INTEGRATION.md), [MR Live workflow](LIVE_WORKFLOW.md), [Live edit architecture](MR_LIVE_EDIT_ARCHITECTURE.md).
- [MR Separation Engine](STEM_SEPARATION.md), [Audio-to-MIDI / MuScriptor Python](AUDIO_TO_MIDI.md), [AI Foundation](AI_FOUNDATION_CONTRACT.md).
- [MR Rig architecture](MR_RIG_ARCHITECTURE.md), [MR Reverb](MR_REVERB.md), [Harma Waves](HARMA_WAVES_NATIVE.md).

## Интерфейс

- [MR Unified Design System](MR_UNIFIED_DESIGN_SYSTEM.md) — канонический визуальный язык обеих программ и родных плагинов.
- [MR UI Kit](MR_UI_KIT.md), [SVG runtime](MR_SVG_RUNTIME.md), [`assets/ui/mr-uds/`](../assets/ui/mr-uds/README.md), [GUI Skill](../.agents/skills/mr-unified-gui/SKILL.md).

## Работа с репозиторием

- [Облачная разработка и CI](CLOUD_DEVELOPMENT.md), [Skill сборки](../.agents/skills/mr-studio-build-test/SKILL.md).
- [Тематические направления](DEVELOPMENT_TRACKS.md), [долгосрочное видение](PROJECT_VISION.md), [roadmap как карта направлений](ROADMAP.md).
- [Контекст-указатель](PROJECT_CONTEXT.md) и [быстрый вход](START_HERE.md) — навигация, не отдельный источник статуса.
- [История версионирования](VERSIONING.md) и [старой локальной разработки](LOCAL_BUILD_POLICY.md) — архивные указатели, **действующие правила** только в AGENTS/CI.

**Не использовать** старые Stage/Fix/Upd/Handoff-файлы как актуальные инструкции. Уникальные контракты сохраняются в тематических спецификациях; старые версии документов доступны через историю Git.
