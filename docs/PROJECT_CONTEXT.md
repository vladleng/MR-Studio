# MR Studio: текущий контекст разработки

Обновлено: **2026-10-12**, Asia/Krasnoyarsk. Это основной краткий снимок для нового
чата. Постоянные правила: [AGENTS.md](../AGENTS.md). Подробная история остаётся
в тематических документах; старые верхние записи README/START_HERE не являются
текущим статусом. Проверяй ветку, файлы и более новые инструкции пользователя.

## Где работать

Правило пользователя 2026-10-12: **GitHub — главный источник информации о проекте**.
Документацию и issues синхронизировать после содержательной работы/приёмки, а не только
при релизах; прежнее правило 2026-10-08 заменено. Локальный checkout — фактические
исходники и результаты тестов. Source main отстаёт от локальной0.3f: публикация документов
не публикует код/пакеты. Более новые remote планы сохраняются при сверке.

| Назначение | Локальный путь |
|---|---|
| Исходники, Git, CMake, tests, docs | `C:/Users/Vladislav/Documents/GitHub/MR-Studio` |
| Папка чатов, Builds, Saves, навыки | `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio` |
| Навыки проекта | Рабочая папка чатов: `.codex/skills/<имя>/SKILL.md` |
| Активная ветка | `mrs/0.2g-fix1-editor-space-local`; опубликованный main/release v0.2g сохранён |
| Существующая сборочная папка | Репозиторий: `build/asio-local` |
| GitHub для issues/docs | `vladleng/MR-Studio` |

GitHub source может отставать от локальной разработки. Не заменяй локальный код
клоном/main/pull ради синхронизации документации. Машинные пути выше — текущая
конфигурация; на другой машине сначала найди существующий checkout.

## Постоянный ярлык — 2026-10-12

По запросу пользователя создан `C:/Users/Vladislav/Desktop/MR Studio.lnk`:
первая цель — принятая 0.3c upd13, текущая — 0.3f в отдельном package; рабочая папка — этот package,
аргументы пустые. Значок MR + голубая waveform:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Launchers/MR Studio.ico`
(16/24/32/48/64/128/256px, PNG preview рядом). Link target/icon проверены,
preview просмотрен; DAW не запускалась. При выдаче следующих локальных билдов
обновлять цель этого же ярлыка, сохраняя старые packages. Код/бинарники
upd13 не менялись; никакой push/merge/Actions.

## Продукт и активное направление

Активно **#83 /0.3f**, 2026-10-12: «Поехали #83», user выбирает tempo до #80.
#80 остаётся open. База sourcec871edc/accepted0.3e upd2; HEADdf94aef, checkout clean
перед работой. Реализованы Core staged transaction/UpdateTempoMap, Engine TimingPlan,
callback splits at tempo/loop, Application time-only edit/history и TempoTrack UI.
Нет новых callback allocation/locks/I/O; plugin/routing/WAV samples/MIDI ticks и
sample head сохраняются. [TEMPO_MAP_83.md](TEMPO_MAP_83.md). Schema20/archive1.
Release/full129 PASS146.00s (GUI6 включены), новые desktop_tempo/MIDI/VST3/click,
atomic rollback/backend/publish/overflow/history и mixed sample-budget checks.
Логи build/asio-local/validation/tempo83; первые compile/fixture/geometry/callback-count
отказы сохранены и исправлены. Screenshots100/150 просмотрены. #83 ждёт ручной
ASIO приёмки; FEATURE READY WITH MANUAL CHECK. Source `eb2f30e53406d87402803b6b1ab0aaa353992df7`.
Package `MR-Studio-0.3f-Tempo-Map-JUCE-ASIO-Windows-local`:13 runtime files/12 hashes reverified;
GUI SHA256 `8591719AB45ABF2E1893AE9F04D139A57EA21E8ABB816FBAC353B7AC9A46FDA7`.
Packaged diagnostics/J3 PASSexit0, свежие Tempo100/150 и editor snapshots просмотрены.
Symbols `build/asio-local/deliveries/0.3f-tempo-map`; старые packages сохранены.
Прежний MR Studio.lnk теперь0.3f, рабочая папка/пустые args/иконка/существование EXE проверены.
После transport errors GitHub #83 обновлён итоговыми проверками/package/shortcut; остаётся open до приёмки.
Следующий шаг — пользовательская ASIO/визуальная приёмка #83 (120→90→140, MIDI/Click,
Undo/Redo/SaveOpen, guards/WAV/sample head/plugin editors); #83 не закрывать до неё. Следующие issues не начинать автоматически.
Без push/merge/Actions.

## Последняя принятая сборка и оставшийся план

**0.3e upd2 / #81** принята2026-10-12: «Отлично принимаю! Что дальше?».
#81 closed completed. Sections/markers: drag, inline rename, palettes, alpha0.72;
source `c871edc801133e204066c9edf0c73edfe3d44a17`, full128 PASS164.54s,
final GUI6 PASS76.10s, packaged diagnostics/J3 PASSexit0.
Пакет `MR-Studio-0.3e-upd2-Sections-Markers-JUCE-ASIO-Windows-local` сохранён;
[контракт и приёмка](SECTION_COLORS_BOUNDS_81.md).

GitHub states проверены2026-10-12: #25 open; #79/#81/#82/#85/#86 closed;
#80/#83/#84/#87 open. #83 сейчас тестирует пользователь; не закрывать до ответа.
#80 — полный Arranger/copy, #84 — meter, #87 — общая финальная сверка;
порядок следующей работы определяет пользователь, не начинать автоматически.
MR Reverb #128–132 закрыты completed: [реализация/приёмка](REVERB_VALIDATION.md),
[gate/Mix](REVERB_GATE_MIX.md), [исходный дизайн](MR_REVERB.md).
P4 #54/#113 закрыты completed, #114 closed not_planned; подробная физическая
матрица и причина43+43 counters не устанавливались: [P4](ENGINE_P4_ACCEPTANCE.md).
#55 закрыт; source0.3f/schema20/archive1 локальны, последний GitHub releasev0.2g.

## Сохранённые планы с GitHub

Эти документы — концепции/backlog, не реализованные возможности текущей сборки:
- [Pre-Spatial Routing](PRE_SPATIAL_ROUTING.md) / [MR Strip Link](MR_STRIP_LINK.md),
  #133: отдельный аудиопуть MR Strip для каждого источника, синхронизация выбранных
  EQ/Dynamics/Saturation. Без специального возврата Group Bus на инструменты.
  Dry tap до сцены и обход Spatial у FX Sends/Returns; обычный суммированный Bus —
  один Spatial-источник. Не переоткрывать #22 и не менять принятый MR Reverb.
- [Harma Waves](HARMA_WAVES_NATIVE.md), [roadmap](HARMA_WAVES_ROADMAP.md),
  [handoff](HARMA_WAVES_CODEX_HANDOFF.md), #115/#116–127: план нативного гармонического
  движка. Код не перенесён/не собран; сначала по поручению аудит последней локальной
  Smart-Voicing/Harma Waves0.5g, parity и перенос фактического Core. Source mainstable0.5
  не доказывает отсутствие более свежей локальной версии.
- [Parallel VSTi](ENGINE_INSTRUMENT_PARALLELISM.md), #112: самостоятельная будущая
  возможность, не блокирует принятый P4. [MR Saturator](MR_SATURATOR.md) — дизайн/backlog.

## Документация и история

[Индекс документов](README.md), [Stage5](STAGE_5_PLAN.md), [правила разработки](../AGENTS.md).
Сверка выполняется docs-only поверх GitHub main690f1bd; remote-only документы сохранены.
Последний запрос пользователя: «Я хочу чтобы гитхаб был главным источником информации для проекта».
Код/сборки/пакеты/ярлык этой синхронизацией не меняются, Actions не запускаются.
[Локальная история поставок](history/PROJECT_CONTEXT_LOCAL_BEFORE_SYNC_20261012.md)
и [старый GitHub контекст](history/PROJECT_CONTEXT_GITHUB_BEFORE_SYNC_20261012.md)
сохранены как архивы, не текущие инструкции.
