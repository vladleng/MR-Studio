# Moon River Studio — Roadmap

**Stage5/#25 завершён2026-10-13 — 9/9**, #87 accepted/closed completed.
Пользователь: «#87 пройдено, все сохраняется, undo/redo работает.». Все редакторы и #80 приняты ранее.
Последняя принятая сборка0.3h upd2/source803774d, schema21/archive1; package/manifest/
shortcut/source прежние. Использованы прежние full130/finalfocused3/persistence/GUI
результаты; новых build/tests/аппаратных измерений не заявляем. [Контекст](PROJECT_CONTEXT.md).
Следующий этап — только по выбору пользователя; AI Foundation пока backlog.

Актуальные состояния — [GitHub issues](https://github.com/vladleng/MR-Studio/issues) и
[PROJECT_CONTEXT](PROJECT_CONTEXT.md). Stage5/#25 завершён и принят на0.3h upd2; #79–#87 закрыты.
Следующее направление выбирает пользователь. MR Reverb завершён. Разделы ниже описывают общий план,
не доказывают приёмку и не назначают автоматически следующий этап.


## 1. Главный принцип

**Moon River Studio** (рабочее сокращение `MRS` / `MR Studio`) — единственная DAW и единственный продукт проекта.

**Live Mode** — встроенный Performance / Show режим Moon River Studio, по роли близкий к Show Page в Studio Pro. Это не отдельное приложение и не отдельная продуктовая версия.

Разработка организована тремя техническими потоками:

```text
MRS    = пользовательские DAW-функции Arrange / Edit / Mix
SHARED = единый Core / Engine всей DAW
LIVE   = встроенный Live Mode
```

При этом version line существует только у Moon River Studio:

```text
MRS 0.1 -> 0.2 -> 0.3 -> ...
```

Live Mode отслеживается через `LIVE Stage 0..7` и входит в соответствующие сборки MRS.

Главный tracking issue: #1.

---

# SHARED — единое ядро Moon River Studio

Parent: #13

SHARED не является отдельным продуктом. Это backend, которым одновременно пользуются Arrange/Edit/Mix и Live Mode.

## SHARED Stage 0 — Core Contracts — #15

- Project Model v1;
- stable IDs;
- Command / Undo;
- `ITransport`;
- sample/musical position;
- event/state API;
- serialization/versioning foundation;
- mocks/fixtures.

## SHARED Stage 1 — Audio Engine / ASIO — #16

- vendor ASIO drivers;
- device/sample-rate/buffer/channel management;
- realtime-safe callback;
- playback graph;
- low-latency input path;
- streaming/preload foundation;
- routing contracts;
- xrun/dropout metrics;
- performance benchmark against Studio Pro.

**Performance gate:** если Moon River Studio заметно уступает Studio Pro по live-stability на одинаковом hardware/driver/buffer setup, расширение тяжёлых функций не имеет приоритета над анализом и оптимизацией engine.

## SHARED Stage 2 — Musical Timeline — #17

- tempo/meter map;
- bar/beat/sample conversion;
- Chord Track model;
- Arranger Section model;
- markers/cues;
- current/next chord/section services;
- navigation contracts.

## SHARED Stage 3 — MIDI / Plugin Graph — #18

- MIDI event/device/routing model;
- processor/plugin graph;
- VST3 state model;
- latency reporting;
- native processor interface;
- preset/patch state;
- preload/warm hooks.

## SHARED Stage 4 — Persistence / State / Recovery — #19

- project serialization/migrations;
- mixer/plugin/MIDI state;
- live metadata inside project;
- show-level state references;
- autosave/recovery foundation;
- compatibility tests.

## SHARED Audio capability — Time Stretch / Pitch Shift — #56

Post-stage capability общего Audio Engine. Первый backend — **Signalsmith Stretch** (MIT), но Core остаётся backend-neutral.

Child issues:

- #57 — shared contract/state;
- #58 — pinned dependency + adapter + licensing;
- #59 — realtime playback/latency/RT safety;
- #60 — offline/HQ render/cache;
- #61 — clip controls/tempo mapping/persistence;
- #62 — quality/CPU/regression benchmark.

Полный план: `TIME_STRETCH.md`.

Это не часть Plugin/Native DSP #23 и не отдельный Live engine. Arrange/Edit/Mix/Live используют один SHARED stretch path.

---

### Core rule

```text
                  SHARED CORE
                       |
       +---------------+---------------+
       |               |               |
   Arrange/Edit        Mix          Live Mode
```

Никаких отдельных Audio Engine, Transport, MIDI Engine или Plugin Host для Live Mode.

---

# MRS — основная линия развития DAW

Parent: #12

## MRS Stage 0 -> MRS 0.1 — DAW Foundation — #20

- monorepo/application structure;
- desktop shell;
- Windows CI;
- logging/config/tests;
- technology spike;
- workspace navigation: Arrange / Edit / Mix / Live;
- SHARED Project Model/Transport connection;
- basic timeline/playhead;
- audio settings over SHARED Audio Engine;
- high-DPI foundation.

### Acceptance
MRS запускается как DAW shell, открывает project model, управляет общим Transport и воспроизводит SHARED audio prototype.

### Параллельный Live старт
После фиксации SHARED contracts #15/#17 можно параллельно начинать LIVE Stage 0 #28 на mock backend. Это UI/prototyping work, а не отдельная DAW.

---

## MRS Stage 1 -> MRS 0.2 — Audio Arrangement — #21

- audio tracks;
- WAV import;
- clips/events;
- non-destructive move/trim/split;
- waveform;
- zoom/scroll/selection;
- disk read-ahead;
- recording/monitor foundation;
- project save/load.

### Acceptance
Создать проект, импортировать и отредактировать audio clips и стабильно воспроизвести их через native ASIO.

---

## MRS Stage 2 -> MRS 0.3 — Mixer / Routing — #22

- gain/pan;
- mute/solo;
- meters;
- buses/subgroups;
- sends/returns;
- input/output routing;
- multi-output;
- routing/device profiles;
- mixer state persistence.

Routing graph принадлежит SHARED Core и позднее напрямую используется Live Mode.

---

## MRS Stage 3 -> MRS 0.4 — Plugins / Native DSP — #23

- VST3 scan/cache/load/bypass;
- plugin state;
- latency reporting;
- parameter access;
- insert-chain UI;
- native processor API;
- utility DSP;
- Cab IR convolution;
- preset/state model;
- neural model foundation;
- plugin isolation strategy.

Подробнее: `DSP_MODELING.md`.

---

## MRS Stage 4 -> MRS 0.2a–0.2h — MIDI / метроном — #24

Актуальное разбиение от 2026-10-06: [MRS_STAGE_4_PLAN](MRS_STAGE_4_PLAN.md).
4a реализован локально: [MIDI input/VST3](MIDI_LIVE_INPUT.md); Release/111 CTest
и packaged GUI smoke прошли. [Физическая приёмка](MRS_STAGE_4A_CHECKLIST.md)
ожидается; #63 и весь #24 открыты. 4b–4h ещё не реализованы.
4a / 0.2a — MIDI-вход и VST3-инструменты; 4b / 0.2b — клипы/playback;
4c / 0.2c — запись; 4d / 0.2d — piano roll; 4e / 0.2e — quantize/transpose;
4f / 0.2f — CC/Program Change; 4g / 0.2g — внешний MIDI;
4h / 0.2h — метроном и precount для записи,
затем итоговая совместная проверка Stage 4.
Доработки получают updN, исправления fixN в пределах базовой версии.
Прежний MRS 0.5 был ориентиром milestone; пользовательская схема выше имеет приоритет.

- MIDI devices;
- MIDI tracks/clips;
- notes/velocity/duration;
- CC/program data;
- playback/recording;
- piano roll;
- quantize/transpose;
- plugin/external routing;
- метроном и precount для audio/MIDI записи.

Тот же MIDI backend используется Live Mode для automation и hardware control.

---

## MRS Stage 5 -> MRS 0.6 — Musical Structure — #25

- Chord Track editor;
- Arranger Track editor;
- section colors/bounds;
- markers;
- tempo/meter editing;
- section/marker navigation;
- current/next musical context.

### Acceptance
Проект MRS сам содержит музыкальную структуру, необходимую Live Mode.

Рабочая спецификация #79–87, зависимости, чек-поинты и приёмка:
[STAGE_5_PLAN.md](STAGE_5_PLAN.md). Arranger первого объёма редактирует metadata
секций без переноса clips. Tempo/meter не растягивают WAV. #87 проверяется в
каждом feature slice; текущая Project schema 15.
MRS 0.6 выше — исторический ориентир roadmap, не назначение следующей сборки.

### Live integration point
Этап предоставляет shared Chord/Arranger/Transport context активного проекта для
будущего LIVE Stage 1 #29 без экспорта/копирования. Готовность этих данных не означает
готовность всего Live/show UI: .mrlive, setlist и performance workflow относятся
к LIVE stages и описаны в [LIVE_WORKFLOW.md](LIVE_WORKFLOW.md).

---

## MRS Stage 6 -> MRS 0.7 — AI Foundation — #26

- Project Context API;
- query API;
- DAW Tool / Command API;
- READ / SUGGEST / EDIT / AUTO permissions;
- preview/diff;
- Undo/Redo integration;
- MIDI generation/edit contract;
- project snapshot;
- optional OpenAI prototype;
- AI worker isolation from realtime audio.

Подробнее: `AI_INTEGRATION.md`.

---

## MRS Stage 7 -> MRS 0.8+ — Advanced DAW / Reliability — #27

- automation editor;
- advanced recording/takes/comping;
- advanced MIDI;
- project autosave/recovery;
- plugin isolation refinement;
- native DSP/neural expansion;
- compatibility matrix;
- installer/signing;
- long-session stress tests;
- performance regression suite.

---

## Future MRS Feature — ARA2 Host / #88 (не реализовано)

**Решение 2026-10-14:** реализовать ARA2 Host поверх существующих JUCE/VST3, Audio Clip/Timeline/Project и SHARED Engine для внешних вокальных/аудиоредакторов. Это будущая функция из [#78 FEATURES](https://github.com/vladleng/MR-Studio/issues/78), а не переоткрытие принятого VST3 Stage 3 или отдельный Live engine. Она не входит автоматически в версию сборки, заданную историческими MRS Stage N.

**Source of truth:** [docs/ARA2_HOSTING.md](ARA2_HOSTING.md), [#88](https://github.com/vladleng/MR-Studio/issues/88). Порядок задач: [#177](https://github.com/vladleng/MR-Studio/issues/177) SDK/JUCE host → [#178](https://github.com/vladleng/MR-Studio/issues/178) clip/timeline/ARA regions → [#179](https://github.com/vladleng/MR-Studio/issues/179) редактор/compatibility → [#180](https://github.com/vladleng/MR-Studio/issues/180) project state/Undo/recovery → [#181](https://github.com/vladleng/MR-Studio/issues/181) AI Tool API.

AI использует текущий #26 Foundation и управляет только разрешёнными host-level операциями; ARA2 не обещает стандартного доступа к внутренним pitch blobs. Отдельный native MR Pitch #73 не входит в этот план. **Все 5 задач backlog, без implementation/build/user acceptance.** Начинать первый slice лишь по отдельному запросу.

---

## Future MRS UI Infrastructure — MR UI Kit + Gallery / #189 (backlog, 2026-10-14)

[Parent #189](https://github.com/vladleng/MR-Studio/issues/189) в [#78 FEATURES](https://github.com/vladleng/MR-Studio/issues/78) — единый JUCE design system и controls для основной DAW/всех нативных плагинов, плюс **MR UI Gallery** для настройки крутилок/кнопок/фейдеров, палитры/типографики/density/состояний и проверки 100/125/150/200% preview. Базироваться на **существующих** `ui::Theme`/`Controls.h`, принятом [UI_REFERENCE_UPD9.md](UI_REFERENCE_UPD9.md), не открывать закрытые #145/#146.

[Архитектура](MR_UI_KIT.md), [Gallery UX](MR_UI_GALLERY.md), [roadmap](MR_UI_KIT_ROADMAP.md), [Codex P1](MR_UI_KIT_CODEX_HANDOFF.md). План: [#190](https://github.com/vladleng/MR-Studio/issues/190) Audit → [#191](https://github.com/vladleng/MR-Studio/issues/191) Design Tokens → [#192](https://github.com/vladleng/MR-Studio/issues/192) MR Controls → [#193](https://github.com/vladleng/MR-Studio/issues/193) live Gallery → [#194](https://github.com/vladleng/MR-Studio/issues/194) parameter bindings/Undo → [#195](https://github.com/vladleng/MR-Studio/issues/195) opt-in pilot/QA. **Статус: документация, без реализации**; не блокировать текущую MR Cab/прочие задачи, действующий UI не мигрировать без приёмки. Проектные/звук/DSP state не меняются при просмотре Gallery. Код/builds/tests только локально, GitHub docs/issues [skip ci].

---

## Future MRS Native Plugin — MR Rig / #197 (2026-10-15; planned)

**[#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197)** — отдельная перспективная native guitar sound environment над текущим #182 MR Cab WAV/Full Combo backend. Пользователь сначала выбирает **готовый звук с фотографией**, а не сырой WAV. Библиотеки **IR Models / NAM Models / My Rigs**; собственный IR import — дополнительная опция. GUI: **PRE FX слева → одна модель Rig Core + обложка в центре → POST FX справа**, один inspector, полный RigPreset с упорядоченными реальными педалями. IR Full Combo WAV — один linear convolver без второго кабинета; NAM Full Rig — neural без дополнительного Cab; NAM Amp Only — optional Cab. Не называть кабинетный IR моделью преампа.

**Статус на момент согласования:** #185 Cab Simple принят/закрыт; #196 Full Combo реализован локально как 0.4b upd2 и ожидает отдельной пользовательской приёмки. **Ни один из этапов Rig не реализован данным планом**, #182, #196 не закрывать, accepted GUI/source не заменять автоматически.

**Docs:** [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md), [MR_RIG_GUI.md](MR_RIG_GUI.md), [MR_RIG_LIBRARY_PRESETS.md](MR_RIG_LIBRARY_PRESETS.md), [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md), [MR_RIG_ROADMAP.md](MR_RIG_ROADMAP.md), [MR_RIG_CODEX_HANDOFF.md](MR_RIG_CODEX_HANDOFF.md).
**Stages (logical sub-issues):** [#198](https://github.com/vladleng/MR-Studio/issues/198) audit/migration → [#199](https://github.com/vladleng/MR-Studio/issues/199) library/cards → [#200](https://github.com/vladleng/MR-Studio/issues/200) GUI/RigPreset → [#201](https://github.com/vladleng/MR-Studio/issues/201) Pre/Post chain → [#202](https://github.com/vladleng/MR-Studio/issues/202) pedals → [#203](https://github.com/vladleng/MR-Studio/issues/203) NAM → [#204](https://github.com/vladleng/MR-Studio/issues/204) QA. Native GitHub Sub-issue attachment still needs manual linking. Codex starts **only with audit #198 on separate command**, no source code/GitHub Actions changed by this plan.

---

## Future MRS Native Plugin — MR Cab 2.0 / #182 (planned only, 2026-10-14)

**Новый этап [#182](https://github.com/vladleng/MR-Studio/issues/182) в категории [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111)**. Основа — **закрытая инфраструктура Cab IR [#23](https://github.com/vladleng/MR-Studio/issues/23)** и принятый MR Reverb [#128](https://github.com/vladleng/MR-Studio/issues/128); их повторно не открывать. Цель: универсальный каталог комплектов **WAV IR** с импортом фактической локальной Celestion, выбором заводских ready-mixes, compact Simple UI как MR Reverb, затем Advanced mic/phase/room, другие TONEX WAV/generic packs и модульные точки будущих расширений.

**Source of truth:** [MR_CAB_COLLECTIONS.md](MR_CAB_COLLECTIONS.md), [MR_CAB_ROADMAP.md](MR_CAB_ROADMAP.md), [MR_CAB_CODEX_HANDOFF.md](MR_CAB_CODEX_HANDOFF.md). Этапы: [#183](https://github.com/vladleng/MR-Studio/issues/183) audit/Celestion → [#184](https://github.com/vladleng/MR-Studio/issues/184) manifest/import → [#185](https://github.com/vladleng/MR-Studio/issues/185) Simple/presets → [#186](https://github.com/vladleng/MR-Studio/issues/186) Advanced → [#187](https://github.com/vladleng/MR-Studio/issues/187) TONEX WAV/generic → [#188](https://github.com/vladleng/MR-Studio/issues/188) modular compatibility/QA. Не реализовывать .txm/.txp, Amp DSP или NAM в этом этапе. Пользовательские IR не копировать и не публиковать. **Все этапы backlog, реализация начнётся только по запросу и после аудита локальной коллекции.**

---

# LIVE — встроенный Live Mode

Parent: #14

Live Mode не имеет отдельного version line. Его Stage показывает степень готовности performance-режима внутри текущей Moon River Studio build.

## LIVE Stage 0 — UX Foundation — #28

Можно вести параллельно после фиксации #15/#17 contracts.

- fullscreen/performance workspace;
- flat UI;
- setlist rail placeholder;
- moving horizontal Chord Track;
- current/next section;
- cue/notes/patch panel;
- bar/beat/time;
- transport controls;
- mock/fixture backend;
- laptop/high-DPI readability.

## LIVE Stage 1 — Real Project / Transport Integration — #29

- real Project Model;
- real Transport;
- real Chord/Arranger data;
- markers/cues;
- elapsed/remaining;
- section navigation/loop;
- no-export integration with open MRS project.

## LIVE Stage 2 — Setlists / Show Workflow — #30

- setlists referencing MRS projects;
- previous/current/next song;
- preload next project;
- start/end policies;
- show notes/state;
- preflight;
- recovery;
- safe project switching.

## LIVE Stage 3 — Playback / Click / Cue — #31

Uses SHARED Audio Engine:

- stems/multitrack where needed;
- sample-locked playback;
- click/cue;
- dedicated outputs;
- live playback gain;
- non-destructive Trim Start/End;
- next-song preload;
- transition stress tests.

## LIVE Stage 4 — Live Inputs / Patches — #32

Uses SHARED Audio/Plugin Graph:

- low-latency monitoring;
- guitar/keys/vocal patches;
- patch per song/section;
- preload/warm next patch;
- safe switching;
- plugin latency/live-safe warnings.

## LIVE Stage 5 — MIDI Automation / Hardware Control — #33

Uses SHARED MIDI backend:

- PC/CC/Note actions;
- bar/beat/section/marker triggers;
- foot-controller mapping;
- learn mode;
- reconnect/safety;
- panic/all-notes-off.

## LIVE Stage 6 — Remote / Mobile Companion — #34

- local live-state API;
- discovery/pairing;
- read-only mobile/tablet first;
- chord/section/bar/cue/setlist state;
- later limited controls;
- network completely isolated from realtime audio.

## LIVE Stage 7 — Concert Reliability — #35

- multi-hour playback;
- repeated song/project switching;
- device recovery;
- MIDI reconnect;
- remote disconnect;
- plugin failure handling;
- full preflight;
- show-state recovery;
- performance regression;
- rehearsal/full-show acceptance.

Any backend performance fix belongs to SHARED Core, not to a separate Live engine.

---

# Integration milestones

## A — Core Prototype
#15 + #16 + #20

## B — Live UI Prototype
#28 in parallel on mocks.

## C — First native Live integration
#17 + #25 + #29: Live Mode reads the same open MRS project.

## D — First Rehearsal Build
#30 + #31 plus #32/#33 as required.

## E — First Full Show Build
#35 passes rehearsal/full-show stress tests.

---

# Studio Pro compatibility

Studio Pro is an optional import/migration source, not a runtime dependency.

Issue: #3

```text
Studio Pro project
      |
Bridge / Import adapter
      v
MRS Project Model
      |
Arrange / Edit / Mix / Live Mode
```

This preserves existing Studio Pro projects while keeping Moon River Studio self-contained.

## Версии сборок — уточнение 2026-10-03
Пользовательские версии подэтапов и суффиксы определяются VERSIONING.md:
0.1b, 0.1c; updN для небольших обновлений, fixN для ошибок.
Номерные MRS version targets выше — прежние ориентиры функциональных milestones;
они не задают имя текущего артефакта. Текущий tracks/import/waveform — 0.1b,
clip editing — следующая 0.1c. Stage IDs не перенумеровываются.

## Актуальное продолжение — 2026-10-04
Пользователь принял локальную 0.1f / Stage 2a; PR #48/#47 интегрированы в main.
Следующий подтверждённый подэтап: 0.1g / Stage 2b — buses/subgroups,
track/bus outputs, cycle-safe shared routing, bus controls/meters, Undo/persistence.
Локальные ASIO Release configure/build, 62/62 CTest и GUI smoke пройдены.
Пользовательская приёмка локальной 0.1g пройдена. Сборки/пакеты выполняются локально,
без GitHub Actions; на GitHub обновляются только issues и документация.
Отправка кода, создание PR и слияние — только по отдельной просьбе пользователя.
Существующий PR #49 не слит; 0.1h реализована локально и ожидает приёмку.
Sends/returns, hardware multi-output и profiles — последующие slices #22.


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
