# #99 — компактная панель Note Editor и overlay controllers

Начато 2026-10-09 после приёмки #100 пользователем («Работает! Принимай и
переходи к #99»). Межэтапной backlog #55; не Stage 5.

## Контракт и слои

Существующие команды/поля нот и контроллеров перемещаются в компактную левую
панель, controls сверху, typography Theme::channelFont как VST3 browser.
При недостаточной высоте панель прокручивается. Горизонтальной формы между
roll и controllers нет. CC1/CC11/CC64 на тонком разделителе выбирают lane,
не создают MIDI events. Перетаскивание свободной части разделителя вверх
увеличивает controller graph поверх roll; размеры и scroll position roll
не меняются. Видимая область графа принимает ввод, закрытая область roll — нет.

AFFECTED: Studio JUCE UI/layout/input, transient UI state, view notifications,
tests/docs. NOT AFFECTED: project model/domain/engine/commands/Undo/persistence
schema/project files/audio/RT. Нового Live продукта или Undo stack нет.
Существующая preference controllersOpen сохраняется прежним путём; overlay
height и lane selection transient, в проект не записываются. Editing остаётся
через существующие commands, preview/cancel/one-commit/Undo. Standalone controller
editor сохраняет прежнюю форму. Attach/Detach/fullscreen используют тот же panel.

## Реализация и проверки

Source **3343c86 +187b540**, локальная ветка mrs/0.2g-fix1-editor-space-local.
ControllerPanel сохраняет владельца модели/commands; только controlsContent
переподчинён sidebar. Его собственная Form сохраняет Escape/Undo/Delete routing
и disabled state; callback очищается до уничтожения sidebar/CC buttons.
Standalone layout прежний. Notes и Controllers могут быть открыты одновременно.
Divider 22px, CC buttons 48x18; свободная область принимает drag, Up/Down меняют
высоту, Escape отменяет текущий drag. Нижний scrollbar roll остаётся доступным.
Высота overlay ограничена геометрией окна: graph не становится нулевым и
заголовок roll остаётся доступным. Header/viewport не изменяются при drag.

GUI regression PASS: overlay bounds/roll scroll invariance, min/max и Escape/
keyboard resize, quick CC state/filter/events/revision invariant, cancel point
preview при смене CC, Form Escape/disabled routing, hit testing roll/graph/CC,
small window720x300 с overflow sidebar, shared scroll/zoom/alignment #100.
Existing note/controller editing, group selection, drag preview/commit/cancel,
shared Undo, Attach/Detach, floating/attached fullscreen, settings fixtures PASS.
Cached configure/JUCE Release PASS, focused CTest **5/5 PASS(35.00s)**:
midi_clips, desktop_workspaces, juce_j2_smoke, juce_j3_smoke, juce_diagnostics_smoke.
Предыдущие green проходы34.86s/34.71s также были; первый fixture hit test падал
из-за скрытого test component, исправлен setVisible. Small-window caption polish
выявил JUCE changeItemText без обновления выбранного текста: выбор канала теперь
восстанавливается без notification; отдельная regression PASS.

Software100/150 и small-window previews просмотрены, включая финальный packaged
150/small; номер канала читается без сжатия шрифта. Длинные ComboBox подписи
имеют ellipsis вместо уменьшения font. Physical DPI/mixed-monitor/manual input/
ASIO NOT RUN; UI-only изменение не требует новой hardware/RT матрицы.

## Доставка и приёмка

Пакет **MR-Studio-0.2g-upd8-Note-Editor-Layout-JUCE-ASIO-Windows-local** в Builds.
Packaged J3+diagnostics PASS exit0, log version0.2g upd8 подтверждён, EXE/PDB
совпадают с финальной Release; helpers/доступные symbols/assets/licenses включены.
EXE SHA256 **DFB0D8C1E6A2D79320ACD73FF695BB8CC7C2C0AB970A20E1927001CEA1823F7D**.
Принято пользователем 2026-10-09: «#99 принят, закрывай!».
FEATURE ACCEPTED; issue99 закрыт completed, соответствующие пункты parent55
отмечены выполненными. Пакет upd8, EXE/PDB и SHA256 неизменны. Новые build/tests
не запускались: это только запись приёмки. Приёмка не означает выполнения
дополнительной hardware/mixed-DPI матрицы или закрытия всего parent55.
#100 принят/закрыт2026-10-09, пакет upd7 неизменен. #98/#95/Stage5 не начинать.
Документация/код локально, GitHub обновлён только в issues; push/Actions нет.
