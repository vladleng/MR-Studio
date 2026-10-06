# Правила версионирования Moon River Studio

## Stage 4 / MIDI и метроном — схема от 2026-10-06

По прямой инструкции пользователя MIDI начинается с **0.2a**.
Каждый подэтап получает свою следующую букву: 4a / 0.2a, 4b / 0.2b,
4c / 0.2c, 4d / 0.2d, 4e / 0.2e, 4f / 0.2f, 4g / 0.2g,
4h / 0.2h (метроном и precount).
Объём и приёмка: [MRS_STAGE_4_PLAN.md](MRS_STAGE_4_PLAN.md).

Доработки: `upd1`, `upd2`, далее; исправления: `fix1`, `fix2`, далее.
Счётчики независимы внутри базовой версии и сбрасываются при смене буквы.
Комбинация допустима: `0.2a upd1 fix2`; имя пакета содержит `0.2a-upd1-fix2`.
0.2a реализована локально: MIDI input и VST3 instruments; Release/111 CTest прошли.
Пакет и пользовательская приёмка — в PROJECT_CONTEXT. 0.2b–0.2h ещё не выпущены.
Исторический milestone MRS 0.5 не определяет версию MIDI-пакета.

## 0.1m upd1 fix1 — local compatibility/UI update, 2026-10-04

Requested after the user found Nuro loading and TH-U preset-reopen issues in 0.1m.
Vendor folders start collapsed. Arrange / Edit / Mix are at the bottom right with
BROWS; hiding the sidebar expands the arrangement. VST3 layout negotiation includes
all declared audio buses (inactive auxiliary/sidechain buses remain unassigned).
Opaque plugin state is authoritative: no blanket parameter replay on activation or
save; complete legacy parameter snapshots are ignored on restore in favor of opaque
state, while sparse explicit host overrides remain supported. Pending host/editor
controls are flushed with zero samples while callbacks are stopped before state capture.
Local source `d5015f2043629178291f3d4824371d293ddf6bf8`, branch `mrs/0.1m-upd1-fix1-local`.
No project schema change (v10). Local package: MR-Studio-0.1m-upd1-fix1-ASIO-Windows-local.
87/87 CTest and hidden GUI checks passed. Installed Nuro Audio effects and TH-U passed
load, finite processing and opaque-state restore checks. TH-U selected-preset auditory
comparison in the user's project remains pending. Stage 3c stays open until acceptance.
Code/builds local; GitHub issues/docs only, no code push/PR/merge/Actions/install.
See [VST3 workflow](VST3_HOSTING.md) and [3c checklist](MRS_STAGE_3C_CHECKLIST.md).

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

Текущая локальная сборка: **0.1i upd1** — UI по референсу и wheel navigation/track zoom.
Пакет MR-Studio-0.1i-upd1-ASIO-Windows-local. Build, 69/69 CTest и GUI smoke пройдены;
**0.1i upd1 принята** пользователем 2026-10-04: «Все работает».
Ниже — исторические записи; счётчики upd/fix независимы. GitHub issues/docs only.

Текущая локальная сборка: **0.1i**, hardware outputs / multi-output / device profiles.
Пакет MR-Studio-0.1i-ASIO-Windows-local. Local configure/build, 69/69 tests и GUI smoke
пройдены; пользовательская/физическая приёмка ожидается. Последняя принятая — 0.1h fix3.
Ниже — исторические записи предыдущих сборок. Код публикуется только по просьбе пользователя.

Текущая локальная сборка: **0.1h fix3**, устранение мигания Audio settings.
0.1h fix3 принята пользователем 2026-10-04: «теперь ничего не мигает. Все работает».
Пакет MR-Studio-0.1h-fix3-ASIO-Windows-local; код 0911855dce7d179e03df33f05a120684e4637d37.

Текущая локальная сборка: **0.1h fix2**, устранение мигания при отпускании фейдера.
Пакет MR-Studio-0.1h-fix2-ASIO-Windows-local; пользовательская проверка ожидается.

Текущая локальная сборка: **0.1h fix1** — устранение мигания native кнопок
при mixer repaint. Функции 0.1h проверены пользователем; проверка fix1 ожидается.
Пакет: MR-Studio-0.1h-fix1-ASIO-Windows-local. См. MRS_0_1H_FIX1.md.

Зафиксировано по инструкции пользователя 2026-10-03.

- Пользовательские версии подэтапов: 0.1b, 0.1c и далее.
- Небольшие функциональные обновления внутри подэтапа: upd1, upd2 и далее.
- Исправления ошибок внутри подэтапа: fix1, fix2 и далее.
- Номера upd и fix считаются независимо в пределах базовой версии.
- При переходе к следующей базовой версии счётчики начинаются заново.
- Примеры отображения: 0.1b, 0.1b upd1, 0.1b fix1.
- В именах веток/артефактов пробел заменяется дефисом: 0.1b-fix1.
- Источник версии для UI: apps/studio-desktop/include/mrs/version.hpp.
- Stage/issue IDs обозначают структуру плана и не определяют название сборки.
- CMake numeric VERSION и схемы project/config/archive — технические версии;
  буквенная пользовательская версия не меняет схемы сохранений.

Текущее соответствие:
| Подэтап | Пользовательская версия |
|---|---|
| Принятый MRS Stage 1a: tracks/import/waveform | 0.1b |
| Принятый фикс Pause/seek/delete после 1a | 0.1b fix1 |
| Принятый MRS Stage 1b: clip editing | 0.1c |
| Принятый MRS Stage 1c: disk read-ahead | 0.1d |
| Принятое UI обновление: gray / Files / no Live button | 0.1d upd1 |
| Принятое исправление моно L/R, включено в UI сборку | 0.1d fix1 |
| Принятый MRS Stage 1d: record/monitor + save/load | 0.1e |

Принятая локальная версия: `0.1g` / Stage 2b, ASIO сборка принята 2026-10-04.
PR #48 и включённый #47 слиты в main. Весь MRS Stage 1 завершён.
MRS Stage 2b / #22 — buses/subgroups и track/bus outputs принят пользователем.
Полный Stage 2 ещё не завершён. Следующая часть — sends/returns; ещё не начата.
Код и сборки остаются локальными; GitHub — только issues и документация.
Публикация кода/PR/слияние — только по отдельной просьбе пользователя; PR #49 не слит.

Эта схема имеет приоритет над прежними номерными примерами roadmap.
Live Mode входит в ту же сборку и не получает отдельную продуктовую версию.

При объединении upd и fix в одной сборке оба счётчика указываются: `0.1d upd1 fix1`.
Имя артефакта: `MR-Studio-0.1d-upd1-fix1-ASIO-Windows`.


## 0.1e upd1 fix1 — requested folder follow-up
User requested project-owned content folders on 2026-10-03 after accepting 0.1e.
Small update keeps base 0.1e: UI version 0.1e upd1 fix1;
artifact MR-Studio-0.1e-upd1-fix1-ASIO-Windows. PR #47, user acceptance pending.
See PROJECT_FOLDERS.md and MRS_PROJECT_FOLDERS_CHECKLIST.md. These changes are
included in 0.1f / Stage 2a / PR #48, which now adds the first Mixer/Routing slice.


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
