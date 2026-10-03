# Moon River Studio / Moon River Live

**Moon River Studio (MRS)** — проект собственной DAW Moon River Studio.

**Moon River Live (MRL)** — встроенный Live / Performance workspace внутри MRS, по смыслу близкий к Show Page в Studio Pro, но использующий тот же Project Model, Transport, Audio Engine, MIDI Engine и plugin graph, что и production-режимы DAW.

> Текущий репозиторий исторически называется `Moon-River-Live`, но архитектурное направление проекта расширено до Moon River Studio. Переименование/миграция репозитория может быть выполнено отдельным шагом позже.

## Основная концепция

```text
Moon River Studio
├── Arrange
├── Edit
├── Mix
├── Project
└── Live (MRL)
```

Один проект используется и для production, и для performance.

```text
                    Moon River Studio
                           |
        +------------------+------------------+
        |                  |                  |
     Arrange              Mix               Live
        |                  |                  |
        +------------------+------------------+
                           |
                    Shared Project Model
                           |
        +------------------+------------------+
        |                  |                  |
   Audio Engine           MIDI             Plugins
        |                  |                  |
       ASIO            MIDI I/O             VST3
```

MRL не имеет отдельного ASIO engine и не требует экспортировать проект в другую программу.

## MRS — основные направления

- audio tracks/clips/events;
- MIDI tracks/editor;
- mixer, buses and routing;
- VST3 hosting;
- native DSP;
- automation;
- tempo/meter map;
- Chord Track;
- Arranger Track;
- markers;
- project save/load;
- AI / ChatGPT integration;
- Live workspace.

## MRL — основные направления

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

## Performance-first

Audio performance является blocking requirement.

На Windows основной live/performance path должен работать напрямую через родной vendor ASIO driver аудиоинтерфейса.

Критические принципы:

- realtime audio thread отделён от UI/network/AI/file I/O;
- no allocation/file I/O/network in audio callback;
- preload/read-ahead;
- plugin latency accounting;
- live-safe processing path;
- xrun/dropout diagnostics;
- benchmark относительно Studio Pro на одинаковой конфигурации.

Подробнее: [`docs/AUDIO_ENGINE.md`](docs/AUDIO_ENGINE.md).

## AI / ChatGPT

MRS проектируется так, чтобы AI мог работать со структурированным Project Model через Context/Tool API.

Будущие возможности:

- понимать tracks/clips/MIDI/chords/sections;
- анализировать аранжировку и mixer state;
- генерировать MIDI партии;
- предлагать изменения;
- после разрешения пользователя выполнять project commands;
- управлять DAW естественным языком.

AI не является частью realtime audio path и при его недоступности DAW/Live продолжают работать нормально.

Подробнее: [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md).

## Native DSP / IR / Amp modeling

Планируется возможность встроенных MRS processors:

- EQ/compressor/saturation;
- convolution/Cab IR;
- amp/preamp/pedal DSP;
- neural model player;
- собственные Moon River captures/models.

Factory Content и User Library должны быть лицензированно разделены.

Подробнее: [`docs/DSP_MODELING.md`](docs/DSP_MODELING.md).

## Studio Pro

Fender Studio Pro больше не является обязательным authoring environment.

Он остаётся:

- performance/UX reference;
- возможным import/migration source;
- optional compatibility target для существующих проектов.

Studio Pro Bridge можно развивать отдельным compatibility track, не блокирующим основную разработку MRS/MRL.

## Параллельная разработка

Используются три issue track:

```text
[MRS]    DAW / production / core implementation
[MRL]    Live / Performance workspace
[SHARED] contracts/models/services used by both
```

MRS и MRL имеют независимую нумерацию версий и могут разрабатываться параллельно. MRL может использовать mock services до готовности реального MRS backend.

Подробнее: [`docs/DEVELOPMENT_TRACKS.md`](docs/DEVELOPMENT_TRACKS.md).

## Документация

- [`docs/MOON_RIVER_STUDIO_VISION.md`](docs/MOON_RIVER_STUDIO_VISION.md) — целевая концепция MRS и роль MRL.
- [`docs/PROJECT_VISION.md`](docs/PROJECT_VISION.md) — общее видение продукта.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — архитектура MRS Core и workspaces.
- [`docs/AUDIO_ENGINE.md`](docs/AUDIO_ENGINE.md) — ASIO, realtime rules и performance benchmark.
- [`docs/AI_INTEGRATION.md`](docs/AI_INTEGRATION.md) — ChatGPT/OpenAI integration, Context/Tool API и permissions.
- [`docs/DSP_MODELING.md`](docs/DSP_MODELING.md) — native DSP, Cab IR, amp/preamp/pedal и neural models.
- [`docs/DEVELOPMENT_TRACKS.md`](docs/DEVELOPMENT_TRACKS.md) — параллельные MRS/MRL issue tracks.
- [`docs/UI_UX_CONCEPT.md`](docs/UI_UX_CONCEPT.md) — UI/UX-концепция MRL.
- [`docs/DATA_MODEL.md`](docs/DATA_MODEL.md) — текущая модель metadata; должна эволюционировать в общий MRS Project Model.
- [`docs/STUDIO_PRO_INTEGRATION.md`](docs/STUDIO_PRO_INTEGRATION.md) — optional Studio Pro compatibility/import track.
- [`docs/LIVE_WORKFLOW.md`](docs/LIVE_WORKFLOW.md) — live workflow.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — параллельный roadmap MRS/MRL.

## Roadmap в одном экране

### MRS

| Stage | Цель |
|---|---|
| MRS 0.1 | Core Foundation + ASIO Performance Gate |
| MRS 0.2 | Audio Arrangement |
| MRS 0.3 | Mixer / Routing |
| MRS 0.4 | VST3 / Native DSP |
| MRS 0.5 | MIDI |
| MRS 0.6 | Chord/Arranger/Musical Structure |
| MRS 0.7 | AI Foundation |
| MRS 0.8+ | Advanced DAW / Reliability |

### MRL

| Stage | Цель |
|---|---|
| MRL 0.1 | Live UX Foundation |
| MRL 0.2 | Real Project / Transport Integration |
| MRL 0.3 | Setlists / Show Workflow |
| MRL 0.4 | Playback / Click / Cue |
| MRL 0.5 | Live Inputs / Patches |
| MRL 0.6 | MIDI Automation / Hardware Control |
| MRL 0.7 | Remote / Mobile Companion |
| MRL 1.0 | Concert Reliability |

## Лицензия

Лицензия проекта пока не определена.
