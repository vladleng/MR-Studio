# 0.2f upd3 — редакторы и визуализация записи

Запрос пользователя 2026-10-07: семь изменений редактора/записи, несмотря на общий
план отложить остальные доработки до завершения Stage 4. 0.2g/0.2h не входят сюда.

## Управление

ЛКМ + drag из пустой области Arrange выделяет clips; drag выбранного clips двигает
всю группу одним Undo, Escape отменяет. ПКМ selection заменена по уточнению пользователя.
Record добавляет take в выбранной позиции без append-only ограничения; loop off.
Start/Stop сохраняет plugin instances, Stop сразу очищает recording preview.

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
  Edit открывает её; повторный Edit закрывает редактор. Разделитель меняет высоту, браузер остаётся справа.
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
shared commands/Undo. Controllers open/closed и Attach/Detach сохраняются в локальном juce-view.json.
Filter/zoom/selection/preview transient, не записываются в проект;
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

## 0.2f upd1 fix1 — 2026-10-07

По пяти замечаниям пользователя выпущена **0.2f upd1 fix1**.
Record сохраняет красный цвет при фокусе; после отказа UI показывает фактический
статус записи. На скриншоте обе R выключены: перед Record включить R нужной дорожки.
Автоматического Arm нет. Проверены отказ без Arm и успешная запись с Arm.
Piano roll использует глобальные ticks проекта. Play/Pause/Stop в редакторе и
Space управляют общим транспортом; белые плейхэды piano roll/контроллеров берут
позицию engine. Нажатие на линейку piano roll выполняет общий seek.
Все MIDI-клипы той же дорожки видны на общей шкале: активный светлее, остальные
затемнены и доступны только для просмотра; промежутки тёмные. Source offsets
сохраняются для нот/контроллеров и точных форм. Zoom/scroll обеих областей общий.
ПКМ больше не удаляет ноты. Двойной левый клик/Delete и общий Undo сохранены.
При вертикальном переносе ноты audition выключает старую высоту и включает новую;
отпускание мыши, Escape, скрытие и потеря фокуса выключают ноту. Один Undo на жест.
Source `68f1dc5968e89254213cfe48e899b877e1fadd1f`, только локально; schema 13 без изменений. Новые callback
allocation/locks/I/O не добавлены; используется существующая очередь audition.
Release без warnings; **113/113 CTest PASS (60.06 s)**; packaged recording/clips/live/J3 PASS.
Регрессии: отказ Record без Arm, real state UI, global timeline/Play/Pause,
ghost clips read-only, ПКМ без mutation, drag retune одной voice, Escape release PASS.
Software previews 100%/150% просмотрены. Физическая MIDI/ASIO/VST3 и mixed DPI
для fix1 NOT RUN; #68/#24 остаются открыты до пользовательской приёмки.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd1-fix1-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `D67EE9F15146E770F7A9A307FBC5F9A451854C36E1F7F31E357DA54C4D1C2090`.
0.2g/0.2h не начаты. Следующий шаг: пользователь проверяет пять исправлений,
затем продолжение Stage 4. Остальные доработки по прежней договорённости отложены.

## 0.2f upd2 — 2026-10-07

По семи новым требованиям пользователя выпущена **0.2f upd2**.
Убраны отдельные Play/Pause/Stop редактора; общий транспорт и Space сохранены.
ЛКМ по области соседнего MIDI-клипа в piano roll делает его активным в том же
редакторе. ЛКМ по audio/MIDI-клипу в Arrange переключает открытый редактор;
при закрытом редакторе только выбирает клип. Глобальная шкала, темные промежутки
и затемнённые неактивные MIDI-клипы сохранены.
Controllers open/closed — общая настройка MIDI-редактора; Attach/Detach — общая
настройка audio/MIDI-редактора. Оба флага сохраняются в локальный juce-view.json,
применяются ко всем клипам и после перезапуска. Старые settings без новых полей
совместимы: Controllers open, редактор detached. Project schema 13 не меняется.
Повторный Edit закрывает floating или attached редактор, следующий открывает его
с запомненным размещением. Смена клипа не создаёт второй параллельный редактор.
Удержание ПКМ и drag в Arrange выделяет пересекающиеся audio/MIDI-клипы рамкой;
не меняет transport/model. Delete/Backspace удаляет всю выбранную группу одной
shared Application command, один Undo восстанавливает клипы и IDs.
Record теперь может запускаться во время Play: device stop/join, фактическая позиция,
quiescent Pause, подготовка recorder/graph, Play и device start. Структурные изменения
по-прежнему запрещены при playback/recording. MIDI-only и совместная audio/MIDI
запись проверены из Stop и Play, с/без monitor, 4 processing workers, callback allocation 0.
Статус RECORDING виден текстом; ошибка автоматической остановки выводится в строку
статуса. Arm, MIDI input, hardware audio, loop off и linear append после существующих
MIDI-клипов остаются необходимы. Изменение лечит воспроизведённый отказ Play → Record;
причина конкретного аппаратного случая на скриншоте без runtime диагностики не доказана.
Пользователь по 0.2f upd1 fix1 подтвердил изменение звука при переносе и «всё остальное
работает», но сообщил повторный отказ записи при включённой R. Общая hardware
приёмка записи и текущих новых требований не объявляется пройденной.
Source `b2975843459c20685e9d20364787ee3159be386b`, только локально. Release без warnings; **113/113 CTest PASS (61.22 s)**;
packaged recording/clips/live/J3 PASS. UI регрессии: shared settings roundtrip,
ghost/Arrange activation, Edit close/reopen attached/detached, mixed clip rectangle,
Delete group/one Undo, отсутствие отдельных transport buttons PASS.
Software previews 100%/150% просмотрены. Физическая Komplete/MIDI/ASIO/VST3,
перезапуск пользовательского приложения и Windows mixed DPI для upd2 NOT RUN.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd2-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `31D3B7114017C018DF9516640EBAF5E3C1829E5D8404BB9FED248B3EC188D161`. #68/#24 открыты до приёмки.
Следующий шаг: пользователь проверяет семь изменений, особенно R → Play → Record
и R → Record из Stop. 0.2g/0.2h не начаты; остальные доработки после Stage 4.

## 0.2f upd3 — 2026-10-07

По четырём замечаниям пользователя выпущена **0.2f upd3**.
Запись MIDI больше не запрещена до конца существующих клипов: новый take добавляется
в выбранной позиции, включая перекрытие; прежние клипы не удаляются. Loop recording
ещё не реализована: loop off, R, выбранный MIDI input и работающий hardware audio
остаются требованиями. Ошибки запуска/остановки отображаются в строке статуса.
Start/Stop записи сохраняют существующие track/master PreparedGraph/VST3 instances,
когда chains и device config неизменны. Не выполняется новый probe/load/restore плагинов.
После stop/join callbacks материализуется take, выполняется один command/Undo и
обновляется playback graph с теми же processors. Clip-only Undo/Redo и grouped move
также сохраняют runtime instances; plugin/track/device edits используют прежний rebuild.
Не требуется сериализовать plugin state ради clip-only recording commit; Save/capture
по-прежнему читает состояние действующих processors. Schema 13 без изменений.
Desktop очищает красный recording preview синхронно после завершения Stop, вместо
ожидания timer; лишняя пауза загрузки VST3 устранена. Stop/join driver, audio file flush
и обработка большого take остаются необходимы; нулевая hardware latency не обещается.
ЛКМ в пустой Arrange-области + drag выделяет audio/MIDI-клипы рамкой. ПКМ рамку
больше не создаёт. Обычный клик без drag сохраняет seek по пустой области.
ЛКМ на выбранном клипе сохраняет группу и перемещает её целиком. Preview локальный;
release выполняет Application::move_clips одной атомарной command/Undo. Общий
sample shift сохраняет относительные позиции, MIDI ticks вычисляются Timeline;
snap по якорному клипу, общая граница timeline, track shift сохраняет относительные
номера дорожек. Несовместимые destination tracks отклоняются атомарно. Escape отменяет.
Source `6a38c765a2c884271a45628862664fa4f371a446`, только локально. Release без warnings; **113/113 CTest PASS (60.73 s)**;
packaged recording/clips/live/J3 PASS. Десять повторных Record/Stop в одной позиции
поверх существующих clips, stable insert_generation Start/Stop/Undo/Redo, immediate
preview clear PASS. MIDI/audio recording, monitor on/off, 4 workers и callback allocation 0 PASS.
Mixed group selection/move preview/commit, relative shift, one Undo/Redo, Escape и
atomic negative-start rejection PASS. Software previews selection/move 100%/150% просмотрены.
Физическая Komplete Kontrol/ASIO/VST3 и Windows mixed DPI для upd3 NOT RUN.
Пользователь подтвердил звук при переносе в upd1 fix1; в upd2 сообщил повторные отказы
Record и задержку red → blue. Эти аппаратные жалобы остаются на ручной проверке,
не объявляются полностью закрытыми лишь по offline regression PASS.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd3-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `73C70F432A099AF12B854D484E4F418A9A6A8833240A2A2B597BDB52EB479359`. #68/#24/#65 открыты до соответствующей приёмки.
Следующий шаг: пользователь проверяет повторную запись в выбранной позиции,
Stop → blue clip, ЛКМ selection и grouped move. 0.2g/0.2h не начаты.
