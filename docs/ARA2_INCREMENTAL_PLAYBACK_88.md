# ARA incremental PCM preparation — #88

Этот контракт описывает резервный PCM path. Последующая реализация прямого
zero-latency playback вынесена в [ARA2_LIVE_PLAYBACK_88.md](ARA2_LIVE_PLAYBACK_88.md).
В live mode ARA content edits не запускают описанную ниже PCM-подготовку.

Контракт от 2026-10-19. Это оптимизация существующего stopped PCM пути,
не подключение vendor DSP к AudioEngine callback и не editor audition.
Исходные WAV, project schema, ARA archive и host/plugin Undo не меняются.

## Владение и публикация

Session хранит immutable cache каждого региона. При content-only обновлении
не отправляет заново неизменившуюся модель, selection и region properties.
Изменение clips/sources/time map/tracks или восстановление состояния требует
консервативной полной подготовки. Explicit Update playback также принудительный.

Host учитывает audio scope, source/modification и playback-region time ranges.
Modifications используют source offset; region notifications — playback time.
Диапазоны объединяются до успешной публикации, включая последовательные правки.
Regions вне диапазона переиспользуют тот же PCM, остальные получают новый
буфер-копию с заменой затронутого участка. Старые данные не изменяются.
Отмена, ошибка или новая audio revision сохраняют старый опубликованный cache
и накопленную dirty область; частично подготовленный результат не публикуется.

Patch расширяется на ARA head/tail, VST3 tail, PDC latency и защитный блок.
Позиция process context остаётся абсолютной; output latency компенсируется
перед записью участка. Изменение latency во время подготовки не допускает
публикацию несовместимого cache. Бесконечный/невалидный tail, неизвестный range,
global parameter/restart/analysis update и invalid ranges требуют full render.
Callback с не-model thread использует только atomic global invalidation,
не изменяет range структуры. Все copy/allocation/model calls выполняются
на control thread, DSP/PCM access — joined preparation worker; RT path прежний.

## Границы гарантии

Наличие ARA2 не гарантирует, что vendor сообщает точные time ranges.
NULL range законен: хост не может угадать внутренние pitch edits по GUI/нотам.
В этом случае пересчитывается затронутый source/region целиком; другие source
кэши могут переиспользоваться. Поэтому popup на тяжёлой реальной правке всё ещё
возможен, и общий пункт бесшовного playback остаётся открытым до vendor проверки.
После ручной проверки 2026-10-19: пользователь не наблюдает ускорения, окно
подготовки сохраняется; vendor seamlessness не принята. Исходная подсказка
Ready не показывалась, поскольку в ARA panel не было TooltipWindow.
Диагностика последней подготовки общего документа теперь видна прямо под
кнопками: full/partial/reused/frames/ms и Update cause. Подсказка также подключена.
Причины: model/cache changed, VST parameter, analysis, VST restart, host refresh,
off-thread notification, no precise range, context/tail, latency changed.
При pending/Play панель сохраняет последнюю успешную статистику; отменённый
или провалившийся рендер её не заменяет. Причины глобальной инвалидации считаются
монотонными atomic counters и сравниваются с baseline успешного cache commit:
отмена/ошибка не теряет события. Диагностика не меняет audio invalidation policy,
DSP, время появления progress или формат проекта и не решает стоимость full render.

При Play не включается автоматический bypass. Host power/update/remove controls
заблокированы, статус явно показывает ARA ON либо ARA BYPASSED. Звучит PCM cache.
EditorRenderer audition (например, нота при drag в Vovious) пока не подключён:
его текущий context worker отбрасывает output. Это отдельный audio/lifecycle
путь с проверкой отсутствия double playback, не результат этой оптимизации.

## Проверка корректности

Synthetic fixture меняет реальный сигнал только в заданном диапазоне source.
Частичный результат сравнивается с принудительным full render по всем сэмплам:
mono/stereo, source offset, playback placement, tempo/meter boundaries,
17-sample latency, начало/конец региона, region/modification notifications,
неизвестный/невалидный range, отмена и structural invalidation.
Проверяется переиспользование соседних PCM и фактическое число DSP frames.
Результаты build/test и пользовательская приёмка — в #88; synthetic equivalence
не заменяет Vovious/VocAlign/RePitch/physical ASIO/DPI/listening проверку.
