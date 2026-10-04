# Правила версионирования Moon River Studio

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
