# 0.2c / Stage 4c — MIDI recording

## Пользовательский поток

На instrument track выбрать MIDI input/channel и VST3, подключить ASIO, включить
R (Arm) в заголовке дорожки и нажать Record. Только MIDI — без выбора WAV-файла;
при совместной audio/MIDI записи выбирается WAV для audio take. Stop, Pause или
повторный Record завершают дубль. Одна сессия, включая несколько вооружённых
дорожек, добавляется одним command/Undo. Arm — временное состояние, не проектные данные.

Базовая линейная запись идёт в свободной области: курсор должен находиться после
имеющихся MIDI-клипов вооружённой дорожки, либо нужна новая instrument track.
Loop выключен; overdub/takes/punch не входят в 4c. Во время записи seek, loop,
структурное редактирование, Undo и save/device changes заблокированы. Закрытие/
смена проекта через UI сначала завершает запись, затем предлагает сохранение.
Сбой процесса не имеет recovery для несохранённой MIDI-памяти; это не disk journal.

Мониторинг и запись независимы: выключенный I не мешает capture вооружённого
входа. Мониторинг по-прежнему получает live events на offset 0 следующего callback.
Не вводится новый MIDI clock, UI note pump или отдельный playback backend.

## Модель и совместимость

Core schema **13** читает 1–12. Для старых MIDI clips channel events пусты.
`MidiClip.events`: stable ID, source-relative tick, kind/channel/data1/data2.
Kind совпадает со стабильными ordinal `MidiKind` 2–6: CC, Program Change,
channel pressure, pitch bend, poly pressure. Notes хранят pitch/velocity/channel
и start/length; sustain не увеличивает duration ноты, а сохраняется как CC64.
Повторный Note On одного key закрывает предыдущую ноту. Открытые ноты закрываются
при конце дубля/панике/отключении; удерживаемый sustain получает CC64=0.
Ноты, чьи начало и конец округлились в один tick, пропускаются.

События сохраняются/reopen вместе с инструментом и маршрутом. Duplicate/Split
создают новые event IDs; trim сохраняет source events. Playback восстанавливает
последние значения перед trimmed source head и при seek/resume. На границах клипа
и Pause выпускается sustain; Stop использует существующий panic/reset.
Применение controller/program к конкретному VST3 зависит от его event/IMidiMapping.
Редактор controller lanes относится к 4f; здесь сохраняется записанное исполнение.
Старые сборки не читают schema 13 — работать на копиях проектов.

## Timing и RT

Windows WinMM short-message timestamp (elapsed milliseconds с midiInStart)
переводится в steady-clock ns через anchor открытого порта. Driver callback кладёт
raw message/timestamp в bounded port queue; существующий non-RT bridge передаёт
их в engine SPSC с route generation. Driver/bridge mutex не захватывается audio
callback. При одной смене поколения без изменения routes порт не переоткрывается.

Device callback сопоставляет timestamp с текущим transport sample head:
`head - elapsed_ns * sample_rate / 1e9`, clamp к началу дубля. События без timestamp
получают текущий head; future timestamp также clamp к head. Sample→tick использует
общую tempo map/PPQ 960. Конвертация выполняется при quiescent finish, IDs создаются
там же. Timestamp сохраняет время доставки MIDI, а не измеренную физическую задержку.
WinMM granularity, driver/OS jitter и audio callback anchor/drift не калиброваны.

Synthetic check: задержка 100 ms при 48 kHz вычитается ровно в 4800 samples,
ранний timestamp clamp к take start; round-trip samples/ticks проверен.
На 120 BPM/48 kHz один tick = 25 samples, округление до ближайшего tick.
Это не аппаратное измерение MIDI→ASIO latency; hardware timing остаётся checklist.

RT-CRITICAL: capture/advance/fault, ingress drain и playback. RT-ADJACENT: prepare,
routes и quiescent device stop/join. NON-RT: WinMM bridge, finish/model/Undo/save/UI.
Хранилище capture выделяется до начала; callback не выделяет память, не пишет файлы,
не блокируется и не обращается к UI/model. Instrument/DSP принадлежит device-domain;
ahead clone не содержит MIDI capture или device MIDI events. Raw audio Recorder tap
по-прежнему перед mixer/PDC. Workers не делят один экземпляр VST3 между потоками.

Лимиты: до 32 armed tracks; запись до 4096 notes/track и 8176 channel events с
резервом для release, не более 64 различных control keys и 128 held keys.
Свободные проектные/track budgets учитываются до записи. Capture storage 65536
entries/track; playback chunk 256 events вместе с live ingress. Слишком плотное
playback получает panic/dropped counter. Queue/input/record overflow завершает
запись с предупреждением и сохранением захваченного prefix, закрывая held notes.

## Проверки

`midi_recording`: timestamp projection, open-note/sustain closure, zero-tick notes,
schema 12 migration/13 round-trip, event IDs/Undo, chase/Pause, capture overflow,
stale generation/input loss, Workers 1/2/4 + mixed Process Buffer, monitor On/Off,
synthetic VST3 recording/playback/save/reopen и совместная raw audio запись.
Host allocation probe в проверенных callback paths = 0.

J3: видимый instrument Arm, Record без WAV chooser, Stop → clip, single Undo/Redo,
transport SVG/state и software previews 100%/150%. SVG из пользовательской папки
`assets/ui/icons/transport` встроены в BinaryData; семь существующих кнопок обновлены.
Неактивные функции (metronome/rewind и прочее) не добавлены только из-за наличия SVG.
Итоговые build/CTest/package results — [PROJECT_CONTEXT](PROJECT_CONTEXT.md).
