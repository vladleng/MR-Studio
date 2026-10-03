# Moon River Studio

**Moon River Studio** (`MRS`, рабочее сокращение `MR Studio`) — проект собственной performance-first DAW.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Studio Pro. Это не отдельное приложение и не отдельная продуктовая версия.

> Репозиторий называется `MR-Studio`; историческое имя — `Moon-River-Live`. Текущая архитектура охватывает всю Moon River Studio.

## Новый чат / новый разработчик

Начинать с [`docs/START_HERE.md`](docs/START_HERE.md).

Там зафиксированы:

- краткая архитектурная формула проекта;
- правило одного SHARED Core / Engine;
- роль встроенного Live Mode;
- порядок чтения документации;
- карта Issues;
- правила параллельной разработки;
- критические вещи, которые нельзя переизобретать или дублировать.

После `START_HERE.md` открыть актуальный Stage issue, над которым продолжается работа.

## Текущая реализация

**SHARED Stage 0 / issue #15** принят после Windows-checker, PR #36 слит в main.

**SHARED Stage 1 / issue #16** интегрирован через PR #37: общий realtime renderer,
родной vendor ASIO через PortAudio, WAV preload/playback, мониторинг и метрики.
Базовые hardware-тесты WAV и input monitoring при 48k/128 пройдены.
Оставшиеся длительные тесты и Studio Pro benchmark отложены пользователем;
performance gate остаётся pending и не блокирует дальнейшую разработку.
Это общий C++20 backend: Project Model v1, Command/Undo, Transport API,
tempo/meter contracts, подписки, fixtures и versioned snapshot.

- [Core contracts](docs/CORE_CONTRACTS.md) — API, ownership, threading и границы этапа.
- [Проверка Windows-сборки](docs/SHARED_STAGE_0_CHECKLIST.md) — консольный checker.
- [Audio core](docs/AUDIO_CORE.md) — realtime/device contracts и границы прототипа.
- [Проверка ASIO](docs/SHARED_STAGE_1_CHECKLIST.md) — guided Windows tester и benchmark.
- [Статус реализации](docs/IMPLEMENTATION_STATUS.md) — продолжение работы.

**SHARED Stage 2 / issue #17** принят пользователем, PR #38 интегрирован: Musical Timeline, Chord/Arranger lanes,
общие context/navigation services, snapshot v2 с чтением v1.
[Musical contracts](docs/MUSICAL_TIMELINE.md) · [Windows checker](docs/SHARED_STAGE_2_CHECKLIST.md).

GUI ещё нет. MRS 0.1 не считается завершённой до DAW Foundation (#20)
и прохождения Audio Engine / ASIO performance gate (#16).

## Основная концепция

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live
```

Один project используется и для production, и для performance.

```text
                    Moon River Studio
                           |
        +------------------+------------------+
        |                  |                  |
     Arrange              Mix               Live
        |                  |                  |
        +------------------+------------------+
                           |
                       SHARED CORE
                           |
        +------------------+------------------+
        |                  |                  |
   Audio Engine           MIDI             Plugins
        |                  |                  |
       ASIO            MIDI I/O             VST3
```

Live Mode не имеет отдельного ASIO engine, Transport, MIDI engine, plugin host или project copy.

## Основные направления MRS

- audio tracks/clips/events;
- recording/playback;
- MIDI tracks/editor;
- mixer, buses and routing;
- VST3 hosting;
- native DSP;
- automation;
- tempo/meter map;
- Chord Track;
- Arranger Track;
- markers;
- project save/load/recovery;
- AI / ChatGPT integration;
- Live Mode.

## Live Mode

- setlists;
- moving Chord Track strip;
- current/next section;
- cues/markers;
- transport;
- click/cue;
- live inputs;
- patches;
- MIDI automation;
- foot control;
- preflight/recovery;
- remote/mobile companion.

Live Mode появляется на определённом этапе развития MRS, а затем может развиваться параллельно с production-функциями на том же SHARED Core.

## Performance-first

Audio performance — blocking requirement.

На Windows основной professional/live path должен поддерживать прямую работу через родной vendor ASIO driver аудиоинтерфейса.

Критические принципы:

- realtime audio thread отделён от UI/network/AI/file I/O;
- no blocking file/network/UI work in audio callback;
- preload/read-ahead;
- plugin latency accounting;
- low-latency monitoring path;
- xrun/dropout diagnostics;
- benchmark относительно Studio Pro на одинаковой конфигурации.

Подробнее: [`docs/AUDIO_ENGINE.md`](docs/AUDIO_ENGINE.md).

## AI / ChatGPT

MRS проектируется так, чтобы AI мог работать со структурированным Project Model через Context/Tool API.

Будущие возможности:

- понимать tracks/clips/MIDI/chords/sections;
- анализировать аранжировку и mixer state;
- генерировать и редактировать MIDI партии;
- предлагать изменения;
- после разрешения пользователя выполнять project commands;
- управлять DAW естественным языком.

AI не является частью realtime audio path и при его недоступности DAW/Live Mode продолжают работать нормально.

Подробнее: [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md).

## Native DSP / IR / Amp modeling

Планируется возможность встроенных processors:

- EQ/compressor/saturation;
- convolution/Cab IR;
- amp/preamp/pedal DSP;
- neural model player;
- собственные Moon River captures/models.

Factory Content и User Library должны быть лицензированно разделены.

Подробнее: [`docs/DSP_MODELING.md`](docs/DSP_MODELING.md).

## Fender Studio Pro

Studio Pro больше не является обязательным authoring environment.

Он остаётся:

- performance/UX reference;
- возможным import/migration source;
- optional compatibility target для существующих проектов.

Compatibility/import ведётся отдельно в issue #3 и не блокирует основную разработку MRS.

## Организация разработки

Используются три issue track:

```text
[MRS]    DAW features/workspaces
[SHARED] common Core / Engine
[LIVE]   embedded Live Mode
```

Это три потока разработки **одного приложения**, а не три продукта.

Версионируется только Moon River Studio. Live Mode имеет Stage readiness и входит в соответствующие MRS builds.

Live UI может использовать mock SHARED services до готовности real backend, что позволяет вести работу параллельно.

Подробнее: [`docs/DEVELOPMENT_TRACKS.md`](docs/DEVELOPMENT_TRACKS.md).

## GitHub Roadmaps

- #1 — master roadmap;
- #12 — MRS roadmap;
- #13 — SHARED Core roadmap;
- #14 — Live Mode roadmap;
- #15–#19 — SHARED stages;
- #20–#27 — MRS stages;
- #28–#35 — LIVE stages;
- #3 — Studio Pro compatibility/import.

## Документация

- [`docs/START_HERE.md`](docs/START_HERE.md) — обязательная точка входа для новых чатов/разработчиков.
- [`docs/MOON_RIVER_STUDIO_VISION.md`](docs/MOON_RIVER_STUDIO_VISION.md) — целевая концепция MRS.
- [`docs/PROJECT_VISION.md`](docs/PROJECT_VISION.md) — общее видение продукта.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — единый Core и workspaces.
- [`docs/AUDIO_ENGINE.md`](docs/AUDIO_ENGINE.md) — ASIO, realtime rules и performance benchmark.
- [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md) — ChatGPT/OpenAI integration, Context/Tool API и permissions.
- [`docs/DSP_MODELING.md`](docs/DSP_MODELING.md) — native DSP, Cab IR, amp/preamp/pedal и neural models.
- [`docs/DEVELOPMENT_TRACKS.md`](docs/DEVELOPMENT_TRACKS.md) — параллельные MRS/SHARED/LIVE issue tracks.
- [`docs/UI_UX_CONCEPT.md`](docs/UI_UX_CONCEPT.md) — UI/UX-концепция Live Mode.
- [`docs/DATA_MODEL.md`](docs/DATA_MODEL.md) — Project Model / musical/live data.
- [`docs/STUDIO_PRO_INTEGRATION.md`](docs/STUDIO_PRO_INTEGRATION.md) — optional Studio Pro compatibility/import track.
- [`docs/LIVE_WORKFLOW.md`](docs/LIVE_WORKFLOW.md) — live workflow внутри MRS.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — общий roadmap.

## Roadmap в одном экране

| MRS version / stage | Основная цель |
|---|---|
| MRS 0.1 | Foundation + Core contracts + ASIO performance gate |
| MRS 0.2 | Audio Arrangement |
| MRS 0.3 | Mixer / Routing |
| MRS 0.4 | VST3 / Native DSP |
| MRS 0.5 | MIDI |
| MRS 0.6 | Chord/Arranger/Musical Structure + first native Live integration |
| MRS 0.7 | AI Foundation |
| MRS 0.8+ | Advanced DAW / Reliability |

Live Mode начинается как UI prototype после базовых SHARED contracts и далее развивается параллельно через LIVE Stage 0–7.

## Рабочее название

`Moon River Studio` / `MR Studio` — рабочее название всей DAW. Финальное название может быть изменено на поздней стадии.

## Лицензия

Лицензия проекта пока не определена.
