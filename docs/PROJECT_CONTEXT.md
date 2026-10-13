# MR Studio: текущий контекст разработки

Обновлено: **2026-10-13**, Asia/Krasnoyarsk. Это основной краткий снимок для нового
чата. Постоянные правила: [AGENTS.md](../AGENTS.md). Подробная история остаётся
в тематических документах; старые верхние записи README/START_HERE не являются
текущим статусом. Проверяй ветку, файлы и более новые инструкции пользователя.


## AI Foundation: декомпозиция #26 — 2026-10-13

По запросу пользователя создано 11 native sub-issues #157–#167 для исходных чек-поинтов #26.
Требования, зависимости, scope и критерии приёмки заполнены; parent links проверены через GitHub API.
Все задачи открыты/BACKLOG, код интеграции не реализован и реализация не начата.
Первый API prototype #167: ProxyAPI / `openai/gpt-5.6-luna`, сменные provider/base URL/model,
защищённое хранение ключа, READ + SUGGEST; edits после permissions/preview/Undo/MIDI.
Порядок и ссылки: [AI_INTEGRATION.md](AI_INTEGRATION.md#13-декомпозиция-ai-foundation--2026-10-13).
Изменения только issues/docs; source HEAD/ветка, сборка0.3g и пакеты сохранены.
Проверки: 11/11 дочерних задач и parent#26; build/tests не требуются для документации.
Следующий шаг — пользователь выбирает задачу; текущий roadmap не переключён автоматически.


## Где работать

Правило пользователя 2026-10-12: **GitHub — главный источник информации о проекте**.
Документацию и issues синхронизировать после содержательной работы/приёмки, а не только
при релизах; прежнее правило 2026-10-08 заменено. Локальный checkout — фактические
исходники и результаты тестов. Source main отстаёт от локальной0.3h: публикация документов
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
первая цель — принятая 0.3c upd13, текущая — 0.3h upd1 в отдельном package; рабочая папка — этот package,
аргументы пустые. Значок MR + голубая waveform:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Launchers/MR Studio.ico`
(16/24/32/48/64/128/256px, PNG preview рядом). Link target/icon проверены,
preview просмотрен; DAW не запускалась. При выдаче следующих локальных билдов
обновлять цель этого же ярлыка, сохраняя старые packages. Код/бинарники
upd13 не менялись; никакой push/merge/Actions.

## Продукт и активное направление

**#80 /0.3h upd1**, 2026-10-13: follow-up по screenshot, приёмка ожидается.
Markers caption+compact list; Sections/Chord Track короткие+ на x208/width34.
Alt-copy shadow alpha0.35 не исчезает при overlap, salmon border/без commit;
capture7logicalpx от raw pointer позволяет точную стыковку и off-grid при Snap.
Рамка из пустой области Sections и Shift+ЛКМ toggle выбирают группу, transient.
Shared EditSectionGroup/App: group move/copy/Duplicate/delete, одна Undo;
duration/gaps/IDs/bindings/content/guards/stale/atomic rejection preserved.
Новые копии не наследуют Live binding; linked group delete отклоняется целиком.
Schema20/archive1/prefs прежние, engine/DSP не менялись. [Контракт](ARRANGER_EDITOR_80.md).
Release/full130 PASS136.47s до final UI boundary clamp; finalfocused2 PASS26.14s.
GUI group/Shift/marquee/touch/offgrid/coarseSnap/
invalid-shadow/Delete/Undo/Redo; Core group move/copy/stale/overlap/binding checked.
Сборки без ошибок; intermediate full130 PASS136.44s до final raw-pointer capture.
GUI100/150 и свежий packaged150 inspected; packaged diagnostics/J3 PASSexit0.
Hardware ASIO/physical DPI NOT RUN. FEATURE READY WITH MANUAL CHECK.
Source `9ab296b5e91614ac814f21fd70d75a1b788458ac`; package `MR-Studio-0.3h-upd1-Arranger-Editor-JUCE-ASIO-Windows-local`;
GUI SHA256 `A00C08B3D39E640EA40433044AABCB738EFAB7708EA5626D804C743234CE310C`;13files/12hashes verified, old0.3h unchanged.
Symbols deliveries/0.3h-upd1-arranger-editor. Прежний MR shortcutupd1/icon retained/
target+workingdir+emptyargs verified. Логи validation/arranger80-upd1.
GitHub docs/issues sync; source/build/tests/packages local. Чужие AI docs сохранены.
#80 open до приёмки; #87 не начат. Следующий шаг — пользовательская проверка
унификации/прозрачной тени/стыковки/групп; автоматически новых задач не начинать.

Начато **#80 /0.3h**, 2026-10-13 по прямому запросу пользователя.
База: принятая0.3g/source7d90ad2; local HEAD1641da3, ветка прежняя.
Чужие AI planning изменения docs/AI_INTEGRATION.md и AI блок контекста сохранены,
не включены в собственные source/docs commits; они уже соответствуют remote main.
CopySection/App metadata path: new ID/name/color/duration, без клипов/других lanes/
Live binding; Alt+drag preview/Escape, toolbar/ПКМ/Ctrl+D Duplicate после source,
overlap/stale/range/loop/play/record guards, одна Undo, SaveOpen. Пять иконок
Markers/Sections/Chord Track/Tempo/Meter слева от ruler, active blue/tooltips;
динамическая высота всех paint/hit/drop/scroll paths, preferences mask/default31,
не проект/Undo. Caption Chord Track теперь Theme::browserFont, как Sections.
[Контракт и приёмка](ARRANGER_EDITOR_80.md). Schema20/archive1 прежние.
Release/full130 PASS98.38s, focused2 PASS25.44s, GUI all32 masks/Alt-copy/cancel/
duplicate/legacyprefs, plugin binding/guards/history/SaveOpen. Snapshots100/150 inspected.
Начальные compile warning/private-test-access и наложение build процессов исправлены;
логи validation/arranger80 сохранены. Hardware ASIO/physical DPI NOT RUN.
Source `57c329ecc6e637edbdbcc6651e767a0c54ec0f36`. FEATURE READY WITH MANUAL CHECK; пользовательская приёмка ожидается.
Package `MR-Studio-0.3h-Arranger-Editor-JUCE-ASIO-Windows-local`;13files/12hashes verified;
GUI SHA256 `F2CC9919AC24EA07629519B8E63AD308262ECDEB798C69BB76482F097DAB3511`; packaged diagnostics/J3 PASSexit0.
Symbols deliveries/0.3h-arranger-editor. Прежний MR Studio.lnk обновлён на0.3h,
icon retained/target+workingdir+emptyargs verified; старые packages сохранены.
#80 open до приёмки; #87 не начат. Исходные Stage5 tasks:7/9 приняты;
native parent#25 также содержит дополнительный#150, фактический счёт7/10.
GitHub docs/issues синхронизируются docs-only; source/build/tests/packages локальны.
Следующий шаг: пользователь проверяет0.3h; после приёмки закрыть#80, затем
выбор#87 для итоговой интеграционной проверки. Автоматически#87/AI не начинать.

Принято **#84 /0.3g**, 2026-10-12: «Начинай работу на #84».
Clean HEAD8a1b1fb перед работой; baseaccepted0.3f. Core/App meter command и существующая
time-only transaction; подготовленные meter segments/SDK meter+bar context,
callback boundary split, MeterTrack под Tempo и denominator-aware grid/piano labels.
[Контракт/RT classification](METER_MAP_84.md). Schema20/archive1, musical ticks/WAV/
sample head unchanged. Release/full130 PASS140.74s, включая GUI6, новые meter/App/MIDI/VST3/Click/count/record
и существующие mixed/ahead/RT/persistence регрессии. Первый linker/J3 geometry failure
исправлены, логи validation/meter84 сохранены. Main100/editor150 inspected.
FEATURE ACCEPTED2026-10-13: «Принимаю. Что у нас дальше?». Подробная ASIO/DPI матрица не предоставлена. Source `7d90ad2478c0cb4de792dcb9a2df9f976b26ef40`.
Package `MR-Studio-0.3g-Meter-Map-JUCE-ASIO-Windows-local`;13 files/12 hashes reverified.
GUI SHA256 `39D65DE7DC63DC42928DD4E0F399B67AC9B5CAD88417DBAEBC5348F8D9899304`; packaged diagnostics/J3 PASSexit0.
Symbols deliveries/0.3g-meter-map. Свежий packaged150 snapshot inspected.
Прежний ярлык MR Studio теперь0.3g/icon retained/target+workingdir+emptyargs verified.
Пакеты0.3f и все более старые сохранены. #84 закрыт completed; parent#25 отмечает7/9.
Приёмка docs-only: source/full130/package/manifest/shortcut прежние, rebuild не нужен.
Следующий рекомендованный шаг — #80 /0.3h: copy/дублирование секций и завершение Arranger;
затем #87 — общая финальная проверка. #80 начат по запросу2026-10-13; актуальный результат выше.
Документация и issues синхронизируются docs-only с GitHub; source/пакеты только локально.

Принято **#83 /0.3f**, 2026-10-12: «Поехали #83», user выбирает tempo до #80.
#80 остаётся open. База sourcec871edc/accepted0.3e upd2; HEADdf94aef, checkout clean
перед работой. Реализованы Core staged transaction/UpdateTempoMap, Engine TimingPlan,
callback splits at tempo/loop, Application time-only edit/history и TempoTrack UI.
Нет новых callback allocation/locks/I/O; plugin/routing/WAV samples/MIDI ticks и
sample head сохраняются. [TEMPO_MAP_83.md](TEMPO_MAP_83.md). Schema20/archive1.
Release/full129 PASS146.00s (GUI6 включены), новые desktop_tempo/MIDI/VST3/click,
atomic rollback/backend/publish/overflow/history и mixed sample-budget checks.
Логи build/asio-local/validation/tempo83; первые compile/fixture/geometry/callback-count
отказы сохранены и исправлены. Screenshots100/150 просмотрены. Пользователь2026-10-12: «#83 прошел приемку, что у нас дальше?».
FEATURE ACCEPTED; новые ASIO/DPI измерения и подробная матрица не предоставлены. Source `eb2f30e53406d87402803b6b1ab0aaa353992df7`.
Package `MR-Studio-0.3f-Tempo-Map-JUCE-ASIO-Windows-local`:13 runtime files/12 hashes reverified;
GUI SHA256 `8591719AB45ABF2E1893AE9F04D139A57EA21E8ABB816FBAC353B7AC9A46FDA7`.
Packaged diagnostics/J3 PASSexit0, свежие Tempo100/150 и editor snapshots просмотрены.
Symbols `build/asio-local/deliveries/0.3f-tempo-map`; старые packages сохранены.
Прежний MR Studio.lnk теперь0.3f, рабочая папка/пустые args/иконка/существование EXE проверены.
Приёмка документальная: исходники, проверки, пакет/manifest/ярлык неизменны; rebuild не нужен.
#83 закрывается completed; parent#25 отмечает6/9. Следующий рекомендованный шаг —
#84 /0.3g, карта размера. Пока не начат: ждать прямого выбора пользователя.
Без push/merge/Actions.

## Предыдущая принятая сборка и оставшийся план

**0.3e upd2 / #81** принята2026-10-12: «Отлично принимаю! Что дальше?».
#81 closed completed. Sections/markers: drag, inline rename, palettes, alpha0.72;
source `c871edc801133e204066c9edf0c73edfe3d44a17`, full128 PASS164.54s,
final GUI6 PASS76.10s, packaged diagnostics/J3 PASSexit0.
Пакет `MR-Studio-0.3e-upd2-Sections-Markers-JUCE-ASIO-Windows-local` сохранён;
[контракт и приёмка](SECTION_COLORS_BOUNDS_81.md).

GitHub states проверены2026-10-12: #25 open; #79/#81/#82/#83/#84/#85/#86 closed;
#80/#87 open. #83 принят по сообщению пользователя.
#80 — полный Arranger/copy, #84 принят, #87 — общая финальная сверка;
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
Сверка завершена2026-10-12:58 документов опубликованы docs-only commit
[`4dbc7e5`](https://github.com/vladleng/MR-Studio/commit/4dbc7e5827f2f3294233a4cdb2773192167b0ba4)
поверх main690f1bd с [skip ci]. Remote HEAD и совпадение58 документов с локальной копией
проверены; remote-only планы сохранены. Apps/Core/tests/build config/workflows/assets
не изменены; пакет0.3f и его12 хешей проверены повторно. Docs-only работа: rebuild не нужен.
Логи сверки: `build/asio-local/validation/docs-sync-20261012`; source main по-прежнему0.2g.
Последний запрос пользователя: «Я хочу чтобы гитхаб был главным источником информации для проекта».
Код/сборки/пакеты/ярлык этой синхронизацией не меняются, Actions не запускаются.
[Локальная история поставок](history/PROJECT_CONTEXT_LOCAL_BEFORE_SYNC_20261012.md)
и [старый GitHub контекст](history/PROJECT_CONTEXT_GITHUB_BEFORE_SYNC_20261012.md)
сохранены как архивы, не текущие инструкции.
