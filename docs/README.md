# MR Studio / MR Live — индекс технических контрактов

Этот файл — **необязательный тематический указатель**, не инструкция по запуску работы и не журнал этапов. Для реализованного поведения используй код и тесты активного режима; требования, незавершённую работу и приёмку смотри в GitHub Issues (CLOUD) или DSH Task Manager (LOCAL). Постоянные границы — в корневом [AGENTS.md](../AGENTS.md).

Документы ниже открывай **только по конкретной необходимости**, адресно по разделам.

| Тема | Документы |
| --- | --- |
| Архитектура Studio / Live / Shared Core | [ARCHITECTURE](ARCHITECTURE.md), [CORE_CONTRACTS](CORE_CONTRACTS.md), [MR Live integration](MR_LIVE_NATIVE_INTEGRATION.md) |
| Модели и сохранение проектов | [DATA_MODEL](DATA_MODEL.md), [PERSISTENCE](PERSISTENCE.md) |
| Audio, realtime, transport | [AUDIO_ENGINE](AUDIO_ENGINE.md), [PROCESSING_RELIABILITY](PROCESSING_RELIABILITY.md), [MUSICAL_TIMELINE](MUSICAL_TIMELINE.md) |
| MIDI и хостинг | [MIDI clips](MIDI_CLIPS.md), [MIDI processor graph](MIDI_PROCESSOR_GRAPH.md), [VST3](VST3_HOSTING.md), [ARA2](ARA2_HOSTING.md) |
| Live и музыкальные инструменты | [Live workflow](LIVE_WORKFLOW.md), [MR Rig](MR_RIG_ARCHITECTURE.md) |
| Разделение аудио и транскрипция | [STEM_SEPARATION](STEM_SEPARATION.md), [AUDIO_TO_MIDI](AUDIO_TO_MIDI.md) |
| Интерфейс и SVG | [MR UDS](MR_UNIFIED_DESIGN_SYSTEM.md), [SVG runtime](MR_SVG_RUNTIME.md) |
| Локальная сборка и версии | [LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md), [VERSIONING](VERSIONING.md) |

Исторические `Stage/Fix/Upd/Handoff` и прежние планы не определяют текущий статус. Для истории исходников используй Git; для задач и решений — трекер активного режима. CI подтверждает только фактические проверки, не пользовательскую приёмку.
