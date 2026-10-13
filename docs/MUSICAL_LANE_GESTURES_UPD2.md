# Musical lanes: unified gestures — 0.3h upd2 / #80

Принято2026-10-13: **0.3h upd2/#80**, «Все работает - закрывай!».
#80 closed completed; parent#25 актуально8/9, остаётся#87 (не начат).
Приёмка docs/issues-only, source/пакет/manifest/ярлык и прежние проверки неизменны;
новая ASIO/physical DPI матрица не предоставлена. [Контекст](PROJECT_CONTEXT.md).

Пользователь2026-10-13 принял дополнения секций0.3h upd1 («Отлично!») и запросил
такие же copy/group/ghost gestures у маркеров и аккордов, меню цветов и radius3
у аккордов. Новое обновление не закрывает#80 до своей приёмки; #87 не начат.

## Контракт и слои

UI/shared Core/Application/history/persistence/tests/docs AFFECTED. Engine/DSP/
device/callback/routing NOT AFFECTED: существующий metadata-only путь, без rebuild
плагинов. Shared commands, Studio Arrange UI. Transient selection/preview не
сохраняются в проект и не добавляют Undo. Legacy editing/playback/record guards
аккордов/маркеров прежние; секции сохраняют свой stopped/loop contract.

Рамка из пустого места полосы и Shift+ЛКМ добавляют/убирают элементы. Drag body,
Alt-copy и toolbar/Ctrl+D Duplicate работают со всей выбранной группой;
Backspace/Delete удаляет группу. Одна committed операция — одна общая Undo entry.
Escape/focus/смена проекта/временной карты/масштаба отменяют preview. Alt-copy
рисует original и alpha0.35 ghost всегда; invalid chord overlap отмечен salmon,
без commit. Chords sorted/non-overlap/[start,end), gaps/duration сохраняются;
touch7px от ограниченного raw pointer захватывает точную границу, выше Snap.
Markers допускают совпадения по времени; старые IDs/порядок/tie order сохраняются,
новые копии appended с новыми IDs, без переноса LiveAction bindings.
Linked group deletion отвергается целиком; stale/invalid/overflow без dirty/history.
Marker Duplicate offset=max(group span,ppq/4 — шестнадцатая нота); chord offset=group span.
ПКМ action menu остаётся для конкретного элемента. Палитра аккорда — как у
секций/маркеров, открывается в точке нажатия. Radius3/тонкая обводка/alpha0.72.

Chord optional RGB сохраняется в schema21; schema1–20 читаются, legacy default
unset сохраняет прежний neutral/current fill. Archive envelope1 не меняется.
RGB/presence validation; color/copies/Undo/Redo/SaveOpen сохраняют metadata.
Ранние версии приложения schema21 не читают; прежние packages/проекты не меняем.

## Проверка

Core/App group history/stale/overlap/order/IDs/references и schema20→21/defaultRGB/
roundtrip; GUI Shift/marquee/Alt-shadow/touch/negative/Escape/duplicate/delete,
palette/color/history/SaveOpen, rendering100/150. Полный Release и packaged smoke.
Hardware ASIO/physical DPI отдельно; результат добавляется после проверок.

## Результат2026-10-13

FEATURE ACCEPTED2026-10-13: «Все работает - закрывай!». Source `803774d1df9b7a7554550530b5b500f37ec170cc`.
Release/full130 PASS148.13s до final marker marquee bounds; finalfocused3 PASS43.39s.
Initialfocused3 PASS42.41s, Core/persistence29 PASS0.55s.
GUI100/150 inspected; packaged diagnostics/J3 exit0. Новые group/Shift/marquee/
copy/ghost/Escape/negative/Duplicate/Delete/one Undo/palette checks; Core exact color/
copies/SaveOpen/schema20 migration/stale/overlap/order/ties/actions protection tested.
Initial test fixture/compile failures corrected; logs validation/arranger80-upd2.
J3 timeout75s for expanded coverage; no masking of failed assertions.
Package MR-Studio-0.3h-upd2-Unified-Lanes-JUCE-ASIO-Windows-local,13files/12hashes;
GUI SHA256 `01538A2CD57ACE48093BF3116145C9A8F18BD1E294BE4DCF6694E83D056E9080`. Symbols deliveries/0.3h-upd2-unified-lanes;
MR shortcutupd2/icon retained, oldupd1 package unchanged. ASIO/physical DPI NOT RUN.
Приёмка: группы обоих lanes рамкой/Shift, move/Alt-shadow/Duplicate/Ctrl+D/Delete/
Undo/Redo; chord touching/overlap, marker ties/list, ПКМ colors и radius3, SaveOpen.
#80 closed completed; parent#25:8/9, #87 не начат; source/build/packages local, docs/issues GitHub.
