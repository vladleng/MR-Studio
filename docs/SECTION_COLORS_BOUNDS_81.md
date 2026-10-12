# #81 — цвета и границы секций, 0.3e upd1

Прямой запрос 2026-10-12: «Пtререходи к #81». Это следующий musical slice
после принятой0.3d upd3. #80 остаётся открытым: body move/copy
секций не входят в этот срез. Rename/delete добавлены в upd1 по запросу пользователя. Создание section — минимальный prerequisite,
чтобы цвета и границы были доступны в пустом проекте.

## Контракт и слои

Project sections — единственный источник данных. Shared Core AddSection и
UpdateSection/RemoveSection сохраняют stable ID, сортируют по start и отклоняют stale expected.
Project validation проверяет overlap/[start,end)/colorRGB. Application публикует
metadata без graph prepare/plugin capture/device restart. History идёт тем же
путём, отдельно проверяя shape/loop/count/record guards и валидность связей.
UI/commands/Undo/validation/shared context/tests/docs затронуты; audio callback,
WAV/MIDI samples/ticks, routing и plugin state не меняются. Persistence использует
существующие sections schema19/archive1, новые поля не нужны. Future Live читает
те же section IDs; исполнение actions не добавляется.

## Интерфейс

Sections между маркерами и Chord Track. + Section / double click пустой полосы
создают section. ПКМ на блоке открывает выпадающий редактор в месте нажатия,
со стилем и шрифтом остальных меню. Имя редактируемое; Start/end —
bar:beat:tick, end exclusive. Точные границы и RGB находятся только в меню;
на блоке отображается одно имя. Внизу семь цветных квадратов: выбор применяет
текущие поля одним действием. Apply также применяет поля, Enter в поле — Apply;
Escape/нажатие вне меню отменяют неприменённые изменения.
Светлые секции получают тёмный текст, тёмные — белый. Рамка и заливка используют
одинаковые bounds/radius/stroke; по вертикали равные отступы2px. Дробная musical
позиция остаётся точной, искусственного приведения к линии такта нет.
ЛКМ выбирает секцию и сохраняет navigation. Backspace/Delete удаляет выбранную
секцию; Remove section доступен в меню. Другие lanes снимают selection.
Имя/удаление имеют Undo/Redo/SaveOpen; удаление связанной SectionPatch отклоняется
с объяснением, без разрушения связей. #80 целиком этим апдейтом не закрывается.

Body click сохраняет #86 seek. Edges имеют6 logical px hit area, ограниченную
для узких секций; paint/hit-test используют единый tick→sample transform.
Drag preview временный, соседние границы ограничивают resize; snap1/16.
MouseUp — одна Undo entry, Escape/focus loss/stale model/time/zoom/scroll
отменяют preview. Проект и границы не меняются до commit.

Color/Undo/Redo не меняют loop/playhead/DSP. Create/bounds/history этих операций
требуют Pause/Stop, no active loop, no recording/count. No-op/cancel не dirty.
Связи SectionPatch проверяются document.validate; ID никогда не заменяется.
Clips/chords/markers не двигаются при редактировании section bounds.

## Проверки

План: color/bounds history, stale/invalid/overlap rollback, playback/loop guards,
retained device/insert generations/sample head, SaveOpen/IDs/references; GUI
palette/dialog/precise positions/edges/cancel/preview/oneUndo/zoom/scroll и
100/150/200 software previews. Локальный Release/full suite, packaged J3.
Статус IMPLEMENTING, проверки ещё не заявлены пройденными.

Ручная приёмка: создать Intro/Verse/Chorus с gaps, выбрать цвета и RGB, изменить
обе границы диалогом/drag, отменить жест, Undo/Redo/SaveOpen; проверить current
section и navigation, отсутствие перемещения clips, loop/record guards.
Hardware ASIO/physical mixed-DPI отдельно от программных snapshots.

Первый Release имел ошибку mixed-auto объявления в тесте; исправлена.
Focused desktop_sections + J3 **2/2 PASS34.10s** на первоначальном срезе.
Дополнительно проверены linked SectionPatch/unknown extensions и recording/count
rejection, команды PASS. Frontend history_preserves_plugins сохраняет editor
для metadata Undo/Redo; full GUI regression проверяет native insert window.
Подписи широких sections показывают итоговые bar:beat:tick; клавиатурный Enter
открывает форму, Backspace/Delete в этой полосе не удаляют соседние lanes.
Финальный Release/full128 в работе, результаты ещё не заявлены.

Release/full CTest **128/128 PASS140.94s**, включая sections/MIDI/VST3/RT/GUI.
200% root preview первоначально дописывался к старому PNG; test exporter
исправлен truncate/reset перед записью. Финальный GUI run после test-only
исправления в работе; packaged smoke и fresh100/150/200 inspection далее.

Final GUI6/6 **PASS72.26s**, fresh100/150 form и200 lane просмотрены.
Software scaling не является physical monitor DPI-переходом. Package/J3 далее.

Delivered **0.3e/#81**, source `61c3fbd2173d9bebde576f2b8676bac31ada6fa2`.
Release/full128/128 PASS140.94s, final GUI6/6 PASS72.26s, packaged J3 PASSexit0.
Fresh lane100/150/200 и editor100/150 previews inspected (software scaling):
`build/asio-local/validation/sections81/packaged-smoke`.
Compact13 files/12 hashes rechecked:
`Builds/MR-Studio-0.3e-Section-Colors-Bounds-JUCE-ASIO-Windows-local`.
GUI SHA256 `3D2F36016C07BAC654C407B7CE382DCFDE07E825B7C9D85F52542163A372FF52`.
Symbols `build/asio-local/deliveries/0.3e-sections81`. Persistent MR shortcut
updated, icon retained/target verified. Accepted0.3d upd3/older packages retained.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK; #81 user acceptance
pending; hardware ASIO/physical DPI not measured. #80 remains open.

## Follow-up 0.3e upd1 — 2026-10-12

Исходная0.3e не принята: замечания к рамке/подписям/модальной форме,
Backspace/readonly name. Добавлены RemoveSection expected guard + Application
metadata validation, rename, themed PopupMenu custom editor/палитра, ПКМ anchor.
Новая схема не нужна, audio callback/graph не менялись. Новые проверки включают
ПКМ, editable name, delete/Undo/Redo, dismiss/no-op, overlap/stale/ref rejection,
SaveOpen имени и fractional tick/zoom/scroll geometry. Проверки upd1 в работе.

Release upd1 собран. Focused desktop_sections + J3 **2/2 PASS22.02s**.
Первый GUI запуск отклонил Cyrillic rename fixture: тест передавал non-ASCII
литерал в ASCII-конструктор JUCE String. Исправлен только fixture на fromUTF8,
также проверен Backspace внутри поля имени (символ, не секция).
Первый full128:127/128 PASS66.89s, diagnostics/J3 упал на существующей native
editor Space focus-проверке до section tests. Повтор этой проверки через CTest
также упал; изолированные hidden diagnostics/J3 предыдущей0.3e и текущейupd1
оба exit0, текущая22.53s/clean_shutdown. Product keyboard код не менялся.
Полный повтор в работе; failed logs сохранены как ctest-first/diagnostics-recheck.

Финальный full CTest **128/128 PASS128.38s**, diagnostics/J3 PASS22.87s,
обычный J3 PASS; без новых изменений в keyboard/product между двумя прогонами.
Software lane100/150/200 и popup form100/150 generated; lane100/popup150
просмотрены, ровная рамка/читаемые поля/палитра подтверждены. Упаковка далее.

Доставлена **0.3e upd1**, source `4ce08e0825476ac101cc1dc5f1be38e492d474e1`.
Package `Builds/MR-Studio-0.3e-upd1-Section-Menu-JUCE-ASIO-Windows-local`,
GUI SHA256 `6473418C246143FF8539D9DDF3C56C09B453FB26999EFAEF581657D8D95F1AF4`.
Packaged diagnostics/J3 PASSexit0/clean shutdown, fresh popup150 inspected;
13 files/12 hashes rechecked после smoke. Symbols `deliveries/0.3e-upd1-section-menu`,
logs `validation/sections81-upd1`; постоянный MR shortcut upd1/icon retained/target
verified. Старые packages неизменны. #81 открыт, пользовательская приёмка upd1
ожидается. #80 открыт, body move/copy в этом срезе не добавлялись.

## Follow-up 0.3e upd2 — 2026-10-12

По10 новым замечаниям (приёмка upd1 не подтверждена): Section band33px,
блок30px, font14/1.04 как у обычных кнопок, radius3, clipFillAlpha0.72.
Sections44..77, Chords77..101; между видимыми блоками3px вместо5px.
Drag середины: preview/сохранение длины/snap/один Undo/Cancel, допустимые gaps,
перекрытие соседа отклоняется в preview. Создание/границы/перемещение/удаление
имеют прежние Pause/Stop/loop/count/record guards. Clips не перемещаются.
Двойной клик существующей секции/маркера — inline name editor с выделенным именем;
Enter/потеря фокуса применяют, Escape отменяет, stale/project replacement guarded.
ПКМ секции сохраняет полный editor, ПКМ маркера — меню в точке клика,
Rename/Position-kind/Remove/Seek/coincident choice и7 квадратов палитры внизу.
Маркерная плашка alpha0.72; kind не меняется при выборе цвета/переименовании.
Marker.color optionalRGB, schema20; legacy1–19 -> unset/kind-color fallback
(blue, warning amber), archive1 unchanged. Unknown chunks/IDs/bindings сохраняются.
UI/core/serialization/migration/tests затронуты; audio callback algorithms/routing
не менялись, весь код с новым model layout пересобирается до запуска тестов.
Build/tests в работе, старые packages/shortcutupd1 пока сохраняются.

Upd2: первое compile отклонено /WX из-за optional<uint32> vs int comparison
в GUI тесте; unsigned literal исправлен, product код не затронут этим исправлением.
Focused31 core/musical/persistence/Desktop tests PASS; первый J3 выявил fake
ПКМ/menu pumping dismissal на native temporary surface. GUI fixture выбирает
палитру до unrelated OS cursor pump, меню в реальном ПКМ остаётся anchored.
Focused J3 PASS36.78s, full CTest128/128 PASS164.54s. По fresh snapshot обнаружено,
что paintGrid перезаписывает font секций; font14 устанавливается после grid.
После этой единственной UI/font правки Release rebuilt, final GUI6 в работе.

Final Release/font correction + GUI6/6 PASS76.10s; fresh section150 и marker
ПКМ palette100 inspected. Font после grid соответствует browser/button14.
Упаковка далее, user acceptance pending; hardware ASIO/physical DPI не измерены.

Delivered **0.3e upd2**, source `c871edc801133e204066c9edf0c73edfe3d44a17`.
Full128/128 PASS164.54s; final GUI6/6 PASS76.10s after font-only correction.
Packaged diagnostics/J3 PASSexit0; fresh section150/menu100/marker100 inspected,
13 files/12 hashes reverified. Package `Builds/MR-Studio-0.3e-upd2-Sections-Markers-JUCE-ASIO-Windows-local`.
GUI SHA256 `3F2C6A54C90ECA6EC8BD45A73436AC4BF8F4471AA72D860B3C74468ACD861BA2`. Logs validation/sections81-upd2,
symbols deliveries/0.3e-upd2-sections-markers. MR shortcutupd2/icon retained/target
verified; old packages unchanged. #81 open/acceptance pending; #80 open (copy/общая
приёмка впереди). Все10 требований реализованы; hardware ASIO/physical DPI отдельно
не измерены. Schema20/reader1–20/archive1, source/build/docs только локально.

## Приёмка — 2026-10-12

Пользователь: «Отлично принимаю! Что дальше?» **0.3e upd2 / #81 accepted**;
issue закрыт completed, parent #25 checkbox #81 отмечен. Sourcec871edc,
full128 PASS164.54s/finalGUI6 PASS76.10s/packaged diagnostics exit0 сохраняются.
Package/manifest/shortcut неизменны, новая сборка не требуется. #80 остаётся
open: copy/дублирование и итоговая сверка полного редактора, рекомендован0.3f.
Новый код не начат. После #80 — #83 tempo, #84 meter, #87/#25 final.
Документация только локально; на GitHub обновлены только issues.
