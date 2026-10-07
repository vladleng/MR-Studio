# 0.2f / Stage 4f — MIDI controller events

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

## Изменения 0.2f upd1 — 2026-10-07

Панель встроена под piano roll. CC/bend/pressure: прямые отрезки; Program
и CC64/65/66: ступени. Playback сохраняет дискретные события без дополнительной
интерполяции. [Редакторы, dock и preview записи](EDITOR_WORKFLOW.md).

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
