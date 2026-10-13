# Arranger editor #80 — 0.3h

Принято2026-10-13: **0.3h upd2/#80**, «Все работает - закрывай!».
#80 closed completed; parent#25 актуально8/9, остаётся#87 (не начат).
Приёмка docs/issues-only, source/пакет/manifest/ярлык и прежние проверки неизменны;
новая ASIO/physical DPI матрица не предоставлена. [Контекст](PROJECT_CONTEXT.md).

## Дополнения0.3h upd1 — 2026-10-13

По пользовательскому screenshot: Markers получает подпись и компактный list
на x70/width132; Sections и Chord Track используют отдельный «+» x208/width34,
как Tempo/Meter. Прежние add/navigation/edit semantics сохраняются.
Alt-copy preview всегда видим, alpha0.35; invalid overlap — salmon outline,
release без commit. Границы в пределах7 logical px захватываются точно встык,
с сохранением длины и внутреннего расстояния между выбранными секциями.
Рамка из пустой области полосы выбирает группу; Shift+ЛКМ добавляет/убирает
конкретную секцию. Выделение transient, не Save/Undo. Move/Alt-copy/Duplicate/
Backspace работают с группой атомарно через shared EditSectionGroup/Application;
одна Undo, stale/overlap/overflow/binding/transport guards действуют на всю группу.
Name/color/bounds popup остаётся редактором конкретной секции.
Слои UI/Core/App/history/tests/docs затронуты; engine/DSP/schema не изменены.
FEATURE ACCEPTED2026-10-13: «Все работает - закрывай!». Source `9ab296b5e91614ac814f21fd70d75a1b788458ac`.
Release/full130 PASS136.47s до final UI boundary clamp; finalfocused2 PASS26.14s.
Final GUI100/150 inspected; packaged diagnostics/J3 exit0.
Новые GUI regression: Shift toggle/marquee/group Alt copy/touch/offgrid+coarseSnap/
invalid-shadow/group Delete/one Undo; App group copy/move/atomic stale/overlap/binding.
Package MR-Studio-0.3h-upd1-Arranger-Editor-JUCE-ASIO-Windows-local,13files/12hashes;
GUI SHA256 `A00C08B3D39E640EA40433044AABCB738EFAB7708EA5626D804C743234CE310C`. Old0.3h package preserved; MR shortcutupd1/icon retained.
Приёмка: компактные headers, копия встык с Snap/on/off и off-grid bounds, видимая
тень на overlap, рамка/Shift toggle, group move/Alt-copy/Duplicate/Delete/Undo/SaveOpen.
Hardware ASIO/physical DPI NOT RUN. #80 open, #87 не начат.

Начато 2026-10-13 по прямому запросу пользователя. База: принятая0.3g/#84.

## Контракт

Принятые CRUD/rename/move/bounds/colors/delete из #81 сохраняются. Duplicate
(toolbar/ПКМ/Ctrl+D) создаёт секцию сразу после выбранной, как Duplicate для
аккордов. Alt+drag тела копирует секцию в указанное место, с preview до отпускания
ЛКМ, Escape отменяет. При overlap копия отклоняется, секции не сдвигаются.
Имя/цвет/длина сохраняются, ID новый, SectionPatch не наследуется. Содержимое
дорожек, аккорды, маркеры, tempo/meter и sample playhead не изменяются командой.
Create/copy/move/bounds/delete: Pause/Stop, loop off, без recording/pre-count.
Одна завершённая операция — одна запись общей Undo; invalid/stale — без публикации.

Пять иконок в левом заголовке линейки переключают Markers, Sections, Chord Track,
Tempo, Meter. Активные — синие, с tooltip/accessibility name. Скрытая полоса
освобождает место; painting/hit/drop/scroll используют общую динамическую геометрию.
Все полосы по умолчанию видны. Битовая маска сохраняется в UI preferences version1,
legacy default31; проект и Undo не меняются. Смена видимости отменяет previews/
inline editing/popups/выделение полос. Chord Track caption — Theme::browserFont,
как Sections; стиль остальных компонентов сохраняется.

## Слои и проверки

UI/commands/shared Core/Application/history/tests/docs: AFFECTED. Persistence:
existing sections schema20/archive1, новый проектный формат не нужен. Preferences:
новый optional key. Engine/DSP/audio callback/routing/transport: NOT AFFECTED;
используется существующий control-thread metadata-only path, без graph rebuild.
Studio/Live: shared model/command, Arrange UI Studio; Live bindings сохраняются.
Clipboard Ctrl+C/V и content rearrangement/ripple: вне этого slice.

Проверки: copy stale/overlap/overflow/uniqueID/Undo/Redo/SaveOpen/binding/guards,
GUI Alt-copy/cancel/Duplicate, все32 комбинации видимости/layout/hit, legacy prefs,
100/150% snapshots, Release/CTest/package smoke. Пользовательская приёмка и
physical DPI/ASIO проверка отдельно от автоматических результатов.

## Результат — 2026-10-13

FEATURE ACCEPTED2026-10-13: «Все работает - закрывай!». Source `57c329ecc6e637edbdbcc6651e767a0c54ec0f36`.
Release/full130 PASS98.38s, focused2 PASS25.44s; core/App copy guards, exact Undo/
Redo/SaveOpen/content/binding retention; GUI Alt-copy/cancel/overlap/toolbar Duplicate,
все32 masks/compact geometry/clip hit/preferences, legacydefault/invalidmask/inline
rename cancellation. GUI100/150 screenshots inspected, packaged diagnostics/J3 exit0.
Нет engine/DSP/routing изменений. Initial compile warning/private-test-access и
build-overlap/PDB отказ исправлены, логи validation/arranger80 сохранены.
Package MR-Studio-0.3h-Arranger-Editor-JUCE-ASIO-Windows-local,13files/12hashes verified;
GUI SHA256 `F2CC9919AC24EA07629519B8E63AD308262ECDEB798C69BB76482F097DAB3511`. Symbols deliveries/0.3h-arranger-editor.
Прежний MR shortcut0.3h/icon retained/target+workingdir+emptyargs verified;
старые packages сохранены. Hardware ASIO/physical DPI NOT RUN.

Приёмка: включить/скрыть каждую полосу, перезапустить и проверить сохранение;
Alt-drag секции в gap, Duplicate toolbar/ПКМ/Ctrl+D, overlap/Escape, Undo/Redo,
Save/Open. Проверить обычное clip dragging и MIDI/VSTi drop при скрытых полосах.
#80 открыт до приёмки; #87 отдельно, не начат. GitHub docs/issues, код/пакеты локально.
