# 4a / 0.2a — MIDI input и VST3-инструменты

Issue #63, parent Stage 4 / #24. Windows JUCE/ASIO, один Core/Transport/Mixer.
Дата: 2026-10-06. Пользовательская/физическая MIDI-приёмка ожидается.

## 0.2a fix1 — замечания пользователя

Пользователь сообщил MIDI Off после выбора Komplete Kontrol A49, общую панель
вместо инструмента и уход окна за DAW. Затем SWAM Alto Flute 3 загрузился,
MIDI стал connected; снимок показывал Offline clock (no sound). Позже пользователь
подтвердил звук через Komplete Audio ASIO / B 128 / Workers 2 после отключения
Process Buffer («DSP»). Это подтверждение игры в сообщённом режиме, не весь checklist.

fix1 сохраняет в MIDI status имя выбранного входа/канал и отдельно audio disconnected;
Off означает отсутствие назначения. При открытии редактора без runtime безопасно
готовится offline graph, без автоматического открытия ASIO. Native editor и общая
панель параметров имеют владельца DAW HWND; Pin сохраняет прежний смысл.
Offline clock не выводит звук на устройство: выбрать ASIO и Connect в Audio settings.
CPU/driver latency в offline показываются `--` по прежнему контракту.

Исправлена причина тишины с Process Buffer: producer-копия mixed graph ошибочно
сохраняла live MIDI flags и проваливала anticipation_safe(), выставляя device fault.
Теперь live flags отсутствуют только в producer-копии; инструмент и downstream
остаются device-owned. Добавлены регрессии live Notes/Play/panic при Workers 2/4
и Process 1024/4096, а также disconnected native/generic editor ownership/status.
Cached configure/Release fix1 прошли; финальный **111/111 CTest, 58.10 s**.
Первый прогон fix1: 110/111; новый GUI test обращался к отсутствующему peer
после J3 cleanup. В тесте добавлен scoped DAW peer, повторный полный прогон прошёл.
Результаты fix1 package — в PROJECT_CONTEXT. Физическая повторная
проверка Process Buffer ON и поведения SWAM окна ожидается.

## Пользовательский сценарий

1. Подключить ASIO через Transport → Audio settings.
2. Track → Add instrument track.
3. В Mix добавить нужный VST3-инструмент первым insert (из browser или списка
   VST3). После него можно добавить обычные эффекты. Сканирование и plugin editor
   используют существующий host, preset/state и Pin workflow.
4. В мини-панели дорожки Arrange нажать MIDI input и выбрать клавиатуру,
   All channels или канал 1–16. I включает/выключает MIDI monitoring.
5. Играть: live MIDI и звук инструмента работают также при Stop/Pause.
   Gain/pan, mute/solo, buses/sends/Master и PDC используют общий mixer.
6. Transport → MIDI panic / all notes off гасит зависшие ноты.
7. Сохранить проект и проверить повторное открытие. Вход, канал, monitoring,
   insert chain, plugin state и routing сохраняются; проектные правки отменяются
   через общий Undo/Redo. Новая схема Core 11 читает 1–10 с MIDI input Off.
   Старые сборки не читают схему 11: тестировать на копии проекта.

Наличие VST3 в каталоге не означает, что он инструмент. Первый insert инструментальной
дорожки должен иметь event input и mono/stereo audio output; несовместимый плагин
отклоняется до изменения цепи. Инструменты с нулём аудиовходов поддерживаются.
Изменение MIDI input/channel/monitor и структуры цепи выполняется после Pause/Stop,
через существующий quiescent rebuild; во время игры можно менять mixer controls.
Если сохранённый инструмент не загрузился, дорожка остаётся в проекте с исходным
state и показывает Instrument unavailable; её выход молчит. Отсутствующий порт
показывает MIDI: missing; занятый/недоступный — MIDI: unavailable. Вход можно
переназначить. Имя и vendor/product ключ WinMM сохраняются вместо session index;
порты с полностью одинаковой идентичностью различаются порядковым номером,
поэтому после изменения набора одинаковых устройств назначение следует проверить.

## State flow и RT

ProjectStore → SetMidiInput/SetInserts → остановка callbacks/producer/workers →
подготовка графа → immutable runtime channel indices + generation → запуск audio.
Новый TrackKind::instrument имеет обычный аудиоканал. TrackKind::midi сохраняет
старый контракт; модель MIDI-клипов/recording относится к 4b/4c.

WinMM callbacks записывают short messages в ограниченную очередь порта (1023).
Port mutex используют только driver callback и NON-RT bridge; audio его не берёт.
Один bridge thread опрашивает очереди примерно раз в 1 ms и является единственным
producer engine MIDI queue (1023). Маршруты публикуются через NON-RT mutex;
bridge не обращается к графу/плагину/ProjectStore. Device callback ограниченно
дренирует очередь, фильтрует старые generation и готовит по 256 events на канал.
Processor получает live MIDI в начале первого host chunk текущего callback.
Нет нового allocation/locks/driver I/O/UI calls в audio callback.

Instrument channels резервируются за device и передают live dependency downstream,
даже с отключённым MIDI monitor. Ahead producer не получает live events и не
вызывает тот же экземпляр. PDC сохраняется; raw audio capture не содержит
host-generated instrument audio. Mon включает fixed PDC инструментального пути,
отдельно от driver latency; акустическая задержка не измерена.

VST3 note on/off/poly pressure доставляются через preallocated IEventList.
CC/sustain, pitch bend, channel pressure и Program Change используют IMidiMapping,
запрошенный вне RT для каждого канала. Поддержка конкретного controller зависит
от mapping плагина. Отсутствующие назначения не выдумываются. Panic/Stop/Seek
сбрасывают отслеженные ноты и sustain; не обещается удаление reverb/release tails.
Контроллеры на одной sample position сохраняют последнее значение, в том числе
отпускание sustain после серии сообщений. Overflow запрашивает panic вместо
риска потерять только note-off. При overflow
весь MIDI-набор данного блока может быть отброшен; это диагностируемый отказ,
не lossless delivery. Порты перечисляются заново примерно раз в секунду;
смена набора/маршрута запрашивает panic и переоткрывает необходимые inputs.
Bridge закрывает inputs и joins до уничтожения engine.

Это basic live ingress: WinMM timestamps ещё не калибруются к sample clock.
Offset 0 относится к следующему обработанному callback, с OS/bridge/device/PDC
latency; sample-accurate MIDI recording не заявляется. UI polling не переносит
каждую ноту: input работает при остановившемся UI pump после публикации маршрута.

## Проверки и границы

`midi_live` использует временную копию синтетического VST3 без audio inputs,
manual device, decoder, Core 10 migration, Undo/Redo/save/load, Notes/velocity,
sustain/bend, Stop/panic, overflow, generation rejection, короткие blocks,
bus/gain/PDC и callback allocation probe. Missing ports/plugin state retention
проверяются без открытия физического MIDI/ASIO.
JUCE J3 smoke создаёт инструментальную дорожку через меню, назначает fixture,
проверяет note ingress/panic и software snapshots 100%/150%.
Локальные cached configure и Release build прошли; финальный полный CTest —
**111/111, 57.15 s**. Просмотрены snapshots 100%/150%. Первый общий прогон
выявил stack overflow в двух parallel-тестах: per-channel MIDI buffers перенесены
в heap на quiescent prepare; финальный прогон подтверждает исправление.
Тест controller burst сохраняет последнее sustain release. Package/source/hash
и packaged smoke фиксируются в PROJECT_CONTEXT.

Физическая MIDI/ASIO игра, disconnect и latency остаются пользовательским gate.
Внешний MIDI output, MIDI-клипы, запись, piano roll и controller editor — следующие
подэтапы. SysEx/MIDI2/MPE, seamless patch switching, multi-output VST3 instruments
и arbitrary native plugin crash isolation не входят в 4a.
