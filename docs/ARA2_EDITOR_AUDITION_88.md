# ARA EditorRenderer audition — #88

## Контракт

Пункт 2, 2026-10-19: предпрослушка ноты при её перемещении использует
постоянный VST3/ARA экземпляр EditorRenderer. Его PlaybackRenderer не содержит
назначенных regions вне подготовки PCM; звук обычных клипов остаётся в прежнем
единственном live/PCM тракте. Один общий документ имеет один preview renderer.

Когда native editor открыт и анализ завершён, control thread заранее готовит
mono/stereo buffers до 8192 frames, layout, rate и activation. Device-domain
callback обрабатывает EditorRenderer с актуальными sample/tempo/meter/cycle и
playing/stopped context при любом состоянии транспорта. Не используется редкий
UI-timer worker как источник слышимого аудио. Однократный stopped warm-up worker
сохранён только для инициализации постоянного экземпляра перед live playback.

Выход preview подмешивается один раз перед Master processors/inserts/gain,
на основные mono/stereo Master outputs. Track gain/mute/solo/inserts и sends
не применяются к этому отдельному preview; это прослушивание редактора, а не
ещё один playback voice. Plugin latency и Master inserts влияют на задержку
прослушивания естественным образом; дополнительная timeline PDC/pre-roll для
произвольного preview не добавляется. Обычная PDC клипов не меняется.

При raw audio/MIDI recording preview обрабатывается, но не подмешивается;
он не попадает в raw capture. Bypass выбранного ARA source подавляет preview.
Закрытие окна останавливает обработку, повторное открытие готовит её заново.
Ошибка EditorRenderer отключает его с диагностикой без остановки clip playback.
Работа предпрослушки зависит от того, реализует ли её сам плагин.

## Владение и ограничения

Graph держит prepared renderer; он удерживает document/provider/readers.
Atomic entry gate и single-user guard не дают callback ждать control thread.
Перед setup/model edits/state capture/view removal/deactivation control thread
закрывает gate и ждёт завершения уже вошедшего блока. Освобождение DSP/model
происходит только на control thread после quiescence. В callback хост не делает
allocation/free, locks, disk I/O, GUI или model edits.
SDK transport requests, запрещённые из render thread, отбрасываются до mutex;
разрешённые concurrent non-render requests сохраняют прежнюю очередь.

Master с preview принадлежит device domain. Speculative producer не обрабатывает
preview; его копия графа не владеет preview route. В offline/export graph preview
отсутствует по умолчанию; Application включает его только для device graph.
Состояние preview не добавляет project schema, host Undo или vendor archive.

## Дополнение: выбор Vovious

Для клика по клипу host передаёт object selection с одним playback region и его
region sequence, `timeRange=NULL`, как требует ARA EditorView contract. Прежний
ненулевой timeRange ошибочно сообщал range selection. Соседние sequence не
скрываются: API `notifyHideRegionSequences` предназначен для скрытых host tracks.
Pin продолжает запрещать автоматическую отправку нового выбора. Реакцию Vovious
на исправленный object selection нужно подтвердить вручную.

## Локальные проверки

Release build, 25 адресных CTest PASS: ARA model/processed/editor, GUI smoke,
VST3 editor/realtime, ARA Test/JUCE lifecycle, audio transport/render/metrics,
recording/mixer/PDC/callback blocks, domains/mixed/ahead/parallel ownership.
Новый synthetic preview проверяет stopped/paused, Master gain, mono/stereo
route, блоки 1/17/128/256/1024/8192, close/reopen, один renderer общего документа,
отсутствие preview в offline graph и isolated preview fault.
Mixed producer с настоящим ARA preview и raw capture проверены отдельно:
preview слышен ровно один раз, во время записи output без preview, raw WAV
содержит только поданный generated input. Объектный выбор проверен на стороне
fixture EditorView; render-thread transport request не запускает Play.

1000×128 callbacks на 44100/48000/96000 Hz: host alloc/free=0, software deadline
misses=0; финальный замер max 29300/24000/42600 ns соответственно. Это synthetic
software measurement, не физический ASIO и не аудит внутренних locks/allocations
коммерческого плагина. Слуховая проверка Vovious preview и переключения его
дорожек, реальные vendor/physical DPI/ASIO — ожидают пользовательской приёмки.
Полный CTest и ARA-OFF build в этом пункте не запускались.

