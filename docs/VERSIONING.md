# Правила версионирования Moon River Studio

## 0.2f / Stage 4f — 2026-10-06

Piano roll → Controllers...: CC/sustain, pitch bend, channel/poly pressure и
Program Change. Выбор канала/CC/note, Add/Apply/Delete, графическая lane со snap,
перемещение позиции/значения одним Undo, Escape отменяет жест. Notes/IDs сохраняются;
общий SetMidiEvents, Undo/Redo, save/reopen; schema 13 без изменений.
Controller chase при seek/resume/loop; на границе клипа sustain off/bend center,
события начала следующего клипа применяются после сброса независимо от порядка clips.
Запись резервирует завершающие события: базовый raw limit 8144, worst case 8192 compiled.
Source `e8d714e41a9e0a46f849f0bbbcda30dfe7db12c8`, только локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings; **113/113 CTest PASS (59.31 s)**.
Packaged recording/clips/live/J3 PASS; controller previews 100%/150% просмотрены.
Проверены все типы/диапазоны, stable IDs, invalid input atomicity, Undo/serialization,
seek/loop/boundary ordering, callback allocation 0, полный recording event budget;
GUI add/apply/delete/drag/single Undo/Escape cancel.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-Controllers-JUCE-ASIO-Windows-local`; EXE SHA256 `9A3827B6BCF358E6946ABFDA39E69AA1D6081D904DDFA2321D239C8457A69142`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK.
Физические MIDI/ASIO/VST3 и Windows mixed DPI для 0.2f NOT RUN.
[Контракт](MIDI_CONTROLLERS.md), [checklist](MRS_STAGE_4F_CHECKLIST.md).
#68 открыт до пользовательской приёмки; #24 остаётся открыт. 0.2g/0.2h не начаты.
0.2e принята: «работает, приступай к 0.2f», #67 закрыт.
Дополнительные доработки после реализации всех пунктов Stage 4.

## 0.2e / Stage 4e — 2026-10-06

Piano roll → Musical edit...: Quantize (grid/strength, start/length separately),
Transpose группы, Velocity и Length Set/Add/Scale. Scope Selected / Whole clip
явный; empty selection сообщает ошибку. Интервалы transpose сохраняются, pitch
0–127, velocity 1–127, length минимум 1 tick; clip/trim края учитываются.
Одна Apply — один общий Undo; no-op без history. ID/channel/controller events
сохраняются. EditMidiNotes/transform_midi_notes общие для Studio/Live; UI — Studio.
Schema 13 и audio callback не изменены. Editing после Pause/Stop.
Source `abd77b09f3edb2045873cf33e19d291095eda26b`, только локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings, **113/113 CTest PASS (58.96 s)**.
Packaged midi recording/clips/live/J3 PASS; musical dialog 100%/150% просмотрен.
Тесты: strength 0/50/100, targets, selection/isolation, transpose limits/intervals,
velocity/duration clamp, trim edges, IDs/controllers, Undo/Redo/serialization,
GUI transpose/whole-clip scope/invalid input/empty selection/no-op.
Пакет `MR-Studio-0.2e-Musical-edit-JUCE-ASIO-Windows-local`; EXE SHA256 `5371EFAA368955563AFCC84184E95CE573F7C8E2C720CB6178C0D891E607FE13`.
FEATURE READY WITH MANUAL CHECK. Физические ASIO/VST3 и Windows mixed DPI
для 0.2e не запускались. [Контракт](MIDI_MUSICAL_EDIT.md), [checklist](MRS_STAGE_4E_CHECKLIST.md).
#67 закрыт: пользователь «работает, приступай к 0.2f» подтвердил приёмку 0.2e и запросил 0.2f.
Дополнительные доработки отложены до реализации всех пунктов Stage 4.

## 0.2d / Stage 4d — 2026-10-06

Piano roll: клавиатура/ruler, zoom/scroll, snap, создание/удаление и групповое
выделение/перемещение/длина/velocity, copy/paste, audition. Один жест — один
общий Undo; stable IDs/проект/события контроллеров сохраняются. Точная note form
остаётся. Editing и audition после Pause/Stop. Отдельная bounded UI SPSC для
preview MIDI; driver bridge не получает второго producer, audition не записывается.
Семь transport hit regions одинаковые: 42×30 logical px, gap 3, icon-only.
Source `92ac0f2fceea2a79bdd7d77873a199ee87fe7c45`, локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings; **113/113 CTest PASS, 59.81 s**.
Packaged recording/clips/live/J3 PASS. Piano roll/Arrange/Mix software previews
100%/150% просмотрены. Checked callback host allocation 0; Workers 1/2/4,
stale generation, invalid track, audition overflow/panic и exclusion из recording.
GUI: snapped move/group single Undo, fresh paste IDs, delete, Escape, resize,
velocity и pitch/clip edge clamp. Схема 13 без изменений, читает 1–12.
Пакет `MR-Studio-0.2d-Piano-roll-JUCE-ASIO-Windows-local`; EXE SHA256 `BED650A86669056C81F1F5B471BE257D10BC925BBA1DAB6C917751C52202582D`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические ASIO/VST3
и Windows mixed DPI для 0.2d не запускались. [Пользовательская проверка](MRS_STAGE_4D_CHECKLIST.md),
[архитектура/ограничения](PIANO_ROLL.md). #66 открыт до приёмки; 0.2e не начинать автоматически.

## 0.2c fix1 — косметический фикс, 2026-10-06

По запросу пользователя: transport buttons рисуют только SVG без фона/рамки;
области нажатия, layout, tooltips и accessibility names сохранены. Hover/press,
keyboard focus и toggle отражаются цветом glyph. Названия треков/каналов, dB,
кнопки и insert names в Strip используют общий с VST3 tree системный шрифт 13 px
(ранее Noto Sans 13 pt). Остальная типографика не изменена.
Source `d3da9f739915103e9952a641c154b167cf436cbf`, только локально. Release GUI build PASS без предупреждений;
packaged J3 PASS (MIDI 4a/4b/4c, transport/Arm/Record/Stop/Undo/Redo).
Software Arrange/Mix previews 100%/150% просмотрены. Полный CTest не повторялся:
113/113 PASS относится к базовой 0.2c. Физические MIDI/ASIO и Windows mixed DPI
для косметического фикса не запускались; пользовательская визуальная приёмка ожидается.
Пакет `MR-Studio-0.2c-fix1-UI-JUCE-ASIO-Windows-local`; EXE SHA256 `DA313BC3E7A1E176B26DC60AD16ACC93BC67947C3E9A4A671E46563A53CAC236`.
0.2c/#65 ещё не принята целиком; 4d не начат, schema 13 и engine без изменений.

## 0.2c — 2026-10-06

Stage 4c: линейная MIDI recording, instrument Arm, timestamp/sample→tick,
notes + channel events/sustain/bend, joint audio/MIDI, единый Undo/save/reopen.
Семь transport SVG встроены в приложение из пользовательского assets/ui/icons/transport;
прежняя компоновка и компактный микшер сохранены. Core schema 13 читает 1–12.
Source `f5247ede6775657f6067109047b7786af2f849b8`, только локально.
Release/113 CTest PASS (58.60 s), packaged recording/live/clips/J3 PASS.
Пользовательская ASIO-приёмка ожидается. Workflow/limits: MIDI_RECORDING.

## 0.2b — 2026-10-06

Stage 4b: MIDI-клипы, минимальный note form, clip editing/Undo/Redo,
tempo-aware playback и chasing/loop/Stop. Core schema 12 читает 1–11;
старые сборки не читают 12, использовать копии проектов.
Source `1c26828606c19d1f298417f3d1edf7821b7e4a67`, только локально.
Release и 112/112 CTest PASS (58.30 s), packaged MIDI/live/J3 PASS.
Принятая визуальная база 0.2a upd1 fix1 сохранена. 0.2b принята пользователем: «Тест пройден, идем дальше». #64 закрыт; #24 открыт. Пакет/границы — PROJECT_CONTEXT и MIDI_CLIPS.

## 0.2a upd1 fix1 — 2026-10-06

Компактный микшер: каналы 86 logical px, ручка фейдера 16 px, Noto Sans 13 pt
в кнопках/меню/подписях треков и timeline, центрированный транспорт, CPU слева.
Сборка принята пользователем: «Отлично, задокументируй, чтобы другой чат мог
продолжать с этого билда». Продолжать с отдельного Mixer-пакета и незакоммиченных
UI-исходников в текущем checkout. Пути, SHA256 и точный объём проверок —
PROJECT_CONTEXT. Включён 0.2a fix1; schema 11 и состояние MIDI/P4 не меняются.

## 0.2a fix1 — 2026-10-06

Исправлены live MIDI с Process Buffer, статус назначенного входа без audio runtime,
подготовка native editor из Disconnected и владение окнами редакторов/параметров.
Configure/Release, 111 CTest и финальные focused/package checks прошли.
Fix1 принят пользователем 2026-10-06: «Все работает». Конкретные режимы
повторной проверки не перечислены; полный checklist 4a отдельно не подтверждён.
Пакет/source/проверки: PROJECT_CONTEXT; schema остаётся 11.

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
Пакет и пользовательская приёмка — в PROJECT_CONTEXT. 0.2b принята, 0.2c выпущена локально с ожидаемой ASIO-приёмкой; 0.2d–0.2h ещё не выпущены.
Исторический milestone MRS 0.5 не определяет версию MIDI-пакета.


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
