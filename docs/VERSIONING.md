# Правила версионирования Moon River Studio

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

Принятая версия: `0.1e` / Stage 1d. Весь MRS Stage 1 завершён.
Далее MRS Stage 2 / #22 Mixer / Routing; имя следующей сборки определить при выборе подэтапа.

Эта схема имеет приоритет над прежними номерными примерами roadmap.
Live Mode входит в ту же сборку и не получает отдельную продуктовую версию.

При объединении upd и fix в одной сборке оба счётчика указываются: `0.1d upd1 fix1`.
Имя артефакта: `MR-Studio-0.1d-upd1-fix1-ASIO-Windows`.


## 0.1e upd1 — requested folder follow-up
User requested project-owned content folders on 2026-10-03 after accepting 0.1e.
Small update keeps base 0.1e: UI version 0.1e upd1;
artifact MR-Studio-0.1e-upd1-ASIO-Windows. PR #47, user acceptance pending.
See PROJECT_FOLDERS.md and MRS_PROJECT_FOLDERS_CHECKLIST.md. Next Mixer/Routing
substage/version is still to be planned after this update.
