# 0.2f — пользовательская проверка

На копии проекта: MIDI clip → Controllers...

- [ ] Записанные CC/sustain/bend/pressure/Program видны в списке и своих линиях.
- [ ] Add/Apply/Delete; выбор каналов 1–16, CC/полинодного номера 0–127.
- [ ] Double click и drag события, snap, Escape, один Undo/Redo на жест.
- [ ] Bend -8192/0/8191, Program 0/127, CC64 Off/On, pressure endpoints.
- [ ] Редактирование не теряет ноты, другие events и ID. Save/reopen сохраняет всё.
- [ ] Play/seek/loop: последние значения восстанавливаются; Pause/Stop сбрасывают.
- [ ] Два соседних клипа: sustain/bend предыдущего не затирают новый head.
- [ ] Trim/split и source offsets, события в одной позиции, разные каналы.
- [ ] Неверный ввод и editing во время Play/Record не меняют проект.
- [ ] ASIO/VST3 mapping, Windows scaling и длительное playback без зависших нот.

Physical ASIO/VST3/mixed DPI подтверждаются пользователем; software PASS отдельно.
Дополнительные доработки после Stage 4. 0.2g/0.2h остаются следующими пунктами.

## 0.2f upd1 — дополнительные проверки

- [ ] CC1 точки связаны прямыми отрезками; CC64 остаётся ступенчатым.
- [ ] Lane под piano roll; horizontal zoom/scroll и trim offsets совпадают.
- [ ] Ctrl + wheel vertical, Ctrl + Shift + wheel horizontal, Shift + wheel scroll.
- [ ] Double left click удаляет ноту, пустое место создаёт; Undo/Redo.
- [ ] Растущий MIDI take виден до Stop, held notes удлиняются, Stop/Undo/Redo прежние.
- [ ] Edit открывает audio/MIDI clip; audio waveform/trim и Undo.
- [ ] Attach/Detach, Mix/Edit/Arrange, panel resize, selection/zoom сохраняются.
- [ ] Save/reopen сохраняет edits; dock/zoom transient, Windows 100%/150% DPI.

Эти семь доработок отдельно запрошены пользователем 2026-10-07.

## 0.2f upd1 fix1 — ручная приёмка

- [ ] Без R запись не начинается; ошибка видна, Record не синеет и не остаётся включённой.
- [ ] R инструментальной дорожки → Record: запись начинается, MIDI take растёт до Stop.
- [ ] Play/Pause/Stop редактора и Space управляют Arrange; обе области показывают одну позицию проекта, seek по линейке общий.
- [ ] Соседние MIDI-клипы той же дорожки затемнены, промежутки тёмные; сдвинутый/подрезанный активный клип и controllers совпадают по шкале.
- [ ] ПКМ не удаляет ноту; двойной ЛКМ/Delete и Undo работают.
- [ ] Перенос по высоте слышен; отпускание/Escape/закрытие редактора не оставляют зависшую ноту.
- [ ] Attach/Detach, horizontal scroll/zoom, Windows 100%/150% mixed DPI.

Автоматические проверки и пакет:
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

## 0.2f upd2 — ручная приёмка

- [ ] В MIDI-редакторе нет отдельных кнопок транспорта; Space и главный транспорт работают.
- [ ] ЛКМ на соседнем MIDI-клипе piano roll меняет активный; ЛКМ в Arrange переключает открытый audio/MIDI editor.
- [ ] Controllers open/closed сохраняется при смене MIDI-клипа и после перезапуска.
- [ ] Attach/Detach сохраняется при смене клипа, закрытии через Edit и после перезапуска.
- [ ] Повторный Edit закрывает редактор; следующий открывает в запомненном размещении.
- [ ] ПКМ + drag выделяет audio/MIDI clips; Delete/Backspace удаляет группу; один Undo восстанавливает.
- [ ] R → Record из Stop и R → Play → Record: статус RECORDING, растущий MIDI take, Stop сохраняет ноты, Undo/Redo.
- [ ] Komplete Kontrol A49/ASIO, повторное открытие проекта и Windows mixed DPI.

Автоматические проверки и пакет:
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
