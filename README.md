# MR Studio — музыкальная экосистема

**MR Studio** — настольная DAW для создания и обработки музыки. **MR Live** — самостоятельное приложение для сценического исполнения и show-процессов. Оба приложения развиваются в одном репозитории и переиспользуют **Shared Core** (audio/MIDI, transport, processing graph, plugin host, persistence), но имеют собственные документы и независимые линии версий.

> Это индекс исходников, а не журнал разработчика. Актуальный статус, критерии приёмки и планы каждой задачи находятся в [GitHub Issues](https://github.com/vladleng/MR-Studio/issues). Прошлые проверки — в PR и [Actions](https://github.com/vladleng/MR-Studio/actions).

## Разработка

1. Прочитать [AGENTS.md](AGENTS.md): правила работы, безопасности и версий.
2. Найти назначенную Issue и проверить её актуальный объём.
3. Прочитать **только необходимые** разделы [индекса технической документации](docs/README.md) и применимый Skill в [`.agents/skills/`](.agents/skills/).
4. Для Windows-сборок и тестов сверяться с [облачной политикой](docs/CLOUD_DEVELOPMENT.md) и действующими `.github/workflows/`.

## Карта репозитория

- [`apps/studio-desktop/`](apps/studio-desktop/) — desktop application и общий application layer.
- [`apps/studio-juce/`](apps/studio-juce/) — JUCE frontend, графический интерфейс.
- [`core/`](core/) — Shared Core, audio, MIDI, processing и модели.
- [`tests/`](tests/) — тесты поведения и контрактов.
- [`tools/ci/`](tools/ci/) — проверка и упаковка; GitHub Actions определяет порядок CI.
- [`assets/ui/mr-uds/`](assets/ui/mr-uds/) — общая библиотека GUI ресурсов.

## Основные контракты

[Архитектура](docs/ARCHITECTURE.md) · [Core](docs/CORE_CONTRACTS.md) · [Audio Engine](docs/AUDIO_ENGINE.md) · [ARA2](docs/ARA2_HOSTING.md) · [MIDI / Audio-to-MIDI](docs/AUDIO_TO_MIDI.md) · [MR Live](docs/MR_LIVE_NATIVE_INTEGRATION.md) · [MR Unified Design System](docs/MR_UNIFIED_DESIGN_SYSTEM.md).

**История изменений** хранится в Git, закрытых Issues, PR и запусках Actions. Устаревшие описания этапов не являются спецификацией текущего поведения.
