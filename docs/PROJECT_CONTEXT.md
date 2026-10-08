# MR Studio: текущий контекст разработки

Обновлено: **2026-10-08**, Asia/Krasnoyarsk. Это основной краткий снимок для нового
чата. Постоянные правила: [AGENTS.md](../AGENTS.md). Подробная история остаётся
в тематических документах; старые верхние записи README/START_HERE не являются
текущим статусом. Проверяй ветку, файлы и более новые инструкции пользователя.

## Где работать

| Назначение | Локальный путь |
|---|---|
| Исходники, Git, CMake, tests, docs | `C:/Users/Vladislav/Documents/GitHub/MR-Studio` |
| Папка чатов, Builds, Saves, навыки | `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio` |
| Навыки проекта | Рабочая папка чатов: `.codex/skills/<имя>/SKILL.md` |
| Активная ветка | `mrs/0.1q-fix3-processing-local` |
| Существующая сборочная папка | Репозиторий: `build/asio-local` |
| GitHub для issues/docs | `vladleng/MR-Studio` |

GitHub source может отставать от локальной разработки. Не заменяй локальный код
клоном/main/pull ради синхронизации документации. Машинные пути выше — текущая
конфигурация; на другой машине сначала найди существующий checkout.

## Продукт и активное направление

Одна DAW с режимами Arrange/Edit/Mix и будущим встроенным Live Mode, единым
Project Model/Transport/Core. Активная разработка — Windows JUCE + ASIO.
Следующий фокус разработки — MIDI по решению пользователя 2026-10-06. P4
пользователь продолжает тестировать в процессе; его оставшиеся проверки не
блокируют начало MIDI. Собственные эффекты/инструменты/Amp-Preamp отложены. Пользователь стремится к уровню
эффективности Fender Studio Pro. Универсальность важнее настройки под один проект.

В пользовательском примере TH-U и Xvox находятся на разных каналах, ONE — на
Master. Не превращай их в одну последовательную цепь при оценке параллелизма.

## Актуальная сборка для продолжения: 0.2f upd6

## 0.2f upd6 — 2026-10-08

Пользователь принял upd5: «Тесты прошли успешно». Точные hardware settings,
длительности и отдельная mixed-DPI matrix этой репликой не перечислены.
По трём новым требованиям выпущена **0.2f upd6**:

- При запуске Project home вместо demo: фоновый поиск `.mrsproject` рекурсивно
  в Windows `Документы/MR Studio`, только файлы; папки/symlinks не отображаются.
  ЛКМ/Enter открывают проект. Сортировка по изменению, поиск по имени/относительному
  пути, New/Open/Refresh; одинаковые имена различаются путями. Missing/corrupt
  оставляют текущий проект и показывают ошибку. Начальный home не просит сохранять
  невидимый пустой проект. Saved audio reconnect выполняется после New/Open.
  File → Project home сохраняет текущую сессию и ставит Pause; Continue current
  project возвращает её. Прежняя защита несохранённых изменений при замене/закрытии.
- Плейхэды Arrange (включая запись), piano roll и controller lane обновляются
  отдельными прозрачными компонентами на 60 Hz, только узкие old/new полосы.
  Общая message-thread интерполяция к existing coherent engine snapshot,
  fractional samples/ticks/pixels. Обычно около одного UI frame визуального отставания,
  окно сглаживания не больше 50 ms, без ухода дальше последнего observed sample.
  Stop/Pause/backward/loop/large seek/reset немедленно re-anchor; малый forward seek
  сходится за короткое UI-окно. Tempo segments учитываются с дробными ticks.
  30 Hz meters/content polling и MIDI preview rate сохранены; callback/DSP/recording
  timestamps/audio buffering/Undo/schema 13 не менялись. UI state transient.

Source `89987a78a35694585a7f6ed84c95b759d28d7210`, локальная ветка
`mrs/0.1q-fix3-processing-local`. Cached configure/full Release PASS без warnings.
**113/113 CTest PASS (72.65 s)**; после финальных UI дополнений **J2/J3 2/2 PASS
(7.84 s)**. Packaged J3 и recording/clips/live PASS. Home nested/case-insensitive/
duplicate/filter/async/open/resume/new/missing/corrupt checks PASS. Fractional cursor
paint/old stripe clear/mouse passthrough и 44.1/48/96 kHz, 128/256/2048 frames,
60 Hz bounded monotonic display, stalls/Stop/Pause/Seek/loop/tempo changes PASS.
Software home 1200×700/1400×850 100%/150% и MIDI 150% просмотрены.
Пакет `MR-Studio-0.2f-upd6-Home-JUCE-ASIO-Windows-local` в папке чатов `Builds`;
EXE SHA256 `AFD9B7B9EAB87F6BE4C609AB95F4BA035ED4B688AEBB209FBDC2A71D1339FB44`.
Контракт: [PROJECT_HOME](PROJECT_HOME.md); [ручная проверка](MRS_STAGE_4F_CHECKLIST.md).
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физическая ASIO
плавность записи/редактора и mixed DPI ожидают пользователя. #68/#24/P4 scope
этой поставкой не закрывается. GitHub/source push/Actions не выполнялись.
Следующий шаг: проверить новый стартовый экран и плейхэды на пользовательской
системе; 0.2g/0.2h не начаты, автоматически не начинать.

## Предыдущая принятая поставка: 0.2f upd5

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

## Предыдущая поставка: 0.2f upd4

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

## Предыдущая поставка: 0.2f upd3 fix1

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

## Предыдущая поставка: 0.2f upd3

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

## Предыдущая поставка: 0.2f upd2

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

## Предыдущая поставка: 0.2f upd1 fix1

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

## Предыдущая поставка: 0.2f upd1

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

## Предыдущая поставка: 0.2f / Stage 4f

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

## Предыдущая принятая поставка: 0.2e / Stage 4e

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
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2e-Musical-edit-JUCE-ASIO-Windows-local`; EXE SHA256 `5371EFAA368955563AFCC84184E95CE573F7C8E2C720CB6178C0D891E607FE13`.
FEATURE READY WITH MANUAL CHECK. Физические ASIO/VST3 и Windows mixed DPI
для 0.2e не запускались. [Контракт](MIDI_MUSICAL_EDIT.md), [checklist](MRS_STAGE_4E_CHECKLIST.md).
#67 закрыт: пользователь «работает, приступай к 0.2f» подтвердил приёмку 0.2e и запросил 0.2f.
Дополнительные доработки отложены до реализации всех пунктов Stage 4.

## Принята 0.2d; порядок продолжения

Пользователь: «Да, все работает». 0.2d принята, #66 закрыт. Первоначальный запрос
0.2f исправлен: «0.2e начинаем, я перепутал»; 0.2f затем реализована по новому запросу после приёмки 0.2e. Пользователь
указал: «Все доработки будем делать когда все пункты stage 4 будут реализованы».
После приёмки 0.2e продолжать базовые 0.2f/0.2g/0.2h, без дополнительных upd.
Точные аппаратные режимы при приёмке 0.2d не перечислены; новых измерений нет.

## Предыдущая принятая поставка: 0.2d / Stage 4d

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
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2d-Piano-roll-JUCE-ASIO-Windows-local`; EXE SHA256 `BED650A86669056C81F1F5B471BE257D10BC925BBA1DAB6C917751C52202582D`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические ASIO/VST3
и Windows mixed DPI для 0.2d не запускались. [Пользовательская проверка](MRS_STAGE_4D_CHECKLIST.md),
[архитектура/ограничения](PIANO_ROLL.md). #66 закрыт после приёмки «Да, все работает»; 0.2e реализована следующим запросом.

## Пользовательская приёмка 0.2c fix1

Пользователь: «Вроде все работает» и «Поехали 0.2d». На присланном скриншоте
два MIDI take, connected Komplete Kontrol и Komplete Audio ASIO. Визуальный фикс
и показанный рабочий сценарий приняты; равные transport bounds включены в 0.2d.
Отдельные sustain/bend, joint recording, unplug и полный 4c hardware checklist
не перечислены; #65 остаётся открытым для этих проверок. #24/#54/#16 не закрываются.

## Предыдущая поставка: 0.2c fix1

По запросу пользователя: transport buttons рисуют только SVG без фона/рамки;
области нажатия, layout, tooltips и accessibility names сохранены. Hover/press,
keyboard focus и toggle отражаются цветом glyph. Названия треков/каналов, dB,
кнопки и insert names в Strip используют общий с VST3 tree системный шрифт 13 px
(ранее Noto Sans 13 pt). Остальная типографика не изменена.
Source `d3da9f739915103e9952a641c154b167cf436cbf`, только локально. Release GUI build PASS без предупреждений;
packaged J3 PASS (MIDI 4a/4b/4c, transport/Arm/Record/Stop/Undo/Redo).
Software Arrange/Mix previews 100%/150% просмотрены. Полный CTest не повторялся:
113/113 PASS относится к базовой 0.2c. Физические MIDI/ASIO и Windows mixed DPI
для косметического фикса не запускались; визуальная приёмка впоследствии получена (см. выше).
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2c-fix1-UI-JUCE-ASIO-Windows-local`; EXE SHA256 `DA313BC3E7A1E176B26DC60AD16ACC93BC67947C3E9A4A671E46563A53CAC236`.
0.2c/#65 ещё не принята целиком; 4d не начат, schema 13 и engine без изменений.

## База фикса: 0.2c / Stage 4c

По прямому запросу пользователя после приёмки 0.2b реализованы MIDI recording
и лёгкое обновление transport SVG. 0.2c пока не принята пользователем; #65/#24 открыты.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2c-MIDI-recording-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`; scanner рядом. Предыдущие пакеты сохранены.
- EXE SHA256: `E168C92CF6D1E826C9B279882F094D7F2B77AB3A12B9E5C705F0C4C27D18CE24`.
- Source `f5247ede6775657f6067109047b7786af2f849b8`, только локально,
  ветка `mrs/0.1q-fix3-processing-local`. Принятая компактная UI-база сохранена.
- Instrument Arm R, live capture с независимым monitor, notes/velocity/channel,
  CC/sustain/pitch bend/program/pressure. WinMM timestamp → transport samples →
  PPQ ticks по общей tempo map. Открытые ноты/педаль закрываются при завершении.
  Общая audio/MIDI сессия — один Undo; save/reopen и контроллеры поддерживаются.
- Линейная запись: новая instrument track либо курсор после всех её MIDI clips;
  Loop/overdub/punch не включены. Только MIDI — Record без WAV chooser, совместная
  запись выбирает WAV для audio. Offline clock не записывает. Seek/loop/structural
  edits/save/device changes во время записи запрещены. UI close/project change
  завершает запись до предложения Save. Unsaved MIDI не имеет crash journal.
- Core schema **13**, читает 1–12; старые builds не читают 13. Использовать копии
  проектов. Event IDs/trim/split/duplicate/Undo сохранены; clip controller chase
  и boundary release добавлены. Controller lanes — 4f, piano roll — 4d.
- Семь transport buttons используют предоставленные SVG из `assets/ui/icons/transport`,
  встроенные в BinaryData. Размер glyph 20 logical px, прежние hit regions/layout,
  tooltips/accessibility names сохранены. Остальные SVG не включают новые функции.
- Cached configure/full Release PASS; финальные **113/113 CTest, 58.60 s**.
  Packaged midi_recording/midi_clips/midi_live PASS; hidden J3 exit 0.
  Software previews 100%/150% просмотрены. В проверенных RT callbacks host allocation 0.
  Проверены Workers 1/2/4 + mixed Process Buffer, input loss/overflow, timestamp
  clamp, controller chase, live event order, raw audio и сохранение MIDI.
- FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические A49/ASIO,
  SWAM и Windows mixed DPI не запускались в этом чате. Timestamp arithmetic измерен
  synthetic-тестом (100 ms/48 kHz → 4800 samples); WinMM jitter/driver latency не
  калиброваны. Подробнее: [MIDI_RECORDING](MIDI_RECORDING.md).
- Следующий шаг: [checklist 0.2c](MRS_STAGE_4C_CHECKLIST.md): Arm → Record → игра →
  Stop → MIDI take → Play, sustain/bend, Undo/save/open, joint audio/MIDI и unplug.
  4d не начинать автоматически. Chord/Arranger — Stage 5; metronome/precount — 4h.
  P4/#54/#16 и отдельная multi-instance VSTi идея остаются самостоятельными задачами.

## Предыдущая принятая поставка: 0.2b / Stage 4b

По запросу «Поехали дальше, работаем над 0.2b» реализованы MIDI-клипы и playback.
Пользователь принял 0.2b: «Тест пройден, идем дальше. Работаем над 0.2c». #64 закрыт; #24 открыт. Точные аппаратные режимы не перечислены. 4c реализован по следующему запросу; текущая поставка описана выше.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2b-MIDI-clips-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`; scanner рядом. Прежние версии сохранены.
- EXE SHA256: `2418BB4F9D31E5B816EB97327820007578BC05D61B7B89D4C6B0943486A219F6`.
- Source `1c26828606c19d1f298417f3d1edf7821b7e4a67`, текущая локальная ветка
  `mrs/0.1q-fix3-processing-local`. Включает принятую UI-базу 0.2a upd1 fix1.
- Clip/note IDs, PPQ 960, pitch/start/length/velocity/channel; создание и простой
  note form, move/trim/split/duplicate/delete, Undo/Redo, Save/Save As/reopen.
  Общая tempo map, tick-сетка и Transport → Project tempo. Это минимальный ввод
  нот; полный piano roll — 4d, MIDI recording — 4c.
- Playback: block-relative offsets, chasing после seek/resume, Pause/Stop/loop,
  overlap policy и bounded overflow. MIDI clips остаются device-owned с Workers
  и Process Buffer; перед заменой графа обработчики останавливаются/присоединяются.
  В проверенных новых callback paths host allocation 0. RT SAFE WITH MANUAL CHECK.
- Core schema **12**, читает 1–11 с пустым MIDI payload. Старые сборки не читают
  12: работать на копиях проектов. Audio asset persistence пропускает MIDI clips.
- Cached configure/full Release PASS. Финальные **112/112 CTest, 58.30 s**.
  Packaged `midi_clips`, `midi_live`, hidden `--j3-smoke` PASS/exit 0.
  Software Arrange/note form previews 100%/150% проверены; физический DPI/ASIO,
  Komplete Kontrol/SWAM и длительная матрица не запускались в этом чате.
  FEATURE READY WITH MANUAL CHECK. Подробности/границы: [MIDI_CLIPS](MIDI_CLIPS.md).
- Следующий шаг: пользовательский [checklist 0.2b](MRS_STAGE_4B_CHECKLIST.md):
  instrument → Edit → Create MIDI clip at cursor → Add note → Play;
  редактирование после Pause/Stop, операции над клипом, tempo/loop/save/reopen,
  ASIO и Process Buffer Off/1024/4096 с Workers 1/2/4.
  4c не начинать автоматически. Chord/Arranger — Stage 5; metronome/precount — 4h.
  P4/#54/#16 и полный checklist 4a этой поставкой не закрываются.

## Предыдущая принятая визуальная база: 0.2a upd1 fix1

**Принята пользователем 2026-10-06**: «Отлично, задокументируй, чтобы другой чат
мог продолжать с этого билда». Это приёмка поставленной дизайн-доработки;
полный MIDI checklist и P4 этой репликой не закрываются.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2a-upd1-fix1-Mixer-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`, scanner рядом. Предыдущие пакеты сохранены.
- EXE SHA256: `DC5DD5DE8191055E1E1B666A051D85EC5D1EC231C16D52C1D3EC993FD9E50009`.
  Пакет содержит licenses, README, VALIDATION, SHA256.json и smoke previews.
  Не переписывать выпущенный пакет для новой записи о приёмке.
- Принятая визуальная база: Noto Sans 13 pt, как заголовок проекта, для кнопок,
  меню, подписей треков/микшера и timeline; длинные button labels имеют ellipsis
  без автоматического уменьшения шрифта. Тёмная серо-синяя палитра, выделенный
  header/selected state. Обычные и Master strips 86 logical px с шагом 88 px;
  ручка вертикального фейдера 16 px, meter/fader сближены. Транспорт центрирован,
  CPU расположен слева внизу. Ещё не реализованные функции не имитируются.
- Исходники в существующем checkout, ветка `mrs/0.1q-fix3-processing-local`.
  HEAD `38e89e738b5b494b90d56ba0093c45d4ffbc6891` **не включает UI-доработку**:
  на момент той поставки она была в рабочем дереве. В 0.2b она сохранена и включена в source commit. Исторические изменения:
  `apps/studio-juce/src/Controls.h`, `apps/studio-juce/src/Desktop.cpp`,
  `apps/studio-desktop/include/mrs/version.hpp` и документацию.
  Не сбрасывать checkout и не заменять его remote-кодом.
- Проверки: локальная Release-сборка MoonRiverStudioJuce PASS; packaged
  `--j3-smoke --fixture mrs_vst3_fixture.vst3` exit 0 после финального изменения
  шрифта. Software preview 150% финального пакета просмотрен; 100%/150%
  проверялись в ходе итерации. CTest J3 1/1 PASS (2.36 s) относится к предыдущей
  итерации 116 px. Полная suite для финальной косметической версии не повторялась.
  Физический DPI, screen-reader и реальный ASIO не проверялись в этом чате.
- Audio engine, routing, Undo, schema 11 и plugin lifecycle не менялись;
  исправления 0.2a fix1 входят в этот исторический пакет. Текущая поставка — 0.2b выше.

## Точное состояние этапов

| Область | Состояние |
|---|---|
| P1 / [#51](https://github.com/vladleng/MR-Studio/issues/51), 0.1r | Принят 2026-10-05, issue закрыт. При 128 frames регулярные щелчки исчезли, редкие остаются |
| P2 / [#52](https://github.com/vladleng/MR-Studio/issues/52), 0.1s | Принят 2026-10-06, issue закрыт. Первый native whole-graph anticipation slice |
| P3 / [#53](https://github.com/vladleng/MR-Studio/issues/53), 0.1t | Принят 2026-10-06: «P3 вроде стабилен. Переходим дальше». Issue закрыт для поставленного mixed slice; длительная matrix остаётся P4/#16 |
| 0.1t fix1, редакторы плагинов | Принят пользователем 2026-10-06: «теперь с плагинами все как надо». Bypass, порядок окон и Pin подтверждены |
| P4 / [#54](https://github.com/vladleng/MR-Studio/issues/54) | 0.1u software slice принят пользователем 2026-10-06. #54 открыт для оставшихся проверок; пользователь тестирует P4 параллельно MIDI |
| Parent Stage 3 / #23, длительная matrix #16 | Не закрыты этой приёмкой |
| 3e2 / 3e3 | Безопасные переходы/dynamic latency и recovery/isolation остаются отдельными задачами |
| 4c / [#65](https://github.com/vladleng/MR-Studio/issues/65), 0.2c | Реализован локально, 113/113 CTest/package PASS; пользовательская приёмка ожидается |
| 4b / [#64](https://github.com/vladleng/MR-Studio/issues/64), 0.2b | Принят: «Тест пройден, идем дальше»; #64 закрыт. Режимы аппаратного теста не перечислены |
| 4a / [#63](https://github.com/vladleng/MR-Studio/issues/63), 0.2a fix1 | Fix1 принят 2026-10-06: «Все работает». Полный checklist 4a отдельно не подтверждён; #63 открыт |

В пользовательской переписке «0.1c показывает хорошие результаты» было принято
как подтверждение тогдашней 0.1s/P2; не возвращай roadmap к старой 0.1c.

## Предыдущая функциональная поставка: 0.2a fix1

- **0.2a fix1**, 2026-10-06. Пользователь подтвердил загрузку SWAM Alto Flute 3,
  MIDI connected и звук через Komplete Audio ASIO / B 128 / Workers 2 после
  отключения Process Buffer («DSP»). После поставки fix1 пользователь сообщил
  «Все работает» — fix1 принят. Конкретные режимы повторной проверки не перечислены;
  полный checklist 4a отдельно не подтверждён.
- Устранена причина тишины с Process Buffer: live MIDI flags не копируются в
  ahead producer graph. Инструмент/downstream остаются device-owned; producer
  больше не отклоняет свой граф по anticipation_safe и не выставляет этот fault.
- MIDI status сохраняет имя назначенного порта/канал и показывает audio disconnected
  отдельно от Off. Длинный текст имеет ellipsis с читаемым шрифтом и полный tooltip.
  Offline clock не выводит звук и не даёт hardware CPU/latency; ASIO подключать явно.
- Открытие plugin editor без runtime готовит offline graph после Stop; отсутствие
  runtime больше не выдаётся за отсутствие GUI. Native и generic parameter windows
  owned by DAW HWND; editor generation обновляется после lazy prepare.
- Source `8e2c7d48f7a8857469a791ea4ddb18721983894b` (основной fix `0899d1c`),
  та же локальная ветка. Configure/Release прошли. **111/111 CTest, 58.10 s**;
  после финальной правки шрифта **4/4 focused, 4.42 s**. Packaged J3 и MIDI — exit 0.
  Первый GUI test падал из-за отсутствующего DAW peer после J3 cleanup; fixture
  добавляет scoped peer, повторный общий прогон зелёный. Snapshots 100%/150% просмотрены.
- MIDI regression: live Notes/Play/panic при Workers 2/4 × Process 1024/4096,
  host callback allocation 0. GUI: disconnected native view, HWND owner обеих
  панелей, сохранённый MIDI port/status. Установленный SWAM в этих tests не запускался;
  физические MIDI/ASIO устройства не открывались.
- Пакет `Builds/MR-Studio-0.2a-fix1-MIDI-JUCE-ASIO-Windows-local`; старый 0.2a сохранён.
  EXE SHA256 `F857D4BD6BB8C6BE95BA2B151E012A18EEAA03966EC83924F752C38CDAC52D90`.
  Проверены совпадение Release/пакета и SHA256 manifest. Core schema остаётся 11.
- Следующий шаг: продолжение MIDI по следующему запросу пользователя; оставшийся
  [checklist](MRS_STAGE_4A_CHECKLIST.md) не отмечать пройденным без свидетельства.
  #63/#24 остаются открытыми, 4b не начат. Приёмка fix1 записана в #63.
- Уточнение о многопоточности: Workers распараллеливает независимые каналы,
  включая instrument tracks. Один вызов DSP экземпляра плагина имеет одного
  владельца; host не делит его между workers. Live MIDI остаётся device-domain,
  Process Buffer не вычисляет его заранее. Внутренний multicore SWAM не подтверждён.
- Fix1 docs-only GitHub main `e08b5ce77dcd476c54cb81ab24d09b6f9db5f474`
  опубликован с `[skip ci]`, remote SHA проверен; #63 обновлён и открыт.
  Source-ветка не отправлялась, Actions не запускались. Пакет заморожен с docs
  `88a0fae`, manifest 118 файлов проверен; запись синхронизации пакет не меняет.

## Исходная поставка 0.2a

- **0.2a / Stage 4a**: MIDI input Windows, instrument tracks, VST3 event input,
  monitoring при Stop/Play, общий mixer/buses/Master/PDC, panic и missing status.
- Пакет: `Builds/MR-Studio-0.2a-MIDI-input-JUCE-ASIO-Windows-local` в папке чатов.
  Запуск: `Moon River Studio JUCE.exe`; scanner рядом. Старые пакеты сохранены.
- Source: `ee55afc9384c99695c8e582d51da60cabc681616`, ветка
  `mrs/0.1q-fix3-processing-local`, только локально.
- Cached configure/Release build прошли. Финальный **111/111 CTest, 57.15 s**;
  packaged hidden J3 с MIDI smoke — **exit 0**. MIDI snapshots 100%/150% просмотрены.
  SHA256 EXE: `654675B8BB474F3E617B9FF829B7C129E1683D5C9AAA810157E0E343116D402F`.
  EXE совпадает с Release; package SHA256 manifest включает файлы/логи/документы.
- `midi_live`: decoder, Notes/velocity/sustain/bend, controller burst/release,
  Stop/panic/overflow/stale generation, short blocks, bus/gain/PDC, save/open,
  Undo/Redo и missing plugin/port. В проверенных host callbacks allocation 0.
  Первый общий прогон: 109/111, два parallel-теста падали из-за stack overflow;
  MIDI buffers перенесены в heap на stopped prepare, финальный прогон весь зелёный.
- Core schema **11** читает 1–10; MIDI input/channel/monitor и instrument/state
  сохраняются. Старые сборки не читают 11 — проверять на копиях проектов.
- Basic live MIDI имеет offset 0 следующего callback; sample-accurate timestamp
  recording не реализован. Controllers зависят от IMidiMapping инструмента.
  Структура цепи и MIDI input/channel/monitor меняются после Pause/Stop.
- Физические MIDI/ASIO устройства тестами не открывались. [Workflow/RT contract](MIDI_LIVE_INPUT.md),
  [пользовательский checklist](MRS_STAGE_4A_CHECKLIST.md). #63/#24 открыты;
  приёмка пользователя ещё не получена. P4/#54/#16 этим выпуском не закрываются.

## Предыдущая принятая поставка — 0.1u

- Пакет: `Builds/MR-Studio-0.1u-P4-profiling-JUCE-ASIO-Windows-local` в папке чатов.
- Запуск: `Moon River Studio JUCE.exe`; `mrs_vst3_scan.exe` рядом.
- Source commit: `ef11071bd221e30f822c2fd09e4689bfbf0b98ad` (только локально).
- SHA256 EXE: `3B75C8CF1886CC983544254171D6B9499D515E7CA0500F193BE50CF874DBCBA9`.
- Cached configure/full Release прошли. **110/110 CTest, 57.29 s**;
  финальные profiling/J3 проверки — 2/2, 1.71 s. Packaged hidden J3 — exit 0.
- 112 канал-конфигураций OFF + 112 ON: совпадают checksum; worker timeout 0,
  Late 14/13. Playback/mixed — 210 OFF + 210 ON, 67 200 точных PCM-блоков;
  producer/disk/worker misses 0, Late 13/13. Все выбросы сохранены в CSV.
- По минуте streamed mixed raw recording и playback при 48 kHz / Device 128,
  Workers 4, profiling ON: по 22 540 точных блоков, Late/U/D/timeouts 0.
  Запись: 2 885 120 mono raw frames, каждый равен исходному входу, fault 0.
  Это paced synthetic callbacks; физический ASIO не открывался.
- Панель/CSV проверены, software snapshots 100%/150% просмотрены. Пакет имеет
  SHA256 manifest; EXE совпадает с Release. Старые 0.1t/fix1 и другие версии сохранены.
- [Методика](ENGINE_PROFILING.md), [замеры и ограничения](ENGINE_PROFILING_BENCHMARK.md).
  Результаты не доказывают Fender parity, общий CPU speedup или отсутствие щелчков.

## Существенные границы реализации

P1 обрабатывает независимые каналы параллельно и сводит результаты детерминированно;
зависимые buses/Master ждут входы. P2 заранее готовит подходящий native playback.
P3 разделяет независимый native playback и live/shared DSP через ограниченную
PCM-очередь; фактические DSP/PDC/source-cursor имеют по одному владельцу.

Process Buffer по умолчанию Off. Для mixed нужны Workers >= 2 и подходящие
prerecorded native-каналы; Process 1024/4096 не меньше Device Buffer. VST3,
live/shared buses и Master обрабатываются напрямую. VST3 anticipation не реализован;
TH-U/Xvox/ONE-only граф не получает обещания нового ускорения сверх P1.
Mon — оценка driver I/O + fixed PDC, не измеренная акустическая задержка.
Недостающий playback PCM не останавливает live/capture/device time; восстановление
может обрезать tails. Seamless history/crossfade и native crash/hang isolation не готовы.

0.1t fix1: редактор — owned top-level окно Windows над MR Studio; Pin сохраняет
его при открытии другого плагина, закрытие одного не закрывает остальные.
Host bypass по-прежнему после Pause/Stop: PDC-граф пересобирается, native child
редактора переподключается в том же JUCE-окне. Pin — состояние открытого окна,
не проекта. Structural/device/project changes и Undo/Redo могут закрывать редакторы.

0.1u P4: Transport -> Engine profiling... включает timing каналов/insert/Master,
worker job balance и отдельные device/ahead banks, экспорт CSV из UI. По умолчанию
OFF; закрытие отключает сбор. Timing добавляет overhead. Dependency cost estimate
исключает source/reduction/queue/scheduler wait и не равен полной critical-path
wall time. Сумма пересекающихся jobs не равна process CPU. Engine counters сбрасываются
при stopped prepare, processor counters — при пересоздании PreparedGraph. Снимки
приблизительны, project schema/Undo не менялись. Полная семантика — в ENGINE_PROFILING.

## Что читать по задаче

- Общий смысл продукта: [PROJECT_VISION](PROJECT_VISION.md), [ARCHITECTURE](ARCHITECTURE.md),
  [DEVELOPMENT_TRACKS](DEVELOPMENT_TRACKS.md). Их исторические статусы не заменяют снимок выше.
- Движок: [план](ENGINE_PERFORMANCE_PLAN.md), [reliability](PROCESSING_RELIABILITY.md),
  [P1](ENGINE_PARALLEL_PROCESSING.md), [P2](ENGINE_ANTICIPATIVE_PROCESSING.md),
  [P3](ENGINE_MIXED_PROCESSING.md), [P4](ENGINE_PROFILING.md).
- Плагины: [редакторы/fix1](ENGINE_PLUGIN_EDITORS.md), [VST3](VST3_HOSTING.md),
  [пресеты](PLUGIN_PRESETS.md).
- UI: `mr-studio-ui-design-system/SKILL.md`, [JUCE J3](JUCE_J3.md).
- Проверки: `mr-studio-build-test/SKILL.md` и [локальные команды](LOCAL_DEVELOPMENT.md).

Карта исходников: `apps/studio-juce/src/` — актуальный UI; `apps/studio-desktop/`
— Application/services и исторический Win32 UI; `core/audio/` — engine/worker/ahead/mixed;
`core/processing/` — processor graph, VST3, native DSP, PDC; `core/src/` — model,
commands, serialization; `tests/` — регрессии. JUCE GUI smoke находится также
в `apps/studio-juce/src/Smoke.cpp`.

## Пользовательский ASIO CSV: Test 01

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 01.csv`.
Только анализ, не пользовательская приёмка P4 и не подтверждение записи.
Komplete Audio ASIO Driver, 48 kHz, B 256, Workers 4, Process 256, profiling ON.
Callback load p50/p95/p99 17/24/28%, max 1.8651 ms при deadline 5.3333 ms;
Late/XR/disk/worker timeout 0. Но Ahead underruns **17 212**, invalidations 5,
buffered 0, producer max 0.4337 ms. Проверить starvation/учёт очереди отдельно:
metrics накоплены за engine session, timing cells — только пока profiling ON;
по CSV нельзя вычислить текущую частоту underrun или акустические последствия.
7 709 device timing calls соответствуют 41.1147 s обработанных frames; это не
известная длительность всей сессии/не гарантия непрерывной игры или записи.

Средние device channel jobs: Voc 0.8046 ms, GTR 0.5794 ms, PB 0.4983 ms,
Keys 0.0067 ms; времена пересекаются, не складывать в callback load.
Xvox Pro 0.6044 ms, TH-U 0.3725 ms, Xrack Pro 0.4860 ms;
две Ambiente 0.1867/0.1917 ms. ONE имеет 0 measured calls; пользователь
после Test 01 подтвердил, что ONE был в bypass.
Следующий полезный контроль — свежий session CSV с Process Off / 1024 при
неизменной остальной нагрузке; текущие данные не являются 128-frame проверкой.
Исходный CSV и пользовательский проект не изменялись. Code/build/package без
изменений; #54 не закрывать по этому снимку.

## Пользовательский ASIO CSV: Test 02

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 02.csv`.
Пользователь называет режим «выключенным DSP»; CSV показывает Process Buffer Off,
Workers 4 и активный processor DSP, включая ONE. Komplete Audio ASIO Driver,
48 kHz / Device 256. Callback load p50/p95/p99 24/34/37%, max 2.3907 ms
при deadline 5.3333 ms. Late/XR/disk/worker timeout 0. Ahead counters 0 ожидаемы
при отключённом producer и не подтверждают исправление Test 01 queue starvation.

ONE: 385 вызовов, mean 0.4243 ms, max 0.8427 ms. Prepared graph PDC 2465 samples
= 51.3542 ms; это fixed compensation, отдельно от driver I/O и Process Buffer,
не акустическая latency и не пропущенный deadline. Device Voc/GTR/PB mean
0.6197/0.5394/0.4274 ms. Timing frames 98 560 = 2.0533 s обработки при profiling;
полная длительность session из CSV неизвестна. Это короткий profiling sample,
не длительная ASIO приёмка. Прямое сравнение с Test 01 не изолирует Process:
одновременно изменились bypass ONE и Process, окна сбора тоже различаются.

Для чистого сравнения держать ONE и остальные plugin states одинаковыми,
сравнить Process Off / 1024 свежими counters, дать более длинный profiling window.
P4 остаётся открыт. Исходные CSV/проект/код/билд/пакет не изменены; только анализ
и локальная запись контекста.

## Пользовательский ASIO CSV: Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 256 / Workers 4 / Process 1024,
profiling ON. ONE активен: 9 410 measured calls. PDC прежний: 2465 samples.
Callback p50/p95/p99 25/33/37%, max 2.7451 ms (51.47% deadline 5.3333 ms).
Late/XR/disk/worker timeouts и Ahead underruns/invalidations **0**;
queued 1024 frames в момент snapshot, producer max 0.4354 ms.
Timing содержит 2 408 960 frames = 50.1867 s обработки при profiling; это не
доказательство wall duration всей сессии, записи или непрерывного транспорта.

С Test 02 (Process Off, ONE active): p50 24->25%, p95 34->33%, p99 37->37%,
max 2.3907->2.7451 ms. Ускорение callback не установлено; окна сбора различаются
(~2.05 vs ~50.19 processed s), max нельзя трактовать как доказанное ухудшение.
ONE mean 0.4676 ms, max 1.2369 ms. Основные VST3 inserts остаются device-owned;
producer timings каналов малы, поэтому выраженного выигрыша anticipation тут
не видно. Test 01 Process 256 имел 17 212 накопленных underruns и ONE bypass;
при 1024 текущие counters чистые, но это не доказательство найденной/исправленной
причины прежних misses. Снимок поддерживает работоспособность очереди 1024 на
этой нагрузке. #54 остаётся открыт без явной приёмки и более широкой matrix.
CSV/проект/код/билд/пакет не изменены; результаты сохранены только локально.

## Пользовательский ASIO CSV: Device 128 / Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 128 dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 128 / Workers 4 / Process 1024,
profiling ON. ONE 0 measured calls, PDC 0; соответствует выбранному live bypass.
Callback p50/p95/p99 16/27/32%, max 1.4593 ms при deadline 2.6667 ms (54.72%).
Late/XR/disk/worker timeouts 0. Ahead buffered 1024, producer max 0.6036 ms;
**Ahead underruns 43 и invalidations 43**. Совпадение totals не устанавливает
причину или связь каждого события. В mixed code rebase вызывают как command/mix
revision, так и head.sequence > producer sequence после отставания; поэтому
нельзя автоматически объявить конкретную причину. Пользователь ответил «нет»
на вопрос о щелчках/потере playback и Pause/Seek/изменениях параметров: слышимых
проблем и указанных действий не было. Не списывать 43 события на ручной транспорт.
Для диагностики проверить producer scheduling/queue starvation и корректность
счётчиков, в том числе вне активного playback; причины CSV сам по себе не даёт.

Timing содержит 80 135 device calls / 10 257 280 frames = 213.6933 s обработки
(~3 min 34 s), не полную wall duration сессии или доказательство 5-min continuous
playing/записи. В producer 80 167 calls: может содержать заранее queued frames.
Host plugin PDC теперь 0; driver/converter latency остаётся, acoustic не измерена.
Субъективный live/playback контроль без слышимых проблем подтверждён пользователем.
Пользователь затем уточнил: в самом мультитреке была небольшая пауза.
Вероятно, речь о тишине/разрыве в материале; это не подтверждение нажатия Pause.
Не приписывать пропуски этому месту: CSV содержит только totals, без времени
событий. Тишина может скрыть слышимые последствия недостающего playback PCM,
но сама по себе при непрерывном транспорте не доказывает нормальность underrun.
43 очередных события остаются диагностическим пунктом P4; record acceptance
и полный scope #54/#16 этим CSV/ответом не подтверждены.
User CSV/проект/код/билд/пакет не изменены; локально сохранён только анализ.

## Приёмка 0.1u и переход к MIDI — 2026-10-06

Пользователь согласился принять поставленный software slice 0.1u, оставить
#54 открытым для оставшихся проверок и перейти к MIDI в следующем чате:
«Да, давай, хочу в следующем чате уже заняться миди, а P4 буду в процессе тестировать».
Это приёмка сборки/профилирования и разрешение сменить фокус разработки;
не подтверждение полной sustained recording/transport/reconnect matrix.
43 Ahead underrun/invalidation остаются диагностическим пунктом, причины не
установлены. Live 128 / Workers 4 / Process 1024 / ONE bypass без слышимых
проблем принят в сообщённом пользователем объёме; сведения о паузе в материале
сохранены выше. Более широкие #16, 3e2/3e3 не закрыты. Выпущенный пакет и его
manifest не переписываются после новой пользовательской приёмки.

## Продолжение и обновление этого файла

2026-10-06: по запросу пользователя Stage 4 / MIDI / #24 разбит на **4a–4h**:
0.2a MIDI-вход/VST3-инструменты; 0.2b клипы/playback; 0.2c запись;
0.2d piano roll; 0.2e quantize/transpose; 0.2f CC/PC;
0.2g внешний MIDI; **4h / 0.2h — метроном и precount для audio/MIDI записи**.
Итоговая проверка Stage 4 — после 4h. По последнему уточнению пользователя
Arranger Track, Chord Track и остальная музыкальная структура остаются в Stage 5 / #25.
Доработки — updN, исправления — fixN;
независимые счётчики сбрасываются при переходе к новой букве.
План и критерии: [MRS_STAGE_4_PLAN](MRS_STAGE_4_PLAN.md), схема — VERSIONING.
GitHub: parent #24 обновлён; созданы issues #63–#70 для 4a–4h соответственно.
Stage 5 / #25 сохраняет Chord/Arranger и музыкальную структуру; его scope не менялся.
План, roadmap, VERSIONING и этот контекст опубликованы отдельным docs-only
коммитом `dcd2dad1b5dbd5dd95199d8071af0daf18955e69` в GitHub main с `[skip ci]`;
remote SHA проверен. Локальная source-ветка не отправлялась, код/пакет не менялись.
Планирование завершено, по запросу «Приступай к 0.2a» реализован 4a и выпущен
локальный пакет выше. Следующий шаг — пользовательская игра через MIDI/ASIO:
проверить клавиатуру, выбранный канал, sustain/bend, Stop/panic/reconnect,
save/open и прежние audio/plugin editors. Прочитать MIDI_LIVE_INPUT и
MRS_STAGE_4A_CHECKLIST. Не закрывать #63 без подтверждения физической приёмки.
4b / 0.2b реализован по следующему запросу пользователя; текущая поставка описана выше. 4c / 0.2c автоматически не начинать.
Chord/Arranger остаются Stage 5; метроном/precount — 4h. MIDI-клипы/recording,
piano roll, external MIDI в 0.2a отсутствуют. Собственный synth не поставляется;
нужен VST3-инструмент. Callback ingress/worker ownership не менять на UI note pump.

GitHub #63/#24 обновлены результатами 0.2a и остаются открытыми. Шесть выбранных
docs опубликованы в main отдельным docs-only коммитом
`5d1908936a3494d53df93fd8d72d14f6b4f5c735` с `[skip ci]`; remote SHA проверен.
Remote-only time-stretch roadmap сохранён. Source-ветка не отправлялась, Actions
не запускались. Пакет заморожен с docs `7207ad6` и проверенным manifest 114 файлов;
эта запись синхронизации не переписывает выпущенный пакет.

P4 теперь остаётся проверкой пользователя параллельно разработке MIDI. Не
навязывай завершение #54 как условие начала MIDI и не продолжай engine fixes
самостоятельно вместо MIDI. При новом CSV/аудио-дефекте анализируй сообщение
и фиксируй результаты; по запросу исправления возвращайся к нужной части P4.
Осталось: выяснить 43 queue events, подтвердить запись и sustained transport/
reconnect/state checks. #54/#16 не закрывать автоматически; фиксировать точный
объём будущей приёмки и реальные durations, без vendor/project optimizations.

После содержательной работы обновляй этот снимок: дату, задачу, подтверждённый
результат, незавершённое, ветку/source commit, пакет/проверки, приёмку и следующий шаг.
При передаче незавершённого кода укажи файлы/ошибку/последнюю команду и безопасное
продолжение. Не копируй весь журнал чата и не запускай старые patch-скрипты.

## Decision: instrument parallelism in P4

Recorded 2026-10-06 after the user's Omnisphere piano comparison at 48 kHz / 128 frames.

P4 / #54 now also owns **Instrument Parallelism / multi-instance VSTi sharding**. Existing
P1 already parallelizes independent tracks/nodes, but one VST3 instrument instance remains
one host scheduling node. The new goal is to let one logical instrument track optionally use
2/4 synchronized hidden instances, distribute MIDI note ownership, process them on existing
workers, deterministically sum audio, then run the shared post-instrument FX chain once.

Initial policy is opt-in/capability-gated with Single Instance fallback; do not force this on
mono/legato/sequenced/random/global-state instruments. Never make scheduling depend on the
Omnisphere name/vendor. Omnisphere piano/high-polyphony is only the first representative
hardware benchmark.

No implementation was started by this documentation update. Code/build/test work remains
local; GitHub receives issues/docs only.

## Синхронизация поставки 0.2b

GitHub #64/#24 обновлены и открыты. Пять выбранных docs опубликованы с [skip ci];
remote-only решение о multi-instance VSTi / P4 сохранено в контексте отдельной записью.
Source-ветка не отправлялась, Actions не запускались. Пакет заморожен: 29 файлов,
SHA256 manifest проверен. Последующая запись синхронизации пакет не меняет.

## Синхронизация поставки 0.2c

GitHub #64 закрыт по приёмке 0.2b; #65/#24 обновлены и открыты. Шесть выбранных
docs опубликованы отдельным docs-only main commit `0d1720e0d7c2bab5bcb79a06c4465156cade6bd9`
с [skip ci], remote SHA проверен. Remote-only P4/multi-instance VSTi контекст сохранён.
Source-ветка не отправлялась, Actions не запускались. Пакет заморожен с docs
`4a87c66`: manifest 48 файлов проверен, EXE совпадает с Release. Эта запись пакет не меняет.
