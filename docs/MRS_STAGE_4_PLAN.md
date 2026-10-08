# MRS Stage 4 — MIDI и метроном: подэтапы

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


План зафиксирован 2026-10-06 по запросу пользователя. Parent: MRS #24.
Это MIDI-этап DAW, отдельный от исторического SHARED Stage 4 / #19.
Каждый подэтап получает следующую букву версии, отдельный локальный пакет,
проверки и пользовательскую приёмку. Планирование не означает готовность функций.

## Версии

- Начало этапа: **4a / 0.2a**. Далее 4b / 0.2b и так далее.
- Доработки внутри подэтапа: `0.2a upd1`, `0.2a upd2`.
- Исправления ошибок: `0.2a fix1`, `0.2a fix2`.
- Счётчики upd и fix независимы в пределах базовой версии; следующая буква
  начинает оба счётчика заново. Совместный пример: `0.2a upd1 fix2`.
- В именах пакетов/веток пробелы заменяются дефисами: `0.2a-upd1-fix2`.
- Эти версии заменяют старый ориентир MRS 0.5 для Stage 4; Stage/issue IDs
  и технические версии схем от буквенной версии не меняются.

## Последовательность

2026-10-08: upd8 принята («Все тесты прошли»), пользователь разрешил «Поехали
0.2h делать». 4h реализуется локально: [контракт](METRONOME_PRECOUNT.md),
[checklist](MRS_STAGE_4H_CHECKLIST.md). 4g не начат; совместная приёмка после 4g.

Уточнение пользователя 2026-10-08: **сначала 4h / 0.2h — метроном и precount,
затем 4g / 0.2g — внешний MIDI**. Номера, issue mapping и содержание сохранены;
итоговая совместная проверка после обоих подэтапов. Реализация не начата.

GitHub tracking: [parent #24](https://github.com/vladleng/MR-Studio/issues/24).
Подэтапы 4a–4h: #63, #64, #65, #66, #67, #68, #69, #70 соответственно.
4b / #64 закрыт по пользовательской приёмке («Тест пройден, идем дальше»).
4a / #63 открыт для полного checklist; fix1 ранее принят. 4c / #65 реализован локально,
Release/113 CTest/package PASS, физическая ASIO-приёмка ожидается. 4d / 0.2d принят («Да, все работает»), #66 закрыт. 4e / 0.2e принят («работает, приступай к 0.2f»), #67 закрыт. 4f / 0.2f реализован локально, Release/113 CTest/package PASS; #68 ожидает приёмку. 4g–4h запланированы.

| Подэтап / версия | Пользовательский результат и объём | Критерий приёмки |
| --- | --- | --- |
| **4a / 0.2a — MIDI-вход и VST3-инструменты** | Windows MIDI input: список устройств, выбор входа/канала, статус подключения. Создание инструментальной дорожки, назначение VST3-инструмента, MIDI monitoring, звук через общий mixer/buses/Master. Сохранение назначения инструмента, его state и маршрута. Panic, безопасные Stop/отключение устройства и явное состояние отсутствующего порта/плагина. | Подключённая клавиатура играет через VST3; velocity, sustain и pitch bend передаются; ноты не зависают при штатных переходах. Проект открывается с тем же инструментом и маршрутом. Физическая MIDI/ASIO проверка отдельно от mock-тестов. |
| **4b / 0.2b — MIDI-клипы и воспроизведение** | Ноты в ticks PPQ 960: высота, начало, длительность, velocity, канал; стабильные IDs. Создание клипа и минимальный ввод/изменение нот для проверки playback; move/trim/split/duplicate/delete клипов в Arrange. Расписание событий по общей tempo map, корректные границы блоков и loop/seek/Stop. Undo/Redo и миграция старых проектов. | Созданный MIDI-клип играет через инструмент, совпадает с музыкальной шкалой при смене темпа; редактирование и повторное открытие сохраняют результат. Проверены ноты на границах клипа/loop и политика note chasing. |
| **4c / 0.2c — MIDI-запись** | Arm MIDI-дорожки, запись живого исполнения в клип вместе с мониторингом; timestamp/sample-to-tick привязка к общему transport. Ноты, sustain, pitch bend и другие поддерживаемые channel events сохраняются. Базовая линейная запись; завершение открытых нот при Stop/отключении. Одна завершённая запись — одна операция Undo. | Записанное исполнение воспроизводится и сохраняется с нотами и контроллерами; timing измерен и ограничения документированы. Совместная audio/MIDI запись не нарушает raw audio capture. Loop overdub/takes остаются будущим расширением. |
| **4d / 0.2d — Piano roll** | Полноценный редактор: клавиатура, zoom/scroll, сетка/snap, создание/удаление/выделение нескольких нот, перемещение, изменение длины и velocity, copy/paste, прослушивание нот. Редактор работает с той же моделью и общим Undo. | Создать и отредактировать партию без внешнего инструмента редактирования; Arrange и piano roll согласованы. Проверены DPI, края/перекрытия нот и Undo/Redo групповых жестов. |
| **4e / 0.2e — Музыкальное редактирование** | Quantize с выбором сетки и силы, отдельной политикой начала/длины; transpose выделенных нот/клипа; массовое изменение velocity/длительности. Предсказуемая обработка границ и MIDI pitch 0–127. | Операции применяются к выбранному материалу, воспроизводятся и полностью отменяются; результат сохраняется. Обработка не создаёт нулевых/отрицательных длительностей или недопустимых нот. |
| **4f / 0.2f — CC и Program Change** | Просмотр и редактирование записанных CC, sustain, pitch bend, channel/poly pressure и Program Change; ввод/удаление/перемещение событий, выбор канала, Undo/Redo. Определённые правила восстановления controller state при seek/loop и его сброса. | Записанные и вручную введённые события управляют инструментом на нужной позиции; переходы не оставляют sustain или bend в случайном состоянии. Проект сохраняет и восстанавливает все поддерживаемые данные. |
| **4h / 0.2h — Метроном и precount для записи** | Включение метронома при playback/recording, уровень и акцент первой доли; синхронизация с общей tempo/meter map. Включаемый precount с выбором количества тактов перед audio/MIDI записью; видимый отсчёт, отмена через Stop. Запись начинается в выбранной позиции после отсчёта; precount не создаёт записанного материала. Настройки сохраняются с явно определённой областью project/preferences. | Щелчки попадают на доли, акценты следуют размеру, темповые изменения и loop не вызывают рассинхронизацию. Audio и MIDI начинают запись после указанного числа тактов; проверены начало проекта, отмена и совместная запись. Щелчок не подмешивается хостом в raw audio capture. После этого — итоговая совместная проверка всех подэтапов и пользовательская приёмка Stage 4 в подтверждённом объёме. |
| **4g / 0.2g — Внешний MIDI** | Windows MIDI output, назначение внешнего порта/канала, live thru и playback клипов на внешние устройства; сохранение маршрутов, статус отсутствующих портов, reconnect и panic. Предотвращение MIDI feedback. Совместная проверка instrument/external/audio routing, записи, loops, save/load и старых проектов. | Внешнее устройство получает Notes/CC/PC в документированных пределах timing; отключение/возврат порта обрабатывается предсказуемо. Пройдены локальные регрессии и физическая MIDI/ASIO проверка внешнего routing. |

4a — первый самостоятельный полезный сценарий. 4b опирается на него; 4c и 4d
используют модель клипов 4b; 4e расширяет редактор; 4f завершает редактирование
данных, уже сохраняемых записью 4c; 4g завершает внешний routing.
По уточнению пользователя 2026-10-06 Arranger Track, Chord Track и остальная
музыкальная структура остаются в Stage 5 / #25. В Stage 4 дополнительно входит
4h / 0.2h — метроном и precount, использующие общие transport и audio/MIDI recording.
Итоговая приёмка всего Stage 4 следует после 4h и 4g; последним выполняется 4g.

## Общие требования к каждому подэтапу

- Один Project Model/Transport/MIDI backend для Arrange/Edit/Mix и будущего Live.
  Не создавать отдельный clock или изменяемую копию проекта для MIDI.
- Разделить данные инструментальной дорожки и её аудиовыход; согласовать
  существующий TrackKind::midi с mixer, audio graph, VST3 event buses и PDC.
  Точную структуру выбрать после аудита кода перед 4a.
- Все постоянные изменения проекта проходят через commands, Undo/Redo и
  persistence с явными defaults/migrations. Device preferences и временные
  arm/monitor/selection состояния классифицировать отдельно перед реализацией.
- Текущие block-relative очереди не выдавать за timeline scheduler или точную
  MIDI-запись. Для timestamp ingress, playback и record определить владельцев,
  ограниченные буферы, overflow, стабильный порядок и политику note-off/chasing.
- Не вводить allocation, locks, driver I/O или UI calls в audio callback;
  сохранить single-owner DSP, принятую PDC и границы live/ahead processing.
- Для каждого пакета: затронутые model/Undo/serialization/engine регрессии,
  локальная Release-сборка, GUI smoke и конкретный manual MIDI/ASIO checklist.
  Offline PASS не заменяет физическую приёмку.
- Код, сборки и пакеты локально; прежние пакеты сохраняются. P4/#54 и #16
  остаются отдельными проверками и автоматически этим этапом не закрываются.

## Границы и текущий статус

По запросу пользователя «Приступай к 0.2a» реализован локальный 4a:
[MIDI input и VST3-инструменты](MIDI_LIVE_INPUT.md),
[checklist физической приёмки](MRS_STAGE_4A_CHECKLIST.md). Build/test/package
результаты — в PROJECT_CONTEXT. По следующему запросу реализован 4b / 0.2b: [MIDI clips](MIDI_CLIPS.md), [checklist](MRS_STAGE_4B_CHECKLIST.md). 112/112 CTest и packaged checks PASS; 4b принят пользователем, #64 закрыт. Реализован 4c / 0.2c: [MIDI recording](MIDI_RECORDING.md), [checklist](MRS_STAGE_4C_CHECKLIST.md). 113/113 CTest/package PASS; физическая ASIO-приёмка ожидается. 4d / 0.2d реализован: [Piano roll](PIANO_ROLL.md), [checklist](MRS_STAGE_4D_CHECKLIST.md). Release/113 CTest/package PASS; пользовательская приёмка ожидается. 4d принят пользователем, #66 закрыт. 4e / 0.2e реализован: [musical edit](MIDI_MUSICAL_EDIT.md), [checklist](MRS_STAGE_4E_CHECKLIST.md), Release/113 CTest/package PASS. 4e принят пользователем, #67 закрыт. 4f / 0.2f реализован: [controller lanes](MIDI_CONTROLLERS.md), [checklist](MRS_STAGE_4F_CHECKLIST.md), Release/113 CTest/package PASS. #68 ожидает приёмку; 4g–4h не начаты. Дополнительные доработки после реализации всех пунктов Stage 4 по указанию пользователя.
Собственные инструменты/эффекты остаются отложенными. MIDI 2.0, SysEx,
MIDI Clock/MTC, SMF import/export, MPE, loop overdub/takes, advanced MIDI и
Live foot-controller/section automation не включены в эти подэтапы.
Добавление их в Stage 4 требует явного изменения плана, а не скрытого расширения upd.

Основа: [MIDI_PROCESSOR_GRAPH](MIDI_PROCESSOR_GRAPH.md),
[MUSICAL_TIMELINE](MUSICAL_TIMELINE.md), [VERSIONING](VERSIONING.md).
Актуальная сборка и незавершённые проверки: [PROJECT_CONTEXT](PROJECT_CONTEXT.md).

0.2f upd1 (2026-10-07): отдельно запрошены семь изменений
[editor workflow](EDITOR_WORKFLOW.md), Release/113 CTest/package PASS; #68 ожидает
приёмку. Остальные доработки отложены; 0.2g/0.2h не начаты.

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
