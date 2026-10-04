# Moon River Studio

## 0.1m / Stage 3c — local build ready, 2026-10-04

Cab IR: mono/stereo WAV import, embedded kernel, live Mix/Gain/low-high cuts/polarity,
Neutral/Warm/Bright control presets, zero additional algorithmic convolution latency.
Right VST3 tab: vendor folders, drag/drop to mixer tracks/buses/Master and single-click
insert native editor. Native insert workflow retained; structure edits after Pause/Stop.
Core v10 reads v1–v9; keep a project backup before saving. Code
`c505128089916350950bc7bdc36ce7a36152f6d4`, local branch `mrs/0.1m-cab-ir-browser-local`.
Offline-dependency local ASIO configure/build and 87/87 CTest passed; hidden GUI tests
cover actual tree drag notifications, invalid/cancelled drops, track/bus/Master targets,
Undo, single-click editor/reuse, Cab controls/presets and existing DPI/flicker regressions.
Package `MR-Studio-0.1m-ASIO-Windows-local` includes owned synthetic test WAVs.
Celestion WAV is obtained separately via user email subscription; no signup or redistribution
performed and no Celestion-specific audition claimed. Physical user check of 0.1m pending.
Latest accepted: 0.1l / 3b. Whole #23 open; 3d/3e planned, #16 deferred.
Code/builds local; GitHub issues/docs only; no code push/PR/merge/Actions/install.
See [Cab IR](CAB_IR.md), [3c checklist](MRS_STAGE_3C_CHECKLIST.md).

## 0.1l / Stage 3b — локальная сборка готова, 2026-10-04

VST3 effects: scan/cache в отдельном процессе с timeout, загрузка/удаление/bypass,
окно плагина, generic параметры, opaque component/controller state и reported latency.
Channel EQ объединяет три bell-полосы и HP/LP: суммарная кривая, точки frequency/gain,
Q колесом, band enable/bypass и параметры во время playback без остановки устройства.
Фейдеры track/bus/Master и pan двигаются относительно исходного значения от ручки;
нажатие на шкалу не меняет значение. Ctrl — точная регулировка.
Core v9 читает v1–v8; сохраните копию проекта перед сохранением новой версией.
Код 95c719fcd0893652f92a93beb9b20cb538cd84b9, ветка mrs/0.1l-vst3-eq-local.
Локальные configure/build, 85/85 CTest, GUI smoke и previews пройдены. Установленный
Blue Cat Gain 3 Stereo проверен на обработку, gain automation, state roundtrip и editor.
Пакет MR-Studio-0.1l-ASIO-Windows-local в Builds. Последняя принятая — 0.1k / 3a;
0.1l / 3b ожидает пользовательской приёмки. #23 открыт; #16 остаётся отложенным.
Следующие подэтапы: 3c Cab IR, 3d Amp/Preamp, 3e Processing Reliability.
Код/пакеты только локально, GitHub только issues/docs; без code push/PR/merge/Actions.
См. [VST3](VST3_HOSTING.md), [Channel EQ](NATIVE_INSERTS.md),
[приёмка 3b](MRS_STAGE_3B_CHECKLIST.md).


## 0.1k / Stage 3a accepted — 2026-10-04

Пользователь подтвердил: «Проверил, вроде все работает, закрывай под-этап.»
Подэтап 3a (Native Inserts) завершён; принята локальная сборка 0.1k, включая плавную вертикальную прокрутку и Space Play/Stop.
Принятый код: 2393797e41f093219f957e7f1a67271b7579917a. Локальные configure/build, 77/77 CTest и GUI smoke прошли ранее; для отметки приёмки проверки не повторялись.
Полный Stage 3 / #23 остаётся открытым. Следующий подэтап — 3b VST3; его реализация ещё не начата. #16 остаётся отложенным.
Код и сборки остаются локальными; GitHub только issues и документация, без code push/PR/merge/Actions.


## 0.1k / Stage 3a готова локально — 2026-10-04
Stage 3 разделён по запросу пользователя: 3a Native Inserts; 3b VST3;
3c Cab IR; 3d Amp/Preamp/model foundation; 3e Processing Reliability.
Отдельная локальная сборка и приёмка для каждого подэтапа; следующий ещё не начат.
0.1k: insert chains на дорожках/шинах/Master, Gain/High-pass/Low-pass/one-band EQ,
add/remove/reorder/bypass, параметры, Undo/Redo и сохранение. Plain wheel: 32 logical
pixels/notch с fractional deltas; Space: Play/Stop с возвратом к старту, без autorepeat.
Core v8 читает v1–v7. Изменение эффектов после Pause/Stop; запись остаётся raw.
Local cached offline-dependency ASIO configure/build, 77/77 CTest, expanded GUI smoke
и UI/editor previews пройдены. Пользовательская/physical приёмка 0.1k ожидается.
Код 2393797e41f093219f957e7f1a67271b7579917a; ветка mrs/0.1k-native-inserts-local,
пакет MR-Studio-0.1k-ASIO-Windows-local. Последняя принятая — 0.1j; #22 завершён.
План: [Stage 3](docs/MRS_STAGE_3_PLAN.md), [inserts](docs/NATIVE_INSERTS.md),
[приёмка](docs/MRS_STAGE_3A_CHECKLIST.md). #23 открыт; #16 matrix остаётся отложенной.
Код/пакеты локально; GitHub только issues/docs. Без code push/new PR/merge/Actions.
Исторические статусы ниже заменены этой записью.

## 0.1j принята; Mixer / Routing завершён — 2026-10-04
Пользователь: «Все проверил, все работает!». Последняя принятая локальная сборка —
0.1j / Stage 2e; код 43fe2b510f14e77453a5e5f68e0659621f0c80bf,
пакет MR-Studio-0.1j-ASIO-Windows-local. Mono/stereo inputs, simultaneous multitrack
capture, R/I на дорожках, L/R meters и Stop return приняты. Ранее прошли локальные
configure/build, 73/73 CTest и GUI smoke; при отметке приёмки проверки не повторялись.
Весь #22 Mixer / Routing завершён. #16 длительная matrix остаётся отложенной.
Следующий этап — #23 Plugins / Native DSP. Предлагаемый первый подэтап 0.1k:
insert chains на дорожках/шинах/Master, add/remove/reorder/bypass, native utility
gain/filter/EQ, параметры, Undo и сохранение. Далее VST3 workflow, IR и amp foundation.
Реализация следующего подэтапа ещё не начата. Старые pending/not-started статусы ниже
заменены этой приёмкой. Код/пакеты локально; GitHub issues/docs only;
приёмка не разрешает code push/new PR/merge. GitHub Actions не используются.

## 0.1j / Stage 2e — локальная реализация готова, 2026-10-04
Mono/stereo inputs на дорожках, одновременная запись нескольких дорожек,
независимые R (Arm) / I (Monitor) рядом с M/S, отдельные L/R шкалы stereo.
Stop возвращает к старту Play/Record; Pause сохраняет текущую позицию.
Core v7 читает v1–v6; input/Monitor сохраняются и используют Undo; Arm — session-only.
Offline-dependency Windows x64 ASIO configure/build, **73/73 CTest** и expanded GUI
smoke пройдены, UI preview проверен. Пользовательская/physical ASIO приёмка ожидается.
Последняя принятая сборка — 0.1i upd1; исторические статусы ниже заменены этой записью.
Пакет MR-Studio-0.1j-ASIO-Windows-local в Builds; ветка mrs/0.1j-multi-input-local.
См. [inputs](docs/INPUT_ROUTING.md) и [checklist](docs/MRS_STAGE_2E_CHECKLIST.md).
#22 открыт до приёмки; затем #23 Plugins / Native DSP. #16 matrix остаётся отложенной.
Код/сборки локально; GitHub только issues/docs. Без code push/new PR/merge/Actions.

Локальная **0.1i upd1** готова: компоновка по пользовательскому референсу Studio Pro,
вертикальный зум дорожек и четыре сочетания колеса мыши. Только работающие controls.
**69/69 CTest**, GUI smoke и visual review пройдены; **0.1i upd1 принята** пользователем 2026-10-04.
Базовая **0.1i принята** («Все работает!»). См. [изменения](docs/MRS_0_1I_UPD1.md).
Код/сборки локально, GitHub issues/docs only; исторические статусы ниже заменены этой записью.

Локальная **0.1i** готова к проверке: physical outputs на дорожках/шинах/Master,
multi-output и профили аудиоустройства. Configure/build, **69/69 CTest** и GUI smoke
пройдены локально с кешем зависимостей. Физическая ASIO-приёмка ожидается.
Последняя принятая версия — **0.1h fix3**. Код и пакеты остаются локальными;
GitHub используется только для issues/docs, без Actions и автоматического push/merge.
См. [контракты](docs/HARDWARE_OUTPUTS.md) и [проверку 0.1i](docs/MRS_STAGE_2D_CHECKLIST.md).
Исторические статусы ниже не заменяют эту запись.

Последняя принятая локальная сборка: **0.1h fix3**, 2026-10-04.
Пользователь подтвердил отсутствие мигания и работу всех функций Stage 2c.
Hardware output routing/multi-output/device profiles реализованы локально в 0.1i; приёмка ожидается.

Локальная **0.1h fix3** устраняет постоянное мигание Audio settings. Fix2 проверен
пользователем; проверка fix3 ожидается. См. [MRS_0_1H_FIX3.md](docs/MRS_0_1H_FIX3.md).

Локальная **0.1h fix1** исправляет мигание кнопок при движении фейдеров и открытии
меню. Функции 0.1h проверены пользователем; проверка исправления ожидается.
Подробности: [MRS_0_1H_FIX1.md](docs/MRS_0_1H_FIX1.md). Код остаётся локальным.

## Локальная 0.1h / MRS Stage 2c готова к проверке — 2026-10-04
Посылы/возвраты (до 8 на канал, pre/post-fader, уровни, nested buses, cycle rejection),
вертикальные фейдеры/стереометры, Mix поверх аранжировки, мини-панели дорожек
(горизонтальные gain/meters, колесо pan, mono input selection), Files → Open recent project.
Core snapshot v5 читает v1–v4; desktop config v3 читает v1/v2. Undo и сохранение общие.
Локальные Windows x64 ASIO Release configure/build, 65/65 CTest и расширенный GUI smoke
пройдены; зависимости использованы из кеша, GitHub Actions не использовались.
Контракты: SENDS.md; пользовательская проверка: MRS_STAGE_2C_CHECKLIST.md.
Пользовательская/физическая ASIO приёмка 0.1h ожидается; последняя принятая версия — 0.1g.
Ветка mrs/0.1h-sends-ui-local остаётся локальной. На GitHub — только issues/docs;
код push/PR/merge исключительно по отдельной просьбе. #22 и отложенный #16 остаются открыты.
Далее в #22: hardware multi-output и device profiles. Multi-input recording — будущая работа.


## Принята локальная 0.1g / MRS Stage 2b — Buses / Subgroups

Принятая пользователем локальная сборка — 0.1g: микшер, portable folders, bus channels,
track/bus outputs, cycle-safe routing, bus controls/meters, Undo и persistence.
PR #48/#47 интегрированы в main; PR #49 не слит. Разработка и сборки выполняются локально.
На GitHub обновляются только issues и документация; публикация кода и слияния — по просьбе пользователя.
[Микшер](docs/MIXER.md) · [Шины](docs/BUSES.md) · [Приёмка Windows](docs/MRS_STAGE_2B_CHECKLIST.md).

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
Это общий C++20 backend: Project Model, Command/Undo, Transport API,
tempo/meter contracts, подписки, fixtures и versioned snapshot.

- [Core contracts](docs/CORE_CONTRACTS.md) — API, ownership, threading и границы этапа.
- [Проверка Windows-сборки](docs/SHARED_STAGE_0_CHECKLIST.md) — консольный checker.
- [Audio core](docs/AUDIO_CORE.md) — realtime/device contracts и границы прототипа.
- [Проверка ASIO](docs/SHARED_STAGE_1_CHECKLIST.md) — guided Windows tester и benchmark.
- [Статус реализации](docs/IMPLEMENTATION_STATUS.md) — продолжение работы.

**SHARED Stage 2 / issue #17** принят пользователем, PR #38 интегрирован: Musical Timeline, Chord/Arranger lanes,
общие context/navigation services, snapshot v2 с чтением v1.
[Musical contracts](docs/MUSICAL_TIMELINE.md) · [Windows checker](docs/SHARED_STAGE_2_CHECKLIST.md).

**SHARED Stage 3 / issue #18** принят пользователем, PR #39 слит: общая MIDI/processor инфраструктура, native gain,
подготовленный graph и patch-state. VST3 host и hardware MIDI — будущие adapters.
[Contracts](docs/MIDI_PROCESSOR_GRAPH.md) · [Windows checker](docs/SHARED_STAGE_3_CHECKLIST.md).

### SHARED Stage 4 — persistence/state/recovery
Stage 4 was accepted and merged through PR #40. It adds one versioned project
archive for all workspaces, show references, legacy migration, preserved unknown
chunks, safe save/backup, background autosave and stopped recovery.
See [persistence contracts](docs/PERSISTENCE.md) and
[Windows acceptance checklist](docs/SHARED_STAGE_4_CHECKLIST.md).
Build normally, then run mrs_persistence_check (no audio hardware required).

MRS Stage 0 / #20 принят пользователем 2026-10-03, PR #41 слит в main.
Подтверждены воспроизведение WAV и восстановление ASIO после смены файла и перезапуска.
Следующий этап — MRS Stage 1 / #21 Audio Arrangement.
Полный ASIO performance gate (#16) остаётся pending и не блокирует разработку.

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


## MRS Stage 0 — DAW Foundation
Native C++20/Win32 desktop shell with Arrange/Edit/Mix navigation, shared
ProjectStore/EngineTransport/GraphStore, timeline/playhead, Open WAV/project,
Save/Undo and vendor ASIO settings. Starts in an explicit silent offline mode.
[Desktop contracts/stack](docs/DESKTOP_FOUNDATION.md) ·
[Windows acceptance](docs/MRS_STAGE_0_CHECKLIST.md).
ASIO Actions artifact: MR-Studio-MRS-Stage-0-ASIO-Windows; run MoonRiverStudio.exe.
Edit/Mix are initial read-only views; detailed editors arrive in their own stages.

## MRS Stage 1a — Audio Arrangement
First slice of #21: New project, audio track create/delete/reorder, batch WAV import
into the current project, per-channel waveforms and zoom/scroll. Uses the same
shared ProjectStore/Undo/AudioEngine and retained ASIO connection for stopped edits.
[Contracts](docs/AUDIO_ARRANGEMENT.md) · [Windows checklist](docs/MRS_STAGE_1A_CHECKLIST.md).
Stage 1a accepted by user on 2026-10-03; PR #42 merged into main.
Stage 1b (0.1c) accepted by user on 2026-10-03; PR #44 merged into main.
Stage 1c (0.1d upd1 fix1) accepted; recording is Stage 1d / 0.1e.

## Версии сборок
Правила пользователя: 0.1b, 0.1c и далее; upd1/upd2 для небольших обновлений,
fix1/fix2 для ошибок. Stage IDs сохраняют структуру плана.
[Правила и текущее соответствие](docs/VERSIONING.md).
**0.1b fix1** принят пользователем и интегрирован через PR #43.
Принятый подэтап: **0.1c / MRS Stage 1b** — выбор, перемещение, обрезка и разделение клипов,
удаление отдельного клипа и общий Undo/Redo. UI drag preview, snap 1/16, сохранение позиции Pause.
[Windows checklist](docs/MRS_STAGE_1B_CHECKLIST.md). Пользователь подтвердил все функции 2026-10-03; PR #44 слит в main.
Принят **0.1d upd1 fix1 / MRS Stage 1c** — disk read-ahead, UI follow-up и моно L/R.

## MRS Stage 1c — 0.1d disk read-ahead
Long WAVs use bounded background disk buffers in the same SHARED AudioEngine.
Per-voice offsets, seek/loop priming and separate disk underrun/error counters;
waveform peaks build from bounded blocks with cancellation. Small WAVs preload.
[Contracts](docs/AUDIO_ARRANGEMENT.md) · [Windows checklist](docs/MRS_STAGE_1C_CHECKLIST.md).
PR #45; базовая 0.1d проверена пользователем. upd1 интерфейса и fix1 моно также приняты, PR #45 слит в main. Whole #21 stays open for recording/save-load acceptance.

## 0.1d upd1 fix1 — UI follow-up
Neutral gray background/buttons; thin Files menu for project/WAV actions;
Arrange/Edit/Mix navigation without Live button. Mono routes to the selected main pair.
[Acceptance checklist](docs/MRS_STAGE_1C_UPD1_CHECKLIST.md). All six CI jobs passed; user accepted UI/mono follow-up on 2026-10-03; PR #45 merged.
Live is a separate show mode with .mrlive documents referencing .mrsproject songs,
using the same SHARED Core/Engine. Its file commands/screen belong to LIVE stages.

Следующий подэтап: **Stage 1d / 0.1e** — запись, мониторинг и итоговая приёмка save/load.
Stage 1c завершён; общий #21 остаётся открытым. 0.1e принят пользователем; PR #46 слит в main.

## MRS Stage 1d — 0.1e recording/monitor
Один вход ASIO, одна вооружённая audio track, raw mono float32 WAV через bounded
фоновой писатель. Record/Arm track/Monitor в общем транспорте; завершение дубля,
Undo/Redo и сохранение/открытие проекта с внешним WAV. Существующие клипы слышны
во время записи; файл содержит только вход. При dropout сохраняется валидная часть
с предупреждением. Без loop recording и компенсации задержки в этой версии.
[Контракты](docs/RECORDING.md) · [Windows checklist](docs/MRS_STAGE_1D_CHECKLIST.md).
Принятая ASIO сборка: MR-Studio-0.1e-ASIO-Windows. Пользователь подтвердил работу;
Stage 1 / #21 завершён. Далее Mixer / Routing #22.


## Приёмка 0.1e — 2026-10-03
Пользователь подтвердил: «Все работает, записал на несколько каналов».
MRS Stage 1d / 0.1e принят; PR #46 слит в main. Подтверждение относится к текущему foundation workflow;
одновременная запись нескольких ASIO inputs не добавлялась (один выбранный input
и одна вооружённая дорожка за дубль). Весь MRS Stage 1 / #21 принят.
Проверенный code head: f78e0123cc7651f3418f0a42f8bc9fce861dfed7.
PR CI: все шесть jobs пройдены (56/56 Linux/ASIO, 57/57 Windows offline).
Следующая работа после паузы: MRS Stage 2 / #22 Mixer / Routing; реализация не начата.
Пользователь попросил продолжить 2026-10-04 по Asia/Krasnoyarsk. 


## 0.1e upd1 fix1 — folders and portable projects
Windows content root: Documents/MR Studio with Projects and Lives.
Each project owns <name>/<name>.mrsproject, Media and Mixdown. Imported WAVs
copy into Media; recordings write there. Archives use relative Media/... references.
Save As copies Media/Mixdown and retains Undo/device continuity; original project
and source files remain intact. Move the whole folder, then reopen its .mrsproject.
[Contracts](docs/PROJECT_FOLDERS.md) · [Windows checklist](docs/MRS_PROJECT_FOLDERS_CHECKLIST.md).
PR #47, acceptance pending. Lives prepares storage for future .mrlive workflow;
Mixdown prepares storage for later export. Mixer/Routing #22 follows after upd1.


## Included fix1: concurrent seek read-head protection
Seek priming previously published the future target as the current read head before
its queued transport command reached the callback. A worker could then evict the
still-playing page in that interval. Prime sets the initial head only once,
keeps the future target separately warm, and lets the callback move the active head.
The worker skips protected pages before claiming ownership. A control/worker-only
atomic gate serializes the ready snapshot with victim selection, closing the stale
snapshot window without waiting, locking or I/O on the audio callback.
Prepared UI seeks are coalesced and applied only after the callback pins all needed
target pages. If a later prime displaced an earlier target, the callback continues
the current position and retries on its next block; worker retry pages remain
protected. This closes the queued-command handoff race without blocking RT.
The concurrent seek exact-sample/zero-underrun/zero-RT-allocation regression is
repeated eight times in every Debug/Release CI job for this fix.
