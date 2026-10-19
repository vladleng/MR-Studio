# ARA2: прямое воспроизведение — #88

## Причина изменения

Ручная диагностика Vovious 2026-10-19: одна правка без пригодного точного
временного диапазона вызвала full=1, frames=459264, ms=1284. NULL range допустим
в ARA; range-aware PCM patch не решает этот сценарий. PlaybackRenderer из
ARA SDK предназначен для воспроизведения общего документа непосредственно.

## Текущий контракт

Application предпочитает отдельный VST3/ARA PlaybackRenderer для каждого
playback region. Постоянный экземпляр редактора/документ сохраняются; transient
playback instances связываются с тем же document controller только с ролью
PlaybackRenderer. Правки ARA content, включая NULL range, слышны через общий
документ, без подготовки PCM, пересоздания графа или renderer после каждой ноты.
В статусе видно `Ready - live ARA`, статистика содержит `live=N`, `frames=0`.

Первая реализация ограничена экземплярами с нулевой заявленной VST3 latency,
mono/stereo и не более 32 regions на документ. Ненулевая latency, неподдержанный
realtime setup/layout или превышение этого лимита используют прежний
компенсированный PCM path. Это сохраняет прежнюю PDC; live PDC для ненулевой
latency не реализована. Структурные правки и VST parameter/restart инвалидируют
подготовку через существующую stopped transaction. UI-only state не требует
пересоздания аудио. Настройка режима не меняет project schema/Undo/архивы.

Сохранённый PCM/нулевой geometry placeholder в live mode служит только для
соответствия voice/clip и не подмешивается к live output. Engine использует
ровно один источник: renderer вместо asset. Fades/routes/mixer применяются
после него прежним путём. EditorRenderer audition по-прежнему не подключён
к выходу; это изменение не добавляет предпрослушку перетаскиваемой ноты.

## RT и lifetime

- Instantiation, role binding, source preload, buffer allocation, parameter
  capture, activation/deactivation и reclamation происходят вне callback.
- Каждый playback renderer имеет заранее подготовленные planar buffers на
  8192 frames, неизменный channel layout и собственный DSP. Callback передаёт
  sample/tempo/meter/cycle context и читает output без host alloc/free/locks/I/O.
- ARA audio readers берутся из заранее подготовленного пула 64 slots/source.
  Их PCM загружается до enableAudioSourceSamplesAccess; освобождение reader
  не удаляет память в callback. Отключение доступа синхронно завершает readers
  по контракту ARA перед удалением source.
- Renderer принадлежит подготовленному graph; shared document/host/model
  живут до последней graph reference. Session не создаёт ownership cycle.
  Перед model mutation renderer закрывает atomic entry gate; control thread
  ждёт завершения единственного DSP caller, останавливает processing/activation,
  снимает region assignment. Callback не ждёт и не освобождает renderer.
- Один renderer нельзя присвоить двум voices одного graph. Atomic entry check
  также обнаруживает ошибочный конкурентный вызов. ARA channels закреплены за
  device domain; ahead producer не вызывает тот же DSP. Остальные пригодные
  channels могут сохранять существующую mixed anticipation.
- Process failure/non-finite output или live latency/IO/reload restart дают
  явный failed status и processing fault/silence, без второго dry тракта.
  Явный Update playback/повторная подготовка может восстановить graph.
- Постоянный экземпляр редактора инициализируется отдельно, даже если первый
  native editor ещё не открывали: один stopped context block на worker до
  начала live playback. Это устраняет зависимость teardown от прежнего bake().
  В RePitch 2.0.0 probe неподготовленный persistent instance приводил к heap
  corruption при terminate; инициализация проверяется отдельно.

## Проверка

Фактические build/test/vendor результаты и ручная приёмка — в #88.
Synthetic проверки включают NULL-range изменение настоящего сигнала без
synchronize, первый и последующие callback без alloc/free, mono/stereo,
seek/loop/variable blocks, failure/silence, возврат к PCM и host Undo/model edits.
GUI проверяет видимый live mode, отсутствие pending PCM после NULL-range edit,
звук через AudioEngine и закрытие editor во время playback.

Generated vendor probes не проверяют реальный vocal tuning/alignment, смешанные
vendor проекты, физические ASIO/DPI и слух. Эти проверки нельзя подменять
эквивалентностью live/PCM на сгенерированных источниках или общей CPU цифрой.

