# Приёмка 2026-10-10 — 0.3b

Принято2026-10-13: **0.3h upd2/#80**, «Все работает - закрывай!».
#80 closed completed; parent#25 актуально8/9, остаётся#87 (не начат).
Приёмка docs/issues-only, source/пакет/manifest/ярлык и прежние проверки неизменны;
новая ASIO/physical DPI матрица не предоставлена. [Контекст](PROJECT_CONTEXT.md).

0.3h upd2/#80, 2026-10-13: uniform marker/chord group gestures, chord color palette/radius3.
Schema21 (legacy1–20), archive1. Full130 PASS148.13s, packaged diagnostics/J3 exit0;
GUI100/150 inspected. Upd2 accepted2026-10-13 («Все работает - закрывай!»); #80 closed completed.
[Контракт/проверки](MUSICAL_LANE_GESTURES_UPD2.md). #87 не начат; code/build/package local.

Пользователь: «Маркеры работают. Закрывай». #82 закрыт completed. Правильная версия
среза 0.3b; исторический runtime package и embedded version 0.3a upd2 не переписаны.
Новый sub-issue увеличивает букву, следующий #85 — 0.3c. Ниже отчёт о выданном пакете.

# #82 — редактор маркеров

Начато 2026-10-10 после пользовательской приёмки #79 / 0.3a upd1.
База source68ff0cd, docs2566b6b; ветка mrs/0.2g-fix1-editor-space-local.
Следующий срез выбран по согласованному рабочему плану Stage 5.

## Контракт

- Полоса Markers под ruler; секции/Chords/track content сдвигаются на 24 logical px.
  Линейка времени по-прежнему занимает первые 20 px; PPQ960/tick/sample transform
  и принятые клипы/Snap/zoom/scroll сохраняются.
- Список слева раскрывает каждый marker ID, включая одинаковые ticks, с индексом,
  именем, позицией и kind. Линии/flags показывают tick; [+N] обозначает совпадения.
- + / double click пустого места / ПКМ Add открывают форму name/tick/kind.
  Double click события / Enter / ПКМ Edit редактируют; Backspace/Delete/toolbar
  удаляют выбранный маркер. Drag переносит выбранный marker, Snap1/16 как Arrange.
- Generic/Cue/Warning/Lyric/Action/Navigation — существующие MarkerKind.
  Name1..256 UTF-8 bytes, не пустое/пробельное, tick0..max_tick; общая модель
  проверяет kind и global ID. Читатель старых проектов не изменён.
- Append создаёт новый ID, edit сохраняет ID и project vector order. Markers
  не сортируются при редактировании; shared context сортирует индексы стабильно,
  поэтому current/next и tie order прежние. Каждая операция/gesture — один Undo.
- Preview, selection и list choice — временный UI state. Escape/focus loss,
  project/store/time/view change отменяют preview; forms/menus/gestures проверяют
  store и expected baseline. Invalid/no-op/cancel не меняют dirty/history.
- AddMarker/UpdateMarker/RemoveMarker общие Core Commands. Application проверяет
  candidate Project и полный candidate ProjectDocument; удаления с LiveAction
  отклоняются понятным reference guard до mutation. Никаких hidden binding removals.
- Metadata-only path/history допускает playback/loop, блокирует recording/pre-count.
  Никакого DSP rebuild/plugin capture/device restart/transport shift в этих edits.
  Name/kind edits не исполняют action/cue, seek/load также не добавляют executor.
  Callback не меняется, новых RT locks/allocations/I/O/UI calls нет.
- Schema15 / archive1 сохранены. Existing musical lanes, actions/ports, unknown
  chunks, clips/routing/plugin state сохраняются. Полные #85/#86/#87 не закрываются.
- Дополнение пользователя: из Chord Track удалена обводка всей области при фокусе.
  Выделение конкретного аккорда, Backspace и keyboard focus работают как прежде.

## Проверки

Feature source `e653440277f47adf6741dd0fbddce23b32858882`: cached configure/Release
all-target build PASS; полный CTest **120/120 PASS (98.48s)**. Desktop_marker
проверяет все kinds/ties/order/UTF-8, no-op/invalid/stale, playback/loop/sample/
device/plugin generations, reference guard, unknown chunks/actions/ports Save/Open,
recording/pre-count отказ. GUI smoke проверяет каждое совпадение в selector,
forms, body drag/Snap, one Undo/Redo, Escape/focus loss, stale/project replacement,
Backspace, Save/Open и ruler/clip/Chord regressions; pixel test подтверждает
отсутствие обводки всей Chord Track с keyboard focus.

Delivery source `7bcabaf1ce4b036584a6623fa576a60cb19dc704`: только высота track
heading и короткая подпись Del mark после visual review. GUI rebuild PASS и
packaged J3/diagnostics PASS / exit0 / version=0.3a upd2 / clean_shutdown;
session `mr-session-1791600776941-13764-1.log`. Fresh software100/150 previews
просмотрены. Первоначальные build/test FAIL не считаются PASS: test API typos,
старые hardcoded header coordinates и selector increment/evaluation-order bug
исправлены перед финальными проверками.

Компактный пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-upd2-Markers-JUCE-ASIO-Windows-local`.
EXE SHA256 `BFC1B9F38F08444B6CA4C2ED566CB96C2E3F3E68388990D25BBA1EEF644FE38D`.
Matching PDB/previews/logs отдельно: `build/asio-local/deliveries/0.3a-upd2`.
Build logs: `03a-upd2-regression-build.log`, `03a-upd2-selector-build.log`,
`03a-upd2-delivery-build.log`; полный CTest log `03a-upd2-final-ctest.log`.
Старые пакеты сохранены. FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK.
Реальная pointer/ASIO/physical DPI и пользовательская приёмка #82 пока NOT RUN.

## Пользовательская проверка

1. + в Markers: создать Generic/Cue/Warning/Lyric/Action/Navigation и русское имя.
2. Создать два marker на одном tick, выбрать каждый в списке и изменить независимо.
3. Enter/double click flag: имя/kind/tick; drag/Snap, Undo/Redo, Escape отменяет.
4. Backspace удалить выбранный marker; Undo восстановит точный ID/order.
5. Save/Open сохраняет marker values/ties и прежние аккорды/клипы.
6. Проверить ruler seek, zoom/scroll и клипы; Chord Track больше не обведён целиком
   при клике, но отдельный выбранный аккорд виден и редактируется.
7. Linked LiveAction delete guard проверен на synthetic project автоматикой;
   Action marker сам по себе не отправляет MIDI.
