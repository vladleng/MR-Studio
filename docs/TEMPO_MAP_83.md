# #83 — карта темпа / 0.3f

2026-10-12: прямой запрос «Поехали #83»; #80 остаётся open. База c871edc,
принята0.3e upd2, local HEAD df94aef перед работой, ветка mrs/0.2g-fix1-editor-space-local.

C0: time-only prepare/commit. Core ProjectStore transaction предварительно
выделяет candidate/history/event. Commit меняет state/history без уведомления;
finish публикует после успешного backend start. Неуспешный transaction восстанавливает
snapshot/revision/Undo/Redo без transient project events. Existing execute не меняется.
Engine TimingPlan заранее строит Timeline-derived tempo samples, MIDI/external
schedulers и Metronome, без DSP/asset/plugin recreation. Device stop/join precedes
symmetric stopped timing swap; restart repopulates ahead/mixed owners from new map.
Backend start failure: stop/quiesce, timing/store rollback, restart old map; recovery
failure сообщается явно, без скрытого disconnect. Sample playhead остаётся прежним,
queued controls/live MIDI routes/generation и plugin editors сохраняются.
Guard Pause/Stop/no recording/precount/active loop действует в edit/history.

RT-CRITICAL: существующий loop считает chunk до ближайших tempo/loop boundaries,
поиск tempo binary search; channels/legacy/Master получают один position/BPM/quarter
на каждом chunk. Без новых allocation/locks/I/O; profiler aggregate по callback.
UI/commands/engine/history/tests/docs affected; persistence schema20/archive1 existing
TimeMap grammar без изменения, WAV samples/MIDI musical ticks/other IDs unchanged.
Tempo anchor0 не удаляется/не двигается, BPM1–1000 finite, unique sorted ticks;
rounded sample collision также отклоняется до runtime mutation.

UI: полоса Tempo под Chord Track, список всех точек и + в позиции курсора.
Double click по пустой полосе добавляет точку; double click/ПКМ по точке открывает
меню Position bar:beat:tick / BPM / Apply. Body drag меняет tick с preview/Snap,
Escape отменяет; Backspace/Delete удаляют выбранную точку, кроме anchor0.
Карта применяется одним Undo; смена проекта/time/zoom/scroll отменяет жест.
Временные selection/preview не сохраняются. Стили Theme/font14 сохранены.

Проверки: Release PASS; итоговый полный CTest129/129 PASS146.00s, включая
GUI6, desktop_tempo, MIDI/VST3, mixed/ahead/record/persistence/RT регрессии.
Новые тесты проверяют120→90→140, backend restart/publish failure без project events,
Undo/Redo failure, invalid/stale/no-op, timeline overflow до mutation, WAV/music
anchoring, sample head/plugin generations, click golden через две границы, VST3
channel/instrument/Master context внутри блока и при loop, zero host allocations.
Mixed equivalence — побитовое сравнение с serial, каждый sample через Master один раз.
J3 popup/drag/invalid/Undo/anchor и screenshots100/150 просмотрены. Ранние compile/
fixture/geometry/callback-count отказы сохранены в validation/tempo83; после исправлений
финальный полный прогон зелёный. GUI J2 окно учитывает новую полосу; DSP не исправлялся
ради старого call-count предположения. Actual ASIO/physical DPI NOT RUN; acceptance pending.
FEATURE READY WITH MANUAL CHECK. Схема20/archive1 и старые packages не изменены.

Ручная приёмка после выдачи:
1. В Pause/Stop создать120→90→140 через Tempo, переместить/удалить/изменить BPM;
   anchor0 не двигается/не удаляется. Дубликат позиции/BPM0 отклоняются.
2. Проверить MIDI+Click при Play/seek и через обе границы; WAV не растягивается,
   sample playhead после правки остаётся на месте, музыкальная позиция пересчитывается.
3. Undo/Redo, Save/Open возвращают карту; вставленные плагины/editor сохраняются.
4. Playback/loop/recording/precount запрещают timing edit без частичных изменений;
   после отключения ограничений карта редактируется и запись работает.
Никаких ramps/time stretch/detection; schema20/archive1 остаются прежними.

Доставка: source `eb2f30e53406d87402803b6b1ab0aaa353992df7`, package `MR-Studio-0.3f-Tempo-Map-JUCE-ASIO-Windows-local`,
GUI SHA256 `8591719AB45ABF2E1893AE9F04D139A57EA21E8ABB816FBAC353B7AC9A46FDA7`. Packaged diagnostics/J3 PASSexit0;
13 runtime files/12 hashes reverified. Прежний ярлык обновлён/иконка сохранена,
старые пакеты остаются. Symbols deliveries/0.3f-tempo-map.
#83 open до пользовательской приёмки; #80 open. Без push/merge/Actions.
