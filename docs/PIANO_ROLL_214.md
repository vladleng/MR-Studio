# Piano roll — #214

Объём [#214](https://github.com/vladleng/MR-Studio/issues/214). Приёмка ожидается.
Пользователь подтвердил Alt-drag для копии, Ctrl+Alt-drag для velocity и режимы
«Velocity / Цвет клипа» 2026-10-17. Приложенный скрин используется как reference
заполнения velocity внутри ноты; чужая графика не включается в исходники/пакет.

## Вид нот

Активная нота имеет заливку с opacity 80% (20% прозрачности). Две соседние области
полной высоты показывают velocity/127: светлая слева, приглушённый остаток справа.
Они не накладываются друг на друга, поэтому opacity тела не становится 96%.
Выделение — отдельный светлый контур; не заменяет цветовую информацию ноты.
Context clips остаются приглушёнными и переключают активный клип по прежнему правилу.

Velocity mode: непрерывная градация от холодного синего через teal к тёплому amber.
Clip mode: точный сохранённый RGB соответствующего клипа, default как у клипов.
В обоих режимах длина заполнения показывает velocity. Это настройка вида
`ViewSettings::noteClipColors`, сохраняемая между запусками, без project Undo.
Старые настройки без поля получают Velocity mode. JSON view settings остаётся v1.

При ширине ноты >=34 px и высоте >=15 px видны pitch+octave, например C#4.
Используется C4=60, MIDI pitches 0..127; velocity 1..127. Сетка, Snap, meter/tempo,
scroll, controller lane и временная привязка source offset сохраняются.

## Манипуляции

- Shift-клик добавляет/снимает ноту; снятие решается на mouse-up, чтобы выбранная
  группа оставалась доступной для Shift-drag. Ctrl-клик сохраняет additive смысл.
- Shift/Ctrl + рамка добавляет пересечения к прежней группе. Selection/preview не
  меняют проект. Обычный drag перемещает группу с сохранением взаимных позиций.
- Alt-drag создаёт копию группы; исходные ноты не меняются. Новые IDs стабильны
  во время preview и сохраняются на Redo. Alt-клик без перемещения не создаёт копию.
- Ctrl+Alt-drag по вертикали меняет velocity группы. Shift при drag временно
  отключает Snap, как в Arrange. Keyboard/velocity поля и controller editor сохранены.
- Оба края ноты имеют скобку того же курсора, что у клипов. Левый край меняет
  start/length, удерживая конец; правый — length, удерживая start. Групповая дельта
  ограничивается целиком, минимум 1 tick. Новые значения не выходят за разрешённый
  source/clip диапазон; исходные скрытые за trim части не обрезаются автоматически.
- Up/Down: полутон; Shift+Up/Down: октава; Ctrl+Shift+Up/Down: копия на октаву.
  Если хотя бы одна нота выходит за 0..127, операция отклоняется целиком.
- D: копия сразу после временного span выделенной группы. Ctrl+C/V сохраняют
  взаимное время, pitch, velocity, channel; paste от cursor. Если вся группа не
  помещается в MIDI-клип, paste/duplicate отклоняются целиком, без частичного trim.
- Delete/Backspace удаляет группу. Контекстное меню содержит copy/paste/duplicate/
  split/delete/select-all. Notes details остаётся доступен в боковой панели.

Над roll — панель высотой 28 px с Pointer (1) и Split (3), те же существующие UDS
SVG, glyph14 px и hit32×28. Режим инструмента локален MIDI-редактору. Split-клик
по ноте разрезает выбранные ноты, пересекающие Snap-cursor; если нота не выбрана,
сначала выбирается она. S разрезает группу в cursor. Граница/нет пересечений — no-op.
Левые половины сохраняют IDs, правые получают новые; velocity/channel сохранены.
Разрез означает два note-on/off события, включая retrigger на общей границе.

Все правки нот используют существующий общий `SetMidiNotes`, одна запись Undo
на завершённый drag/keyboard/menu action. Изменения заметны до release только
в preview; Escape/focus loss/замена Store или исходного MidiClip отменяют drag.
TextEditor/ComboBox удерживают свои цифры/стрелки; Space/F11/Attach и sidebar
контроллеров сохраняют прежний контекст. Редактирование требует Stop/Pause и
отсутствия записи. MIDI audition освобождается перед завершением/отменой gesture.

## Слои и совместимость

UI, existing commands, Undo и view preferences затронуты. Новых MIDI model полей,
project/archive схемы и DSP/callback изменений нет: schema27/archive1 прежние.
Существующий stopped reprepare удерживает инструмент/FX по channel ID;
Undo/Redo нот теперь сообщает UI о сохранении plugin instances, чтобы toolbar
не закрывал редакторы перед retained history. Сами processors не перезагружаются.
Clips, controller events, routing и automation не меняются операциями над нотами.

## Проверка и приёмка

Адресные MIDI/model/history/host regressions, GUI gesture/state/persistence,
native+synthetic VST3 editor lifetime; previews normal/minimum/zoom и software
100/125/150/200. Прежние controller scroll/overlay/sidebar checks остаются.
Физические ASIO/слух/MIDI keyboard/mixed DPI отдельно NOT TESTED без прогона.
Фактические commit/build/PASS/FAIL/ограничения — в Issue; закрытие после приёмки.