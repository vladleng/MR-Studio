# 0.2b / Stage 4b — MIDI-клипы и воспроизведение

Дата: 2026-10-06. Tracking: [#64](https://github.com/vladleng/MR-Studio/issues/64),
parent [#24](https://github.com/vladleng/MR-Studio/issues/24).
База интерфейса — принятая 0.2a upd1 fix1. Пользовательская приёмка 0.2b отдельно.

## Пользовательский сценарий

1. Создать instrument track, назначить VST3-инструмент и подключить ASIO.
2. Выбрать дорожку и позицию. Edit → Create MIDI clip at cursor создаёт клип
   длиной четыре четвертных доли. Двойной щелчок по пустому месту instrument lane
   создаёт клип там; двойной щелчок по MIDI-клипу открывает редактор нот.
3. В редакторе Add note добавляет ноту; выбранная нота изменяется через Apply note,
   удаляется через Delete note. Pitch 0–127, velocity 1–127, channel 1–16.
   Start и Length измерены в четвертных долях. Start — от исходного начала
   содержимого, а не от нового левого края после trim. Начало нумеруется с 0.
4. Play воспроизводит клип через ту же цепь инструмента, mixer/buses/Master/PDC,
   что и живой MIDI. MIDI input Off не отключает playback клипа.
5. Перемещение и изменение границ — drag/edge drag в Arrange. Split — S или Edit;
   Duplicate/Delete/Loop selected clip — Edit. Duplicate создаёт независимую копию
   сразу справа. Loop — временное состояние общего transport.
6. Transport → Project tempo меняет BPM первой tempo point, сохраняя остальные
   точки tempo map. MIDI-клипы сохраняют музыкальную позицию/длину; аудио-клипы
   сохраняют свои sample ranges. Сетка Arrange теперь привязана к ticks.
7. Все изменения клипов/нот/темпа требуют Pause/Stop, поддерживают общий Undo/Redo
   и сохранение проекта. Ввод некорректных значений не меняет модель.

Редактор 4b — минимальная форма для одной ноты. Полный piano roll — 4d,
запись исполнения — 4c, authored CC/PC — 4f, external output — 4g.
Настройки/выделение редактора временные; дополнительного Undo стека нет.

## Модель и совместимость

`Clip::midi` содержит start/length/source_offset в ticks PPQ 960 и MIDI notes:
стабильный ID, source-relative start/length, pitch, velocity, zero-based channel.
Sample поля MIDI-клипа остаются defaults; это не вторая изменяемая позиция клипа.
Аудио-клипы по-прежнему используют samples и asset reference.

Trim скрывает материал за границами, не удаляя исходные ноты. Split сохраняет
source offset; правой части назначаются новые note IDs, как и при duplicate.
Move и изменение одной ноты сохраняют IDs. Undo/Redo возвращает сохранённые IDs.
Перенос MIDI-клипа разрешён на instrument track; audio/MIDI type mismatch отклоняется.

Core schema **12** читает 1–11 с отсутствующими MIDI-клипами. Старые приложения
не читают 12: пользовательские проекты проверять на копиях. В `.mrsproject`
ноты встроены, отдельного MIDI-файла/Media asset нет. Save As/загрузчик WAV
пропускает MIDI-клипы; совместная raw audio recording сохраняет исходный вход.

## Scheduling и границы

NON-RT: `compile_midi_clips` пересекает каждую ноту с видимым source range клипа,
вычисляет sample on/off через общую `Timeline`, проверяет бюджеты до command commit.
`MidiPlayback::prepare` сортирует события (off до on при равном sample) и строит
интервальный индекс для chasing. Только stopped graph preparation публикует данные.

RT-CRITICAL: один device scheduler владеет cursors/active keys, готовит bounded
events для каждого processing chunk, затем Workers обрабатывают независимые
channel chains. В callback нет Timeline conversion, container growth, disk/driver
calls, UI listeners, locks или освобождения проекта. DSP экземпляр остаётся у
одного владельца. MIDI channels/downstream device-owned; mixed producer не получает
их notes/live flags. Playback native audio может продолжать anticipation.

- Диапазоны half-open `[start,end)`. Off на правой границе блока попадает в следующий
  блок offset 0; onset на этой границе также появляется ровно один раз.
- Play/seek/resume внутри ноты восстанавливает её pitch/channel/velocity в offset 0
  и оставшуюся длительность. Нота, уже завершившаяся в позиции seek, не chasing.
- Loop выпускает активные playback keys и начинает/chases материал в новой позиции.
  События у loop end не продолжаются поверх нового витка.
- Pause выпускает playback notes; Stop/seek используют существующий plugin panic/reset.
  При reset новые chased события применяются после него, не теряются.
- Перекрывающиеся ноты одного pitch/channel на одной instrument track объединены
  в один звучащий интервал, velocity первого onset. Соседние интервалы off/on
  не объединяются. Проектные ноты остаются неизменными; merge — runtime projection.
- Live ingress и playback одного pitch/channel имеют обычные общие MIDI key semantics:
  независимые голоса/счётчики для живого исполнения и клипа не заявляются.

Ограничения: 128 clips на уровне Application, 4096 исходных нот на клип,
32768 на проект; compiled visible notes — до 4096 и 128 одновременно на track.
Буфер chunk — 256 MIDI events включая live ingress. При переполнении chunk
очищается, инструмент получает panic, счётчик `midi_dropped` растёт. Не выполняется
неограниченное повторное воспроизведение пропущенных событий. После overflow
пропущенные удерживаемые ноты не восстанавливаются до нового перехода transport.

## Проверки

`midi_clips`: model/IDs, atomic rejection, trim/split/duplicate/delete, Undo/Redo,
schema 11 migration, round-trip, tempo change crossing a note, boundary on/off,
overlap policy/chasing/pause/resume, dense-event overflow, Workers 1/2/4,
short chunks, loop, mixed Process Buffer/bus, authored VST3 sound/save/reopen,
Save As and совместная raw audio recording. Allocation probe на проверенных RT paths.

JUCE J3: menu → clip → note form → shared model, stable note ID, invalid values,
editor survival after graph rebuild, VST3 playback/Pause, duplicate Undo, clip loop,
Arrange/note form software previews 100%/150%. Используется synthetic fixture,
а не установленный SWAM/TH-U/Nuro. Физический ASIO/DPI — пользовательский checklist.
Итоговые результаты и пакет: [PROJECT_CONTEXT](PROJECT_CONTEXT.md).
