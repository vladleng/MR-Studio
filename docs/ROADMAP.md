# MR Studio — направления развития

Это **карта направлений**, не план-график, не статусная таблица и не чек-лист приёмки. Конкретные этапы, очередность и готовность — в [GitHub Issues](https://github.com/vladleng/MR-Studio/issues) и нативных Sub-issues; действующие правила — в [AGENTS.md](../AGENTS.md).

## Границы продукта

- **MR Studio** — самостоятельная DAW: Arrange, Edit, Mix, Project, запись, плагины и музыкальные редакторы.
- **MR Live** — самостоятельное сценическое приложение с собственным show-документом `.mrlive`, setlist/items и сценическими каналами; **не** встроенный режим MR Studio.
- Общие [Core](CORE_CONTRACTS.md), [audio/MIDI engine](AUDIO_ENGINE.md), plugin host, Transport и некоторые модели данных развиваются без второго DSP/backend. Общий Core **не означает**, что Studio-проект автоматически становится Live item.
- Дизайн интерфейсов использует [MR UDS](MR_UNIFIED_DESIGN_SYSTEM.md), но плотность и рабочие сценарии приложений различаются.

## Технические направления

| Направление | Постоянный контракт | Трекинг |
| --- | --- | --- |
| Shared Engine / ASIO / realtime | [Audio Engine](AUDIO_ENGINE.md), [Processing](PROCESSING_RELIABILITY.md) | [Issues](https://github.com/vladleng/MR-Studio/issues) |
| MIDI, таймлайн, persistence | [Core](CORE_CONTRACTS.md), [Data Model](DATA_MODEL.md), [Persistence](PERSISTENCE.md) | [Issues](https://github.com/vladleng/MR-Studio/issues) |
| ARA2 / VST3 | [ARA2](ARA2_HOSTING.md), [VST3](VST3_HOSTING.md) | [#88](https://github.com/vladleng/MR-Studio/issues/88) |
| MR Live | [Native integration](MR_LIVE_NATIVE_INTEGRATION.md) | [#14](https://github.com/vladleng/MR-Studio/issues/14) |
| Separation / Audio-to-MIDI | [Separation](STEM_SEPARATION.md), [Transcription](AUDIO_TO_MIDI.md) | [#74](https://github.com/vladleng/MR-Studio/issues/74), [#342](https://github.com/vladleng/MR-Studio/issues/342) |
| GUI и native effects | [MR UDS](MR_UNIFIED_DESIGN_SYSTEM.md), [MR Rig](MR_RIG_ARCHITECTURE.md) | [#189](https://github.com/vladleng/MR-Studio/issues/189), [#197](https://github.com/vladleng/MR-Studio/issues/197) |

Планируемая функция не считается реализованной без проверки кода и соответствующей приёмки. Исторический Stage 0–7 roadmap остаётся в истории Git, а не в действующих указаниях Codex.
