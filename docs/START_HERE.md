# Moon River Studio — START HERE

**Harma Waves (new planned direction, no MRS implementation yet):** [native architecture](HARMA_WAVES_NATIVE.md) · [roadmap and Smart-Voicing source mapping](HARMA_WAVES_ROADMAP.md) · [Codex handoff / first #116 slice](HARMA_WAVES_CODEX_HANDOFF.md) · [parent issue #115](https://github.com/vladleng/MR-Studio/issues/115). This plan does not override the active local development status below.

> Current development context (2026-10-08): [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).
> Start with that file and the repository AGENTS.md. It records the current
> 0.2g delivery, MIDI acceptance and deferred external hardware validation.
> Entries below are historical snapshots; their old “pending/not started”
> labels do not override the current context.

Selected UI direction: [JUCE migration](JUCE_MIGRATION.md), Windows-only active scope.
Plan recorded; integration not started. Preferred working UI remains 0.1m upd1 fix1.

Future idea (not started, separate from the current build): [Acoustic space prototype](ACOUSTIC_SPACE_PROTOTYPE.md).

## 0.1m / Stage 3c — local build ready, 2026-10-04

Cab IR: mono/stereo WAV import, embedded kernel, live Mix/Gain/low-high cuts/polarity,
Neutral/Warm/Bright control presets, zero additional algorithmic convolution latency.
Right VST3 tab: vendor folders, drag/drop to mixer tracks/buses/Master and single-click
insert native editor. Native insert workflow retained; structure edits after Pause/Stop.
Core v10 reads v1–v9; keep a project backup before saving. Code
`8c805259541c39f2eee59ebba2c38e87cdeae706`, local branch `mrs/0.1m-cab-ir-browser-local`.
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
План: [Stage 3](MRS_STAGE_3_PLAN.md), [inserts](NATIVE_INSERTS.md),
[приёмка](MRS_STAGE_3A_CHECKLIST.md). #23 открыт; #16 matrix остаётся отложенной.
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
См. [inputs](INPUT_ROUTING.md) и [checklist](MRS_STAGE_2E_CHECKLIST.md).
#22 открыт до приёмки; затем #23 Plugins / Native DSP. #16 matrix остаётся отложенной.
Код/сборки локально; GitHub только issues/docs. Без code push/new PR/merge/Actions.

## 0.1i upd1 принята — 2026-10-04
Пользователь: «Все работает». Последняя принятая локальная сборка — 0.1i upd1,
код 1f9a89568e50a83bb44d6f4ec3e7df8aac9032a8. Приёмка интерфейса/зумирования закрыта.
В #22 остался broader input routing: сейчас один mono input/одна armed track за дубль.
Предлагаемая следующая часть — mono/stereo input routing и simultaneous multitrack
capture; реализация ещё не начата. После Mixer/Routing следующий roadmap stage —
#23 Plugins / Native DSP. #22 открыт, #16 matrix отложена. GitHub issues/docs only;
код и сборки локально. Исторические pending-user записи ниже заменены этой.

## Локальная 0.1i upd1 готова к проверке — 2026-10-04
Пользователь принял 0.1i: «Все работает!» и запросил интерфейс по скриншоту Studio Pro.
Upd1: компактная верхняя панель, широкие track headers, аранжировка сверху,
нижний микшер/Master справа, транспорт снизу, серые панели/синие акценты.
Только рабочие команды; header M/S используют общий mixer Undo.
Ctrl+Shift+wheel — horizontal zoom, Ctrl+wheel — track height,
Shift+wheel — horizontal scroll, wheel — vertical scroll. Над микшером
Shift+wheel прокручивает каналы; обычное колесо прокручивает дорожки аранжировки.
Local ASIO Release build, CTest 69/69, expanded GUI smoke и visual preview пройдены.
Пользовательская приёмка upd1 ожидается. Последняя принятая локальная — 0.1i.
См. [MRS_0_1I_UPD1.md](MRS_0_1I_UPD1.md). Пакет MR-Studio-0.1i-upd1-ASIO-Windows-local;
ветка mrs/0.1i-upd1-layout-local. Старый пакет 0.1i сохранён.
Код и пакеты локально; GitHub только issues/docs, без code push/new PR/merge/Actions.
Исторические pending записи ниже заменены этой приёмкой базовой 0.1i.
Полный #22 остаётся открыт; detailed physical/performance matrix #16 не отмечена пройденной.

## Локальная 0.1i / Stage 2d готова к проверке — 2026-10-04
Аппаратные mono/stereo outputs для дорожек, шин и Master; несколько выходов
на общем engine, независимые Main/Monitor/Click/Cue через именованные шины.
Профили устройства: имя, rate/buffer, outputs, mono monitor input и channel labels;
Save/Load/Delete в Audio settings, Load заполняет поля, Connect применяет.
Missing outputs проверяются до закрытия соединения; offline clock сохраняет routes.
Core v6 читает v1–v5; config v4 читает v1–v3. Общие commands/Undo/persistence.
Локальные Windows x64 ASIO Release configure/build с кешем, 69/69 CTest и GUI smoke
пройдены, включая profiles и прежние flicker regressions. Physical ASIO/user
acceptance ожидается; последняя принятая сборка — 0.1h fix3. Полный #22 открыт.
Пакет MR-Studio-0.1i-ASIO-Windows-local в Builds, ветка mrs/0.1i-hardware-profiles-local.
Код/сборки локально; GitHub issues/docs only. Нет code push/new PR/merge/Actions.
Контракты: [HARDWARE_OUTPUTS.md](HARDWARE_OUTPUTS.md);
приёмка: [MRS_STAGE_2D_CHECKLIST.md](MRS_STAGE_2D_CHECKLIST.md).
Click generator/show cues, simultaneous multi-input recording и #16 benchmark
не отмечаются выполненными. Исторические pending записи ниже имеют меньший приоритет.

## Принята локальная 0.1h fix3 — 2026-10-04
Пользователь подтвердил: «теперь ничего не мигает. Все работает».
Stage 2c: sends/returns, recent projects, mixer overlay, vertical faders/meters,
track mini panels/input selection и исправления мигания приняты в локальной сборке.
Принятый код: 0911855dce7d179e03df33f05a120684e4637d37.
Следующая часть #22: hardware output routing, multi-output interfaces и device profiles.
Полный #22 остаётся открыт. Код/сборки локально; GitHub issues/docs only,
code push/PR/merge по отдельной просьбе. Исторические pending записи ниже заменены этой.

## Локальная 0.1h fix3 — Audio settings
Пользователь подтвердил устранение release-мигания в fix2. Fix3 устраняет постоянную
полную перерисовку Audio settings: double buffer/WS_CLIPCHILDREN, обновление только
изменённого статуса не чаще 250 ms. GUI regression воспроизведён до исправления и
прошёл после; пользовательская проверка fix3 ожидается. См. MRS_0_1H_FIX3.md.
Код и сборка остаются локальными; GitHub — только issues/docs.

## Локальная 0.1h fix2 — мигание при отпускании фейдера
Fix1 устранён во время движения; пользователь сообщил остаточное мигание при release.
Fix2 заменяет полный refresh на обновление Undo/Redo, dirty title и canvas;
неизменённые подписи/доступность кнопок и меню повторно не задаются.
Release regression воспроизведён до исправления и прошёл после. Проверка пользователем
ожидается. См. MRS_0_1H_FIX2.md. Код остаётся локальным, GitHub — только issues/docs.

## Локальная 0.1h fix1 — мигание кнопок — 2026-10-04
Пользователь проверил функции 0.1h: «Все работает», но сообщил о мигании кнопок
при движении фейдеров/открытии меню. Fix1 переносит MoveWindow/ShowWindow/EnableWindow
из paint в синхронизацию состояния и layout, применяет изменения только при необходимости.
GUI regression воспроизвёл лишние native layout events до исправления и прошёл после.
Проверка fix1 пользователем ожидается. См. MRS_0_1H_FIX1.md. Код остаётся локальным;
GitHub — только issues/docs, без Actions, push кода, новых PR и слияния.

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


## Принята локальная 0.1g / MRS Stage 2b / #22 — 2026-10-04
Пользователь принял локальную 0.1f: «Я проверил, всё работает отлично».
PR #48 слит в main; включённый PR #47 также отмечен GitHub как merged.
0.1f — принятая базовая версия с gain/pan, mute/solo, meters, master и portable folders.
0.1g: локальные Windows ASIO Release configure/build, 62/62 tests и GUI smoke пройдены.
Проверены вложенные шины, Solo/Mute, cycle rejection, Undo, сохранение, raw recording,
zero RT allocations и GUI bus fader/minimum DPI bounds. Пользователь подтвердил: «Все работает!».
Для принятой 0.1f фактически пройдены 59/59 ASIO tests + GUI smoke и 60/60 offline tests.
Пользователь подтвердил следующий подэтап: шины/подгруппы, выходы track/bus,
защита от циклов, bus controls/meters, Undo и persistence. Контракты: BUSES.md.
Вся разработка, configure/build/tests и пакеты выполняются локально, без GitHub Actions.
На GitHub обновляются только issues и документация. Код разрабатывается локально.
Отправка изменений кода, создание PR и слияние — только по отдельной просьбе пользователя.
Принятие локальной сборки не разрешает автоматическое слияние; открытый PR #49 не сливается.
Код main пока соответствует 0.1f; принятый локальный код 0.1g — 99d9b7be5537281a956bd8fadddaa95071d381f4.
Sends/returns реализованы локально в 0.1h; далее hardware multi-output и profiles.
Полный #22 остаётся открыт; 0.1h ожидает приёмку, hardware multi-output и profiles впереди.
Исторические статусы pending/0.1e ниже заменены этой актуальной записью.

## Актуальное продолжение — 2026-10-03
Stage 0 принят, PR #36 слит. Stage 1/2 интегрированы через PR #37/#38 в main.
Базовые ASIO WAV/monitoring при 48k/128 проверены пользователем. Оставшиеся
hardware/performance тесты отложены пользователем и не блокируют разработку;
gate остаётся pending в #16.
Stage 2 #17 принят. Snapshot schema 2 с чтением v1.
Stage 3 #18 принят после Windows-checker, PR #39 слит, issue закрыт.
Stage 4 #19 принят пользователем, PR #40 слит в main, issue закрыт.
MRS Stage 0 #20 принят пользователем 2026-10-03; PR #41 слит в main.
Пользователь подтвердил фикс восстановления ASIO после открытия другого WAV и перезапуска.
MRS Stage 1a #21 принят пользователем 2026-10-03; PR #42 слит в main.
0.1b fix1 принят пользователем 2026-10-03; PR #43 слит в main.
MRS Stage 1b / 0.1c принят пользователем 2026-10-03: все функции работают; PR #44 слит в main.
MRS Stage 1c / 0.1d upd1 fix1 принят пользователем 2026-10-03; PR #45 слит в main.
Приняты длинные WAV/disk read-ahead, серый интерфейс/меню Files, удаление кнопки Live
и центрирование моно. Все шесть CI jobs пройдены.
MRS Stage 1d / 0.1e принят пользователем 2026-10-03: «Все работает, записал на несколько каналов».
Весь MRS Stage 1 / Audio Arrangement #21 завершён. PR #46 принят и слит в main.
Последняя принятая сборка: 0.1e. Code head f78e0123cc7651f3418f0a42f8bc9fce861dfed7.
Контракты: docs/RECORDING.md; приёмка: docs/MRS_STAGE_1D_CHECKLIST.md.
Предыдущая небольшая доработка: 0.1e upd1 fix1, открытый PR #47; включена в 0.1f.
MR Studio/Projects/<имя>/<имя>.mrsproject + Media/Mixdown; MR Studio/Lives для будущих show.
Импорт и запись принадлежат Media; relative media refs, перенос папки и Save As с копиями.
Контракты: docs/PROJECT_FOLDERS.md; приёмка: docs/MRS_PROJECT_FOLDERS_CHECKLIST.md.
upd1 fix1 ещё не принят пользователем. Последняя принятая версия: 0.1e.
Сейчас: MRS Stage 2a / #22 / 0.1f — track gain/pan, mute/solo, meters и master bus
на том же SHARED engine. PR #48; автоматические проверки и физическая приёмка.
Запись 0.1e: один выбранный mono ASIO input и одна вооружённая дорожка за дубль;
последовательные дубли на разных дорожках поддерживаются. Multi-input recording остаётся будущей работой.
Правила версий: docs/VERSIONING.md. Длительные performance проверки #16 остаются
отложенными и nonblocking. Продолжить по запросу пользователя 2026-10-04 (Asia/Krasnoyarsk).



Этот документ — короткая точка входа для нового чата, разработчика или агента, который подключается к проекту без контекста предыдущих обсуждений.

## 1. Что мы строим

**Moon River Studio** (`MRS`, рабочее сокращение `MR Studio`) — собственная performance-first DAW.

Рабочее название не является окончательным и может быть изменено ближе к зрелой стадии проекта без изменения архитектуры.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Fender Studio Pro.

По уточнению пользователя 2026-10-03: Live — отдельный show-режим внутри MRS
с документом `.mrlive`, ссылающимся на `.mrsproject`. Это не кнопка workspace
рядом с Arrange/Edit/Mix. Show-экран и Files New/Open Live появятся в LIVE этапах.

Live Mode:

- не является отдельным приложением;
- не является отдельной DAW;
- не имеет собственной продуктовой версии;
- не имеет отдельного Audio Engine;
- не имеет отдельного Transport;
- не имеет отдельного MIDI Engine;
- не имеет отдельного plugin host;
- не хранит отдельную копию проекта для обычного workflow.

Главная формула:

```text
Moon River Studio
│
├── Arrange
├── Edit
├── Mix
└── Live Mode
        │
        └── тот же Project Model
            тот же Transport
            тот же Audio Engine
            тот же MIDI backend
            тот же Plugin Graph
```

## 2. Главное архитектурное правило

В Moon River Studio существует **одно общее ядро / SHARED Core**.

```text
                    SHARED CORE
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
  Arrange / Edit        Mix            Live Mode
```

В SHARED Core входят:

- Project Model;
- Command / Undo;
- Transport;
- Audio Engine / ASIO;
- mixer/routing graph;
- MIDI model/backend;
- plugin/native processor graph;
- Musical Timeline;
- Tempo/Meter Map;
- Chord Track;
- Arranger Track;
- Markers/Cues;
- serialization/versioning;
- persistence/recovery foundation.

**Нельзя создавать отдельный backend специально для Live Mode**, даже если это кажется быстрее для конкретной задачи.

Если backend-функция нужна нескольким workspace, она относится к SHARED Core.

## 3. Performance-first

Стабильность аудио — blocking requirement проекта.

На Windows основной professional/live path должен использовать родной vendor ASIO driver аудиоинтерфейса.

Базовые требования:

- realtime audio thread отделён от UI/network/AI/file I/O;
- no blocking I/O и тяжёлых allocations в audio callback;
- playback не зависит от UI responsiveness;
- plugin latency учитывается;
- предусмотрены preload/read-ahead;
- low-latency live path не должен ломаться из-за тяжёлого playback/mix path;
- ведутся xrun/dropout/callback metrics;
- Fender Studio Pro используется как performance benchmark на одинаковом hardware/setup.

Перед активным наращиванием функций Audio Engine должен пройти performance gate.

Подробнее: `AUDIO_ENGINE.md`.

## 4. Как организована разработка

Используются три **issue track одного приложения**:

```text
[MRS]    функции DAW и production workspaces
[SHARED] общий Core / Engine
[LIVE]   встроенный Live Mode
```

Это не три продукта.

Версионируется **Moon River Studio**.

Live Mode имеет только Stage readiness:

```text
Moon River Studio 0.6
├── Arrange / Mix / MIDI
├── Musical Structure
└── Live Mode: Stage 1 complete
```

Live Mode может разрабатываться параллельно на mock/fixture implementations SHARED interfaces. После появления real backend mock должен заменяться без изменения архитектуры Live UI.

## 5. GitHub Issues — карта проекта

Главные tracking issues:

- `#1` — общий master roadmap Moon River Studio;
- `#12` — MRS roadmap;
- `#13` — SHARED Core / Engine roadmap;
- `#14` — Live Mode roadmap;
- `#3` — optional Fender Studio Pro compatibility/import.

SHARED Core:

- `#15` — Core contracts: Project Model, Command/Undo, Transport API;
- `#16` — Audio Engine / ASIO / performance gate;
- `#17` — Musical Timeline: tempo, meter, chords, arranger, markers;
- `#18` — MIDI / Plugin Graph;
- `#19` — Persistence / State / Recovery.

MRS stages:

- `#20` — DAW Foundation;
- `#21` — Audio Arrangement;
- `#22` — Mixer / Routing;
- `#23` — Plugins / Native DSP;
- `#24` — MIDI;
- `#25` — Musical Structure;
- `#26` — AI Foundation;
- `#27` — Advanced DAW / Reliability.

Live Mode stages:

- `#28` — UX Foundation / moving Chord Track;
- `#29` — Real Project / Transport Integration;
- `#30` — Setlists / Show Workflow;
- `#31` — Playback / Click / Cue;
- `#32` — Live Inputs / Patches;
- `#33` — MIDI Automation / Hardware Control;
- `#34` — Remote / Mobile Companion;
- `#35` — Concert Reliability.

## 6. С чего начинать новому чату

Минимальный порядок чтения:

1. `docs/START_HERE.md` — этот файл;
2. `README.md` — краткий обзор всего продукта;
3. `docs/ARCHITECTURE.md` — архитектурные границы;
4. `docs/ROADMAP.md` — этапы разработки;
5. `docs/DEVELOPMENT_TRACKS.md` — правила параллельной работы;
6. открыть master issue `#1`;
7. открыть parent issue нужного track: `#12`, `#13` или `#14`;
8. открыть конкретный Stage issue, над которым продолжается работа.

Для audio/ASIO обязательно дополнительно прочитать:

- `docs/AUDIO_ENGINE.md`.

Для AI:

- `docs/AI_INTEGRATION.md`.

Для встроенного DSP/amp/cab:

- `docs/DSP_MODELING.md`.

Для Live Mode:

- `docs/UI_UX_CONCEPT.md`;
- `docs/LIVE_WORKFLOW.md`.

Для Studio Pro import/compatibility:

- `docs/STUDIO_PRO_INTEGRATION.md`;
- issue `#3`.

## 7. Что важно не перепутать

### Не создавать Moon River Live как отдельное приложение

Историческое имя репозитория — `Moon-River-Live`, но целевой продукт теперь Moon River Studio.

`Live Mode` — встроенный workspace.

### Не создавать отдельный Live Audio Engine

Live использует тот же engine, что Arrange/Edit/Mix.

### Не создавать отдельный Live project format для обычной работы

Live-specific данные хранятся как часть MRS Project Model либо как show/setlist state, ссылающийся на project IDs.

### Не делать Studio Pro обязательной зависимостью

Studio Pro теперь:

- UX/performance reference;
- benchmark;
- optional import/migration source.

Moon River Studio должна быть самодостаточной DAW.

### Не связывать AI с realtime thread

AI работает через Project Context API + Command/Tool API и никогда не вызывается из ASIO callback.

## 8. Live Mode — визуальный ориентир

Базовое направление уже выбрано:

- flat dark UI;
- без выпуклых/glossy элементов;
- высокая читаемость на ноутбуке;
- горизонтальная движущаяся Chord Track strip по принципу Show Page;
- current chord читается относительно playhead;
- previous/next chords остаются видимыми;
- sections/cues/setlist/transport постоянно доступны;
- performance safety важнее декоративности.

Live UI должен быть performance-oriented представлением того же открытого MRS project.

## 9. AI — архитектурная цель

Будущий AI/ChatGPT layer должен получать структурированное состояние DAW через API, а не управлять интерфейсом мышью.

Планируемые возможности:

- читать tracks/clips/MIDI/chords/sections/mixer/plugins;
- анализировать аранжировку;
- генерировать и редактировать MIDI;
- предлагать mixer/plugin changes;
- создавать markers/sections/clips;
- выполнять разрешённые commands с Undo/Redo;
- работать в режимах READ / SUGGEST / EDIT / AUTO.

Даже до реализации AI Project Model должен иметь stable IDs и нормальный Command API.

## 10. Native DSP / guitar processing

В долгосрочном плане MRS предусматривает:

- utility DSP;
- EQ/compressor/saturation;
- convolution/Cab IR;
- amp/preamp/pedal models;
- neural model player;
- собственные Moon River captures/models.

Factory Content и User Library должны быть лицензированно разделены.

## 11. Текущая стартовая логика разработки

Основной фундамент строится через SHARED + ранние MRS stages.

Критическая последовательность:

```text
#15 Core contracts
      │
      ├── #16 Audio Engine / ASIO
      ├── #17 Musical Timeline
      └── #20 MRS DAW Foundation
```

После фиксации нужных contracts Live UI может идти параллельно:

```text
#15 / #17 interfaces
        │
        └── #28 Live UX prototype on mocks
                    │
                    └── #29 real Core integration
```

Новые чаты не должны ждать завершения всей DAW, если текущую задачу можно безопасно вести через зафиксированный SHARED interface.

## 12. Как передать задачу следующему чату

Достаточный стартовый запрос:

```text
Ознакомься с docs/START_HERE.md, docs/ARCHITECTURE.md,
docs/ROADMAP.md и GitHub Issues #1, #12, #13, #14.

После этого открой issue #XX и продолжай работу над ним.
Не создавай отдельный engine или отдельное приложение для Live Mode.
```

Если работа идёт над Audio Engine, нужно добавить:

```text
Обязательно прочитай docs/AUDIO_ENGINE.md и соблюдай performance gate.
```

## 13. Source of truth

При противоречии старых обсуждений и текущего репозитория приоритет имеют:

1. актуальный Stage issue;
2. `docs/START_HERE.md`;
3. `docs/ARCHITECTURE.md`;
4. `docs/ROADMAP.md`;
5. специализированный документ соответствующей подсистемы.

Если обнаружено противоречие между актуальными документами, сначала исправить документацию и только затем продолжать реализацию.


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
