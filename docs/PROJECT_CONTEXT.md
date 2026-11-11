# MR Studio — контекст и навигация

Этот файл — **короткий указатель**, не ежедневный handoff, журнал сборок или альтернативная база статусов. За текущими требованиями, реализацией, тестами и приёмкой обращайтесь в [Issues](https://github.com/vladleng/MR-Studio/issues), [PR](https://github.com/vladleng/MR-Studio/pulls), [Actions](https://github.com/vladleng/MR-Studio/actions) и актуальный код.

## Техническая картина

- [AGENTS.md](../AGENTS.md) — иерархия источников истины, безопасность, версия и CI.
- [ARCHITECTURE.md](ARCHITECTURE.md) — MR Studio и **самостоятельный MR Live** на общем Core, но с отдельными документами и версиями.
- [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md) — `.mrlive`, shows/items/channels и принятые границы интеграции.
- [CORE_CONTRACTS.md](CORE_CONTRACTS.md), [AUDIO_ENGINE.md](AUDIO_ENGINE.md), [DATA_MODEL.md](DATA_MODEL.md), [PERSISTENCE.md](PERSISTENCE.md), [ARA2_HOSTING.md](ARA2_HOSTING.md), [VST3_HOSTING.md](VST3_HOSTING.md) — профильные технические контракты.
- [MR_UNIFIED_DESIGN_SYSTEM.md](MR_UNIFIED_DESIGN_SYSTEM.md) — визуальный язык интерфейсов.
- [STEM_SEPARATION.md](STEM_SEPARATION.md) и [AUDIO_TO_MIDI.md](AUDIO_TO_MIDI.md) — ML workers/модели/импорт; статус этапов — в их Issues.
- [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md) — текущее направление MR Rig вместо отдельной линии MR Cab.
- [CLOUD_DEVELOPMENT.md](CLOUD_DEVELOPMENT.md) — среда и действующие принципы CI.

## Трекинг

- MR Live — см. [#14](https://github.com/vladleng/MR-Studio/issues/14), конкретные native/UI задачи и [#289](https://github.com/vladleng/MR-Studio/issues/289).
- Audio-to-MIDI — [#342](https://github.com/vladleng/MR-Studio/issues/342); Python Medium — [#368](https://github.com/vladleng/MR-Studio/issues/368).
- MR Separation — [#74](https://github.com/vladleng/MR-Studio/issues/74).
- ARA2 — [#88](https://github.com/vladleng/MR-Studio/issues/88).
- UI Kit / MR UDS — [#189](https://github.com/vladleng/MR-Studio/issues/189).
- MR Rig — [#197](https://github.com/vladleng/MR-Studio/issues/197).

Историческая длинная версия этого указателя доступна в истории Git. Её версии сборок, планы и локальные отчёты **не являются актуальным статусом**.
