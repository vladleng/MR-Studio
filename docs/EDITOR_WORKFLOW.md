# 0.2f upd1 — редакторы и визуализация записи

Запрос пользователя 2026-10-07: семь изменений редактора/записи, несмотря на общий
план отложить остальные доработки до завершения Stage 4. 0.2g/0.2h не входят сюда.

## Управление

- Выберите audio/MIDI clip и нажмите Edit. Без выбора используется клип под курсором
  выбранной дорожки; если клипа нет, интерфейс просит выбрать его.
- MIDI: piano roll и controller lane в одном редакторе. Controllers скрывает/показывает
  нижнюю область; Note details открывает точную форму вместо controller lane.
- Ctrl + колесо: вертикальный zoom; Ctrl + Shift + колесо: горизонтальный zoom.
  Shift + колесо: горизонтальная прокрутка, обычное колесо: вертикальная.
  Это те же modifiers, что в Arrange. Ввод текста сохраняет свои стандартные shortcuts.
- Двойной левый клик по ноте удаляет её; по пустой сетке создаёт. Общий Undo/Redo.
- Attach закрепляет текущий редактор в нижней панели вместо Mix; Detach возвращает
  тот же экземпляр в окно. Выделение и zoom сохраняются. Mix/Arrange скрывают панель,
  Edit возвращает её. Разделитель меняет высоту, браузер остаётся справа.
- Audio: waveform выбранного клипа, start/end в project seconds, Apply trim через
  общий Application::trim_clip; Undo и существующее сохранение проекта.

## Вид controller lane

CC, bend, pressure соединены прямыми отрезками. Program Change и педали CC64/65/66
показаны ступенями. Панель использует горизонтальный zoom/scroll piano roll и source
trim offset. Соединяющая линия описывает вид данных; MIDI playback отправляет
сохранённые дискретные события, дополнительные интерполированные события не создаются.
Редактирование позиций/значений по-прежнему сохраняет IDs, один жест — один Undo.

## Запись и владение

Растущий MIDI clip с нотами виден до Stop, включая удерживаемые ноты. Это transient
preview, не новый проектный clip и не операция Undo. Stop сохраняет прежнюю единую
операцию записи; audition по-прежнему не записывается.

RT recorder дописывает Entry в существующий preallocated append-only buffer, затем
публикует число готовых entries atomic release. UI читает count acquire и копирует
только неизменяемый опубликованный префикс. UI не читает живые count_/held_ и не
вызывает finish() на работающем recorder. Конвертация/sort/ID allocation — на UI,
обновление около 10 Hz. Callback добавляет только lock-free scalar publication;
новых locks, allocation, I/O/UI calls нет. Capacity/fault/Stop/join остаются прежними.

## Слои и совместимость

UI/Desktop/layout/shortcuts: AFFECTED. Recording preview shared Core/Application:
AFFECTED; RT immutable-prefix publication AFFECTED. Model/schema13/persistence и
controller playback policy: NOT AFFECTED. Audio trim и MIDI edits используют прежние
shared commands/Undo. Attach/filter/zoom/preview transient, не записываются в проект;
проектные события/notes/trim сохраняются. Все старые пакеты сохранены.

## Поставка и проверка

По семи прямым запросам пользователя 2026-10-07 выполнена 0.2f upd1.
Controller lane под piano roll; CC/bend/pressure связаны прямыми отрезками,
Program и CC64/65/66 — ступенями. Горизонтальная шкала общая с piano roll.
Ctrl+wheel vertical zoom, Ctrl+Shift+wheel horizontal zoom, Shift+wheel scroll,
как в Arrange. Double left click удаляет ноту; empty grid создаёт, общий Undo.
MIDI take с удерживаемыми нотами виден до Stop: transient immutable-prefix preview,
atomic publication; UI conversion, callback allocation 0 проверен.
Edit открывает выбранный audio/MIDI clip; audio waveform и trim через shared Undo.
Attach/Detach перемещает тот же редактор в нижнюю панель вместо Mix и обратно;
selection/zoom сохранены. Mix/Arrange скрывают, Edit возвращает. Divider resize.
Controllers/Note details переключают нижнюю область. Schema 13, save/load,
Undo/recording commit и дискретный MIDI playback сохранены.
Source `b8d14d55c1e0e3dec0be1b43b7925e1bad958524`, локально; ветка `mrs/0.1q-fix3-processing-local`.
Release без warnings, **113/113 CTest PASS (60.63 s)**; packaged recording/clips/live/J3 PASS.
Concurrent preview/capture, open notes, отсутствие commit до Stop, zero callback allocations,
double-click Undo, zoom modifiers, audio trim Undo, MIDI/audio Attach/Detach и Mix/Edit PASS.
Software previews 100%/150% просмотрены: piano roll, CC1 линии, dock, waveform, recording.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd1-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `33CF213400C7297744A3628CB98CCBE712F63D4AA23B866698EFAAB2718B4E9E`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические
MIDI/ASIO/VST3 и Windows mixed DPI для upd1 NOT RUN. #68/#24 открыты до приёмки.
[Workflow](EDITOR_WORKFLOW.md), [checklist](MRS_STAGE_4F_CHECKLIST.md).
Общее откладывание остальных доработок сохраняется; эти семь запрошены отдельно.
0.2g/0.2h не начаты.
