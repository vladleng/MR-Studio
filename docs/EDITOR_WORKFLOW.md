# Редакторы и визуализация записи

## 0.2f upd6 — 2026-10-08

Стартовый Project home показывает кликабельные `.mrsproject` из `Документы/MR Studio`
и вложенных каталогов; папки не показываются. Фоновый поиск, сортировка по изменению,
фильтр имён/путей, New/Open/Refresh и File → Project home → Continue current project.
Загрузка/ошибки/подтверждение несохранённых изменений используют прежний Application.
Плейхэды Arrange, piano roll и controller lane — отдельные прозрачные компоненты
на 60 Hz, с дробными координатами и общей UI-интерполяцией к позиции движка.
Обновление содержимого/метеров осталось прежним; сглаживание не меняет аудиовремя,
события MIDI, запись, callback/DSP, Undo или schema 13. Stop/Pause и loop/seek
контролируются точной engine snapshot, зависший аудиоклок не ведёт курсор вперёд.
Полный контракт и границы: [PROJECT_HOME](PROJECT_HOME.md).
Пользователь принял upd5: «Тесты прошли успешно»; точные режимы/длительности не перечислены.
Проверки и пакет upd6 — в актуальном [PROJECT_CONTEXT](PROJECT_CONTEXT.md).

## 0.2f upd5 — 2026-10-07

По четырём новым замечаниям выпущена **0.2f upd5**.
Mouse-up audition имеет хвост 70 ms: около 35 ms удержания и 35 ms линейного fade out,
затем sample-offset Note Off. Короткая огибающая действует только на paused/stopped
isolated audition track; live held notes, CC64/66, недавний MIDI input (200 ms),
активный audio monitor и Play используют обычный Note Off без изменения live signal.
Новый audition/hardware input/Play отменяет хвост, pending old off идёт перед new on;
retune/active gesture cancellation сохраняют немедленный Note Off.
RT: fixed-capacity queue + arrays/bitsets, callback prepares per-track state,
единственный channel worker продвигает envelope и joins до следующего callback.
Prepare/Stop/panic очищают состояние; нет новых host allocation/locks/I/O в callback.
Preview tail не записывается в take/проект, схема 13 не меняется.
Допуск бокового движения vertical note drag увеличен с 3 до 12 logical px;
ниже порога off-grid start сохраняется. Horizontal drag по-прежнему snap anchor start.
Высоты docked editor и Mix независимы: juce-view.json editorHeight (default/min 500)
и mixerHeight (default 365/min 260); общая доступная высота ограничивается окном.
Старые view settings без editorHeight читаются с default 500. Floating size отдельно.
F2 toggle Edit, F3 toggle Mix, F5 toggle browser; работают в основном окне и
через MIDI/audio editor, без modified keys/modal interception; обычные text/transport shortcut guards сохранены.
Source `7a35904d0260ec807cced5b2792966c4c6d58804`, ветка `mrs/0.1q-fix3-processing-local`, код локально.
Full Release PASS без warnings; **113/113 CTest PASS (61.41 s)**;
packaged recording/clips/live/J3 PASS. Tail fade/zero/retrigger/live protection на
44.1/48/96 kHz, 256-frame callbacks/128-frame chunks, 1/4 workers и checked host
callback allocations 0 PASS. 8 px vertical jitter/time preservation, independent
panel resize/settings roundtrip и F2/F3/F5 toggles PASS. Software snapshots 100%/150%
просмотрены. Пользовательская проверка звучания на Komplete/ASIO/VST3 и mixed DPI ожидается.
Пакет `MR-Studio-0.2f-upd5-Editor-JUCE-ASIO-Windows-local`, EXE SHA256 `C766772EA358B07678AFB55F14C34842FBD0AB9FDAB1DE5B0794993D87E4CA29`.
Следующий шаг: проверить четыре изменения на пользовательской системе; 0.2g/0.2h не начаты.


## 0.2f upd4 — 2026-10-07

Пользователь принял оформление 0.2f upd3 fix1: «Все отлично!».
По семи новым требованиям выпущена **0.2f upd4**:
- MIDI notes alpha 72%, ghost 58%: сетка слегка видна, рамки отсутствуют.
- Horizontal note drag привязывает глобальное начало якорной ноты к выбранной
  сетке (Snap off сохраняет свободное перемещение). Общий сдвиг группы сохраняет
  интервалы; vertical drag с горизонтальным отклонением менее 3 logical px не
  меняет start, даже у записанной ноты вне сетки. Trim/velocity gestures сохранены.
- Up/Down в piano roll меняют pitch выбранных нот на один полутон, без изменения
  времени/длины; границы 0..127 отклоняют весь шаг группы. Один shared Undo на шаг.
- Full screen / Restore и F11 в MIDI editor. Тот же component/selection/zoom;
  attached editor временно отделяется и при Restore возвращается в панель.
  Переход fullscreen сам по себе не сохраняет временный Detach в settings.
  Fullscreen и selection — временный UI state, не поля проекта.
- Горизонтальные grid lines controller lane убраны; числовые подписи сохранены.
- ЛКМ + drag в пустой lane выделяет точки текущего вида/канала/CC рамкой;
  Ctrl/Shift дополняют выбор. Selected points белые. ПКМ не начинает жест.
- Drag выбранной точки переносит выделенную группу по времени/значению с общим
  ограничением границ clip/value и сохранением расстояний. Preview не меняет
  проект; release — один SetMidiEvents/Undo. Escape/focus loss отменяют gesture;
  stale clip отменяет commit. Delete удаляет выбранную группу одной command.
Shared model/commands/serialization/playback существующие; schema 13 неизменна.
Новых изменений callback/DSP/transport нет; editing требует Pause/Stop.
Source `20f78183b9cd77678c35deb42dd8d24dd6233bb6`, локальная ветка `mrs/0.1q-fix3-processing-local`.
Release GUI build PASS без warnings; **113/113 CTest PASS (61.56 s)**;
packaged recording/clips/live/J3 PASS. Регрессии off-grid vertical/horizontal snap,
Up/Down, CC group preview/common time shift/one Undo/Escape, floating full screen
и attached → fullscreen → attached PASS. Software Arrange/editor/controllers
100%/150% просмотрены. Physical mixed DPI и пользовательская приёмка upd4 ожидаются.
Пакет `MR-Studio-0.2f-upd4-Editor-JUCE-ASIO-Windows-local`, EXE SHA256 `B266F149AA523B27FF17FF1280AB2C445765B31214C5CB64DA24756E693C3959`.
Следующий шаг: проверить новые семь взаимодействий; 0.2g/0.2h не начаты.



## 0.2f upd3 fix1 — 2026-10-07

Пользователь подтвердил 0.2f upd3: «Теперь запись работает как надо», перетаскивание
работает и перемещение клипов отображается в редакторе. Это приёмка указанных
исправлений, а не всех remaining hardware checklist пунктов Stage 4.
Косметический фикс: MIDI clip fill в Arrange 72%, ноты piano roll 78% (ghost 65%);
сетка слегка просвечивает. Выбранные клипы/активные ноты рисуются последними,
полупрозрачная заливка показывает нижний MIDI-клип/ноты при перекрытии.
Окантовка audio/MIDI/recording preview clips уменьшена с 1.5 до 0.75 logical px,
углы скруглены на 2 logical px. MIDI notes без окантовки, velocity marker сохранён.
Hit bounds, selection/drag/trim, Undo, transport, DSP и schema 13 не менялись.
Source `2c573f0ea85b1277792078b383d906b3bba23987` локально; пакет `MR-Studio-0.2f-upd3-fix1-UI-JUCE-ASIO-Windows-local`;
EXE SHA256 `1E4701808151314524515D1EE5471DA59548A2525E20441C118C8A5EF1AB3B8A`. Release GUI build PASS без warnings; packaged recording/clips/live/J3 PASS.
Software previews Arrange/piano roll 100%/150% просмотрены. Полный 113-test CTest
для косметического фикса не повторялся: 113/113 относится к базовой upd3.
Physical mixed DPI и визуальная пользовательская приёмка fix1 ожидаются.
Следующий шаг — пользователь проверяет оформление; 0.2g/0.2h не начаты.


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
