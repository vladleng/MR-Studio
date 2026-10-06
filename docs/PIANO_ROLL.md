# 0.2d / Stage 4d — Piano roll

Issue #66, parent #24. Windows JUCE, общий Project Model/Undo и schema 13.

## Контракт и слои

Double click MIDI clip в Arrange либо Edit → MIDI clip открывает piano roll.
128 клавиш/строк, beats сетка, snap Off/1/4/1/8/1/16/1/32, horizontal zoom,
двухосная прокрутка; клавиатура и ruler закреплены при scrolling.
Полотно ограничено 1 000 000 logical px по горизонтали; сверхдлинный источник
доступен через меньшее zoom/Arrange trim, точные значения — через нижнюю форму. Double click пустого места создаёт ноту; Ctrl/Shift click
и рамка выделяют группу. Drag перемещает, правый край меняет длину, Alt-drag
меняет velocity. Delete/right click удаляет. Ctrl+C/V/A/Z/Y копирует, вставляет
в позицию оранжевого курсора, выделяет и вызывает общий Undo/Redo.
Нижняя форма позволяет точно задать pitch/start/length/velocity/channel.

UI, Application commands, audio ingress, tests/docs: AFFECTED.
Project note data, MIDI playback, serialization: существующие paths переиспользованы.
Схема и файловая структура: NOT AFFECTED. Selection/snap/zoom/clipboard — transient
UI state, не сохраняются и не входят в Undo. Core и playback общие со встроенным
Live; piano roll — Studio UI. Musical operations/CC lanes — 4e/4f, DEFERRED.

Жест создаёт local preview и один SetMidiNotes при mouse up. Escape/focus loss
отменяет preview. Состояние модели проверяется перед commit; чужое изменение
отменяет жест. Stable IDs сохраняются; paste создаёт новые IDs. Все controller
события остаются в клипе. Перекрытия нот допускаются существующей моделью.
Редактирование только после Pause/Stop; во время recording недоступно.

## Audition и realtime

Нажатая нота/клавиша прослушивается через назначенный VST3; mouse up, focus loss,
cancel/destruction отпускают её. Требуется подключённый audio device и инструмент.
Отдельная UI-producer/device-consumer SPSC (128 slots, максимум 127 reads/callback)
не меняет driver bridge queue и не записывает preview MIDI. Generation/track/range
проверяются в callback; stale events отбрасываются, overflow вызывает panic.
Новых callback allocations/locks/I/O/UI calls нет. MIDI monitor Off не блокирует
явное audition. На один редактор удерживается одна пробная нота.

## Дополнительное оформление

Семь transport hit regions: 42×30 logical px, одинаковый промежуток 3 px.
Группа центрируется по фактической суммарной ширине; фон/рамки по-прежнему скрыты.

## Проверки

Source `92ac0f2fceea2a79bdd7d77873a199ee87fe7c45`, только локально. Full Release без warnings, 113/113 CTest PASS (59.81 s).
Packaged midi recording/clips/live/J3 PASS. Software previews 100%/150% просмотрены.
GUI move/group Undo/paste IDs/cancel/delete/resize/velocity/edge clamp PASS.
Engine audition Workers 1/2/4, stale/invalid route, monitor Off, overflow/panic,
exclusion from recording PASS; checked host callback allocation 0.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK.
Пакет/EXE SHA256: PROJECT_CONTEXT. Физические ASIO/VST3/mixed DPI не запускались.
Software DPI previews не заменяют Windows mixed DPI и аппаратное прослушивание.
