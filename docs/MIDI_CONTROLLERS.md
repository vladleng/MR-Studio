# 0.2f / Stage 4f — MIDI controller events

Issue #68, parent #24. 0.2e принята пользователем: «работает, приступай к 0.2f».
Дополнительные доработки после реализации всех пунктов Stage 4.

Piano roll → Controllers...: список всех записанных событий и одна графическая
линия выбранного type/channel/CC number или poly note. Numeric Add/Apply/Delete,
double click создаёт, drag точки меняет source position/value одним Undo.
Escape/focus loss отменяет preview; чужое изменение clip отменяет commit.
Snap Off/1/4/1/8/1/16/1/32. Позиция — beats от исходного source, включая trim offset.
События за видимым диапазоном доступны через список/поля; graph показывает clip.

CC и sustain: 0–127, CC64 0–63 off, 64–127 on. Program: MIDI 0–127,
не UI 1–128. Channel pressure 0–127; poly pressure: note 0–127, value 0–127.
Pitch bend: signed -8192..8191, центр 0, сохраняется как два 7-bit байта.
Канал UI 1–16 / model 0–15. Между точками состояние удерживается; интерполяции нет.
Изменение типа/канала/CC/позиции/значения сохраняет ID события; Add создаёт ID.
Notes и другие events сохраняются; no-op не создаёт историю. Editing после Pause/Stop.

## Controller state

Seek/resume/loop восстанавливает последнее предшествующее значение каждой
используемой линии. Без предыдущего: bend center, pressure/poly/program 0,
CC7=100, CC10=64, CC11=127, остальные CC=0. Pause/Stop сбрасывают используемые
линии к этим defaults. В clip boundary sustain выключается и bend центрируется.
Прочие CC/pressure/program удерживаются до следующего события/transport reset.
На общей границе: ending authored events → boundary resets → new clip head/chase.
Остальные совпадающие события сохраняют стабильный source/project clip order.
Общий MIDI channel разделяет controller state между перекрывающимися клипами.
Применение Program/CC к конкретному VST3 зависит от его MIDI mapping.

## Слои и RT

Core SetMidiEvents/conversions, Application, JUCE editor, compiler ordering,
tests/docs: AFFECTED. Notes/model/schema13/persistence переиспользованы без новых
полей. Undo единый; window filters/snap/selection transient. Core Studio/Live shared.
Driver ingress и audio scheduling НЕ меняются. Новые allocations/sort только в
NON-RT prepare/UI; callback использует прежние bounded buffers и stable merge.
8192 compiled channel events и 64 distinct controls на track; boundary resets
потребляют event budget. Запись резервирует 48 entries: 16 завершающих sustain
и до 32 compiled sustain/bend resets; базовый raw limit теперь 8144, существующие
клипы учитывают до 32 resets каждый. Проверяется точный worst case 8192 entries. Неверные/избыточные изменения отклоняются до commit.
0.2g/0.2h не входят в эту поставку.

## Локальная поставка — 2026-10-06

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
