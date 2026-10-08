# Правила версионирования Moon River Studio

## 0.2f upd8 — 2026-10-08

Upd7 принята: «По всем предыдущим пунктам тесты пройдены».
Пять заключительных уточнений Arrange: размер ruler font, ruler-only seek,
right-only Split selection, bracket trim с source bounds, growing audio Record
preview. MIDI empty-prefix extension сохраняет позиции source notes/events.
Schema 13 без изменений; preview не сохраняется. Не начало 0.2g/0.2h.
Контракт: [ARRANGE_FINAL_UPD8](ARRANGE_FINAL_UPD8.md).

## 0.2f upd7 — 2026-10-08

Upd6 принята пользователем: «Теперь все в порядке. Тест пройден!».
Новая локальная поставка — пять уточнений Arrange/audio: плейхэд до ruler,
nearest seek и ±6 px bar capture, Record в Media без WAV chooser, прозрачность
audio/MIDI 0.72 и smooth cached waveform envelope. Без изменения schema 13,
PCM/DSP и callback; initial unsaved audio session автоматически получает проект.
Контракт: [ARRANGE_AUDIO_UPD7](ARRANGE_AUDIO_UPD7.md). Не начало 0.2g/0.2h.

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

## 0.2f upd1 — 2026-10-07

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

## 0.1m upd1 fix1 — local compatibility/UI update, 2026-10-04

Requested after the user found Nuro loading and TH-U preset-reopen issues in 0.1m.
Vendor folders start collapsed. Arrange / Edit / Mix are at the bottom right with
BROWS; hiding the sidebar expands the arrangement. VST3 layout negotiation includes
all declared audio buses (inactive auxiliary/sidechain buses remain unassigned).
Opaque plugin state is authoritative: no blanket parameter replay on activation or
save; complete legacy parameter snapshots are ignored on restore in favor of opaque
state, while sparse explicit host overrides remain supported. Pending host/editor
controls are flushed with zero samples while callbacks are stopped before state capture.
Local source `d5015f2043629178291f3d4824371d293ddf6bf8`, branch `mrs/0.1m-upd1-fix1-local`.
No project schema change (v10). Local package: MR-Studio-0.1m-upd1-fix1-ASIO-Windows-local.
87/87 CTest and hidden GUI checks passed. Installed Nuro Audio effects and TH-U passed
load, finite processing and opaque-state restore checks. TH-U selected-preset auditory
comparison in the user's project remains pending. Stage 3c stays open until acceptance.
Code/builds local; GitHub issues/docs only, no code push/PR/merge/Actions/install.
See [VST3 workflow](VST3_HOSTING.md) and [3c checklist](MRS_STAGE_3C_CHECKLIST.md).

## 0.1m / Stage 3c — local build ready, 2026-10-04

Cab IR: mono/stereo WAV import, embedded kernel, live Mix/Gain/low-high cuts/polarity,
Neutral/Warm/Bright control presets, zero additional algorithmic convolution latency.
Right VST3 tab: vendor folders, drag/drop to mixer tracks/buses/Master and single-click
insert native editor. Native insert workflow retained; structure edits after Pause/Stop.
Core v10 reads v1–v9; keep a project backup before saving. Code
`8c805259541c39f2eee59ebba2c38e87cdeae706`, local branch `mrs/0.1m-cab-ir-browser-local`.
Offline-dependency local ASIO configure/build and 87/87 CTest passed; hidden GUI tests
cover actual tree drag notifications, invalid/cancelled drops, track/bus/Master targets,
Undo, single-click editor/reuse, Cab controls/presets and existing DPI/flicker regressions.
Package `MR-Studio-0.1m-ASIO-Windows-local` includes owned synthetic test WAVs.
Celestion WAV is obtained separately via user email subscription; no signup or redistribution
performed and no Celestion-specific audition claimed. Physical user check of 0.1m pending.
Latest accepted: 0.1l / 3b. Whole #23 open; 3d/3e planned, #16 deferred.
Code/builds local; GitHub issues/docs only; no code push/PR/merge/Actions/install.
See [Cab IR](CAB_IR.md), [3c checklist](MRS_STAGE_3C_CHECKLIST.md).

## 0.1l / Stage 3b — локальная сборка готова, 2026-10-04

VST3 effects: scan/cache в отдельном процессе с timeout, загрузка/удаление/bypass,
окно плагина, generic параметры, opaque component/controller state и reported latency.
Channel EQ объединяет три bell-полосы и HP/LP: суммарная кривая, точки frequency/gain,
Q колесом, band enable/bypass и параметры во время playback без остановки устройства.
Фейдеры track/bus/Master и pan двигаются относительно исходного значения от ручки;
нажатие на шкалу не меняет значение. Ctrl — точная регулировка.
Core v9 читает v1–v8; сохраните копию проекта перед сохранением новой версией.
Код 95c719fcd0893652f92a93beb9b20cb538cd84b9, ветка mrs/0.1l-vst3-eq-local.
Локальные configure/build, 85/85 CTest, GUI smoke и previews пройдены. Установленный
Blue Cat Gain 3 Stereo проверен на обработку, gain automation, state roundtrip и editor.
Пакет MR-Studio-0.1l-ASIO-Windows-local в Builds. Последняя принятая — 0.1k / 3a;
0.1l / 3b ожидает пользовательской приёмки. #23 открыт; #16 остаётся отложенным.
Следующие подэтапы: 3c Cab IR, 3d Amp/Preamp, 3e Processing Reliability.
Код/пакеты только локально, GitHub только issues/docs; без code push/PR/merge/Actions.
См. [VST3](VST3_HOSTING.md), [Channel EQ](NATIVE_INSERTS.md),
[приёмка 3b](MRS_STAGE_3B_CHECKLIST.md).


## 0.1k / Stage 3a accepted — 2026-10-04

Пользователь подтвердил: «Проверил, вроде все работает, закрывай под-этап.»
Подэтап 3a (Native Inserts) завершён; принята локальная сборка 0.1k, включая плавную вертикальную прокрутку и Space Play/Stop.
Принятый код: 2393797e41f093219f957e7f1a67271b7579917a. Локальные configure/build, 77/77 CTest и GUI smoke прошли ранее; для отметки приёмки проверки не повторялись.
Полный Stage 3 / #23 остаётся открытым. Следующий подэтап — 3b VST3; его реализация ещё не начата. #16 остаётся отложенным.
Код и сборки остаются локальными; GitHub только issues и документация, без code push/PR/merge/Actions.


## 0.1k / Stage 3a готова локально — 2026-10-04
Stage 3 разделён по запросу пользователя: 3a Native Inserts; 3b VST3;
3c Cab IR; 3d Amp/Preamp/model foundation; 3e Processing Reliability.
Отдельная локальная сборка и приёмка для каждого подэтапа; следующий ещё не начат.
0.1k: insert chains на дорожках/шинах/Master, Gain/High-pass/Low-pass/one-band EQ,
add/remove/reorder/bypass, параметры, Undo/Redo и сохранение. Plain wheel: 32 logical
pixels/notch с fractional deltas; Space: Play/Stop с возвратом к старту, без autorepeat.
Core v8 читает v1–v7. Изменение эффектов после Pause/Stop; запись остаётся raw.
Local cached offline-dependency ASIO configure/build, 77/77 CTest, expanded GUI smoke
и UI/editor previews пройдены. Пользовательская/physical приёмка 0.1k ожидается.
Код 2393797e41f093219f957e7f1a67271b7579917a; ветка mrs/0.1k-native-inserts-local,
пакет MR-Studio-0.1k-ASIO-Windows-local. Последняя принятая — 0.1j; #22 завершён.
План: [Stage 3](MRS_STAGE_3_PLAN.md), [inserts](NATIVE_INSERTS.md),
[приёмка](MRS_STAGE_3A_CHECKLIST.md). #23 открыт; #16 matrix остаётся отложенной.
Код/пакеты локально; GitHub только issues/docs. Без code push/new PR/merge/Actions.
Исторические статусы ниже заменены этой записью.

## 0.1j принята; Mixer / Routing завершён — 2026-10-04
Пользователь: «Все проверил, все работает!». Последняя принятая локальная сборка —
0.1j / Stage 2e; код 43fe2b510f14e77453a5e5f68e0659621f0c80bf,
пакет MR-Studio-0.1j-ASIO-Windows-local. Mono/stereo inputs, simultaneous multitrack
capture, R/I на дорожках, L/R meters и Stop return приняты. Ранее прошли локальные
configure/build, 73/73 CTest и GUI smoke; при отметке приёмки проверки не повторялись.
Весь #22 Mixer / Routing завершён. #16 длительная matrix остаётся отложенной.
Следующий этап — #23 Plugins / Native DSP. Предлагаемый первый подэтап 0.1k:
insert chains на дорожках/шинах/Master, add/remove/reorder/bypass, native utility
gain/filter/EQ, параметры, Undo и сохранение. Далее VST3 workflow, IR и amp foundation.
Реализация следующего подэтапа ещё не начата. Старые pending/not-started статусы ниже
заменены этой приёмкой. Код/пакеты локально; GitHub issues/docs only;
приёмка не разрешает code push/new PR/merge. GitHub Actions не используются.

## 0.1j / Stage 2e — локальная реализация готова, 2026-10-04
Mono/stereo inputs на дорожках, одновременная запись нескольких дорожек,
независимые R (Arm) / I (Monitor) рядом с M/S, отдельные L/R шкалы stereo.
Stop возвращает к старту Play/Record; Pause сохраняет текущую позицию.
Core v7 читает v1–v6; input/Monitor сохраняются и используют Undo; Arm — session-only.
Offline-dependency Windows x64 ASIO configure/build, **73/73 CTest** и expanded GUI
smoke пройдены, UI preview проверен. Пользовательская/physical ASIO приёмка ожидается.
Последняя принятая сборка — 0.1i upd1; исторические статусы ниже заменены этой записью.
Пакет MR-Studio-0.1j-ASIO-Windows-local в Builds; ветка mrs/0.1j-multi-input-local.
См. [inputs](INPUT_ROUTING.md) и [checklist](MRS_STAGE_2E_CHECKLIST.md).
#22 открыт до приёмки; затем #23 Plugins / Native DSP. #16 matrix остаётся отложенной.
Код/сборки локально; GitHub только issues/docs. Без code push/new PR/merge/Actions.

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
