# #100 — общая прокрутка пианоролла и MIDI-контроллеров

2026-10-09 пользователь поручил начать #100. Выпуск 0.2g upd7.

## Контракт

Wheel над встроенным controller graph/background передаётся в тот же обработчик,
что над piano roll. Shift+wheel — горизонтальная прокрутка; Ctrl+Shift+wheel —
масштаб времени с привязкой к позиции мыши. Обычный wheel сохраняет вертикальную
прокрутку нот; Ctrl+wheel — масштаб высоты нот. Native deltaX поддерживается
одинаково над roll и lane. Для вертикального zoom из lane anchor — центр видимого
roll, не находящаяся ниже координата controller graph. Поля/ComboBox сохраняют
свои существующие обработчики. Standalone ControllerPanel без общей шкалы не меняется.

Источник времени — один RollViewport + PianoRoll beatWidth. Lane только проекция
через alignControllers/setTimeView; новая независимая позиция не вводится.
Прокрутка отменяет незавершённый preview точки/ноты, но не создаёт команду,
не меняет events/notes/revision и не добавляет Undo. Границы ограничивает Viewport.
Docked/detached editor использует один MidiClipPanel, переоткрытие/activation сохраняется.

## Слои

AFFECTED: JUCE input/UI transient viewport, notifications viewChanged,
tests/docs. NOT AFFECTED: project model, commands, Undo, persistence/schema,
project files, engine/audio/RT/plugin lifecycle. STUDIO UI на существующем shared
Project Model; отдельный Live продукт/реализация не вводится. Scroll/zoom transient,
новая сериализация или durable preferences не нужны. #99 layout и Stage5 не начинаются.

## Проверки

GUI regression: обе стороны Shift-wheel, native deltaX, Ctrl+Shift time zoom,
plain vertical scroll, Ctrl row zoom, pixel alignment notes/controllers,
левая/правая границы, cancel point preview without commit, неизменный project/revision.
Existing controller editing/drag/Undo fixtures сохраняются. Software100/150 previews.
Cached configure/JUCE Release PASS. Source7e0b56f +6858ccf на ветке
mrs/0.2g-fix1-editor-space-local. First focused5/5 PASS(36.15s), второй5/5
PASS(33.71s), финальный5/5 PASS(33.83s), включая midi_clips/desktop_workspaces/J2/J3/diagnostics.
Packaged J3+diagnostics exit0; log version0.2g upd7 подтверждён.
Software100/150 inspected: note и CC point на одной X-позиции. Physical DPI,
ручные input-device/ASIO проверки NOT RUN. Новые persisted поля не вводились.
Первый compile выявил ambiguous Point overload и immutable MouseEvent assignment;
исправлены explicit Point<float> и новый const event. Перекрытие двух build
процессов вызвало PDB lock; процессы завершены, последовательный build PASS.
Preview regression теперь truncates файл перед повторной записью PNG.

## Доставка

Пакет MR-Studio-0.2g-upd7-Controller-Scroll-JUCE-ASIO-Windows-local;
EXE SHA256:37FF69519DC7193AF29A358BEEB04C783C9EB200C39342807F33293AC6A336EE.
Принято пользователем 2026-10-09: «Работает! Принимай и переходи к #99».
Issue100 закрыт completed, parent55 обновлён; пакет неизменен. #99 начат отдельно
по этому запросу. #98/#95/Stage5 не начинались; source push/Actions нет.
