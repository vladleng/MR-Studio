# MR Studio — контекст и навигация

Этот файл — **короткий указатель**, не ежедневный handoff, журнал сборок или альтернативная база статусов. Требования, статус и приёмка находятся в DSH Task Manager, проект «MR Studio»; реализация и результаты проверок подтверждаются локальным кодом и проверками. Агент ведёт журнал через `tasks_*` по `dsh-task-manager-workflow`. Старые [GitHub Issues](https://github.com/vladleng/MR-Studio/issues), PR и Actions — исторические свидетельства.

## Техническая картина

- [AGENTS.md](../AGENTS.md) — иерархия источников истины, безопасность, версия и локальные проверки.
- [ARCHITECTURE.md](ARCHITECTURE.md) — MR Studio и **самостоятельный MR Live** на общем Core, но с отдельными документами и версиями.
- [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md) — `.mrlive`, shows/items/channels и принятые границы интеграции.
- [CORE_CONTRACTS.md](CORE_CONTRACTS.md), [AUDIO_ENGINE.md](AUDIO_ENGINE.md), [DATA_MODEL.md](DATA_MODEL.md), [PERSISTENCE.md](PERSISTENCE.md), [ARA2_HOSTING.md](ARA2_HOSTING.md), [VST3_HOSTING.md](VST3_HOSTING.md) — профильные технические контракты.
- [MR_UNIFIED_DESIGN_SYSTEM.md](MR_UNIFIED_DESIGN_SYSTEM.md) — визуальный язык интерфейсов.
- [STEM_SEPARATION.md](STEM_SEPARATION.md) и [AUDIO_TO_MIDI.md](AUDIO_TO_MIDI.md) — ML workers/модели/импорт; статус этапов — в соответствующих задачах DSH Task Manager.
- [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md) — текущее направление MR Rig вместо отдельной линии MR Cab.
- [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md) — локальная среда и проверки; [CLOUD_DEVELOPMENT.md](CLOUD_DEVELOPMENT.md) — история облачного процесса.

## Исторический трекинг GitHub

Текущие задачи ведутся в DSH Task Manager, проект «MR Studio». Ссылки ниже сохраняют историю решений и трассировку перенесённых задач, не актуальный статус.

- MR Live — см. [#14](https://github.com/vladleng/MR-Studio/issues/14), конкретные native/UI задачи и [#289](https://github.com/vladleng/MR-Studio/issues/289).
- Audio-to-MIDI — [#342](https://github.com/vladleng/MR-Studio/issues/342); Python Medium — [#368](https://github.com/vladleng/MR-Studio/issues/368).
- MR Separation — [#74](https://github.com/vladleng/MR-Studio/issues/74).
- ARA2 — [#88](https://github.com/vladleng/MR-Studio/issues/88).
- UI Kit / MR UDS — [#189](https://github.com/vladleng/MR-Studio/issues/189).
- MR Rig — [#197](https://github.com/vladleng/MR-Studio/issues/197).

Историческая длинная версия этого указателя доступна в истории Git. Её версии сборок, планы и локальные отчёты **не являются актуальным статусом**.
