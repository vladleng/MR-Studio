# #84 — карта размера / 0.3g

2026-10-12: «Начинай работу на #84». База — принятая0.3f/#83, sourceeb2f30e,
HEAD8a1b1fb, branchmrs/0.2g-fix1-editor-space-local, clean перед работой.

Контракт: bar-based MeterPoint; anchorbar1 editable, не удаляется/не перемещается;
numerator1–64, denominator1/2/4/8/16/32/64, sorted unique bars. Изменение раннего
meter пересчитывает ticks последующих meter points; остальные musical/MIDI ticks,
WAV samples и sample head прежние. Никакого time stretch/content ripple.

UI → Application.set_meters → UpdateMeterMap expected → existing stopped edit_timing
prepare/transaction/engine swap/start/publication. Undo/Redo повторяет preparation;
Pause/Stop/no record/count/loop, atomic failure/stale/no-op. Schema20/archive1 прежние.
MeterTrack под Tempo: +/doubleclick create, popup bar/numerator/denominator,
ПКМ/remove/drag между тактами/preview/Escape/Backspace. Selection/preview transient.
Ruler/Arrange/piano-roll используют denominator-based beats; MIDI quarter snap и
quantize остаются прежними (6/8 beat480, quarter960).

RT-ADJACENT: compile meter sample/quarter segments на control thread; TimingPlan
swap со stopped/joined owners. RT-CRITICAL: bounded binary search/callback split
по meter/tempo/loop boundary; per-chunk numerator/denominator/bar-quarter → channel,
instrument/legacy/Master, SDK timeSig/barPosition valid flags. Без RT allocations/
locks/I/O, one DSP owner. NON-RT: UI/grid/commands/persistence/tests/docs.

Affected layers: Core command/model existing validation; App time-only transaction;
Engine/PreparedGraph/ProcessBlock/VST3 context; JUCE MeterTrack/grid/PianoRoll;
history/persistence compatibility; synthetic/GUI regressions. Live reads shared model.

Проверки: Release PASS; focused3 PASS31.53s, итоговый полный CTest130/130
PASS140.74s, включая GUI6, desktop_meter/tempo, MIDI/VST3, mixed/ahead/recording,
persistence/compatibility/RT allocation suites. 4/4→6/8→3/4 и раннее2/4 проверяют
репроекцию bar5 из14ppq в10ppq; другие ticks/WAV/IDs/sample head и поколения плагинов
сохраняются. Invalid/duplicate/anchor/stale/no-op/overflow (в том числе disconnected),
backend restart/publish failure и history откатываются без частичной карты.

VST3 fixture проверяет timeSig/barPosition valid flags, quarter/bar start и отсутствие
пересечения meter boundary одним plugin chunk; instrument+trackFX+Master, loop/seek,
zero host RT allocations. Ускоренная карта1/64→6/8→3/4 на1000BPM даёт границы180/8820
samples для проверки внутри callback. Click golden по каждому sample через обе реальные
границы4/4→6/8→3/4; запись после правки и precount6 eighth beats=72000 samples при120BPM,
без раннего движения sample head. Recording/count guards и SaveOpen проверены.
GUI: create/edit/drag/Undo/anchor/duplicate/invalid denominator; six eighth-note grid
projections. Свежие main100/editor150 snapshots inspected. Физический ASIO и DPI NOT RUN.
Первый linker failure: новый MeterTrack не был зарегистрирован в CMake; исправлен.
Первый J3 failure: тестовая пустая область оказалась вне уменьшенного viewport;
увеличено только тестовое окно. Логи validation/meter84 сохранены.

FEATURE ACCEPTED2026-10-13: «Принимаю. Что у нас дальше?». Schema20/archive1 прежние. Пакет `MR-Studio-0.3g-Meter-Map-JUCE-ASIO-Windows-local`; #84 closed completed по пользовательской приёмке.
Сценарии выданной приёмки (пользователь сообщил общий PASS без отдельной матрицы):
1. В Pause/Stop/loop off создать4/4→6/8→3/4 через Meter, изменить Bar/numerator/denominator,
   переместить точку по тактам, Undo/Redo/Delete. Bar1 не двигается/не удаляется.
2. Проверить ruler/BBT/piano-roll и MIDI+Click; в6/8 шесть восьмых долей, quarter snap и
   quantize сохраняют прежнюю семантику. Early meter edit пересчитывает последующие meter
   points по их bar numbers, остальные musical ticks/WAV samples не переносит.
3. Save/Open сохраняет карту; sample cursor и plugin/editor state после правки прежние.
   После смены размера работают playback/seek/loop/recording/precount. Во время playback/
   loop/record/count изменение карты отклоняется.

Доставка: source `7d90ad2478c0cb4de792dcb9a2df9f976b26ef40`, GUI SHA256 `39D65DE7DC63DC42928DD4E0F399B67AC9B5CAD88417DBAEBC5348F8D9899304`.
Packaged diagnostics/J3 PASSexit0,13 files/12 hashes reverified; symbols
build/asio-local/deliveries/0.3g-meter-map. MR shortcut0.3g/icon retained/target verified.
Код/build/tests/package локально; docs/issues на GitHub, без source push/merge/Actions.

2026-10-13: приёмка записана без изменений source/package/manifest/shortcut. Новые ASIO/DPI измерения не заявляются; rebuild не нужен. Следующим рекомендован #80/0.3h, пока не начат.
