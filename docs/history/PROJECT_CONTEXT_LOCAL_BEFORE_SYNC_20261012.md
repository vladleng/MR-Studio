# История локальных поставок до синхронизации 2026-10-12

Статусы ниже исторические. Текущая информация: [PROJECT_CONTEXT](../PROJECT_CONTEXT.md).

Предыдущая принятая доставка:
Принято **#81 /0.3e upd2**, 2026-10-12: «Отлично принимаю! Что дальше?»
#81 closed completed; parent #25 checklist обновлён, #80 остаётся open.
Section band33/block30/font14/width1.04/radius3/alpha0.72; gap3px до Chord Track.
Grid сбрасывал font: он восстановлен после рисования grid, размер captions
подтверждён свежим snapshot. Drag середины перемещает section metadata,
сохраняет длину/snap/preview/Undo/Cancel; соседей не перекрывает, clips не двигает.
Double click существующей section/marker — inline name editor, Enter/blur commit,
Escape cancel; stale/project guards. ПКМ section — полный bounds/color popup,
ПКМ marker — menu в точке нажатия, Rename/details/remove/seek/ties и7 цветов
внизу. Marker badge alpha0.72; имя/цвет не меняют tick/kind/ID.
Schema20: optional Marker.color, legacy1–19 -> unset/kind fallback; archive1,
unknown chunks/bindings/routing/RT callback прежние. Новые saves требуют reader20.
Source `c871edc801133e204066c9edf0c73edfe3d44a17`. Full rebuild Release; focused31 model/persistence/Desktop
PASS + J3 PASS36.78s; full CTest128/128 PASS164.54s. После font-only correction
Release + final GUI6/6 PASS76.10s. Packaged diagnostics/J3 PASSexit0/clean shutdown.
Первый compile /WX unsigned comparison в тесте исправлен; fake marker PKM fixture
выбирает palette до unrelated native cursor pump. Ошибки/logs сохранены.
Fresh sections100/150/200 и marker/menu100/150 generated; section150/marker/menu100
inspected.13 files/12 hashes rechecked. Package `Builds/MR-Studio-0.3e-upd2-Sections-Markers-JUCE-ASIO-Windows-local`,
GUI SHA256 `3F2C6A54C90ECA6EC8BD45A73436AC4BF8F4471AA72D860B3C74468ACD861BA2`.
Logs `build/asio-local/validation/sections81-upd2`, symbols
`build/asio-local/deliveries/0.3e-upd2-sections-markers`. Постоянный MR shortcut
обновлён наupd2/icon retained/target verified; old packages preserved.
[SECTION_COLORS_BOUNDS_81.md](../SECTION_COLORS_BOUNDS_81.md).
FEATURE ACCEPTED. Hardware ASIO/physical DPI отдельно не измерены.
Далее рекомендован **#80 /0.3f**: copy/дублирование секций и итоговая сверка
полного редактора; пока не начат. Затем #83 tempo, #84 meter, #87/#25 final.
Приёмка документальная: build/package/manifest/ярлык прежние, rebuild не нужен.
#80 open: copy/целостная приёмка ещё впереди,
body move/rename/delete добавлены прямыми запросами в #81 updates.
Другие issues автоматически не начинать. Код/build/docs локально,
GitHub issues only, без push/merge/Actions.

Предыдущая доставка (upd1, не принята):
Активно **#81 /0.3e upd1**, доставка 2026-10-12. Исходная0.3e не принята:
замечания к рамке, лишним границам на блоке, modal/readonly name/Backspace.
По уточнению пользователя ПКМ по секции открывает меню в точке нажатия.
Реализованы themed PopupMenu с editable name/Start/End/RGB/Apply и7 цветными
квадратами внизу, name-only caption, единая рамка/радиус и2px отступы.
Backspace/Delete и Remove section — shared RemoveSection/metadata/history,
stale/reference/transport guards; linked SectionPatch удаление отклоняется.
Rename/delete/Undo/Redo/SaveOpen/Backspace в поле имени/zoom/scroll проверены;
другие lanes снимают section selection. DSP/callback/schema19/archive1 прежние.
Source `4ce08e0825476ac101cc1dc5f1be38e492d474e1`. Release собран, focused2/2 PASS22.02s,
final full CTest128/128 PASS128.38s (включая GUI6). Первый общий прогон имел
native editor Space focus failure до секций; изолированный baseline/current
и полный повтор PASS, логи сохранены. Product keyboard код не менялся.
Packaged diagnostics/J3 PASSexit0/clean shutdown; fresh popup100/150 и lane100/150/200
снимки, popup150/lane100 inspected. Compact13 files/12 hashes rechecked.
Package `Builds/MR-Studio-0.3e-upd1-Section-Menu-JUCE-ASIO-Windows-local`.
GUI SHA256 `6473418C246143FF8539D9DDF3C56C09B453FB26999EFAEF581657D8D95F1AF4`.
Logs `build/asio-local/validation/sections81-upd1`, symbols
`build/asio-local/deliveries/0.3e-upd1-section-menu`. Постоянный MR shortcut
обновлён наupd1, icon retained/target verified; old packages preserved.
[SECTION_COLORS_BOUNDS_81.md](../SECTION_COLORS_BOUNDS_81.md).
READY WITH MANUAL CHECK: #81 open, приёмка upd1 ожидается. Hardware ASIO/physical
DPI отдельно не измерены. #80 open: body move/copy остаются впереди; rename/delete
добавлены в этот апдейт по прямому запросу. Другие issues автоматически не начинать.
Без push/merge/Actions; GitHub только issues, документация локальная.

Предыдущая доставка (не принята):
Активно **#81 /0.3e**, 2026-10-12: прямой запрос пользователя
«Пtререходи к #81» переопределяет рекомендацию #80 первым. #80 остаётся open.
Для #81 реализуется prerequisite создания section, самостоятельная полоса
Sections, palette/RGB, точные bar:beat:tick start/end, edge preview/Cancel/snap.
Metadata commands/history расширены sections, no DSP reload. Bounds/create
Pause/Stop/loopOff/countOff; color может обновляться при playback/active loop.
Schema19/archive1, clips/routing/audio callback неизменны.
Source `61c3fbd2173d9bebde576f2b8676bac31ada6fa2`; musical.hpp/musical.cpp,
Application/Desktop, SectionTrack.h/cpp, tests/cmake/version. Release/full128/128 PASS140.94s.
Final GUI6/6 PASS72.26s после исправления export200 (test only);
packaged J3 PASSexit0, fresh lane100/150/200 и editor100/150 previews inspected.
[SECTION_COLORS_BOUNDS_81.md](../SECTION_COLORS_BOUNDS_81.md). Package `Builds/MR-Studio-0.3e-Section-Colors-Bounds-JUCE-ASIO-Windows-local`,
compact13 files/12 hashes rechecked. GUI SHA256
`3D2F36016C07BAC654C407B7CE382DCFDE07E825B7C9D85F52542163A372FF52`.
Logs `validation/sections81`, symbols `deliveries/0.3e-sections81` в build.
Постоянный shortcut обновлён на0.3e/icon retained/target verified.
Принятые0.3d upd3/старые packages сохранены. READY WITH MANUAL CHECK:
приёмка #81 впереди, hardware ASIO/physical DPI отдельно не измерены.
Следующие #80/#83/#84/#87 не начинать автоматически, не закрывать #80 за #81.

Принято **0.3d upd3 / #149**, 2026-10-12: «Да, все ок, принимаю.
Что у нас дальше?» #149 closed completed; пакет/manifest/ярлык остаются прежними.
Source `1c0aba5ca1c01647658794b62481044674a669ba`, local delivery docs461587a.
Ранее Release/full127/127 PASS146.99s, packaged J3 PASS, реальный MIDI7605/1989
import/SaveOpen/ManualDevice PASS; приёмка не требует повторной сборки.
Package `Builds/MR-Studio-0.3d-upd3-MIDI-Drop-Fix-JUCE-ASIO-Windows-local`,
GUI SHA256 `A512E3B41E89B874A54CBAB3DECEB0560011BBE077B914BC4CA824BF14BCCE4C`.
Постоянный MR shortcut наupd3; schema19/archive1. FEATURE ACCEPTED.
Физическая ASIO/DPI-матрица отдельно не заявлена пройденной.

Далее по #25 рекомендован **#80 — Редактор Arranger Track**, новая буква
**0.3e**: создание/имя/положение/длительность/удаление секций Intro/Verse/Chorus,
Undo/Redo/SaveOpen и совместимость с navigation #86. В этом объёме редактируются
section metadata; content rearrangement/ripple отдельное расширение.
После #80: #81 цвета/границы, #83 tempo, #84 meter, итоговый #87/#25.
#87 shared model/history/persistence проверяется во всех срезах.
#80/#81/#83/#84/#87 сверены с GitHub: open. Реализация следующего пункта
не начинается автоматически; ждём прямой запрос пользователя.

Предыдущая доставка **#149 /0.3d upd3**, 2026-10-12: upd2 не прошла
ручную приёмку (standard MIDI unsupported, VSTi blank drop no-op).
Причины: 7605-note mono SMF превышает прежний4096 per-clip/playback budget;
JUCE target-discovery передаёт SourceDetails до target-local cursor, и прежний
interest handler ошибочно проверял coordinates. Исправления локально в Core
model/parser/deserialize/playback + Desktop/drop handlers/version + regressions.
Project total32768 notes сохраняется, callback algorithm/storage не меняются.
Release PASS; user MIDI test PASS parts1/notes7605/events1989,
Save/Open/ManualDevice playback, исходный hash неизменен. Full127/127 PASS146.99s;
Source `1c0aba5ca1c01647658794b62481044674a669ba`; packaged J3 PASSexit0.
Compact13 files/12 hashes rechecked:
`Builds/MR-Studio-0.3d-upd3-MIDI-Drop-Fix-JUCE-ASIO-Windows-local`.
GUI SHA256 `A512E3B41E89B874A54CBAB3DECEB0560011BBE077B914BC4CA824BF14BCCE4C`.
Logs `validation/midi-drop149-fix`, symbols `deliveries/0.3d-upd3-midi-drop-fix`
в build/asio-local. Fresh100/150 previews generated/100 inspected.
Постоянный shortcut обновлён наupd3, MR icon retained/target verified.
Старые packages и исходный MIDI неизменны. READY WITH MANUAL CHECK.
Файл пользователя найден и только читается:
`D:/DAW Streamer/Show 02/Take_2026-10-11_20-00-23/MIDI.mid`.
Пользовательская приёмка получена выше; #149 закрыт, отдельная hardware
ASIO/physical DPI матрица не измерена.

Предыдущая доставка **#149 /0.3d upd2**, 2026-10-12: VSTi drop/create/Replace,
Files MIDI/atomic multi-part import и shadow. База `813e146`, рабочее дерево
содержит Core SMF parser, Application import, VST category/cachev2, Desktop/UI,
GUI/unit regressions/version и docs. Source `f455466`; Release PASS; full CTest127/127 PASS150.96s, финальные GUI6/6 PASS41.49s.
Первый compiler/fixture и async-dialog smoke failures исправлены; логи
`build/asio-local/validation/midi-drop149`. Packaged J3 PASSexit0; fresh
MIDI shadow/Files100/150 previews просмотрены.
[MIDI_INSTRUMENT_DROP_UPD2.md](../MIDI_INSTRUMENT_DROP_UPD2.md).
Package `Builds/MR-Studio-0.3d-upd2-MIDI-Instrument-Drop-JUCE-ASIO-Windows-local`,
compact13 files/12 hashes rechecked, GUI SHA256
`1F0C509A69C5057444A82661F3FDC0C05CD89E5DA7CB345B987B94478DA0ECA5`.
Symbols `build/asio-local/deliveries/0.3d-upd2-midi-instrument-drop`.
Постоянный shortcut обновлён наupd2; icon retained/target verified. Старые
packages сохранены. READY WITH MANUAL CHECK: пользовательский VSTi/MIDI
перенос/Replace/cancel/многоканальный импорт и слуховая проверка #149 далее.
Новые MIDI tracks требуют назначения инструмента; темп проекта сохраняется.
Hardware ASIO/physical mixed-DPI не измерены; другие musical issues не начинать.

Принято **#148 /0.3d upd1**: «Хорошо, пока оставим так. Принимаю».
Issue148 closed completed. Source `36d7794`, Release/GUI6/6 PASS44.56s,
packaged J3 PASS, fresh100/150 previews просмотрены. Compact13 files/12 hashes:
`Builds/MR-Studio-0.3d-upd1-Browser-Cards-JUCE-ASIO-Windows-local`.
GUI SHA256 `73D04FB764DE1E3A5C99EB0BA86BDA84174B2AE14B6FD3A430A8E175521A925B`.
Старый package/manifest не изменён; shortcut наupd1/icon retained.
[BROWSER_FONT_CARDS_UPD1.md](../BROWSER_FONT_CARDS_UPD1.md).

Принято **#86 / 0.3d**, 2026-10-12: пользователь «Поехали #86» разрешил
продолжение #25 после принятой upd14. Запрет автоматического перехода не
распространяется на этот прямой запрос. Другие #80/#81/#83/#84 не начинать.
Baseline `56d36c4`; MusicalTimeline guard/Application binding, JUCE navigation
selector/lists/marker/section entry points и tests/docs. Release/CTest126/126
PASS (119.47s), packaged verification далее,
logs `build/asio-local/validation/navigation86`. Schema19/archive1 прежние,
callback/DSP без изменений. [NAVIGATION_86.md](../NAVIGATION_86.md).
Source `8e833de06d6d330d34f6d192996b30577fd986d9`. Packaged J3 PASS (exit0),
fresh software100/150 navigation/footer previews просмотрены, включая deliberate
queue-full error case. Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3d-Navigation-JUCE-ASIO-Windows-local` в рабочей папке чатов.
GUI SHA256 `0DA1A7C6BD262AC99AA1E985964D2BAC418CC37CE5E5F389991C3972C0AA7A2F`;
symbols `build/asio-local/deliveries/0.3d-navigation`. Desktop MR Studio shortcut
обновлён на0.3d с прежним icon, target проверен; accepted upd14 hash unchanged.
READY WITH MANUAL CHECK: следующий шаг — пользовательская проверка #86.
Physical ASIO/mixed-DPI отдельно не измерены. Другие sub-issues не начинать.

Принято #147 / **0.3c upd14**: Close project, M red/S yellow, toolbar cleanup,
context channel menus и durable FX role. Baseline `8d00fd1`, ветка прежняя.
Изменены Core model/serialization, Application/desktop API, JUCE Desktop/Smoke/
version и tests/docs. Schema19: FX_ROLE; прежние1–18 открываются с role=false.
Release/CTest126/126 PASS (144.80s), logs `build/asio-local/validation/upd14`; packaged smoke следующий.
Следующий шаг — завершить проверки, выдать пакет и обновить постоянный ярлык.
Завершено: source `d46be8e4184487a8a518b444f1a561ec8d79a554`, Release/full
CTest126/126 PASS (144.80s), packaged J3 PASS (exit0), fresh100/150 mixer
previews просмотрены. Close clean/Save/Discard/Cancel, FX add/delete Undo/Redo,
routing restoration и actual Save/Open role PASS. Compact13 files / 12 hashes:
`Builds/MR-Studio-0.3c-upd14-Channel-Menus-JUCE-ASIO-Windows-local`.
GUI SHA256 `838B0813E1B07587CA77528B38526ABB10B630A2A0C128D9C4813B2C3431022D`;
symbols `build/asio-local/deliveries/0.3c-upd14-channel-menus`. Desktop MR Studio
shortcut обновлён на upd14, icon сохранён, target проверен; upd13 hash unchanged.
Принято пользователем 2026-10-12: «Отлично, принимаю». #147 closed completed.
Следующий пункт плана #25 — #86 / 0.3d; пока не начинать без прямого запроса.
Physical ASIO/mixed-DPI отдельно не измерялись; schema19 требует upd14+ для
новых сохранённых проектов, старые1–18 читаются.
[PROJECT_CHANNEL_MENUS_UPD14.md](../PROJECT_CHANNEL_MENUS_UPD14.md). Без push/merge/
Actions, Stage5 off. Исходные packages/пользовательские проекты сохраняются.

Одна DAW с режимами Arrange/Edit/Mix и будущим встроенным Live Mode, единым
Project Model/Transport/Core. Активная разработка — Windows JUCE + ASIO.
MIDI и backlog #55 приняты. P4/#54 принят и закрыт 2026-10-09 по
результату пользователя в #113 и триажу #114 (подробности ниже). Собственные эффекты/инструменты/Amp-Preamp отложены. Пользователь стремится к уровню
эффективности Fender Studio Pro. Универсальность важнее настройки под один проект.

В пользовательском примере TH-U и Xvox находятся на разных каналах, ONE — на
Master. Не превращай их в одну последовательную цепь при оценке параллелизма.

## Принято #146 — текст кнопок / 0.3c upd13, 2026-10-11

Дополнительный запрос по New project/Open/Refresh: чуть крупнее и шире,
как подписи второго образца. Общие TextButton Segoe UI14 logical, компактные
height<=22:13 logical, horizontal scale1.04. Кнопки микшера (flatMixerButton) сохраняют12 без расширения по уточнению пользователя. Browser/menu fonts и фоны прежние.
Source `04f97e6d5826eeffe6b4d98accf1bf963ca8bf2e` (baseline `5fce476`).
Кнопки дорожек с stripFont также сохраняют12/scale1. Release/GUI CTest6/6
PASS (65.09s), packaged J3 PASS (exit0). Fresh home100/150 и mixer previews
просмотрены: `build/asio-local/validation/button-font/packaged-smoke`.
Compact13 files / 12 hashes verified: `Builds/MR-Studio-0.3c-upd13-Button-Font-JUCE-ASIO-Windows-local`.
GUI SHA256 `018B5E42E09FBCC4F4E6EF02C050F234DE36608813514679EE6DC49EB5B9E6A6`;
symbols `build/asio-local/deliveries/0.3c-upd13-button-font`. READY WITH MANUAL
CHECK до приёмки; принято 2026-10-11: «Принимаю,тест пройден». #146 closed completed. Model/DSP/schema18 прежние, Stage5 off. Следующий шаг — ждать нового запроса; пакет/manifest не менять.

## #146 — шрифт меню / 0.3c upd12, 2026-10-11

После приёмки upd11 пользователь попросил размер текста выпадающих меню как
в VST3 browser. Font уже12, но JUCE V4 вычисляет default row15px и затем
уменьшает glyph до (15-2)/1.3=10px. Theme теперь даёт строку минимум22px;
PresetMenuTheme использует тот же Segoe UI12 logical, как channelFont.
Model/DSP/commands/schema18 прежние. Baseline `715a545`, ветка прежняя.
Изменены Controls/PluginPreset/Smoke/version и local docs. Smoke теперь
снимает обычное меню без forced row22, проверяя действующий default sizing.
Release/GUI CTest6/6 PASS (43.97s), packaged check далее: `build/asio-local/validation/menu-font`. #146 open,
Source `5b11598ee420fe9f5a2e821bc8793c312d58cdb2`. Packaged J3 fixture smoke
PASS (exit0); fresh default popup/preset menu software100/150 previews
просмотрены в `validation/menu-font/packaged-smoke`. Compact13 files /
12 payload hashes verified: `Builds/MR-Studio-0.3c-upd12-Menu-Font-JUCE-ASIO-Windows-local`
в рабочей папке чатов. GUI SHA256
`98819CD7DF028C1A87EE0F7DDB25A451EDD2BB51BD466794E1AC5FF0312C40D1`.
Symbols `build/asio-local/deliveries/0.3c-upd12-menu-font`. READY WITH MANUAL
CHECK: следующий шаг — пользовательская визуальная приёмка #146.
Physical mixed-DPI не измерялся. Без push/merge/Actions, Stage5 off.

## Принято #145 — вложенные IR browser surfaces / 0.3c upd11, 2026-10-11

Пользователь сохранил светлый цвет card upd10 и уточнил: дерево папок и список
импульсов должны быть тёмными, как VST3/Files. TreeView/ListBox #212830,
общая card #2D353E. Остальной стиль upd10 сохранён. Baseline `7b5b9c0`, ветка
прежняя. Изменены ReverbBrowser/version и local docs. Release и две Reverb GUI проверки PASS (1.36s), logs: `build/asio-local/validation/ir-browser-surfaces`.
Source `8f5e56e1355d710ab4aa98d376bb2933d0384489`. Packaged Reverb browser/editor
smoke PASS (exit0), software100/150 previews просмотрены. Compact13 files /
12 hashes verified: `Builds/MR-Studio-0.3c-upd11-IR-Browser-JUCE-ASIO-Windows-local`
в рабочей папке чатов. GUI SHA256
`DF84930A5875F6307685E2433B6A4DBAEEC2354717380C8DBFC5E3AB472BD1D7`.
Symbols `build/asio-local/deliveries/0.3c-upd11-ir-browser`.
Проверки перед приёмкой PASS; upd11/#145 принят, closed completed,
без push/merge/Actions, #25/Stage5 не начинать.

Upd11 принят 2026-10-11: «Это принимаю». #145 closed completed;
существующий пакет и manifest не переписывались для приёмки.

## #145 — уточнение контраста / 0.3c upd10, 2026-10-11

Два свежих скриншота после upd9: пользователь указал, что зоны сливаются.
IR browser MR Reverb #2D353E (прежняя заливка кнопок), включая tree/list;
общие нейтральные кнопки +5% RGB -> #2F3841; Arrange canvas #262E37.
Source `b042800098093969e0a65c82640dc6a5bc1ad312` (baseline `d7c9f52`), ветка
прежняя. Release/GUI CTest6/6 PASS (69.27s); packaged J3 и Reverb browser smoke
PASS (exit0). Logs `build/asio-local/validation/theme-contrast`; fresh
software100/150 IR browser/Arrange previews просмотрены в `packaged-smoke`.
Compact13 files / 12 payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd10-Theme-Contrast-JUCE-ASIO-Windows-local`.
GUI SHA256 `67E195B5D029D7B79CC24D3F4527D6D38B00A5C9868832CE1204B2584993AA18`;
symbols `build/asio-local/deliveries/0.3c-upd10-theme-contrast`. upd9 package
не изменён. #145 open, READY WITH MANUAL CHECK — пользовательская визуальная
приёмка ожидается; physical mixed-DPI не измерялся. Model/DSP/schema18 прежние,
без push/merge/Actions, #25/Stage5 не начинать.

## #145 — визуальная тема / 0.3c upd9, 2026-10-11

Прямой запрос по bitmap `codex-clipboard-787b7e4f-e50f-4b48-9eff-8c262178e5a9.png`:
фон/рамки/радиус/мелкий шрифт/разделители channels, tracks, native и host plugin
editors, VST3/Files text; round audio/MIDI clip и marker borders. Реализация —
существующий Theme, #212830 background, #3D444D outline1px, radius6px,
Segoe UI12 logical; численные геометрия/font/divider пока визуальный кандидат.
Vendor HWND GUI не стилизуется; host chrome стилизуется. Model/DSP/Undo/schema18
не меняются. [UI_REFERENCE_UPD9.md](../UI_REFERENCE_UPD9.md).

Source `fd07877b690cc836c177aa2a6f6bc2f2300e3e4e` (baseline `8291326`), прежняя
ветка. Release/GUI CTest6/6 PASS (53.40s), packaged J3 fixture smoke PASS (exit0).
Logs `build/asio-local/validation/reference-theme`; свежие software100/150
previews в `packaged-smoke`: просмотрены mixer, Arrange audio/MIDI clips,
markers, native EQ, host editor, VST3/Files.
Compact13 files / 12 payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd9-Reference-Theme-JUCE-ASIO-Windows-local`.
GUI SHA256 `8B70743328CFF54B178C61F6E4E2A31505A6030A791ABFFA59582A9F26D67926`;
symbols `build/asio-local/deliveries/0.3c-upd9-reference-theme`. Accepted upd8
GUI hash unchanged. #145 open, READY WITH MANUAL CHECK: следующий шаг —
пользовательская визуальная приёмка на рабочем DPI. Physical mixed-DPI не
измерялся. Без push/merge/Actions, #25/Stage5 не начинать.

## Принято #137 — inserts loading / Send power, 0.3c upd8, 2026-10-11

Новый прямой запрос возобновил только эти доработки после принятой upd7. Задержки
пользователь видит у TH-U, native EQ и других плагинов. Убраны throwaway VST3
preflight и пересоздание незатронутых channel chains при SetInserts: готовится
одна изменяемая цепь, остальные DSP/IR loaders сохраняются; stop/quiesce до
capture/замены. Добавлен Send power, сохранив gain, live mixer queue, Undo/Redo,
schema18/legacy enabled. Подробности и текущая проверка:
[INSERT_LOADING_SEND_BYPASS_UPD8.md](../INSERT_LOADING_SEND_BYPASS_UPD8.md).

Source `0866533ca157bcca6015fbeb4d1417a02e6697f9` (baseline `a7daf05`), ветка прежняя. Изменены Application/desktop.hpp,
version.hpp, SendLevel/Desktop/Smoke, Core schema/serialization и core/desktop/
VST3 fixture tests, локальные docs. Release build PASS в `build/asio-local`;
logs `validation/inserts-send-bypass`. Release/CTest126/126 PASS (123.17s), включая
GUI power/actual return silence/preview immunity/Undo/Redo; VST3 create1/unrelated0,
getState0 при удалении последнего VST3. Packaged J3 fixture smoke PASS (exit0), fresh
software100/150 previews просмотрены. Compact13 files / 12 payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd8-Inserts-Send-Bypass-JUCE-ASIO-Windows-local`.
GUI SHA256 `D4D49B452A3A7974047FD42D2CD2C2F565119732CEFACC2E25D0F2CECA18D527`;
symbols `build/asio-local/deliveries/0.3c-upd8-inserts-send-bypass`. Принятый upd7
hash проверен неизменным. Пользователь подтвердил: «Тесты прошли приемку».
GitHub #137 закрыт completed, пакет/manifest не менялись. Physical ASIO timings/
mixed-DPI отдельно не измерялись. Следующий прямой запрос — #145 выше. Без push/merge/Actions; #25/Stage5
не начинать. Старый принятый upd7 package сохранён.

## Принято #136 — mixer UI / 0.3c upd7, обновления остановлены, 2026-10-11

Пользователь подтвердил: «Принимай, с обновлениями пока все». **0.3c upd7**
принята, #136 закрыт completed. Обновления остановлены до новой прямой команды;
следующий issue/этап автоматически не начинать, #25/Stage5 остаётся отложенным.
Существующий package/manifest/hashes не изменены; новых build/tests для записи
приёмки не выполнялось. Source4185cc5 и локальные validation результаты ниже.

Пользователь принял upd6: «Все работает, закрывай»; #135 закрыт completed.
Следующие дополнения #136: popup Remove/Editor/Bypass по ПКМ у указателя,
line-only browser drop, выбранный VST3 в браузере, общий draggable insert/send
разделитель и send rail5px/hit11px. [MIXER_UI_UPD7.md](../MIXER_UI_UPD7.md).
Source `4185cc5bf583dcfc99b395eb99446e20a290343f`, прежняя ветка. Изменены JUCE
Controls/Desktop/Editors/SendLevel/Smoke и desktop version. Core/schema17 не
меняются; menu mutations используют существующие Application/Undo/RT paths.
Release PASS; full CTest **126/126 PASS (140.18 s)**; packaged **J3 smoke exit0**.
Software previews100/150% проверены. Package13files/12payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd7-Mixer-UI-JUCE-ASIO-Windows-local`.
GUI SHA256 `4C60B8E9AA93146212504E817F94CF9DD03297FA1B0646ADE8F9BBB7EF137459`.
Logs/fresh packaged previews отдельно `build/asio-local/validation/mixer-ui`;
matching symbols `build/asio-local/deliveries/0.3c-upd7-mixer-ui`. Предыдущие
packages сохранены. Пользовательская приёмка меню/selection/divider/send rail
завершена в сообщённом объёме. Отдельные ASIO/physical mixed-DPI/screen-reader
matrix агентом NOT RUN; приёмка не добавляет этих измерений. Следующий шаг —
ждать новой команды пользователя. #25/Stage5 не начинать; GitHub только issues.

## Принято #135 — I/R и mixer inserts / 2026-10-11

Пользователь принял 0.3c upd5: «Все работает, отлично. Закрывай»; #134 закрыт
completed. Пакет/manifest upd5 не изменены. Следующий прямой запрос — медленные
I/R, авто I при включении R, I/R в Mix под M/S, красный armed R, '+' в заголовке
без Inserts, позиционная вставка browser plugins с подсветкой. Рабочая версия
**0.3c upd6**, #135 закрыт completed по «Все работает, закрывай». Source commit
`1e82e990a8539d63429e79991a25f63f0666b467`, прежняя ветка сохранена.
I/R сохраняют plugin instances и открытые editors; MIDI monitor идёт через
bounded mixer queue, audio ownership/input transitions — quiesced retained
prepare с прежними Pause/Stop ограничениями. R включает I; unarm оставляет I.
I/R добавлены в Mix, R красный; '+' перенесён в title, Inserts удалена.
Browser drag подсвечивает viewport и точную позицию, поддерживает вставку между
plugins. Instrument first и limit8 сохранены. Подробности:
[MIXER_IR_INSERTS_UPD6.md](../MIXER_IR_INSERTS_UPD6.md).

Release PASS; full CTest **126/126 PASS, 137.73 s**; packaged fixture smoke
**exit0**. Software UI previews100/150% проверены. Runtime package13files,
12payload hashes verified:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd6-Mixer-IR-Inserts-JUCE-ASIO-Windows-local`.
GUI SHA256 `18AE3496D3635DA1F7098A8B70498605E11D4A07CCD0480B544CFCF5EF72E43B`.
Logs/previews отдельно: `build/asio-local/validation/mixer-ir`; matching symbols:
`build/asio-local/deliveries/0.3c-upd6-mixer-ir`.
Пользовательская проверка принята в сообщённом объёме; дополнительная физическая
mixed-DPI/stress matrix не заявляется. Следующий шаг — #136 mixer UI.
#25/Stage5 не начинать. Код/docs локально, GitHub только issues.

## Принято #134 — общий Bypass / CPU, 2026-10-11

По новому запросу пользователя подготовлена **0.3c upd5**. VST3 bypass раньше
считался structural edit: state capture/restore, graph reload и editor reconnect.
Теперь native/VST3 переключаются через existing bounded parameter queue на
границе блока, включая playback/Undo. Instance/editor сохраняются. PDC всегда
учитывает latency insert; preallocated постоянно обновляемый dry delay сохраняет
выравнивание при обходе. Capture/preset restore учитывают текущий bypass.
CPU UI сглаживается с tau0,7s, число с одним десятичным знаком; bar4px, accent
blue #006dcc. Raw diagnostics не менялись. Подробнее [BYPASS_CPU_UPD5.md](../BYPASS_CPU_UPD5.md).

Source `9d1a8ee5999b3b3180d3551ca0f387225ff655da`, прежняя ветка.
Release build PASS, CTest126/126 PASS(131.93s), packaged J2/editor fixture smoke
exit0. Numerical delayed bypass impulse/PDC и allocation checks PASS; GUI test
сохраняет graph generation и точный editor HWND, разрешает toggle при playback.
Первое падение vst3_processing было старым ожиданием dry без7-frame delay;
исправлено, финальный полный прогон зелёный. CPU smoothing/format/offline smoke PASS.
Пользователь подтвердил «Все работает, отлично. Закрывай»; #134 закрыт completed.
Это приёмка в сообщённом объёме; отдельная физическая/длительная stress matrix
не выполнялась.

Пакет в chat workspace Builds: `MR-Studio-0.3c-upd5-Bypass-CPU-JUCE-ASIO-Windows-local`,
13files/12payload hashes, GUI SHA256
`A1CDD612B157225F4532DA736B1C665AA3D30ADD24A3B469349AEA974BC2835E`.
Symbols отдельно deliveries/0.3c-upd5-bypass-cpu, logs/package/smoke отдельно
validation/bypass-cpu в build/asio-local. Прежние packages сохранены.
Следующий шаг по новой команде — #135 I/R и mixer inserts. #128/#132 закрыты; #25/Stage5 не начинать.
Код/docs локально, GitHub только issues, без push/merge/Actions.

## MR Reverb #128/#132 — принято и закрыто / 2026-10-11

**Итоговая пользовательская приёмка:** после 0.3c upd4 пользователь подтвердил
«Это сработало, закрывай». Исправление тихого upstream input/Stop CPU принято;
#132 и общий MR Reverb #128 закрыты completed на GitHub. #129–#131 уже закрыты.
Новых точных CPU%, threshold/IR и отдельных результатов Mix пользователь не
сообщил. Полная actual ASIO stress матрица2/8/16, отдельное mono audition,
physical mixed-DPI и внешний A/B не объявляются пройденными; известные offline
пределы сохранены. Это итоговая приёмка рабочего MR Reverb, не гарантия произвольной
realtime ёмкости/Live. Документация обновлена только локально; rebuild не нужен,
код и принятый пакет/manifest не менялись. Следующий этап только по новой команде.

**Актуальная принятая сборка: 0.3c upd4 — Input gate / Mix.** По прямому запросу
пользователя добавлен универсальный gate в Options MR Reverb, OFF по умолчанию,
threshold −120..−60 dBFS, шаг0,1 dB, default−90. Linked stereo peak, immediate
open, hysteresis6dB/hold50ms/release10ms. Только новый wet input; готовый полный
IR/Pre-delay/Tone хвост сохраняется. CPU уходит после хвоста, если вход ниже
порога. Mix теперь имеет квадратический ход: midpoint25%, первые10% хода=1%wet;
значения в проекте и linear DSP blend прежние. Gate поддерживает live parameters,
Undo/Redo, preview gesture, project/preset/reopen/IR restore. Snapshot schema17 /
Reverb state2; старое state1 читается с gate OFF/−90. Новые файлы требуют upd4+.

Source **`f97fba54af24b768b44fbbde123912c93eefa22c`**, прежняя локальная ветка.
Release build PASS; **CTest126/126 PASS(141.65s)**. Новые gate tests проходят
44.1/48/96k ×128/256/512, dry/link/transients/full tail/hold/hysteresis/events и
alloc/free0. GUI Options/Mix/threshold gestures, Undo, preset и layouts760×390 /
900×450, software100/150/200% PASS; визуально inspected narrow/150%, packaged
editor smoke exit0. Первоначальное падение core_sends было жёстким expectation
schema16 в тесте; заменено текущей schema, финальный полный прогон зелёный.
Private quad Large Hall48k/256 same quiet−80dB input: gate OFF237.029us,
gate ON после хвоста8.968us, drained silence7.670us; full-tail oracle8.9407e−8,
alloc/free0/0. Это single offline profile, не ASIO CPU meter / гарантия deadline.

Пакет chat workspace Builds:
`MR-Studio-0.3c-upd4-MR-Reverb-Gate-Mix-JUCE-ASIO-Windows-local`,13files,
12payload hashes verified. GUI SHA256
`7605FD36F459F001DF4423B25891F07BA171005309C97ED56678EC3BE52AFD1B`.
Matching symbols отдельно `build/asio-local/deliveries/0.3c-upd4-reverb-gate`;
logs/profiles/GUI previews/packaging script в `validation/reverb-gate`.
Подробнее [REVERB_GATE_MIX.md](../REVERB_GATE_MIX.md).
**Следующий шаг:** ожидать новую команду пользователя. Приёмка upd4 получена;
actual ASIO stress matrix и physical mixed-DPI остаются NOT RUN. #132/#128
закрыты; #86/#25 и Stage5 автоматически не начинать.
Прежние пакеты и IR сохранены; code/docs локально, issues обновляются,
push/merge/Actions отсутствуют.

### История диагностики и кандидата upd3 (до нового gate)

**Контроль shared return пользователем:** один MR Reverb на return, посылы
от двух TH-U guitar tracks и Omnisphere. Все sends0 → CPU Reverb уходит;
Omnisphere send>0 в Stop → не растёт; TH-U send>0 → возвращается и держится
до send0. Upstream TH-U при этом не bypass-ится. Это изолирует влияние входа
TH-U chain на активность return Reverb от собственной стоимости TH-U. Ненулевой
upstream сигнал в Stop подтверждён управлением send; точный уровень/тип(noise/DC)
и конкретный блок преампа не измерены. Общий отказ finite-support idle не
подтверждается: при прекращении посыла CPU уходит. Дополнительные CSV для этой
причинной связи не требуются. Разница Stop/Play отдельно количественно не измерена.
Active CPU upd3 improvement подтверждён пользователем; вопрос дальнейшей
эффективности на непрерывном тихом входе остаётся, auto gate не добавлен.
#132/#128 открыты до итоговой пользовательской приёмки; код не менялся.

**CSV upd3 Midi Test 02-3:** пользователь уточнил «Был короткий кусок
воспроизведения и потом стоп», поэтому профиль смешанный, delta Stop/Play нет.
7790 blocks(~41.55s),48k/256,Workers metric5. GTR1 TH-U83.29us/Reverb601.89us;
GTR Solo TH-U477.75us/Reverb543.18us; Omnisphere676.20us, Piano Reverb calls0;
Plate Reverb17.98us. Callback p50/p95/p99=22/36/43%, Late/XR/timeouts0.
Относительно прежнего upd2 CSV guitar Reverb mean ниже примерно13.2/19.1%,
но состояния/топология не идентичны: не приписывать точный процент оптимизации
или рост Stop этому сравнению. Пользователь предполагает тихий выход преампа
TH-U. Это согласуется с exact-zero idle и offline tiny-signal поведением, но
не измерено: CSV не содержит input peak/DC. Остаток idle CPU остаётся открытым.

**Feedback upd3:** пользователь сообщает значительное снижение нагрузки,
примерно на10 процентов с тремя экземплярами. С TH-U простой показывает чуть
больше CPU, чем playback; с Omnisphere всё нормально. Текущий process path
подтверждает upd3. Improvement принят как наблюдение, idle anomaly НЕ завершена.
Проверка финальных offline профилей256: normal/tiny median means234.855/235.502us,
сам DSP не показал заметного роста на tiny signal. Adaptive channel dispatch
основан на оценке стоимости/wake overhead, а не прямом Stop-disable; это возможный
фактор общего callback time, не установленная причина. User meter объединяет
TH-U/Reverb/scheduling; нужны separate per-node interval timings upd3 для Stop/Play.
Предыдущие CSV были upd2 и не измеряют новую особенность. Не объявлять её нормой,
не отключать insert processing на Stop и не добавлять gate без input evidence.

**Текущий кандидат: 0.3c upd3, active CPU follow-up.** По прямой команде
«Тогда продолжай» реализованы bounded32-sample slices incremental FFT jobs и
четыре независимых double accumulators в direct head. Нет gate/обрезки IR/added
latency. Source `3c6903cf39985f1f504214759af59e7ad6590794`, та же ветка.
Три парных offline профиля same Large Hall48k/256: signal median mean
**303.977 → 234.855 us (−22.74% vs upd2)**;128/512 −21.86/−25.21%.
Tiny signal также дешевле; drained idle практически прежний. Полный tail и
True Stereo сохранены. Release build PASS, CTest126/126 PASS(138.39s), private
134quad/L-R PASS, alloc/free0/0. Expanded3-instance engine benchmark PASS;
Workers4/256 mean708.488us, это synthetic8s graph, не actual ASIO проект.

Пакет в chat workspace Builds:
`MR-Studio-0.3c-upd3-MR-Reverb-Active-CPU-JUCE-ASIO-Windows-local`,13files,
12payload hashes verified, packaged editor smoke exit0. GUI SHA256
`5C5E1C0F68647D1CDB55D28791B4CCFF34F8EF32EC7D14C5947D666784E63F8A`.
Symbols отдельно deliveries/0.3c-upd3-reverb-active; все logs/paired JSON в
validation/reverb-active. Подробнее [REVERB_ACTIVE_CPU.md](../REVERB_ACTIVE_CPU.md).
Следующий шаг: сохранить/закрыть upd2, открыть upd3, тот же проект/IR/settings,
сравнить playback и Stop CPU с TH-U. Actual ASIO upd3 NOT RUN, #132/#128 открыты.
Старые пакеты сохранены, #86/#25 не начинались, push/merge/Actions отсутствуют.

**Новый Stop CSV:** Midi Test 02-2.mrsproject.csv, предоставлен после инструкции
fresh session / Stop-only / TH-U on. 48k/256, Process1024, Workers metric5,
9856 measured blocks (~52.56 s). TH-U GTR1 mean **76.08 us**, Reverb **693.42 us**;
TH-U GTR Solo **463.81 us**, Reverb **671.68 us**. Max Reverb2139.6/2579.8 us.
Callback p50/p95/p99 **24/35/40%**, late/XR/timeouts0. Piano Omnisphere/Reverb
calls0, не измерялись. По сравнению с предыдущим bypass-TH-U CSV, guitar Reverb
~30× дороже. Реальная активная обработка Reverb на Stop подтверждена; не stale
CPU meter. Входной peak/DC в CSV отсутствует, поэтому точный источник/амплитуда
не доказаны. Дальнейший аудит должен измерить input peak/DC и active convolution
frames на real chain; не просить повторять уже полученные bypass/Stop замеры.
Не считать задачу CPU принятой, #132/#128 остаются открыты.

**Фактический CSV пользователя прочитан:** Projects/Midi Test 02/
Midi Test 02.mrsproject.csv. 48k/256, Process1024, Workers metric5,
7729 measured blocks (~41.22 s). Оба TH-U имеют **calls=0** в этом отчёте,
поэтому он не измеряет их активную обработку. Guitar Reverb mean **22.17/22.84 us**,
Piano Omnisphere mean **554.48 us**, Piano Reverb **601.09 us**; max Piano Reverb
1972.5 us. P50/p95/p99 callback load 11/38/44%, late/XR/timeouts0.
Измеренная тяжёлая цепочка в этом CSV — Piano, не guitar. Не считать это
опровержением прежнего наблюдения: timings cumulative при включённом profiling,
не размечены по Play/Stop и bypass intervals; новый toggle profiling не сбрасывает
их. Предыдущая гипотеза TH-U output не доказана. Для Stop-only профиля нужна новая
prepared session (сохранить/переоткрыть приложение/проект), включённые TH-U/Reverb,
никакого Play/live notes, profiling только в Stop 20–30 s. Изменения DSP пока нет.

**Контроль upd2 получен:** на вопрос Bypass только двух TH-U с включёнными
Reverb пользователь ответил «Да, только с TH-U, а вот с омнисферой сбросил
нагрузку». Падение CPU после Bypass TH-U подтверждено, Omnisphere/Reverb idle
работает. Оставшийся сценарий локализован в гитарных TH-U → Reverb цепочках.
Однако Bypass TH-U одновременно убирает его собственный DSP и меняет его выход:
это не численное доказательство уровня сигнала перед Reverb. Для окончательного
аудита требуется измерить input peak/DC и active convolution frames для этих
inserts либо разделить их времена existing per-node profiler. Не объявлять
остаточный шум TH-U доказанным и не вводить пороговый gate без измерений.

**Последний результат upd2:** пользователь перешёл на правильный пакет;
нагрузка при воспроизведении уменьшилась примерно на 10 процентных пунктов,
но с включёнными TH-U в простое остаётся высокой. Текущий Win32_Process путь
теперь указывает на upd2, SHA256 `6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074`
проверен. Требуется отдельно сравнить Bypass TH-U на ЭТОЙ версии: предыдущий
Bypass эксперимент был на upd1 и не устанавливает поведение upd2. Ненулевой
idle output TH-U/входа — гипотеза; измерения самого upstream выхода пока нет.
Exact-zero idle в текущем DSP намеренно не gate-ит малый сигнал. Приёмка
remaining idle CPU не завершена; #132/#128 открыты. Не отключать весь insert
graph по Stop: это нарушит мониторинг и живую игру.

**Уточнение повторного аудита:** после сообщения «Все плагины отключил ... MR
Reverb нагрузка не падает» проверен реально запущенный процесс через
Win32_Process.ExecutablePath и SHA256. Сейчас работает **старый пакет 0.3c upd1**,
hash `06938DC76E176388B753910F1D99B66994F788E4E0575BACF8EEEF661E2A8C2B`,
из Builds/MR-Studio-0.3c-upd1-MR-Reverb-JUCE-ASIO-Windows-local.
Оптимизированный upd2 существует отдельно, hash
`6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074` проверен.
Это подтверждённое расхождение runtime с проверяемой версией: upd1 не имеет
finite-support idle optimization. Предыдущая near-silence гипотеза не установлена
как причина пользовательского случая. Не утверждать, что все прошлые замеры
сделаны на upd1 — проверен только текущий процесс. Цепочки по пользователю:
TH-U 2 → Reverb на двух гитарных tracks, Omnisphere Piano → Reverb на instrument;
Bypass всех upstream plugins не устранил Reverb CPU. Следующий шаг: сохранить
проект, закрыть upd1, открыть существующий upd2 и повторить Stop/CPU в том же
проекте. Пользовательский процесс не завершался, проекты/настройки не менялись.
Новая DSP правка/новый пакет по этому наблюдению пока не нужны.

**Новое замечание приёмки:** настройки сохраняются, всё работает, но три Reverb
дают около 40% CPU против 10% без них (с инструментами), Stop нагрузку не снижает.
Итоговая приёмка нагрузки НЕ завершена. Повторный аудит: exact-zero idle не
охватывает малый непрерывный сигнал от upstream VSTi/мониторинга. Дополненный
`--profile-ir` на Large Hall 48k/256: steady 277.675 us, вход около −140 dBFS
280.721 us, drained digital zero 7.614 us; alloc/free 0/0, DSP regression PASS.
Это воспроизведение механизма, не доказательство источника в пользовательском
проекте. Stop вызывает graph panic/reset, но insert processing продолжается для
live input. Требуется уточнить размещение трёх inserts и мониторинг после Stop.
Production DSP не менялся, шумовой gate не добавлялся. Логи `audit-*.log` в
validation/reverb-132. Следующий шаг — установить источник ненулевого входа;
профилировать direct head (128 taps × 4 paths) и FFT отдельно перед оптимизацией.

Пользователь принял **0.3c upd2**: «Теперь в режиме простоя примерно 1.5 процента,
в работе 12-14 примерно. Это уже лучше. Закрывай #130 #131 и переходи к #132».
**#130 и #131 закрыты completed на GitHub; #132 в работе, #128 открыт.**
Следующие #86/#25 не начинать автоматически.

Слуховая оценка пользователя: «Он и без проверки звучит убедительно».
Внешний A/B не проводился и по решению пользователя больше не блокирует эту
приёмку. Altiverb 7 не подтверждён как доступный. Аппаратная эквивалентность
не заявляется. Расширенная actual ASIO матрица нескольких экземпляров не выполнена.

Работа #132: отдельный QA executable/reference CTest, swept-sine/direct oracle,
PDC/full tail, offline CPU/RAM матрица 2/8/16 экземпляров с synthetic VST3 instrument,
8/20 s IR, 44.1/48/96 kHz, 128/256/512 frames и 1/8 workers. Добавлена проверка
Application restore того же 48 kHz IR в проектах трёх частот. Production DSP/UI
не меняются; версия и принятый пакет остаются **0.3c upd2**, пакет не переписывается.
QA source **`d674fbdfc87cd55a31bb00466dcfc28fbc6e7114`**. Release build PASS,
CTest **126/126 PASS (137.83 s)**; private library **134/134 quad + L/R пары
PASS**, DSP max error 1.19209e−7, alloc/free 0/0. Swept-sine reference max error
4.28445e−8, физический 7-frame PDC + полный tail PASS.

Offline matrix: **108 режимов / 27648 callbacks**, processing fault/timeouts
0/0, host alloc/free 0/0. **69 режимов / 8644 callbacks** превысили deadline:
тяжёлые режимы не являются realtime PASS. 48k/256/Workers8/20s/16 экземпляров:
mean 3.775 ms, p95 6.047 ms при deadline 5.333 ms, late 27/256.
96k/256/Workers8/20s/16: mean 4.040 ms при deadline 2.667 ms, late 207/256.
Peak process private 2950.95 MiB, Reverb prepared 2890.64 MiB.
Детали, CSV/логи и чек-лист: [REVERB_VALIDATION.md](../REVERB_VALIDATION.md),
`build/asio-local/validation/reverb-132/`. GUI SHA256 остался прежним:
`6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074`.

Следующий шаг: итоговая оценка #132/#128 пользователем с известными пределами.
Actual ASIO multi-instance/VSTi stress и отдельный слуховой mono fold-down
остаются manual/NOT RUN; offline DSP fold-down PASS. Не обещать гарантированные
multi-instance/Live лимиты по одному offline прогону. Внешний A/B снят с
блокирующих gates, не ждать установки другого процессора. #132/#128 не закрыты.
Исторические записи ниже описывают предыдущие кандидаты и не отменяют приёмку upd2.

## MR Reverb: оптимизация — 0.3c upd2 / 2026-10-10

**Готово для проверки нагрузки пользователем.** Пользователь подтвердил
функциональность #130/#131 в upd1 («Все работает»), но сообщил о высокой audio CPU.
По скриншотам: 48 kHz / ASIO 256 / Workers 8 / Process Buffer 1024; один MR Reverb
на пустой аудиодорожке, Stop, quad True Stereo Large Hall 3.945 s, 0 ms / 0 dB /
25%. С ним CPU 16.9–21.6%, без него 0.4% по уточнению пользователя. Первоначально
работу просил не начинать; после обсуждения плана дал прямую команду «Приступай».
Функциональная приёмка не закрывает вопрос нагрузки. #128/#131 остаются открыты;
#132/#86/#25 не начинать автоматически.

Source **`00bad07210df94328eb92d6e1c9565bf6c052bfe`**, та же ветка `mrs/0.2g-fix1-editor-space-local`.
Версия **0.3c upd2** — исправление текущего дополнения, без перехода к новой букве.
Точная finite IR support останавливает свёртку только после полного хвоста при
нулевом входе. Возобновление без added latency; маленькие сигналы не gate-ятся.
Pre-delay/Tone доигрывают, parameters/events/MIDI/80 ms handoff продолжаются.
SIMD FFT butterflies и nonredundant half-spectrum MAC ускоряют активный DSP;
conjugate mirror входит в bounded job. Shared FFT/nonuniform partitions были ранее.
Схема, UI, Undo/presets/identity и scheduling policy не меняются.

Три парных offline прогона окончательного DSP с тем же IR, 48 kHz / 256:
медиана среднего signal **378.544 → 277.961 µs (−26.57%)**, drained silence
**382.989 → 7.56855 µs (−98.02%)**; signal p95 522.0–532.9 → 439.5–453.7 µs.
128/512 также PASS. Это DSP-only timing; ASIO footer — callback/deadline, а не
Windows CPU. Нельзя обещать эти же проценты на ASIO meter. Подробности и
воспроизводимость: [REVERB_OPTIMIZATION.md](../REVERB_OPTIMIZATION.md).

Финальная Release build **PASS**, полный CTest **125/125 PASS (114.65 s)**,
packaged `--reverb-editor-smoke` **PASS**. DSP suite 6.61 s, library 12.54 s,
desktop Reverb 0.21 s, native editor 1.11 s. Новые проверки: arbitrary silent
onsets/gaps, tiny input, последний tap, wet delay/Tone после idle, silent handoff,
20 s True Stereo 48/96 kHz. Полный private quad oracle max error **8.9407e−8**;
RT alloc/free **0/0**. Предыдущий полный прогон до финального one-tap idle reset
исправления: 125/125 PASS (115.62 s). Special installed TH-U/Nuro не повторялись.
Actual ASIO CPU/dropouts/listening нового пакета **NOT RUN**, проверяет пользователь;
Live suitability и весь #132 не заявляются.

Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd2-MR-Reverb-Optimized-JUCE-ASIO-Windows-local`.
GUI SHA256 **`6164D000F0725A9AA05C779ACAEBD87CE12D5175874D2777618226047F12C074`**.
13 files: GUI, два helpers, лицензии, README и manifest; без картинок/PDB/tests/IR.
Matching GUI/crash reporter PDB, raw profiles/summary, build/CTest logs и packaged
smoke: `build/asio-local/deliveries/0.3c-upd2-reverb-optimization`. Старые пакеты сохранены,
приватная библиотека только прочитана, пользовательские настройки не менялись.
GitHub — только issue updates; source/docs push/merge/Actions не выполнялись.

Следующий шаг: пользователь сравнивает CPU при 48 kHz / 256 в том же проекте
после хвоста и на сигнале/мониторинге, проверяет возобновление и отсутствие щелчков.
Дальнейший этап — по отдельной команде.

## #131 GUI/state — реализовано локально, 0.3c upd1 / 2026-10-10

Пользователь: «Переходи к #131». **Готово для пользовательской проверки**;
#131 и #128 остаются открыты. Функциональность upd1 принята позднее, вопрос
нагрузки решается в upd2 выше. #130 normal GUI
entry point теперь подключён. #132/#86/#25 не начинать автоматически.

Source **`ff6dfea4ea606399aa29dca9cf19079b7aa57d15`**, ветка
`mrs/0.2g-fix1-editor-space-local`. Версия **0.3c upd1**, по прямому назначению
пользователя для MR Reverb. В native insert menu добавлен Add MR Reverb для
дорожки/шины/Master. Компактное resizable окно 900×450 (min760×390): общий
Folder, дерево подпапок, Unicode search, single-click IR list, metadata/status,
три плоских регулятора Pre-delay/Tone/Mix, existing Bypass/preset shell.
Ctrl fine, editable units/default reset; Space в поиске не запускает транспорт.

Модель хранит три параметра, Bypass, внешнюю identity (relative/absolute fallback,
bytes/FNV-1a-64, pair/map); root остаётся global preference. Core snapshot **v16**
с MR Reverb state v1 читает v1–15, Cab IR quota не меняется. Save/preset/reopen,
центральный Undo и smooth live parameter mailbox подключены. Missing/changed IR
не стирает сохранённую ссылку и не подменяется похожим файлом. IR history/preset
load/структурные операции следуют существующему Pause/Stop контракту; новых
timeline automation lanes нет. Подробности: [REVERB_EDITOR_IMPLEMENTATION.md](../REVERB_EDITOR_IMPLEMENTATION.md).

Application владеет catalogue/loaders, shutdown+join до graph replacement;
редактор — view. Control activation на Stop сериализована с worker reclamation;
отдельный active_revision устраняет ожидание первой публикации при двух быстрых
выборах IR на Stop. RT kernel handoff остаётся атомарным, без новых callback
locks/I/O/heap work. Общий VST capture не читает kernel/внешнюю identity.

Финальная Release build **PASS**; полный CTest **125/125 PASS (136.32 s)**.
Предыдущий полный прогон до active_revision regression: 125/125 PASS (139.06 s).
Focused core/DSP/library/editor/desktop PASS; quad/pair relative relocation,
absolute fallback, changed/missing digest, rapid Stop requests, IR Undo/Redo,
project/preset reopen, track/bus/Master и native editor lifetime проверены.
GUI browser 0.12 s, editor 1.21 s; software previews 100/150/200% и min bounds
визуально проверены. Packaged `--reverb-editor-smoke` **PASS**. Physical mixed DPI,
actual ASIO/VSTi/reference listening и offline tail acceptance **NOT RUN**, #132/manual.
Специальные installed TH-U/Nuro проверки не повторялись.

Компактный пользовательский пакет:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-upd1-MR-Reverb-JUCE-ASIO-Windows-local`.
EXE, два runtime helper, лицензии, README, SHA256.json; без картинок/PDB/tests/IR.
GUI SHA256 **`06938DC76E176388B753910F1D99B66994F788E4E0575BACF8EEEF661E2A8C2B`**.
Matching GUI/crash reporter PDB, build/CTest logs, packaged smoke/previews:
`build/asio-local/deliveries/0.3c-upd1-editor-131`. Прежние пакеты сохранены.
GitHub обновлён только в issues; source/docs push, merge и Actions не выполнялись.

Следующий шаг: пользователь проверяет MR Reverb со своей библиотекой, параметры,
Undo/пресеты/проект. По приёмке закрыть #130/#131 в принятом объёме; #132 запускать
только по отдельной команде. #128 не закрывать по одному CTest.

## #130 IR library/browser — реализовано локально, 0.3c upd1 / 2026-10-10

Пользователь принял #129: «Отличный результат, переходи тогда к #130»;
#129 закрыт completed. #128 остаётся открыт. #130 реализован как shared library,
асинхронный loader и reusable компонент native editor; приёмки пользователя #130
пока нет, issue открыт. **Нормальный insert menu/editor/model/state подключаются
в #131**; реализованы выше. #86/#25 не продолжать; аппаратная/listening QA — #132.

Source **`46b65e7350f54b0fd1c2d9f6231b8e6272f4800b`**, ветка
`mrs/0.2g-fix1-editor-space-local`, версия приложения остаётся **0.3c upd1**.
Добавлены `core/audio/{include/mrs,src}/reverb_library.*`,
`apps/studio-juce/src/ReverbBrowser.{h,cpp}`, `tests/reverb_library_tests.cpp`.
Изменены cancellable WAV/resampler/kernel preparation, per-node graph publication
и reclamation, ViewSettings root/map, GUI smoke entry, CMake. Layer/ownership и
переход к #131: [REVERB_LIBRARY_IMPLEMENTATION.md](../REVERB_LIBRARY_IMPLEMENTATION.md).
Канонический MR_REVERB не скопирован; user assets только читались.

Есть рекурсивный header-only каталог (max10000 WAV), snapshot/status, search и
folder filter, один Browse Folder, remembered global root/map, metadata/one-click
IR selection. Exact L/R suffixes ` L/R`, `_L/R`, `-L/R`, same directory/base/rate/
length, one logical pair. Ambiguous/orphan/mismatch — Unsupported. Quad только
после явного подтверждения LL/LR/RL/RR, без vendor/channel-count эвристики.
Decode, identity, resampling и FFT preparation на worker; last request wins,
root generation/cancellation suppress stale publication. Ошибки/смена root сохраняют
active identity и прежний kernel. External identity: relative + absolute path,
bytes, off-thread FNV-1a-64 fingerprint + pair/map; serialization/reopen — #131.

Один control producer на node, graph lease до join; несколько loaders одного
графа освобождают только собственные retired kernels. RT callback не берёт
service mutex и не выполняет I/O; allocation/deallocation probes **0/0**. Kernel
publication complete-success only, 80 ms existing DSP crossfade. Ready означает
published, не crossfade finished; idle/quiescent activation должен согласовать
host #131. Пиковая память может включать active + fading + one prepared kernel
и decode buffers; нельзя выдавать single-kernel RAM за общий pipeline peak.

Финальная полная Release-сборка **PASS**, CTest **123/123 PASS (124.33 s)**:
processing_reverb **2.91 s**, audio_reverb_library **12.33 s**, browser smoke **0.12 s**.
Предыдущий полный прогон до per-node reclamation: 123/123 PASS (118.18 s).
Проверены 1018 recursive mixed WAV / 1016 logical entries, thousand-file scan
while DSP runs, 100 rapid selections, cancelled/error/missing/changed/huge/silent/
corrupt IR, independent loaders одного графа, Unicode и extended Windows path >260.
GUI smoke: root preference round-trip, case-insensitive Cyrillic search, load,
missing-root preservation; software previews 100%/150% визуально проверены.
В ходе разработки исправлены deprecated u8path, ошибочный quad fixture writer
и locale-dependent Cyrillic search; финальные проверки зелёные.

Opt-in user catalogue **PASS: quad134 / logical pairs134 / stereo files268**.
Это каталог/metadata, не новая per-file DSP/listening приёмка; прежний #129 oracle
134/134 остаётся отдельным evidence. Обычный CTest private assets не требует.
Логи: `build/asio-local/130-verified-{build,ctest,user-catalogue}.log`;
preview `reverb-browser-{100,150}.png`. Matching internal EXE/PDB/evidence snapshot:
`build/asio-local/deliveries/0.3c-upd1-library-130`, GUI EXE SHA256
`E41AB432FC84681B092080D023158950826E53ECEEE91B823ED7CBBE183712A2`.
Пользовательский пакет ещё
не выпускается до #131/#132. Physical DPI/ASIO/VSTi/reference listening NOT RUN.
GitHub обновлены только issues; локальные source/docs/builds не публиковались.

## #129 DSP — принято, исторический checkpoint 0.3c upd1 / 2026-10-10

Пользователь временно переключил работу с #25 на #128 и назначил **0.3c upd1**.
Каноническое ТЗ прочитано на GitHub: `docs/MR_REVERB.md`, SHA
`b10c60bd3ba913ba072ec1c93b0470c037904df9`; локальная копия не создана по запросу.
На запрос «Начинай #129» завершена реализация и автоматическая проверка DSP-среза.
#129 принят пользователем и закрыт; #128 открыт. #130 результат выше; #131 UI/state,
#132 hardware/listening QA остаются отдельными задачами. #86 не начинать.

Native processor `mr_reverb` работает в shared PreparedGraph: mono/stereo/True
Stereo LL/LR/RL/RR, подтверждённая quad-разметка и проверяемая dual-stereo пара.
Direct head 128 taps, non-uniform FFT tiers 128/1024/8192, распределённые крупные
FFT/MAC без добавленной algorithmic latency (PDC=0). Original leading delay и
amplitude сохраняются. Pre-delay 0–200 ms; wet-only Tone ±6 dB / 3 kHz;
linear Mix 0–1 default .25; smoothing 20 ms, parameter events внутри блока.

В #129 добавлены SSE2 spectral MAC/direct head с scalar fallback, batch FFT,
power-of-two rings и общие input FFT/FDL для LL/LR и RL/RR. Aliases используют
стабильные адреса подготовленного runtime, один DSP owner, никакой синхронизации
между audio workers внутри одного процессора. Cab IR не изменялся: его offline
resampler выделен на предыдущем checkpoint без изменения 1-second лимита/state.

Подготовка/resampling IR вне callback; handoff/crossfade 80 ms; control-поток
удаляет старое ядро после DSP acknowledgement. Исправлена смена IR при bypass:
ownership завершается без вычисления свёртки, история сбрасывается; busy не
снимается, пока producer ещё готовит неопубликованное ядро. Existing bypass других
процессоров сохранён. Source budget 64 MiB / prepared kernel budget 384 MiB на
экземпляр; старое и новое ядра при смене сосуществуют. Это лимиты RAM, не CPU promise.
Denormal guard восстанавливает CSR вызывающего потока; callback allocation/free 0/0.

Проверена интеграция именно в AudioEngine: track → bus → Master, размещение insert
на каждом уровне, Stop/Play/Seek, continuous loop и полный хвост в offline WAV.
Stop/Seek очищают историю, хвост не продлевает Stop. Новый quiescent tail_frames
возвращает IR length−1 + wet Pre-delay + 10 ms Tone settling при ненулевом Tone;
это render-range bound, не PDC. Параметры перед запросом должны быть применены;
последующая automation может увеличить необходимый диапазон. Warm активация IR
доступна только при quiescent graph/device. Offline-тест явно рендерит полный
диапазон через AudioEngine, записывает существующим WAV writer вне DSP и проверяет
decoded samples. Нового export menu/workflow приложения в этом срезе нет.

UI/menu/project identity/presets/Undo/automation authoring — **DEFERRED
#131**. Browser #130 реализован, см. выше. `capture()` при выбранном IR явно отклоняется до #131 вместо потери IR.
Live/anticipation suitability не заявляется. Пользовательский пакет ещё не
выпускается: эффекта нет в authoring/editor, предлагать прослушивание пока нельзя.

Source commit **`2e9e52a783379be926abc65407966cf52585cfdf`**, ветка
`mrs/0.2g-fix1-editor-space-local`. Изменены `core/include/mrs/processing.hpp`,
`reverb.hpp`, `core/processing/src/graph.cpp`, `reverb.cpp`, `tests/reverb_tests.cpp`.
Предыдущий DSP checkpoint `2c13999917ea7323dcae7ccad63a1979e023a056`, opt-in user
library validation `61962238a89abf075418e7d50566a7c745a2fcae` сохранены в истории.

Финальная полная Release-сборка **PASS**, CTest **121/121 PASS (126.75 s)**,
processing_reverb **PASS (2.91 s)**. Численные oracle tests: все routing modes,
crossfeed, rate conversion, arbitrary blocks/tier boundaries, 20-second tails
44.1/48/96 kHz, controls/offsets, invalid/budget rejection, bypass/PDC/reset,
100 concurrent IR handoffs. Long-tail max error **5.96046e-08**. FFT/swap/engine
callback allocation/deallocation **0/0**. GUI diagnostics smoke: version
`0.3c upd1`, `clean_shutdown`; настоящий ASIO/VSTi/listening signoff **NOT RUN #132**.
Логи: `build/asio-local/129-final-{build,ctest,user-library,profile,workers-profile}.log`.

Финальный отдельный offline profile (steady-state FDL, Windows Release, без
параллельного CTest): wall-clock callback cost, не system CPU и не ASIO acceptance.

| True Stereo / rate / count / workers | block | prepared B | mean µs | p95 µs | max µs | deadline µs |
|---|---:|---:|---:|---:|---:|---:|
| 20 s / 48 kHz / 1 / serial | 128 | 97166536 | 318.15 | 422.1 | 568.1 | 2666.67 |
| 20 s / 48 kHz / 1 / serial | 512 | 97166536 | 1279.46 | 1569.9 | 2038.9 | 10666.7 |
| 20 s / 96 kHz / 1 / serial | 128 | 189441224 | 434.944 | 569.3 | 875.4 | 1333.33 |
| 20 s / 96 kHz / 1 / serial | 512 | 189441224 | 1760.82 | 2286.8 | 2976 | 5333.33 |
| dense 8 s / 48 kHz / 8 / 4 | 128 | — | 1379.52 | 2035.7 | 2320.6 | 2666.67 |
| dense 8 s / 48 kHz / 8 / 4 | 512 | — | 5461.7 | 7400.5 | 9224.5 | 10666.7 |
| dense 8 s / 48 kHz / 16 / 4 | 128 | — | 2802.29 | 4055.4 | 4912.8 | 2666.67 |
| dense 8 s / 48 kHz / 16 / 4 | 512 | — | 10335.2 | 13781.2 | 14989.6 | 10666.7 |

Все counts 1/2/8/16, workers 1/4, blocks 128/512 записаны в логах. Engine profile
использует существующий worker pool и forced dispatch (adaptive_parallel=false),
проверяет output matrix/DC, processing faults/timeouts=0; VSTi/hardware не включены.
8 instances / 4 workers уложились в deadline в этом прогоне; serial 8 и 16 instances
превышают deadline. **16 / 4 workers тоже превышают deadline**, нельзя заявлять
эту нагрузку realtime-ready. По сравнению с первым checkpoint RAM 20 s True Stereo
снизилась с 128352448 до 97166536 B (48 kHz), с 251189440 до 189441224 B (96 kHz),
примерно на 24%; mean128 с 565.755 до 318.15 µs и с 716.271 до 434.944 µs.
Профили — разные запуски на этой машине, не гарантия headroom в пользовательском проекте.

Read-only fixture library пользователя:
`D:/Samples/Samplicity - Bricasti IRs version 2023-10, quad channel files, 48 Khz`;
matching sibling `left-right files, 48 Khz`. Финальный повтор **134/134 quad WAV PASS**:
все decoded channels точно совпали с LL/LR/RL/RR явно сопоставленных 134 пар / 268
stereo WAV. Это подтверждение конкретных assets, не runtime эвристика бренда/имени.
DSP max error **1.19209e-07**, callback allocation/deallocation **0/0**;
longest **320001 frames / 6.6666875 s**, max prepared **35824840 B** (было 46563520 B),
max combined quad+pair decode/compare/prepare **208.818 ms**, не ASIO latency.
Opt-in CLI: `mrs_reverb_tests --library <quad-folder> <matching-pair-folder>`.
Обычный CTest не зависит от библиотеки. User assets не изменялись/не копировались.

Matching EXE/PDB/tests/evidence сохранены отдельно:
`build/asio-local/deliveries/0.3c-upd1-dsp-129-final`, GUI EXE SHA256
`9E48641787BB67735382DA2B594A87CB2A326542EEC3FCA4ECB97CF61D8CC683`.
Это внутренний snapshot, не пользовательский пакет; предыдущий snapshot и Builds
сохранены. Обновляются только GitHub issues, source/docs локальны, push/merge/Actions нет.
Следующий шаг по запросу пользователя — #131; не закрывать #128 по DSP/library PASS.

## #85 — принято 0.3c / 2026-10-10

Пользователь принял маркеры: «Маркеры работают. Закрывай». #82 закрыт completed;
parent #25 отмечает #82/C1. Правильное название принятого среза — **0.3b**;
пакет/embedded version 0.3a upd2 сохранены как историческая ошибка нумерации.
Каждый новый sub-issue увеличивает букву. #85 принят на **0.3c**; следующий
по порядку — **#86, будущая 0.3d**, работа ещё не начата.
#86 navigation и editors #80/#81 остаются отдельными, не начаты.

Реализованы компактные current → next подписи chord/section/marker и lane highlights
из одного immutable MusicalTimeline snapshot подтверждённого транспорта. Busy/full
mailbox не даёт optimistic seek; half-open/gap/end/tie semantics общие. При смене
current IDs visual interpolation сбрасывается на границе. Shared marker index
пересортировывается только при изменении markers, не при каждой metadata revision.

По запросу убраны Arrange и Audio settings из панелей (настройки в Transport menu).
Повторное имя дорожки над клипом удалено; clipRect top+1/height-3 даёт зазор 3 logical
px, включая paint/hit-test/drag/trim/recording preview. Имя в левой панели сохранено.
Context row занимает 24 px; максимум mixer dock уменьшен на 24 px для прежнего
минимального Arrange на окне 1200×700 и фиксированных фейдеров.

Feature source `71e232b9798338c3110a531ffe7d7e87d679d716`: Release всех targets PASS,
полный CTest **120/120 PASS (98.34s)**. Final delivery source
`be083addc91c66480dd0ba74f3ba39c7da991804`: поправки placeholder glyph, tooltip hit,
представителя current tied marker и обновления snapshot playhead; GUI rebuild PASS,
packaged J3 + diagnostics **PASS / exit0 / version=0.3c / clean_shutdown**.
Свежие software 100/150 screenshots просмотрены: зазор 3 px, Cyrillic, current/next,
пустые поля, current marker group и отсутствие удалённых кнопок. Первые ошибочные
compile/GUI прогоны отдельно описаны в [отчёте #85](../MUSICAL_CONTEXT_85.md).

Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3c-Musical-Context-JUCE-ASIO-Windows-local`.
EXE SHA256 `F5A949E3E1D1DA40335D47319E686D32E21B9B970A7BF4009508A16FB076C163`.
Runtime package: EXEs/licenses/README/SHA256, без PNG/logs/PDB. Validation/symbols:
`build/asio-local/deliveries/0.3c`, session `mr-session-1791602294738-12620-1.log`.
Ветка прежняя `mrs/0.2g-fix1-editor-space-local`; прежние пакеты сохранены.
Пользовательская приёмка завершена 2026-10-10: «Закрывай, все пункты этого билда
работают». Приняты #85 и все дополнительные UI изменения 0.3c; #85 закрыт completed,
parent #25 отмечает #85. Этап #25 остаётся открыт; C2 включает ещё отдельный #86.
Реальная ASIO/physical mixed-DPI отдельно инструментально не проверялись; прежние
автоматические результаты и пакет сохранены без изменений. Новые build/tests не
нужны для записи приёмки. Следующий пункт плана — #86, будущая 0.3d; не начат.
Документы локально, GitHub только issues; без push/merge/Actions.

## #82 — редактор маркеров, принято как 0.3b (пакет 0.3a upd2) / 2026-10-10

Пользователь принял 0.3a upd1: «Отлично, все работает, закрывай. Переходи к
следующему sub-issues». #79 закрыт completed; parent #25 отмечает #79/C3.
Следующий по рабочему порядку — #82. Дополнительный запрос: убрать обводку
всей Chord Track при клике (выделение отдельного аккорда остаётся).

Реализована 0.3a upd2: shared Add/Update/RemoveMarker, metadata-only Application/history
для chords+markers, candidate archive/reference validation и LiveAction delete guard.
MarkerTrack: отдельная полоса под ruler, список каждого ID в project order,
CRUD name/tick/kind, drag/Snap, stale/cancel guards. Action/cue — только метаданные.
Arrange header увеличен на 24 px; ruler/clip transform сохранены, sections/chords
сдвинуты на одну полосу. Обводка всей Chord Track удалена.
Feature source `e653440277f47adf6741dd0fbddce23b32858882`; Release all-target build
PASS, полный CTest **120/120 PASS (98.48s)**, включая desktop_markers и GUI J1/J2/J3.
Delivery source `7bcabaf1ce4b036584a6623fa576a60cb19dc704`: после только исправления
высоты track heading и длины подписи удаления выполнены GUI rebuild и packaged
J3/diagnostics PASS / exit0 / version=0.3a upd2 / clean_shutdown. Свежие software
100/150 previews просмотрены. Первые прогоны не PASS: опечатки API в test fixture,
старые header coordinates в GUI fixture и ошибка ID первого пункта marker selector
исправлены; финальные проверки выполнены после исправлений.

Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-upd2-Markers-JUCE-ASIO-Windows-local`.
EXE SHA256 `BFC1B9F38F08444B6CA4C2ED566CB96C2E3F3E68388990D25BBA1EEF644FE38D`.
Runtime package компактный, без PNG/logs/docs/PDB; matching symbols/previews/logs
в `build/asio-local/deliveries/0.3a-upd2`, session `mr-session-1791600776941-13764-1.log`.
Прошлые пакеты сохранены. Ветка прежняя; до работы чистый HEAD2566b6b.
Пользовательская приёмка #82 завершена: «Маркеры работают. Закрывай».
#82 закрыт completed 2026-10-10, корректное название среза **0.3b**.
Physical mixed-DPI отдельно инструментально не проверялся.
[Контракт/проверки/manual steps](../MARKER_TRACK_82.md). Следующий срез #85 — 0.3c.
Документация локально; GitHub только issues, без push/merge/Actions.

## #79 — уточнение Chord Track, принято 0.3a upd1 / 2026-10-10

Пользователь подтвердил «Chord track работает» на 0.3a и запросил уточнения
взаимодействий и оформления. Реализовано: double click пустой полосы создаёт Am
без диалога; double click события/Enter открывает редактор; Backspace/Delete
удаляют. Alt-drag копирует с временным preview, кнопка Duplicate копирует после
конца события, как у клипов. CopyChord проверяет expected source, сохраняет
symbol/длительность и создаёт свежий ID; одна Undo entry, overlap/stale отказ
атомарный. Toolbar цели и clip/chord selection согласованы. Сетка/фон общие
с Arrange, alpha 0.72 как у клипов; обводка 0.5/0.75 вместо 1/1.5 logical px.
Schema15/envelope1 и metadata-only engine/history путь сохранены.

Source `68ff0cdf217c7e32c798044ae826af97e9f0eaba` (feature `04f14af` плюс
исправление перезаписи smoke PNG); ветка `mrs/0.2g-fix1-editor-space-local`.
Release GUI/desktop/core/musical/timeline build PASS. Затронутые CTest
**23/23 PASS (44.04s)**: core, musical, desktop_chords, GUI J1/J3/diagnostics.
После исправления только preview harness Release GUI rebuild PASS и packaged
J3/diagnostics PASS / exit0 / version=0.3a upd1 / clean_shutdown.
Software snapshots 100/150 просмотрены из свежей validation directory;
проверены фон/сетка/alpha, создание без формы, Alt-copy, toolbar, Backspace,
Undo/Redo, invalid/stale отказ и сохранение. Новые pointer/ASIO/physical mixed-DPI
изменения приняты пользователем: «Отлично, все работает, закрывай».
Physical mixed-DPI отдельно инструментально не проверялся.

Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-upd1-Chord-Track-JUCE-ASIO-Windows-local`.
EXE SHA256 `A2A791FE4EB651F7E4EE198C473F85E99D16AD904521CB34C2756E80424B4FE9`.
Компактный runtime package; matching PDB, previews и логи отдельно в
`build/asio-local/deliveries/0.3a-upd1`, session `mr-session-1791599144316-12096-1.log`.
Старые пакеты сохранены. #79 закрыт completed по пользовательской приёмке;
следующий срез — #82, с удалением обводки всей Chord Track в новой сборке.
Документация локально, GitHub только issues; без push/merge/Actions.

## #79 — исходный кандидат 0.3a / историческая запись 2026-10-10

Пользователь исправил номер: «Это уже не апдейт, это уже 0.3a» и спросил,
зачем в пакете столько картинок. Актуальный кандидат **0.3a**; прежнее название
0.2g upd11 было ошибкой нумерации и не является отдельной пользовательской приёмкой.
Пакет теперь компактный: runtime EXE/helpers, licenses, README и SHA256.
GUI картинки, fixtures/test executables/logs, docs и matching Symbols остаются
в local build/validation area; [правило](../LOCAL_BUILD_POLICY.md).

Пользователь: «Начинай #79». Его выбранный порядок заменяет исходную рекомендацию
сначала #82; необходимый C0/#87 для chords включён в #79. Другие sub-issues не начаты.
Реализованы shared ID-targeted AddChord/UpdateChord/RemoveChord, candidate validation,
expected baseline/stale guards и chord-only Application/history без DSP rebuild.
Полоса под секциями Arrange: длительности/current highlight, + Chord, double click
форма symbol/start/end (ticks), right-click Add/Edit/Remove, Delete/Enter, body/edge
drag и Snap. Preview временный, одна Undo entry на release; Escape/focus loss/
project replacement/time/zoom change отменяют. UI selection не сохраняется.
Можно edit/Undo при playback и loop; recording/pre-count заблокированы.
Clips/time/routing/MIDI generation/plugin runtime/engine sample head не меняются.
Нет новой schema (Project15/envelope1), harmonic parser/MIDI generation/Live show UI.

Feature source `40f856a5985f35760f9233074487c6ad93577299`;
current source/version `c018d2e6ee5975977dd12c39bd62ed045da1616f`, ветка прежняя.
На feature source cached configure и полная Release сборка PASS;
полный CTest **119/119 PASS (102.23s)**,
включая новый desktop_chords и GUI form/gesture/save/stale/boundary регрессии.
Software100/150 screenshots просмотрены. ASIO audition, реальные pointer/mixed-DPI
и пользовательская приёмка пока NOT RUN. FEATURE READY WITH MANUAL CHECK /
RT SAFE WITH MANUAL CHECK. [Контракт, слои, логи, manual steps](../CHORD_TRACK_79.md).

Пакет:
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.3a-Chord-Track-JUCE-ASIO-Windows-local`.
EXE SHA256 `0817575211B3323104B950D29138A500FF398D47F8B158BA30FB936D7BAF15FD`.
0.3a Release rebuild PASS. Packaged version/J3/diagnostics PASS / exit 0 из
`build/asio-local/deliveries/0.3a`; matching PDB и outputs остаются там.
Session `mr-session-1791597966624-13644-1.log` подтверждает version=0.3a / clean_shutdown.
Предыдущий packaged smoke version0.2g upd11 PASS/exit0; прежние пакеты неизменны.
Последняя принятая пользователем сборка остаётся **0.2g upd10/source15afe65**;
0.3a — кандидат #79, issue остаётся открыт до приёмки. #85/#87 не закрывать
из-за этого среза. Следующий шаг — пользовательская проверка #79, затем новый
выбранный slice; весь Stage 5 не объявлять готовым. Docs локально, issues обновляются;
без push/merge/Actions. Историческая подготовка плана ниже описывает прежний снимок.

## Stage 5 / #25 — русские рабочие спецификации подготовлены 2026-10-09

Пользователь разрешил переход к #25 и затем запросил описания всех sub-issues,
чек-поинты, приёмку и корректировки локальных документов. Первичный
[разбор рисков](../STAGE_5_RISK_REVIEW.md) дополнен
[рабочим планом и полными описаниями #79–87](../STAGE_5_PLAN.md).
GitHub #79–87 получили русские titles/bodies: объём, зависимости, checkpoints,
условия приёмки и проверки. Parent #25 получил scope/общие контракты/порядок/
итоговую приёмку. Все десять issues остаются открытыми, чекбоксы не пройдены;
native состав подзадач не менялся. #15/#17 закрыты.

Рабочая исходная граница: Arranger редактирует section metadata без переноса
clips; chords — авторский текст. Musical objects/MIDI сохраняют ticks, WAV samples;
tempo/meter не растягивают аудио. Timing edits на Pause/Stop с согласованным
prepare/commit и sample playhead; recording/pre-count защищены. Перед
bounds/delete/time edit отключить loop. Stable IDs, отказ при удалении связанных
section/marker; никаких скрытых потерь SectionPatch/LiveAction. Metadata-only
history/context не должны пересоздавать DSP. Это требования к реализации,
а не утверждение, что текущий Application уже обеспечивает весь контракт.

Порядок: C0/#87 command/history/references/compatibility → #82 → #85/#86 →
#79 → #80/#81 → #83/#84 → C6/#87/#25. #87 проверяется в каждом slice.
Первый coding step при продолжении этапа — C0, затем вертикальный marker slice.
Полный Live/show UI, content ripple, time stretch и action execution вне этапа.

Уточнены local musical/persistence/architecture/vision/Live/UI/roadmap документы:
текущая Project schema **15** (проверена в коде и accepted source), archive
envelope **1**; историческая schema 2 относится к первым SHARED stages.
Будущий .mrlive — show-документ с project refs, общий engine активной песни.
Исправлены ошибочные schema14 в local #95/#98 docs; старые упоминания в
историческом журнале ниже не являются текущим форматом. Пакеты не переписаны.

База **0.2g upd10**, source15afe65, ветка
`mrs/0.2g-fix1-editor-space-local`; старт этой документальной работы — чистое
дерево/HEAD e55c474. Изменены только local docs. Код/сборка/пакет/хеши прежние,
новая версия не назначена; build/tests не запускались для документации.
Проверки этого запроса: diff/UTF-8/локальные ссылки, сверка GitHub bodies и
сохранённого состава native sub-issues. Документация не опубликована;
без push/merge/Actions. Реализация и пользовательская приёмка Stage 5 впереди.

## Принято: P4/#54, пользовательские результаты #113/#114 — 2026-10-09

Пользователь: «#113 сбоев не обнаружено. #114 щелчки появляются только при
высокой нагрузке, я считаю в пределах нормы».
#113 закрыт completed на основании пользовательского результата без сбоев.
#114 закрыт not_planned после пользовательского триажа: наблюдаемые щелчки
при высокой нагрузке допустимы для пользователя, дополнительная диагностика
не выполнялась. Причина прежних43 Ahead underruns +43 invalidations неизвестна;
связь этих событий со щелчками не доказана, счётчики не менялись/не скрывались.
#54 закрыт completed в принятом объёме. Детальные настройки/буферы, длительность,
новые CSV и порог высокой нагрузки не предоставлены; полную измеренную матрицу,
Fender parity, round-trip latency или отсутствие щелчков при любой нагрузке
не заявлять. [Журнал приёмки](../ENGINE_P4_ACCEPTANCE.md).

Актуальная принятая сборка **0.2g upd10**, source15afe65; код/пакеты/SHA256
не изменены, новые build/tests для записи приёмки не нужны. #16 и #53 уже закрыты.
Приёмка не запускает Stage5, #112,3e2/3e3 и не закрывает весь parent Stage3/#23.
Следующий шаг — новый запрос пользователя; исправлять новую проблему по её
воспроизведению. Docs локально, GitHub issues обновлены; без push/merge/Actions.

## Принято: #95 и весь backlog #55 — 0.2g upd10

2026-10-09 пользователь подтвердил: «Все работает, закрывай sub-issues и весь issues».
#95 и parent #55 закрыты completed по прямому запросу. Проверены все 10
sub-issues #91–100: девять уже закрыты, последним закрыт #95.
Актуальная принятая сборка — **0.2g upd10**. Приёмка всего backlog #55 получена;
она не означает завершение отдельных hardware/P4 проверок или начало Stage5.
Пакет upd10 и SHA256 неизменны; новые build/tests для записи приёмки не нужны.
Docs остаются локально.

2026-10-09 пользователь поручил #95: название send-канала на кнопке,
полоска-регулятор по верхнему краю без ручки, ПКМ выбирает send; кнопки находятся
в нижней части области Inserts под плагинами, над фейдерами.
База — принятая upd9, чистое дерево7056dd9. Локальный source **15afe65**,
ветка `mrs/0.2g-fix1-editor-space-local`. [Контракт/RT/проверки](../MIXER_SENDS_95.md).

Реализованы отдельные именованные send rows с top-edge уровнем0..16/точным нулём,
Ctrl fine, Escape/focus cancel, keyboard/arrows/Home/End и unity reset.
Preview не меняет model/Undo; mouse-up — один shared SetTrackSends gain command.
ПКМ выбирает/заменяет bus с сохранением уровня/pre/post, меняет pre/post, удаляет;
+ Send добавляет маршрут/new return. ID/current-state и project-store guard против
stale menus, duplicate/self/cycle validation. Отдельные insert/send viewport scroll,
фейдер145px и свободный верхний предел dock прежние; минимум tracks резервирует
ещё28px для одной видимой строки. Высота по умолчанию учитывает inserts+sends.
Схема14/preferencesv7/cachev1 прежние; Save/Open и Undo/Redo по общей модели.
Только message-thread preview через existing bounded MixerUpdate; audio callback
не изменён, cancel/full-queue retry через publish_mix/poll.

Cached configure и Windows x64 Release PASS. Финальный полный CTest
**118/118 PASS (52.44s)**; send model/preview/full-queue/cancel, DSP выбранного
return, playback Undo/Redo, Save/Open, routing/stale-menu/pre-post, 8 sends/8 inserts,
minimum/expanded dock и прежние MIDI/VST3/RT/J2/J3/diagnostics.
Packaged J3+diagnostics **exit0**, session log version0.2g upd10.
Software100/150 previews просмотрены. Исправлены тестовые fixtures: дождаться
существующего5ms gain ramp, максимум8 inserts, waveform probe внутри Arrange
после расширения dock. Физическая мышь, ASIO audition и mixed-DPI **NOT RUN** в автоматическом прогоне;
функциональная пользовательская приёмка получена выше, отдельная матрица не заявлена.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK.

Пакет **MR-Studio-0.2g-upd10-Mixer-Sends-JUCE-ASIO-Windows-local** в рабочей
папке чатов/Builds; matching EXE/PDB, helpers/assets/licenses/previews/logs/SHA256.
EXE SHA256: `64DFF00623BBEDC228EFF2DCAB04A71665C4459AAD37062C7FB3E9855E7B03BE`.
#95 и весь #55 приняты и закрыты; актуальная принятая сборка **0.2g upd10**.
Следующий шаг — ждать нового запроса пользователя. Stage5 не начат.
Предыдущие пакеты сохранены.
GitHub issues обновлены; код/build/tests/docs локально, без push/merge/Actions.

## Принято: #98 — drop инструмента на пустой Arrange, 0.2g upd9

2026-10-09 пользователь подтвердил: «Отлично, закрывай, все работает.»
#98 закрыт completed по прямому запросу; соответствующие пункты parent55 выполнены.
Актуальная принятая сборка — **0.2g upd9**. Приёмка относится к #98, не ко всему
#55/hardware/P4. Пакет upd9 и его хеши неизменны; новые build/tests для записи
приёмки не нужны. Docs остаются только локально.

2026-10-09 пользователь поручил «Начинай #98», Stage 5 не начинать.
Исходное дерево чистое, ветка `mrs/0.2g-fix1-editor-space-local`, база fe9430b.
Локально реализован [контракт/слои/RT/проверки](../INSTRUMENT_DROP_98.md).
VST3 browser drop на свободную область ниже дорожек Arrange создаёт и выбирает
instrument track с одним инструментом, имя по plugin. Ruler/занятая timeline
не создают дорожку; существующий mixer drop прежний, mini Arrange strips
по-прежнему не принимают VST3. Create+assign — одна запись shared Undo/Redo.
Save/Open — прежняя схема14/preferencesv7/cachev1. Creation-only factory требует
Instrument metadata и MIDI event input; обычная загрузка insert не изменена.
Plugin/engine готовятся до публикации модели, validated instance переиспользуется;
существующие processors сохраняются. Ошибка path/class/effect/MIDI effect/no MIDI/
backend start не оставляет track и сохраняет project/revision/Redo. Callback code
не изменён; подготовка/rollback после stop callbacks/workers. Editor failure после
успешного создания отображается отдельно и не удаляет корректно назначенный track.

Source **73fd5f9**, та же локальная ветка. Cached configure/Windows x64 Release PASS,
финальный полный CTest **118/118 PASS (51.75s)**, включая MIDI/VST3/engine/RT/
persistence/Undo/J2/J3/diagnostics. Первые fixture assertions исправлены по
существующим mini-strip policy и foundation graph gain; финальный прогон зелёный.
Packaged J3+diagnostics exit0; diagnostics log подтверждает version0.2g upd9.
Software100/150 previews просмотрены. Physical ASIO/real pointer drag/installed
instrument audition/mixed-DPI **NOT RUN** в автоматическом прогоне; пользовательская
функциональная приёмка #98 получена выше, отдельная hardware/DPI матрица не заявлена.
Пакет **MR-Studio-0.2g-upd9-Instrument-Drop-JUCE-ASIO-Windows-local** в Builds,
matching EXE/PDB/helpers/assets/licenses/manifest; прежние пакеты сохранены.
EXE SHA256 **A68B36837DC644D23B89A5E5B231F1EA66536398788D0E0C3432C708A1DAB0C9**.
**FEATURE ACCEPTED**; issue98 закрыт completed. Предыдущий статус доставки был
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK.
Исторический следующий шаг после приёмки upd9 — запрос на #95; получен, работа выше.
Stage 5 не начат. Issues обновляются; docs только локально, без push/merge/Actions.

## Принято: #99 — компактная панель и overlay, 0.2g upd8

2026-10-09 пользователь подтвердил: «#99 принят, закрывай!».
#99 закрыт completed; соответствующие пункты parent55 выполнены. Приёмка
относится к #99, не ко всему #55 или hardware/P4 матрице. Пакет upd8 и его хеши
не изменяются. Только локальные docs; новые build/tests для записи приёмки не нужны.

2026-10-09 пользователь принял #100: «Работает! Принимай и переходи к #99».
#100 закрыт completed; parent55 обновлён. Пакет upd7 неизменен.
#99 реализован локально: [контракт/слои/проверки](../NOTE_EDITOR_LAYOUT_99.md).
Все прежние controls — в компактном scrollable sidebar слева, browser font;
CC1/11/64 на тонком divider. Controller graph растягивается поверх roll без
изменения его bounds/scroll. Notes/Controllers могут быть открыты одновременно.
Сохранены keyboard routing/disabled state формы, preview cancel, shared Undo,
scroll/zoom #100, Attach/Detach/F11. Overlay height/lane transient; существующая
preference controllersOpen прежняя. Model/schema/engine/audio/RT не изменены.
Source **3343c86 +187b540**, та же локальная ветка. Cached configure/JUCE Release
PASS; финальный focused **5/5 PASS(35.00s)**: midi_clips, desktop_workspaces,
J2/J3/diagnostics. Packaged J3+diagnostics exit0, log version0.2g upd8 подтверждён.
Software100/150/small720x300 previews просмотрены; physical DPI/manual input/ASIO
NOT RUN. Регрессии: overlay invariants/clamps/hit testing, CC filter/state/cancel,
form shortcuts/disabled state, sidebar overflow и MIDI project/revision invariant.
Пакет **MR-Studio-0.2g-upd8-Note-Editor-Layout-JUCE-ASIO-Windows-local** в Builds.
EXE SHA256 **DFB0D8C1E6A2D79320ACD73FF695BB8CC7C2C0AB970A20E1927001CEA1823F7D**.
FEATURE ACCEPTED; issue99 закрыт completed по прямому запросу пользователя.
Исторический следующий шаг после приёмки #99 — отдельный запрос на #98/#95;
запрос на #98 получен 2026-10-09, текущая работа выше. #95/Stage5 не начинать.
Старые пакеты сохранены; source push/Actions нет.

## Принято: #100 — controller horizontal scroll, 0.2g upd7

2026-10-09 пользователь поручил «Начинай #100». [Контракт/слои](../CONTROLLER_SCROLL_100.md).
Controller graph/background передаёт wheel в существующий MidiClipPanel/PianoRoll
обработчик. Shift — horizontal scroll; Ctrl+Shift — time zoom; plain/Ctrl сохраняют
vertical scroll/row zoom. Native deltaX поддержан одинаково над roll/lane.
Общий viewport/beatWidth — один источник; preview отменяется без commit.
Model/Undo/persistence/schema/engine/RT не изменены, scroll transient.
Новая GUI regression: trimmed clip, pixel alignment, обе стороны, zoom anchor,
границы, vertical behavior, cancellation и project/revision invariant.
Source **7e0b56f +6858ccf**, та же локальная ветка. Cached configure/JUCE Release PASS;
финальный focused **5/5 PASS(33.83s)**: midi_clips, desktop_workspaces, J2/J3/diagnostics.
Packaged J3+diagnostics exit0, log version0.2g upd7 подтверждён. Software100/150
просмотрены: нота и controller point на одной X-позиции. Physical DPI/input/ASIO NOT RUN.
Пакет **MR-Studio-0.2g-upd7-Controller-Scroll-JUCE-ASIO-Windows-local** в Builds.
EXE SHA256 **37FF69519DC7193AF29A358BEEB04C783C9EB200C39342807F33293AC6A336EE**.
Принято пользователем 2026-10-09; issue100 закрыт completed. Следующий шаг — #99
по прямому запросу пользователя. #98/#95/Stage5 не начинать. Старые пакеты
сохраняются, GitHub docs не публиковать.

## Принято: компактный footer и сдвиг микшера — 0.2g upd6

2026-10-09 пользователь подтвердил финальную сборку: «Все отлично, принимаю».
Приняты удаление указанных надписей, нижнее расположение CPU/transport/workspaces,
Latency справа от CPU и сдвиг микшера при сохранении размеров каналов/фейдеров.
Пакет и его SHA256 не изменяются задним числом. Приёмка относится к этим UI-правкам,
не к общей hardware/P4 матрице и не ко всему parent55.

2026-10-09 пользователь принял inline rename: «Переименования принимаю».
#97 закрыт completed, parent55 обновлён. Шрифты приняты ранее.
Дополнительный запрос по screenshot: убрать постоянную строку driver/PDC/counters
и VST3 drag hint, CPU/transport/workspace controls опустить, Latency справа от CPU.
Меняются только JUCE layout/paint/tooltips; команды/RT/schema/fonts неизменны.
Диагностическая строка сохраняется в CPU tooltip, ошибки и count-in видимы.
На ширине1200/1400 счётчик времени выше для исключения перекрытий; широкое окно
имеет одну нижнюю строку. Дополнение пользователя: микшер тоже опустить.
Source **ea288ae +232fedb**: mixer dock/разделитель вниз14, высота каналов и
фейдеров сохранена, нижний пустой зазор убран; drag splitter учитывает сдвиг.
Cached offline configure/JUCE Release PASS; final focused CTest **4/4 PASS(34.18s)**:
desktop_workspaces, J2/J3/diagnostics. Geometry/snapshots ширины1200/1400/1920,
software100/150 просмотрены. Physical DPI/ASIO NOT RUN (косметическая правка).
Финальный пакет **MR-Studio-0.2g-upd6-Mixer-Footer-JUCE-ASIO-Windows-local** в Builds.
Packaged J3+diagnostics exit0, EXE SHA256:
**0C05CA8937BEF7044D6A60DA8C8EB003A24BEAB76EFDA0D0B65AD8A11885D33E**.
Первый кандидат Compact-Footer (ea288ae/hash49B0898F…) сохранён без изменений,
но заменён финальным Mixer-Footer по дополнению пользователя. Первый тест после
сдвига выявил старую geometry assertion (-6 вместо -20); обновлён ожидаемый
контракт, все GUI tests повторены PASS. Проверки fixed faders/Insert resizing сохранены.
Визуальная приёмка footer получена; повторная сборка для записи приёмки не нужна.
Первый compile fixture обращался к private buttons; заменён на поиск child
TextButton по caption, без раскрытия продуктовых private полей ради теста.
Следующий sub-issue/Stage5 не начинать до отдельного запроса.
Исторический следующий пункт после upd6 — #100; теперь реализован, ожидает приёмки.
После него очередь #98/#99/#95; они не начаты.

## Принято: #97 inline-переименование — 0.2g upd5 fix1

2026-10-09: шрифты пользователь принял («Со шрифтами все отлично»), но уточнил,
что переименование должно происходить непосредственно в поле имени, без окна.
Source **eac13f7** + финальный version rebuild **1cf7b07**, та же локальная ветка. В Arrange/Mix двойной клик открывает
TextEditor строго в nameBounds; Enter/потеря фокуса сохраняют, Escape отменяет.
Track menu использует тот же inline editor. Пустое/неизменное имя — no-op.
Typing не меняет модель и блокирует глобальные shortcuts. Existing RenameTrack,
Undo/Redo/Save/Open сохранены; queued commit защищён SafePointer/store/id.
Шрифты и RT/engine/schema не менялись. JUCE Release PASS; focused CTest **8/8
PASS (финальный26.16s; первый28.82s)**, включая J2/J3/diagnostics и core/persistence/workspaces/files.
Software100/150 inline snapshots просмотрены; физический DPI/ASIO NOT RUN.
Пакет **MR-Studio-0.2g-upd5-fix1-Inline-Rename-JUCE-ASIO-Windows-local** в Builds.
Packaged J3+diagnostics exit0. EXE SHA256:
**A5D83B0A92570AED95B003A7979003D99EB92907B610004BBFDE3EBAC83924FF**.
Diagnostics helper tests PASS. Финальный packaged log подтверждает version0.2g upd5 fix1.
Issue97 принят и закрыт 2026-10-09; шрифты приняты отдельно ранее.
Следующий шаг — footer upd6 по дополнительному запросу, не #100/Stage5.
Старый upd5 не изменён.

## Предыдущая доставка #97 — 0.2g upd5 (диалог заменён)

2026-10-09 пользователь поручил #97 и единый шрифт/размер каналов Mix/Arrange
как у кнопок плагинов Inserts. [Контракт/слои/проверки](../TRACK_RENAME_97.md).
Double-left-click в nameBounds Strip открывает штатный Rename; Enter/Escape,
existing RenameTrack/Undo/Save/Open, SafePointer/store guard. Master/body/right
click не переименовывают, recording guard. Общий Theme::stripFont11 logical,
тот же default/plain font как insert caption; channel buttons/readouts/names
и track lane captions единообразны. Browser/preset/ruler/toolbar не меняются.
Engine/RT/schema/ownership не затронуты. Изменены Controls.h, Desktop.h/.cpp,
Smoke.cpp, version.hpp (upd5). Source **6e7715e**, та же локальная ветка.
Cached configure/JUCE Release PASS; финальный serial CTest **118/118 PASS
(92.39s)**, packaged J3+diagnostics exit0 и diagnostics helper tests PASS.
Первая GUI fixture + core tests3/3 PASS(14.46s); затем добавлены single/right-click,
stale project dialog guard и native EQ fixture. Software100/150 просмотрены,
длинные/кириллические имена обрезаются в пределах captions; прочая тема сохранена.
Первое compile fixture
использовало protected AlertWindow::keyPressed напрямую, исправлено на public
Component virtual dispatch; продуктовый код не обходил права доступа.
Пакет **MR-Studio-0.2g-upd5-Track-Rename-Font-JUCE-ASIO-Windows-local** в Builds,
matching EXE/PDB/helpers/assets/licenses, test log/previews/SHA256 manifest.
EXE SHA256 **82B6D140D01AB04F02BC88B8BD80DFC0969210FD188177A9EC66F7DFF66EA5B7**.
FEATURE READY WITH MANUAL CHECK / READY WITH MANUAL CHECK. Physical mixed-DPI,
screen-reader/hardware не проверялись; новый код только UI, callback не менялся.
Исторический статус upd5: Issue97 open, диалог не принят. Следующий шаг — проверка fix1,
Arrange/Mix, Enter/Escape, Undo/Redo, Save/Open, drag reorder и единого font.
#100 и Stage5 не начинать. Старые пакеты/хеши сохранены; source push/Actions нет.

## Принято: #96 — 0.2g upd4 fix1

2026-10-09 пользователь подтвердил: «Тесты прошли, принимай».
#96 принят и закрывается как completed; parent55 остаётся open.
Принят текущий контракт скрытой audio/MIDI-предзаписи с исправлением Count.
Детальные параметры устройства и длительность пользовательского теста не сообщены;
общая sustained/ASIO матрица и P4 не считаются пройденными этой приёмкой.
Следующий по плану **#97 — переименование дорожки двойным кликом по имени**,
затем #100, #98, #99, #95. Реализация #97 и Stage5 не начаты; ждать запроса.
Source77c9abd и пакет fix1 ниже сохранены без изменений; приёмка documentation-only,
без пересборки, перепаковки, source push, Actions или релиза.

### Исправление остановки отсчёта

Пользователь сообщил: «Происходит пару кликов и запись не начинается».
Регрессия upd4 подтверждена: Application::poll между callback-ами приводит
к FAIL app.recording(). Скрытый prefix появляется при намеренно paused pre-count;
старое условие автоостановки ошибочно завершало take. Poll теперь использует один
coherent transport snapshot и исключает активный count из проверки stopped/paused.
Fault/device loss по-прежнему завершают запись. Callback и ownership не изменены.
Добавлены MIDI-only count1/2 с poll, отмена count через Pause, реальный Pause после
начала записи, combined prefix с poll на каждом callback и GUI count/prefix poll.
Первый regression FAIL до исправления; после исправления MIDI recording PASS.
Source **77c9abd**, та же ветка. Cached configure/full Release build PASS;
полный serial CTest **118/118 PASS (88.67s)**. Packaged J3+diagnostics exit0,
packaged MIDI recording и crash diagnostics helpers PASS.
Изменены application.cpp, version.hpp, MidiRecordSmoke.cpp, midi_record_tests.cpp.
Пакет: MR-Studio-0.2g-upd4-fix1-Count-Transition-JUCE-ASIO-Windows-local.
EXE SHA256: AEDB78FBC0C6086E66D7CA9E876502EB4E04618DD770D57C6B84150CCB510928.
Matching symbols/helpers/assets/licenses, full test log и SHA256 manifest.
READY WITH MANUAL CHECK / FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL
CHECK. Hardware ASIO/MIDI/sustained NOT RUN; poll fix is NON-RT, callback unchanged.
Первоначальный upd4 не был принят из-за регрессии; fix1 принят пользователем выше.
Результаты NOT RUN относятся к автоматической доставке, не подменяют последующую
пользовательскую приёмку и не утверждают полную аппаратную матрицу.

## Предыдущая доставка: #96 — скрытая audio/MIDI-предзапись (0.2g upd4)

2026-10-09 пользователь принял fix7: «Отличная работа, принимай и переходим
к сл. sub-issues». По актуальному порядку parent55 следующий #96, затем #97,
#100, #98, #99, #95. #96 прочитан: до кода требуется определить длительность,
поведение у начала проекта и отмены. Предложение на согласование: до2 секунд
исполнения в конце pre-count, без видимого сдвига старта; материал до нуля
не создавать как отрицательное timeline-время; остановка до старта не создаёт
клипа. Пользователь повторно поручил «Переходи к #96», работаем по этому контракту.
Design/layers: [PRERECORD_96.md](../PRERECORD_96.md). Feature/RT/build skills.
Изменены desktop.hpp/application.cpp, engine.cpp, Desktop.cpp/WaveformView.h,
version.hpp (upd4), audio_tests.cpp/midi_record_tests.cpp. Source start раньше
visible start; bounded overlap capture при count, existing source_offset у обоих
типов клипа, preview/finish сохраняют visible origin. Schema15 без изменений.
Source **2b0331f**, текущая ветка выше. Configure/full Release build PASS;
full CTest **118/118 PASS (88.53s)**, затем GUI-only test addition/build и
focused GUI **3/3 PASS (26.60s)**. Новые проверки: совместный prefix, crossing
note, raw/click separation, hidden offsets/left trim, Undo/Redo, actual Save/Open,
short window exact samples, zero start, Stop before start, zero RT allocations.
First edge fixture забыла re-arm после Open, исправлен тест (arms transient).
GUI проверяет скрытые offsets/live preview у audio/MIDI; software100/150
просмотрены. PDC/RT lifetime/worker ownership unchanged; new callback math bounded,
existing preallocated journals/SPSC, no new allocations/locks/I/O/UI.
Пакет **MR-Studio-0.2g-upd4-Prerecord-JUCE-ASIO-Windows-local** в Builds,
matching EXE/PDB/helpers/assets/licenses. Three packaged J3+diagnostics exit0,
packaged MIDI recording tests и diagnostics helper tests PASS.
EXE SHA256 **A06A065D1A0D3BB2D2412A2A9E015F24CAF8769B6C4A4D6427A02ECC92261858**.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK:
включить Count, старт с3-го такта, сыграть чуть раньше, Stop и раскрыть clip влево.
Hardware MIDI/ASIO/sustained/DPI NOT RUN. Issue96 open, приёмка ожидается.
Fix7 immutable пакет сохранён, Stage5 и следующие sub-issues не начинать.

## Принято: follow-up fix7 — единая тема меню и фон Mix

2026-10-09 fix6 принят: «Проверка прошла». По новому референсу пользователя
все host dropdown/popups получают фон Browser VST3 (surface=25282b) и едва
заметную рамку (3e4348,1px). Общие роли в Controls.h; PresetMenuTheme использует
те же цвета/фон, сохраняя свой компактный font/width. Standalone input/output/
Sends/plugin menus явно подключены к existing Theme; MenuBar/ComboBox наследуют.
Фон Mix/Master теперь panel=393e43, как mini track channel, selected header и
accent сохранены. Engine/project/persistence/Undo не меняются. Изменены Controls.h,
PluginPreset.h, Desktop.cpp, Editors.cpp, Smoke.cpp, version.hpp (upd3 fix7).
Source **1bd8ba7**, текущая ветка выше. Configure/Release build PASS,
focused CTest **4/4 PASS (29.79s)**: crash_diagnostics, JUCE J2/J3/diagnostics.
Regression: Mix/Master background pixels, popup fill/border, real popup preview;
прежние preset/hover/bypass/layout проверки сохранены. Software100/150 menus
и Mix просмотрены. First build: shadowed scale в тесте (/WX), renamed;
first pixel test read before native Graphics flush — software image + scope fix.
Production theme не маскировалась ради теста. Full118 не повторялся для UI.
Пакет **MR-Studio-0.2g-upd3-fix7-Popup-Mixer-Theme-JUCE-ASIO-Windows-local**
в Builds, matching EXE/PDB/helpers/assets/licenses. EXE SHA256
**F001654A4260CC4320A77DA9AE0A91793E160D639523578C32C595CEC58B11E0**.
Три packaged J3+diagnostics exit0 и packaged diagnostics helper tests PASS;
подробности: [POPUP_MIXER_FIX7.md](../POPUP_MIXER_FIX7.md).
READY WITH MANUAL CHECK: открыть File/Transport, Inserts/plugin submenu,
presets/input/output/Sends и сравнить фон Mix с Track. Fix7 принят 2026-10-09;
physical DPI/screen reader NOT RUN. Следующие sub-issues/Stage5 не начинаются.

## Принято: follow-up fix6 — hover проектов и плоские кнопки Mix

2026-10-09 пользователь принял fix5: «Теперь все отлично. Принимай».
Новый scoped UI follow-up: имя .mrsproject подсвечивается при наведении,
hand cursor только в пределах имени, не в пустой области списка. Кнопки
каналов Mix/Master и названий plugins — плоские, fill brighter(.15f).
Фон панелей, мини-каналы Arrange, power glyph и прежние взаимодействия сохранены.
Source **f09eadf**: ProjectHome.h, Controls.h, Desktop.cpp, Smoke.cpp, version.hpp.
Configure/Release build PASS. Первый scoped тест выявил, что hover paint
проверял позицию курсора вместо Button state; исправлено на getState().
Повторный focused CTest **4/4 PASS (25.26s)**: crash_diagnostics и JUCE
J2/J3/diagnostics. Проверены курсор списка и ширина имени, hover pixel change,
открытие по имени, flat flags Track/Master и brighter solid fill без bevel.
Software previews100/150 просмотрены. Full118 не повторялся для scoped UI.
Пакет **MR-Studio-0.2g-upd3-fix6-Home-Hover-Flat-Mixer-JUCE-ASIO-Windows-local**
в Builds; matching EXE/PDB/helpers, assets/licenses, SHA256 manifest.
EXE SHA256 **4E07E072E3BC6C8E67E44136A1926C1E6FD3BF35B723C3980B90A7B8FB4E1E74**.
Три packaged J3+diagnostics exit0, packaged diagnostics helper tests PASS;
детали: [HOME_MIXER_FIX6.md](../HOME_MIXER_FIX6.md).
READY WITH MANUAL CHECK: проверить наведение над именем/пустым пространством,
кнопки Mix/Master (не фон). Fix6 принят 2026-10-09. Physical DPI/screen reader
NOT RUN. Предыдущие пакеты сохранены, следующие issues/Stage5 не начаты.

## Принято: follow-up fix5 — crash notice и яркость power

2026-10-09 пользователь принял #94: «Да, отлично! Ты все правильно понял! Этот
тест прошел». #94 закрыт completed, parent55 обновлён. Затем попросил убрать
постоянное предупреждение предыдущего вылета и сделать bypass power чуть ярче.
Работа начата, следующие sub-issues НЕ начинаются. Источник popup: каждый запуск
выбирает тот же retained crash report без acknowledgement; реальный последний
crash.txt в Logs датирован 2026-10-08. Логи/дампы пользователя не удалялись.
Source **9af6ff2**, текущая ветка выше: diagnostics.hpp/session.cpp, Main.cpp,
Controls.h, Smoke.cpp, diagnostics_tests.cpp, version.hpp (upd3 fix5). .seen marker
при закрытии notice; latest acknowledged report не повторяется и не поднимает
старые reports, новый снова предупреждает. Marker в existing group retention;
callback хранит только path. Bright active3098ee/offbdc6cf, прежняя geometry.
Configure/Release app+diagnostics probe/tests build PASS; focused CTest4/4
PASS(29.38s). Regression restart×2/new report/idempotence/preservation/retention/
blocked-marker PASS. First fixture ошибочно меняла CRLF через text-mode copy;
исправлена binary-запись, production report contents не менялись.
Пакет **MR-Studio-0.2g-upd3-fix5-Crash-Notice-Bright-Power-JUCE-ASIO-Windows-local**
в Builds; EXE SHA256 **32F53CD94AA90D4C621AFEB7BA5D165CFD0B37F52368B11E4507C03AB25F56F5**.
Matching EXE/PDB/helpers, README/log/previews/SHA256; три packaged J3+diagnostics
exit0, packaged helper tests PASS. Software100/150 просмотрены. Full118 относится
к принятому fix4, не повторялся для scoped fix5. Project/DSP/fatal capture/schema
без изменений. См. [CRASH_NOTICE_FIX5.md](../CRASH_NOTICE_FIX5.md).
READY WITH MANUAL CHECK: при первом старом notice нажать OK, штатно закрыть и
перезапустить — окно не должно вернуться без нового report; проверить яркость
power. Fix5 принят пользователем 2026-10-09. Physical DPI/screen reader NOT RUN. Старые пакеты
сохранены; следующие issues/Stage5 не начаты, GitHub только issues.

## Принятая сборка: #94 — свободная область Inserts (0.2g upd3 fix4)

Окончательное уточнение пользователя 2026-10-08/09: «свободную область для
инсертов можно растягивать», «Все верно». Не ограничивать высоту числом плагинов;
Inserts закреплены сверху, фейдеры фиксированной длины, Sends не менять.
Issue94 title/body приведены к этому контракту. Никаких новых вопросов о дизайне
не требуется. Пользователь запросил продолжение 2026-10-09.
Source **f318c6c**, текущая ветка выше: Desktop.cpp/.h, Settings.h, Smoke.cpp,
version.hpp. Fixed145px фейдеры, свободный resize до окна с минимумом аранжировки
140px, сохранённая base-height семантика mixerHeight и независимый editorHeight.
View JSON version1 прежняя; предел decode mixerHeight повышен до2160 для высоких
окон. Project schema15/preferencesv7/DSP без изменений; resize не проектный Undo.
Configure/Release build PASS; focused GUI3/3 PASS(26.79s), full serial118/118
PASS(86.06s). Исправлены ошибки нового теста: shadow-variable и повторение insert
IDs на разных каналах; production engine не менялся. 100/150 software snapshots
просмотрены (включая пакет): маленькое окно, fixed faders, top rows, overflow.
Пакет: **MR-Studio-0.2g-upd3-fix4-Free-Inserts-Height-JUCE-ASIO-Windows-local**
в рабочей папке Builds; EXE SHA256
**E224D8FDE7C088F83D3AE9DA4939FCA21E0DE4FAD98ABDB8D20A26023192A329**.
Matching EXE/PDB/helpers, README, test logs, previews, SHA256 manifest. Три
packaged J3+diagnostics exit0; packaged crash-helper tests PASS.
См. [MIXER_LAYOUT_UPD3.md](../MIXER_LAYOUT_UPD3.md). READY WITH MANUAL CHECK:
следующий шаг — пользовательская проверка разделителя с 0/1/несколькими plugins,
Master/Bus, scrolling и независимого Edit size. 2026-10-09 пользователь подтвердил
тест; issue94 закрыт completed. Physical mixed-DPI/screen reader NOT RUN. #95–100/Stage5 не
начинать, предыдущие пакеты сохранить. GitHub только issues, без code push/release.

## Принятая сборка: #93 — общая кнопка плагина (0.2g upd2 fix4)

2026-10-08: пользователь прислал screenshot и попросил power на самой кнопке
плагина, без наложения на текст, с меньшим шрифтом. Fix4: editor занимает row,
power — дочерняя кнопка внутри x=2,13×13; текст начинается x=17, left,11px.
Общая поверхность/рамка через existing theme, separate child hit-area сохраняет
бypass без открытия editor. Blue active / neutral off прежние. Изменялись только
Controls.h, UI test assertions, version и docs; Application/DSP/schema без правок.
Release build PASS;3/3 GUI CTest PASS(28.45s), hit/shared-surface/inset assertions,
100/150 snapshots просмотрены. Full118 относится к функциональному fix2.
Новый пакет: MR-Studio-0.2g-upd2-fix4-Inline-Plugin-Power-JUCE-ASIO-Windows-local;
EXE SHA256 B4AB7DF1EA794C97E0F3CFA463B55E92831AED32894F9EC2A2F4DD0AB9970370.
Три packaged J3+diagnostics smoke exit0. Source **f96bff5**, matching EXE/PDB,
README/CTest log/previews/SHA256 manifest в новом пакете; issue93 progress обновлён.
2026-10-08 пользователь подтвердил итоговый fix4: «Принято!». #93 закрыт completed,
parent55 обновлён. Следующая предлагаемая #94 — ограничение высоты микшера по
содержимому каналов, только по отдельному запросу. Пакет не изменять задним числом.
Старые пакеты сохранить, #94–100/Stage5 не начинать; GitHub только issues.

## Предыдущая локальная сборка: #93 — power icon (0.2g upd2 fix3)

2026-10-08: fix2 функционально принят («Все отлично, работает»), пользователь
попросил уменьшить кнопку вдвое, сделать невидимой и заменить B на power icon;
синий = плагин работает, бесцветный = выключен. Fix3: 13×13 квадрат по центру
18px строки, без idle фона/рамки, cached JUCE Path, blue !bypass / neutral bypass.
Название больше не содержит [B]; tooltip/accessibility state/action сохранены.
Менялись только Controls.h, version, smoke UI assertions и локальные docs.
Application/DSP/Undo/PDC/schema15/preferencesv7 не менялись. Build PASS;
3/3 GUI CTest PASS (26.67s), snapshots100/150 просмотрены. Полные118 относятся
к функциональному fix2 ниже, повтор для косметики не выполнялся.
Новый chat Builds: MR-Studio-0.2g-upd2-fix3-Mixer-Power-JUCE-ASIO-Windows-local;
EXE SHA256 FE8A5AB739B82DF7D58080B45CD71C357197B92753891E8ADB627BAFF0685A5E.
Три packaged J3+diagnostics smoke exit 0. Source **2240af0**, matching EXE/PDB,
SHA256 manifest и README в новом пакете. Приёмка нового вида ожидается, #93 открыт.
Stage5/#94–100 не начаты, source/docs только локально, старые пакеты сохранены.

## Предыдущая функциональная сборка: #93 — bypass (0.2g upd2 fix2)

2026-10-08: пользователь разрешил «Приступай» после предложения #93.
MixerInsert содержит отдельные B / editor кнопки; 26 px область bypass слева,
18 px row / 20 px шаг, channelFont и существующая тема. Синий B и [B] означают
bypass, tooltip хранит полное имя. UI читает NativeInsert.bypass, action идёт
через existing applyChain/SetInserts/central Undo/persistence. Queued action
проверяет SafePointer Strip, идентичность ProjectStore и наличие slot.
Native bypass работает в playback через existing parameter queue; VST3 требует
Pause/Stop и existing PDC rebuild/editor reconnect. Callback/DSP/schema15/
preferencesv7 не менялись. Контракт: [MIXER_BYPASS_UPD2.md](../MIXER_BYPASS_UPD2.md).

Cached Release configure/build PASS, focused J2 PASS (2.67s), **118/118 CTest
PASS (82.01s, sequential)**. Начальный -j2 прогон: один existing ahead recovery
timing failure, затем 10 изолированных повторов PASS; engine/test не изменены.
Новые GUI проверки: hit regions, Undo/Redo/UI/model, сериализация, native live
bypass, bus/Master, stale queued slot; VST3 7-sample PDC/reconnect/playing rejection.
Снимки 100%/150% просмотрены; физический ASIO/installed plugins/mixed DPI NOT RUN.
Кандидат chat Builds: MR-Studio-0.2g-upd2-fix2-Mixer-Bypass-JUCE-ASIO-Windows-local;
EXE SHA256 76A3DDDB75897AC85578252F695BF8B7495440788BFEDBC4AD752AE79784AF61.
Восемь packaged J3+diagnostics smoke exit 0; packaged crash diagnostics PASS.
Source **9697c54**, код локально; README/VALIDATION/снимки/CTest log, matching
EXE/PDB и SHA256 manifest в отдельном пакете. Skills сохранили существующий
command/Undo/RT transfer и стиль вместо введения отдельного владельца bypass.
Следующий шаг: пользовательский manual checklist из README пакета.
Issue93 progress обновляется при передаче. #93 открыт до пользовательской
приёмки; #94–100/Stage5 не начинать, source/docs на GitHub не отправлять.

## Принятая локальная сборка: #92 — библиотека пресетов (0.2g upd1 fix2)

2026-10-08: пользователь разрешил #92 и согласовал вложенные папки внутри
библиотеки каждого плагина, сохранение выбранного имени в проекте. Реализованы
компактный selector, иерархическое асинхронное меню, создание папок, Save Folder/Name,
точное сопоставление файлов и текущая метка с Undo/Redo. Старые библиотеки не
перемещены. NativeInsert.preset_name; schema15 читает 1–14, новые файлы требуют
новой сборки. Контракт: [PRESET_LIBRARY_UPD1.md](../PRESET_LIBRARY_UPD1.md).

Пользователь принял #92 2026-10-08: «Тест пройден». Полные 118 CTest прошли до последней правки,
но повторный packaged smoke выявил abort при MIDI message pumping. Дамп второго
шанса и непосредственно предшествующий C++ exception установили путь:
ComboBox::handleAsyncUpdate → FxPanel::sync → Desktop::chain → «Track no longer exists».
Это отложенное уведомление редактора эффекта после удаления дорожки, не доказанный
сбой VST3/DSP. Убрано уведомление начального выбора band; retirement снимает callbacks
bands/parameters/presets, пользовательские sync callbacks проходят owner.run.
Последняя сборка PASS; **118/118 CTest PASS (53.67 s)** и восемь последовательных
packaged GUI/diagnostics smoke exit 0 после исправления. Это не обещание отсутствия
любых других вылетов. Физический ASIO/installed plugins — пользовательская проверка.

Source **c4f1e41**, локальная ветка выше: core model/serialization, desktop
Application/version, JUCE Controls/Desktop/Editors/PluginPreset/Smoke, core/persistence
tests и legacy snapshot helper, docs PLUGIN_PRESETS/VERSIONING/новый контракт;
tools/dump_inspect читает memory64 и пропускает пустые RVA stack regions.
Кандидат chat Builds: MR-Studio-0.2g-upd1-fix2-Preset-Library-JUCE-ASIO-Windows-local;
README с чек-листом и SHA256 manifest готовы; packaged crash diagnostics PASS.
EXE SHA256 A5FDC0FC01EBA902FBC513A2FF7A1C6924B1B4056DCE95961D494F1C33F8D68E.
Дампы debug-2-e06d7363.dmp
и debug-2-c0000409.dmp в кандидате; helper build/asio-local/capture-debug.ps1 только
для расследования. Issue92 обновлён результатами проверок. Следующий шаг:
предложенная #93 — bypass в левой части кнопки плагина микшера, только по запросу.
Issue92 закрыт completed, parent55 обновлён. Stage5/#93–100 не начаты.
GitHub docs/source не отправлять; локальный пакет не изменять задним числом.

## Принятая локальная сборка: 0.2g fix2 — #91, Space во встроенном EQ

2026-10-08: пользователь принял fix1 («Это заработало»), уточнил: обычные VST3
работают, пробел не работал во встроенном эквалайзере. Source `7b702c2`, та же
локальная ветка. FxPanel встроенных эффектов и окно VST3 Parameters теперь
используют общий scoped Space handler: Play/Stop, repeat/text/modifier/modal guards,
снятие регистрации и отмена queued action при закрытии. DSP/model/schema не менялись.
Контракт [EDITOR_SPACE_FIX2.md](../EDITOR_SPACE_FIX2.md).
Пакет chat Builds: `MR-Studio-0.2g-fix2-Audio-Editor-Space-JUCE-ASIO-Windows-local`.
EXE SHA256 `8497CA520BBFEAAA05BA0B3B8F5FEEE24F37D7CBF4CDCE5871BD0DC2D3402AD4`.
Windows x64 Release build PASS; **118/118 CTest PASS (78.83 s)**, включая actual
posted Space в EQ: Play/Stop/repeat/text/queued-close. Три packaged J3+diagnostics
smoke exit 0; packaged crash tests PASS. Matching EXE/PDB, SHA256 manifest.
2026-10-08: пользователь подтвердил fix2: «Тест пройден». Исправление #91 принято
в пользовательском объёме (VST3 и встроенный EQ); это не универсальная проверка
всех opaque text widgets. Следующая предлагаемая задача — #92: компактный выбор
пресетов, текущий пресет и папки; начинать только по запросу. Stage 5/#92–#100 не начаты;
source push/merge/release не выполнялись. Старые пакеты/проекты/настройки сохранены.

## Предыдущая локальная сборка: 0.2g fix1 — #91, Space в редакторе VST3

2026-10-08: пользователь разрешил «Тогда поехали issue #91». Только эта подзадача;
#92–#100 и Stage 5 не начаты. Source `887bd41`, код остаётся локально;
разовое разрешение предыдущего merge/release не переносится на этот fix.
Пакет chat Builds: `MR-Studio-0.2g-fix1-Editor-Space-JUCE-ASIO-Windows-local`.
EXE SHA256 `67D32CA24C386ECD57FEA953D056F7E8E8F8F23A957EDEE7DA1FC39BA305E6B1`.
Контракт [EDITOR_SPACE_FIX1.md](../EDITOR_SPACE_FIX1.md). Native VST3 HWND bypasses
JUCE keyboard route; message-thread WH_GETMESSAGE bridge ограничен visible plugin
editor windows. Space → async существующий основной Play/Stop action; repeats
подавлены, modifiers/modal/посторонние окна не перехватываются. SafePointer и
attachment generation отменяют stale async action; retire снимает регистрацию,
последний editor снимает hook. Нативные HWND плагина не subclassed.
Текстовые JUCE/native Edit/RichEdit/ComboBox и OS caret защищены. Opaque custom
text fields без caret и отдельный plugin UI thread — ограничения, не заявлять
универсальную защиту text input. DSP/callback/model/Undo/schema14/preferencesv7
не изменены. Тема/геометрия прежние; существующий транспорт — единственный owner.

Cached configure/Windows x64 Release PASS без compiler warnings; **118/118 CTest
PASS (78.42 s)**. Реальные posted keydown/up на native fixture: Play/Stop, repeat,
text input, Ctrl, modal и unrelated window PASS. Existing editor Pin/reconnect/
presets/DPI/main Space и audio/VST3 regressions сохранены. Три packaged J3+
diagnostics smoke exit 0; packaged crash tests PASS; matching EXE/PDB + manifest.
Installed Omnisphere/physical ASIO/real keyboard acceptance **NOT RUN**.
Историческая поставка требовала manual check; пользователь принял fix1, затем
fix2 (актуальная запись выше). #91 завершён в принятом объёме. Ограничения opaque
text/separate UI thread сохранены. Старые пакеты и пользовательские проекты/
настройки не изменены; не переходить автоматически к #92 или Stage 5.

## Опубликованная сборка: 0.2g — внешний MIDI

2026-10-08, после release: по запросу пользователя #55 декомпозирован в native
GitHub sub-issues #91–#100: transport focus, presets, mixer bypass/height/sends,
hidden audio/MIDI pre-record, track rename/drop instrument, Note Editor layout
и controller horizontal scroll. Parent содержит исходные требования и индекс;
детальные критерии — в каждой child issue. Это межэтапные доработки, не Stage 5.
На момент декомпозиции — **ТОЛЬКО ПЛАНИРОВАНИЕ**; позднее отдельным запросом
разрешена #91 (реализация выше). Остальные sub-issues остаются не начатыми.
#96 потребует отдельного дизайна изменения pre-count/hidden source contract.

2026-10-08: по запросу «Приступай к 0.2g» реализован Stage 4g / #69 локально.
Source `959ffbc`, исходная ветка `mrs/0.1q-fix3-processing-local`.
2026-10-08: пользователь прямо разрешил документацию/issues, merge в main и
GitHub release. Это разовое разрешение публикации текущего кода и пакета;
GitHub Actions по-прежнему не запускать. Stage 5 не начинать.
Merge в main: `6d61355` (предварительно сохранены remote docs/dashboard).
Post-merge cached configure + clean-first Windows x64 Release PASS без compiler
warnings; повторный CTest **118/118 PASS (47.46 s)**, включая GUI/crash smoke.
Исходники apps/core/tests/CMake/cmake идентичны принятой `88c2e99`.
GitHub release/tag: [v0.2g](https://github.com/vladleng/MR-Studio/releases/tag/v0.2g).
Публикуется ранее проверенный EXE, не новая сборка с другими symbols.
Windows ZIP SHA256 `3377515AE685404B0EC752F867287E743A718BF83586806DAAF6371451A9087A`;
отдельный Symbols ZIP `2FAEBDAFF5CE4AA7F1DCB856E632629C88650F79055529F7F095380BD6074EC8`.
ZIP manifest 32 runtime files проверен; upload digests совпадают. Старый пакет
не изменён. Issues #24/#69 актуализированы и открыты только для remaining gate;
#63–#68/#70 уже закрыты, их приёмка не отменяется. Stage 5 остаётся НЕ НАЧАТ.
Пакет chat Builds: `MR-Studio-0.2g-External-MIDI-JUCE-ASIO-Windows-local`.
EXE SHA256 `89E8E76EE6282E8D1B7930F4AA52C9110DA85402FE4CB753ECFCC0E8D0C8D1FA`.
Instrument track: Windows MIDI output/Original либо channel 1–16; playback,
monitored live thru, notes/CC/PC/bend/pressure, missing/unavailable status,
Panic и stopped Reconnect. Выбор в Arrange через MIDI input/status button.
VST3 необязателен; возврат audio — обычная audio track. Undo/Redo и Save/Open.
Схема 14 читает 1–13 с output Off; старые сборки не читают 14 — Save As копию.
Preferences v7 прежние. Accepted 0.2h upd1/fix1–fix3 сохранены.

RT: device-owned scheduler, bounded SPSC, output driver calls только отдельным
worker; epoch/generation защищают replace/transport, overflow fail-closed/Panic.
Прямой совпадающий input/output endpoint запрещён, сложные кабельные петли
не определяются. WinMM jitter не sample-accurate, hardware latency не калибруется.
Контракт [MIDI_EXTERNAL.md](../MIDI_EXTERNAL.md), [ручная проверка](../MRS_STAGE_4G_CHECKLIST.md).

Cached Windows x64 Release build PASS без compiler warnings; CTest **118/118 PASS
(76.42 s)**. Новые fake backend тесты: model/schema/Undo/Save/Open, output channel,
overlapping notes, playback/thru/record-monitor-off, loop/Mute/audition/transport,
stale generation/epoch, overflow, missing/busy/reconnect/send failure/shutdown,
44.1/48/96 kHz, 1/4 workers, checked callback allocations 0.
Три packaged J3+diagnostics smoke exit 0, packaged crash tests PASS.
Software snapshots 100/150% просмотрены; matching EXE/PDB и SHA256 manifest в пакете.
Пользовательская приёмка 2026-10-08: «Все работает!» после предложения совместной
проверки MIDI-записи, редакторов, VST3 playback, метронома/precount и Save/Open.
Работа доступных MIDI-функций принята в сообщённом объёме; точные настройки и
длительность проверки не перечислены. У пользователя только MIDI-клавиатура,
внешнего принимающего устройства нет. Физический MIDI output / external ASIO
routing/timing **NOT RUN**, отложен до появления устройства; это не отказ теста.
Не утверждать полную hardware matrix или безусловную приёмку всех требований
Stage 4 / #24 и 4g / #69. Следующий шаг — согласовать следующий этап по плану;
Stage 5 автоматически не начинать. Пакет и ранее зафиксированные хеши неизменны.

## Предыдущая принятая сборка: 0.2h upd1 — копирование клипов

Приёмка 2026-10-08: «Все работает, вылетов пока не было обнаружено».
Upd1 принят в сообщённом объёме; отсутствие замеченных вылетов не доказывает
универсальную crash-free стабильность или прохождение отдельной hardware matrix.
Следующий согласованный подэтап был 4g / 0.2g, внешний MIDI; реализован выше
по следующему прямому запросу пользователя. Затем совместная проверка Stage 4.

2026-10-08: пользователь сообщает «Пока вылетов нет» после fix3 и запрашивает
Duplicate / Alt-копирование аудио и MIDI как update. Не утверждать отсутствие
всех возможных crashes или приёмку всего parent-этапа. Это историческая запись upd1.

Пакет chat Builds: `MR-Studio-0.2h-upd1-Clip-Copy-JUCE-ASIO-Windows-local`.
EXE SHA256 `4CAA43511630D59DC04CE915791F55DBC2FB9501E7AA91AF145A73A0DAF3F6CC`.
Source `f86f14c`, ветка прежняя; контракт [CLIP_COPY_UPD1.md](../CLIP_COPY_UPD1.md).
Duplicate в общей панели ищет свободный такт после оригинала на той же дорожке,
сохраняет внутритактовое смещение. Alt при захвате показывает движущуюся копию,
оригинал остаётся; release создаёт одну undoable команду. Escape/focus loss
отменяют preview. Копии выделяются; новые clip/note/event IDs, source references
и hidden source content сохранены. Группа копируется атомарно. WAV не размножается.
Редактирование только Stop/Pause; обычный Snap. Schema 13 / Preferences v7 прежние.
Preview только UI; Application candidate + playback rebuild retains plugins,
без новых RT allocations/locks/I/O или смены DSP ownership. Fix1–fix3 сохранены.

Cached configure/full Windows x64 Release PASS; CTest 117/117 PASS (73.25 s).
Новые проверки встроены в midi_clips, desktop_clip_edits и J3: off-grid,
занятый такт, tempo/meter, fresh IDs, audio playback, mixed group rollback,
Undo/Redo/serialization, Duplicate/Alt preview/commit/Escape/no-motion.
Три packaged J3+diagnostics smoke exit 0; packaged crash tests PASS. Matching
EXE/PDB сохранены; manifest SHA256 проверяется при упаковке.
Software snapshots 100/150% inspected. Физическая ASIO/ручная приёмка upd1
не детализирована: пользователь подтвердил работу upd1, но точные сценарии,
настройки и длительность сессии не перечислены. Отдельная hardware/DPI matrix
остаётся NOT RUN; следующий шаг разработки — 0.2g по отдельному запросу.
READY WITH MANUAL CHECK / FEATURE READY WITH MANUAL CHECK /
RT SAFE WITH MANUAL CHECK. Предыдущие пакеты и пользовательские данные сохранены.

## Предыдущая сборка: 0.2h fix3 — state snapshot crash

2026-10-08: пользователь разрешил исправление после анализа приложенного fix2
дампа. Source `2659a17`, ветка прежняя; контракт STATE_SNAPSHOT_FIX3.md.
Пакет chat Builds: `MR-Studio-0.2h-fix3-State-Snapshot-JUCE-ASIO-Windows-local`.
EXE SHA256 `02FF65C0DD1A3BE04B78CF8389B0F17554427844CA1882061A585C9F67CCD495`.
Paint/refresh и piano playhead держат последний корректный UI snapshot при
занятости mailbox. MIDI/controller timers безопасно отменяют gesture, busy
не разрешает note edit; Space strict-read exceptions обрабатываются UI.
try_state сохраняет output при failure и сбрасывает снятый loop при success.
EngineTransport UI/control projection возвращает последний published state
при contention. RT publisher/callback/DSP/scheduling/plugin lifetime не менялись;
нет новых locks/allocation/I/O/retries на RT. Cache только message-thread display,
не persisted/undoable, resetDevice очищает его; schema 13 / Preferences v7 прежние.

Cached configure/full local Release PASS; CTest 117/117 PASS (74.10 s).
audio_state_snapshot + diagnostics GUI focused 2/2 PASS (7.70 s); deterministic
odd-sequence regression воспроизводит прежний throw, проверяет новые paint,
refresh/timers/Space, fresh-state MIDI guard, неизменную revision и recovery.
50,000 producer callbacks/concurrent reads проверяют coherence/unchanged failure;
busy projection allocation probe = 0. Три packaged J3+diagnostics exit 0,
packaged crash tests PASS, software preview inspected. Final matching PDB сохранён.
READY WITH MANUAL CHECK / FEATURE READY WITH MANUAL CHECK /
RT SAFE WITH MANUAL CHECK: физическая ASIO сессия и пользовательская приёмка
fix3 ещё не выполнялись. Следующий шаг — обычный проект Omnisphere/TH-U на
Komplete ASIO 48 kHz/128/Workers 8/Process Off, переключение окон и ожидание.
Найденный state-read crash path устранён; не утверждать, что все исторические
вылеты имели эту причину. Crash logging продолжает работать, старые пакеты,
пользовательские проекты/настройки/логи не переписаны. 0.2g не начинать автоматически.

## Предыдущая сборка: 0.2h fix2 — crash diagnostics

Получен пользовательский crash `mr-session-1791441386160-12772-1` (Logs,
2026-10-08). Диагностика с Symbols fix2: exception 0xe06d7363, main/UI thread
6912; raw stack содержит std::runtime_error, AudioEngine::state (engine.cpp:483)
и Desktop::paint (Desktop.cpp:220). Исходник подтверждает throw
«audio state busy; poll again» при 32 неудачных чтениях согласованного snapshot;
paint вызывает throwing state() без обработки. Это конкретный host UI/state
failure path, не доказательство crash в Omnisphere/TH-U. Стек утилиты — raw
candidates, не полный unwind. Лог: Komplete ASIO, 48 kHz, buffer 128, Workers 8,
Process Off; последняя heartbeat playback=2, recording=0. clean_shutdown в конце
не отменяет native unhandled-exception report (возможен unwind logger).
Код/пакет не менялись при анализе. Следующий предлагаемый fix: nonthrowing,
bounded UI snapshot reads с последним корректным состоянием/пропуском кадра;
аудит остальных state() consumers и детерминированная contention regression.
RT boundary: аудиопоток публикует snapshot, UI читает; RT RISK FOUND на
RT-adjacent UI consumer, hardware validation нового fix ещё не выполнялась.

2026-10-08: по запросу «Приступай, это будет фикс» добавлена локальная
диагностика. Source `0caf858`, ветка прежняя; контракт CRASH_DIAGNOSTICS.md.
Пакет в chat Builds: `MR-Studio-0.2h-fix2-Crash-Diagnostics-JUCE-ASIO-Windows-local`.
EXE SHA256 `7C0AB8AAF46C4963B572F2239B17F18E36A24E8A2E99C2E2B433FE4862C78984`.
Обычные сессии пишут Documents/MR Studio/Logs; File → Open diagnostics folder.
Скрытый helper сохраняет локальные crash text/minidump с exception/thread/module,
при следующем запуске есть уведомление. Журнал ограничен, старые сессии очищаются
в пределах отдельной Logs-папки; нет сетевой отправки, registry/WER changes,
восстановления проекта или новых I/O/locks/allocation в audio processing.
Schema 13 / Preferences v7 / принятые настройки транспорта прежние.

Full Release build PASS; финальный CTest 116/116 PASS (43.38 s), три packaged
J3 diagnostics smoke exit 0, packaged native crash tests PASS, software preview
проверен. Новые тесты проверяют main/worker dumps, abrupt exit, clean shutdown,
retention/live lock, saturation/missing helper/unwritable folder/menu.
Первый packaged smoke реально упал; лог/дамп с совпадающими EXE/PDB сохранены
в пакете. Адрес: AudioEngine::process_channel, engine.cpp:48, повреждённый индекс.
В J2 обнаружен конкурентный запуск ручного render и threaded Offline clock после
resetDevice; fixture теперь снова подключает ManualDevice перед ручным render.
Production audio engine не менялся. Это отдельная ошибка теста, не доказанная
причина пользовательского вылета. Символы финальной сборки сохранены в Symbols,
старой — Evidence/pre-test-owner-fix; не смешивать их при анализе.

Ручная приёмка fix2 ожидается. Следующий шаг: использовать обычный проект и при
повторном вылете получить соответствующие .log/.crash.txt/.dmp. Реальный вылет
пользователя НЕ объявлен исправленным; Omnisphere не объявлена причиной.
0.2g автоматически не начинать. Старые пакеты и пользовательские данные сохранены.

## Предыдущая принятая сборка: 0.2h fix1

Приёмка 2026-10-08: «Все работает по последним правкам». Toolbar/темп/изменения
note-edit приняты в сообщённом объёме, но crash НЕ принят как устранённый.
Пользователь сообщает спонтанный вылет при просто открытом проекте; неизвестно,
был ли транспорт остановлен и происходила ли фоновая обработка. По его опыту
те же используемые плагины никогда не вызывают вылетов в Studio Pro. Не считать
перенос ноты необходимым trigger и не приписывать причину Omnisphere/другому
плагину без stack/dump. Следующий фокус — диагностика устойчивости MR Studio
в открытом/idle/background проекте, получение exception/module/stack и проверка
host lifetime/thread ownership. Код/пакет/старые manifest не менялись по этой
приёмке; точные ASIO settings/duration не сообщены. 4g автоматически не начинать.

2026-10-08: пользователь сообщил «Все работает» по 0.2h и запросил три
исправления. По уточнению Click/Count должны быть кнопками, настройки Transport
уже правильные. Реализованы переключатели без popup: Click включает/выключает
playback+recording click, Count возвращает последний включённый bar count
(первоначально 1). Transport submenu не менялся; Preferences v7/schema 13 прежние.
Новый проект больше не наследует foundation_demo tempo/meter: TimeMap{} =
постоянные 120 BPM, 4/4. Сохранённые карты темпа не меняются.

Пользователь уточнил вылет: Omnisphere 3, после отпускания перенесённой ноты,
замечен после переключения на ChatGPT. Note replacement / musical transforms
теперь используют clip-only rebuild с сохранением VST3 instances вместо capture/
пересоздания на каждом commit. Остановка callback/producer перед rebuild сохранена.
Причина редкого вылета НЕ доказана; совпадающего нового Windows stack/dump нет.
Не объявлять исправленным старый isolated heap-corruption exit только по PASS.
Следующий ручной шаг: повторить переносы с Omnisphere 3, переключение окон и
ожидание после отпускания, проверить audition/Undo/Redo. Контракт: METRONOME_FIX1.md.

Source `4d718a38019456e1471ea14f75a21ed076d4d896`, ветка прежняя.
Cached configure/full Release PASS; **114/114 CTest PASS (63.41 s)**.
Toolbar callbacks, count restore, New после Demo / uniform timing / Save,
stable insert_generation и 32 auditioned alternating drags PASS; существующие
Undo/Redo/Save/Open/audio/MIDI/RT-regressions PASS. Packaged J3 три раза exit 0
(96 дополнительных drag gestures суммарно). Software count UI 150% просмотрен,
равномерная линейка и кнопки без overlap; physical mixed-DPI/Omnisphere/ASIO
delayed-crash reproduction NOT RUN. READY WITH MANUAL CHECK; RT SAFE WITH MANUAL
CHECK для изменённых путей, root-cause crash investigation остаётся открытым.

Пакет `MR-Studio-0.2h-fix1-Toolbar-Tempo-MIDI-JUCE-ASIO-Windows-local` в Builds
папки чатов; прежние пакеты сохранены. EXE SHA256
`FE00D69DCDC49BE317B61D000BA060A251CC892BCD03D639C06F24D9CE4A1160`.
GitHub/source push/Actions не выполнялись; #70/#24/P4 не закрыты, 4g не начат.
Навыки определили полный state/lifetime regression scope без изменений RT DSP.

## Предыдущая поставка: 0.2h

Пользователь принял upd8 («Все тесты прошли»), поменял порядок на 4h → 4g и
разрешил «Поехали 0.2h делать». Реализованы метроном и precount, scope 4h / #70:

- Click/Count в toolbar и Transport → Metronome / count-in: отдельные playback/
  recording switches, Accent first beat, Level, count Off или 1–4 такта.
  По умолчанию Click/Count выключены, accent on, level 20%. Настройки общие:
  Preferences v7 читает v1–6 с Off; не dirty/Undo, project schema 13 прежняя.
- Prepared click tones и timing projections следуют tempo/meter/loop; после
  Master inserts/PDC, перед Master gain, в Master hardware outputs. Host click
  не добавляется в raw capture; акустическая/аппаратная петля требует наушников.
- Precount использует темп/размер в позиции старта, полные такты без отрицательных
  samples. Cursor удерживается, мониторинг работает, COUNT-IN показывает seconds.
  Запись начинается точно после отсчёта с исходной позиции, даже внутри callback;
  prefix samples/notes не входят в дубль. Stop/Pause отменяет без пустого WAV/clip.
  Count audible независимо от recording click; настройки заморожены до Stop.
- Shared Application/engine; RT tone/capture gating без allocations/locks/I/O/UI.
  Существующие SPSC controls и coherent state mailbox; stop/join перед prepare.
  Whole-graph producer рендерит click, mixed producer выключает его; device
  рендерит один раз. Take command/Undo/portable Save/Open и plugin instances сохранены.

Контракт: [METRONOME_PRECOUNT](../METRONOME_PRECOUNT.md); проверка:
[MRS_STAGE_4H_CHECKLIST](../MRS_STAGE_4H_CHECKLIST.md).
Source `1c954595786a19abc571e391fdf8b2b1e8f1fdfe`, локальная ветка
`mrs/0.1q-fix3-processing-local`. Cached configure/full Release PASS без compiler
warnings/errors; **114/114 CTest PASS (64.41 s)**. Packaged J3 три раза exit 0;
MIDI clips/live/recording, desktop config/recording/project_folders/clip_edits,
audio metronome/recording/multi_input, mixed/ahead equivalence PASS. Tone 44.1/48/96
kHz; accented meter/tempo/loop/seek, allocation 0; mid-callback 193-frame count
сохраняет только 63-frame input suffix. Combined audio/MIDI count/cancel/start0,
stable insert_generation, one Undo/Redo и Save/Open PASS. Software count UI
100%/150% просмотрены. В раннем промежуточном J3 был единичный exit 0xc0000374;
причина не установлена, позднее три последовательных и три packaged запуска PASS;
не объявлять доказанно исправленным. Детали разработки в контракте.
Пакет `MR-Studio-0.2h-Metronome-Precount-JUCE-ASIO-Windows-local` в папке чатов
`Builds`; прежние сохранены. EXE SHA256
`327F72581776C7558ED2845BC2614267617557E140F95F0C238280C28CE4CCB8`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Реальные ASIO audition,
record-start timing и physical mixed-DPI NOT RUN; следующая работа — пользовательская
проверка 0.2h. После приёмки по отдельному запросу — 0.2g внешний MIDI, затем
совместная Stage 4 проверка. #70/#24/P4 не закрыты, 4g не начат; GitHub/source
push/Actions и специальные installed TH-U/Nuro проверки не выполнялись.

## Предыдущая принятая поставка: 0.2f upd8

Upd7 принята пользователем 2026-10-08: «По всем предыдущим пунктам тесты пройдены».
Точные hardware settings и отдельная mixed-DPI matrix этой репликой не перечислены.
По пяти заключительным замечаниям реализована upd8:

- Arrange ruler font — тот же 13 px, что VST3 Browser. Mouse seek/drag только
  на верхней линейке тактов; sections/chords/клипы/пустые дорожки не делают seek.
- Split audio/MIDI оставляет выделенным только правый фрагмент.
- Edge hover — bracket cursor, зона 7 logical px. Trim выделяет один клип из
  группы; preview/commit совпадают, Escape отменяет, один Undo/Redo.
  Audio clamped к исходному файлу; MIDI расширяется за содержимое, влево до
  timeline zero. Shared MIDI empty-prefix trim сохраняет позиции notes/events.
- Audio Record показывает растущий красный клип с raw-input waveform. Fixed
  2048 peak bins/channel, adaptive width; disk worker считает extrema, UI получает
  versioned atomic snapshot с bounded retries. Callback capture не изменён,
  новых callback allocations/locks/I/O нет; Stop немедленно очищает preview.
- Project schema 13 прежняя, trim/takes используют существующие commands,
  Undo и persistence; временные preview/cursor/selection не сохраняются.

Контракт: [ARRANGE_FINAL_UPD8](../ARRANGE_FINAL_UPD8.md).
Source `6c91cffdf375cd0ec8b1e44857f990a5b8ac70c3`, только локально, ветка
`mrs/0.1q-fix3-processing-local`. Cached configure и full Release PASS;
**113/113 CTest PASS (63.30 s)**. Packaged J3 exit 0, MIDI clips/live/recording,
desktop waveform/recording/project_folders/clip_edits, audio recording/multi_input
PASS. Проверены bounds/selection/ruler-only, Escape/Undo/Redo, Save/Open,
mono/stereo preview, repeated takes, adaptive peak reduction и raw samples.
Software Arrange/audio recording previews 100%/150% просмотрены.
Пакет `MR-Studio-0.2f-upd8-Final-Arrange-JUCE-ASIO-Windows-local` в папке чатов
`Builds`; прежние пакеты сохранены. EXE SHA256
`B97492492098364601CAB3049B01D58DA35EDE16C960A5CE07022B05E7A17258`.
Пользователь принял upd8 2026-10-08: «Все тесты прошли. Что у нас дальше?».
Пять заключительных исправлений приняты; точные ASIO settings и отдельная
mixed-DPI matrix не перечислены. Предыдущая отметка manual checks относилась
к моменту выпуска. По решению пользователя 2026-10-08 порядок изменён:
сначала 4h / 0.2h — метроном и precount audio/MIDI, затем 4g / 0.2g — внешний
MIDI (Windows output ports/channels, live thru, playback Notes/CC/PC,
routes, missing/reconnect/panic и защита от feedback). Названия, номера и scope
сохранены; итоговая проверка Stage 4 после обоих подэтапов, последним идёт 4g.
Начать реализацию только по запросу пользователя.
GitHub/source push/Actions не выполнялись; Stage 4/P4/#68/#24 не закрываются,
0.2g/0.2h не начаты. Выпущенный пакет и manifest не изменены после приёмки.

## Предыдущая принятая поставка: 0.2f upd7

Пользователь принял upd6 2026-10-08: «Теперь все в порядке. Тест пройден!».
Не расширять эту приёмку на весь Stage 4/P4 или отдельную hardware/DPI matrix.

По пяти следующим замечаниям реализована upd7:

- Arrange плейхэд проходит через chords/sections до линейки тактов, сохраняя
  общую 60 Hz интерполяцию и mouse passthrough.
- Seek округляет к ближайшей шестнадцатой, не вниз. Тактовые линии захватываются
  в пределах ±6 logical px, с учётом scroll/zoom/tempo/meter. Snap off вне захвата
  использует точный sample под кликом. Clip drag/drop/trim snap не менялся.
- Audio Record без WAV chooser: уникальные Take-ID WAV в Media текущего проекта.
  Первая запись несохранённой сессии создаёт Untitled-ID проект в
  Documents/MR Studio/Projects, включая импортированные media. MIDI-only
  не создаёт WAV/автопроект. Raw recording, один Undo/Redo и portable Save/Open
  прежние; дубли сохраняются на диске после Undo, clips остаются dirty до Save.
- Audio/MIDI fill одинаково 0.72, остальные детали клипа не становятся прозрачнее.
- Arrange/Audio editor waveform используют общие antialiased envelope paths,
  cached extrema с bounded smoothstep на high zoom. PCM/DSP не меняется;
  это envelope, не sample-accurate oscilloscope. Worker peak cache остаётся
  ограниченным 65536 total base bins, начальная детализация 16 frames.
- Перенос media при Save сохраняет подготовленные processors/editors; если
  offline graph ещё не был подготовлен, bootstrap остаётся прежним.

Контракт: [ARRANGE_AUDIO_UPD7](../ARRANGE_AUDIO_UPD7.md).
Source `97d08be8ccc2393043143ecdf61f442354ff15e2`, локальная ветка
`mrs/0.1q-fix3-processing-local`. Cached configure/full Release PASS, без compiler
warnings/errors. Финальный **113/113 CTest PASS (62.85 s)**. Packaged J3 exit 0;
MIDI clips/live/recording и desktop waveform/recording/project_folders PASS.
Software arrangement 1200×700 и high-zoom waveform 100%/150% просмотрены.
Пакет `MR-Studio-0.2f-upd7-Arrange-Audio-JUCE-ASIO-Windows-local` в папке чатов
`Builds`; прежние пакеты сохранены. EXE SHA256
`826C488C03F0B1B6F342D2B024658ABE402F5C16337AA34EB75586CC8A09CFB6`, совпадает с
Release; SHA256 manifest 96 файлов проверен. Логи и fresh previews внутри пакета.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. ASIO hardware/manual
acceptance при выпуске upd7 была NOT RUN; впоследствии пользователь принял пять
пунктов (см. актуальную запись upd8 выше). Исторический checklist выпуска:
[MRS_STAGE_4F_CHECKLIST](../MRS_STAGE_4F_CHECKLIST.md) на пользовательской системе:
cursor/ruler, seek, реальные audio takes в Media, прозрачность и waveform zoom.
GitHub/source push/Actions не выполнялись; #68/#24/P4 не закрываются, 0.2g/0.2h
не начаты. Визуальный peak envelope не обещает sample-accurate editing.

## Предыдущая принятая поставка: 0.2f upd6

## 0.2f upd6 — 2026-10-08

Пользователь принял upd5: «Тесты прошли успешно». Точные hardware settings,
длительности и отдельная mixed-DPI matrix этой репликой не перечислены.
По трём новым требованиям выпущена **0.2f upd6**:

- При запуске Project home вместо demo: фоновый поиск `.mrsproject` рекурсивно
  в Windows `Документы/MR Studio`, только файлы; папки/symlinks не отображаются.
  ЛКМ/Enter открывают проект. Сортировка по изменению, поиск по имени/относительному
  пути, New/Open/Refresh; одинаковые имена различаются путями. Missing/corrupt
  оставляют текущий проект и показывают ошибку. Начальный home не просит сохранять
  невидимый пустой проект. Saved audio reconnect выполняется после New/Open.
  File → Project home сохраняет текущую сессию и ставит Pause; Continue current
  project возвращает её. Прежняя защита несохранённых изменений при замене/закрытии.
- Плейхэды Arrange (включая запись), piano roll и controller lane обновляются
  отдельными прозрачными компонентами на 60 Hz, только узкие old/new полосы.
  Общая message-thread интерполяция к existing coherent engine snapshot,
  fractional samples/ticks/pixels. Обычно около одного UI frame визуального отставания,
  окно сглаживания не больше 50 ms, без ухода дальше последнего observed sample.
  Stop/Pause/backward/loop/large seek/reset немедленно re-anchor; малый forward seek
  сходится за короткое UI-окно. Tempo segments учитываются с дробными ticks.
  30 Hz meters/content polling и MIDI preview rate сохранены; callback/DSP/recording
  timestamps/audio buffering/Undo/schema 13 не менялись. UI state transient.

Source `89987a78a35694585a7f6ed84c95b759d28d7210`, локальная ветка
`mrs/0.1q-fix3-processing-local`. Cached configure/full Release PASS без warnings.
**113/113 CTest PASS (72.65 s)**; после финальных UI дополнений **J2/J3 2/2 PASS
(7.84 s)**. Packaged J3 и recording/clips/live PASS. Home nested/case-insensitive/
duplicate/filter/async/open/resume/new/missing/corrupt checks PASS. Fractional cursor
paint/old stripe clear/mouse passthrough и 44.1/48/96 kHz, 128/256/2048 frames,
60 Hz bounded monotonic display, stalls/Stop/Pause/Seek/loop/tempo changes PASS.
Software home 1200×700/1400×850 100%/150% и MIDI 150% просмотрены.
Пакет `MR-Studio-0.2f-upd6-Home-JUCE-ASIO-Windows-local` в папке чатов `Builds`;
EXE SHA256 `AFD9B7B9EAB87F6BE4C609AB95F4BA035ED4B688AEBB209FBDC2A71D1339FB44`.
Контракт: [PROJECT_HOME](../PROJECT_HOME.md); [ручная проверка](../MRS_STAGE_4F_CHECKLIST.md).
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физическая ASIO
плавность записи/редактора и mixed DPI ожидают пользователя. #68/#24/P4 scope
этой поставкой не закрывается. GitHub/source push/Actions не выполнялись.
Следующий шаг: проверить новый стартовый экран и плейхэды на пользовательской
системе; 0.2g/0.2h не начаты, автоматически не начинать.

## Предыдущая принятая поставка: 0.2f upd5

## 0.2f upd5 — 2026-10-07

По четырём новым замечаниям выпущена **0.2f upd5**.
Mouse-up audition имеет хвост 70 ms: около 35 ms удержания и 35 ms линейного fade out,
затем sample-offset Note Off. Короткая огибающая действует только на paused/stopped
isolated audition track; live held notes, CC64/66, недавний MIDI input (200 ms),
активный audio monitor и Play используют обычный Note Off без изменения live signal.
Новый audition/hardware input/Play отменяет хвост, pending old off идёт перед new on;
retune/active gesture cancellation сохраняют немедленный Note Off.
RT: fixed-capacity queue + arrays/bitsets, callback prepares per-track state,
единственный channel worker продвигает envelope и joins до следующего callback.
Prepare/Stop/panic очищают состояние; нет новых host allocation/locks/I/O в callback.
Preview tail не записывается в take/проект, схема 13 не меняется.
Допуск бокового движения vertical note drag увеличен с 3 до 12 logical px;
ниже порога off-grid start сохраняется. Horizontal drag по-прежнему snap anchor start.
Высоты docked editor и Mix независимы: juce-view.json editorHeight (default/min 500)
и mixerHeight (default 365/min 260); общая доступная высота ограничивается окном.
Старые view settings без editorHeight читаются с default 500. Floating size отдельно.
F2 toggle Edit, F3 toggle Mix, F5 toggle browser; работают в основном окне и
через MIDI/audio editor, без modified keys/modal interception; обычные text/transport shortcut guards сохранены.
Source `7a35904d0260ec807cced5b2792966c4c6d58804`, ветка `mrs/0.1q-fix3-processing-local`, код локально.
Full Release PASS без warnings; **113/113 CTest PASS (61.41 s)**;
packaged recording/clips/live/J3 PASS. Tail fade/zero/retrigger/live protection на
44.1/48/96 kHz, 256-frame callbacks/128-frame chunks, 1/4 workers и checked host
callback allocations 0 PASS. 8 px vertical jitter/time preservation, independent
panel resize/settings roundtrip и F2/F3/F5 toggles PASS. Software snapshots 100%/150%
просмотрены. Пользовательская проверка звучания на Komplete/ASIO/VST3 и mixed DPI ожидается.
Пакет `MR-Studio-0.2f-upd5-Editor-JUCE-ASIO-Windows-local`, EXE SHA256 `C766772EA358B07678AFB55F14C34842FBD0AB9FDAB1DE5B0794993D87E4CA29`.
Следующий шаг: проверить четыре изменения на пользовательской системе; 0.2g/0.2h не начаты.

## Предыдущая поставка: 0.2f upd4

## 0.2f upd4 — 2026-10-07

Пользователь принял оформление 0.2f upd3 fix1: «Все отлично!».
По семи новым требованиям выпущена **0.2f upd4**:
- MIDI notes alpha 72%, ghost 58%: сетка слегка видна, рамки отсутствуют.
- Horizontal note drag привязывает глобальное начало якорной ноты к выбранной
  сетке (Snap off сохраняет свободное перемещение). Общий сдвиг группы сохраняет
  интервалы; vertical drag с горизонтальным отклонением менее 3 logical px не
  меняет start, даже у записанной ноты вне сетки. Trim/velocity gestures сохранены.
- Up/Down в piano roll меняют pitch выбранных нот на один полутон, без изменения
  времени/длины; границы 0..127 отклоняют весь шаг группы. Один shared Undo на шаг.
- Full screen / Restore и F11 в MIDI editor. Тот же component/selection/zoom;
  attached editor временно отделяется и при Restore возвращается в панель.
  Переход fullscreen сам по себе не сохраняет временный Detach в settings.
  Fullscreen и selection — временный UI state, не поля проекта.
- Горизонтальные grid lines controller lane убраны; числовые подписи сохранены.
- ЛКМ + drag в пустой lane выделяет точки текущего вида/канала/CC рамкой;
  Ctrl/Shift дополняют выбор. Selected points белые. ПКМ не начинает жест.
- Drag выбранной точки переносит выделенную группу по времени/значению с общим
  ограничением границ clip/value и сохранением расстояний. Preview не меняет
  проект; release — один SetMidiEvents/Undo. Escape/focus loss отменяют gesture;
  stale clip отменяет commit. Delete удаляет выбранную группу одной command.
Shared model/commands/serialization/playback существующие; schema 13 неизменна.
Новых изменений callback/DSP/transport нет; editing требует Pause/Stop.
Source `20f78183b9cd77678c35deb42dd8d24dd6233bb6`, локальная ветка `mrs/0.1q-fix3-processing-local`.
Release GUI build PASS без warnings; **113/113 CTest PASS (61.56 s)**;
packaged recording/clips/live/J3 PASS. Регрессии off-grid vertical/horizontal snap,
Up/Down, CC group preview/common time shift/one Undo/Escape, floating full screen
и attached → fullscreen → attached PASS. Software Arrange/editor/controllers
100%/150% просмотрены. Physical mixed DPI и пользовательская приёмка upd4 ожидаются.
Пакет `MR-Studio-0.2f-upd4-Editor-JUCE-ASIO-Windows-local`, EXE SHA256 `B266F149AA523B27FF17FF1280AB2C445765B31214C5CB64DA24756E693C3959`.
Следующий шаг: проверить новые семь взаимодействий; 0.2g/0.2h не начаты.

## Предыдущая поставка: 0.2f upd3 fix1

## 0.2f upd3 fix1 — 2026-10-07

Пользователь подтвердил 0.2f upd3: «Теперь запись работает как надо», перетаскивание
работает и перемещение клипов отображается в редакторе. Это приёмка указанных
исправлений, а не всех remaining hardware checklist пунктов Stage 4.
Косметический фикс: MIDI clip fill в Arrange 72%, ноты piano roll 78% (ghost 65%);
сетка слегка просвечивает. Выбранные клипы/активные ноты рисуются последними,
полупрозрачная заливка показывает нижний MIDI-клип/ноты при перекрытии.
Окантовка audio/MIDI/recording preview clips уменьшена с 1.5 до 0.75 logical px,
углы скруглены на 2 logical px. MIDI notes без окантовки, velocity marker сохранён.
Hit bounds, selection/drag/trim, Undo, transport, DSP и schema 13 не менялись.
Source `2c573f0ea85b1277792078b383d906b3bba23987` локально; пакет `MR-Studio-0.2f-upd3-fix1-UI-JUCE-ASIO-Windows-local`;
EXE SHA256 `1E4701808151314524515D1EE5471DA59548A2525E20441C118C8A5EF1AB3B8A`. Release GUI build PASS без warnings; packaged recording/clips/live/J3 PASS.
Software previews Arrange/piano roll 100%/150% просмотрены. Полный 113-test CTest
для косметического фикса не повторялся: 113/113 относится к базовой upd3.
Physical mixed DPI и визуальная пользовательская приёмка fix1 ожидаются.
Следующий шаг — пользователь проверяет оформление; 0.2g/0.2h не начаты.

## Предыдущая поставка: 0.2f upd3

По четырём замечаниям пользователя выпущена **0.2f upd3**.
Запись MIDI больше не запрещена до конца существующих клипов: новый take добавляется
в выбранной позиции, включая перекрытие; прежние клипы не удаляются. Loop recording
ещё не реализована: loop off, R, выбранный MIDI input и работающий hardware audio
остаются требованиями. Ошибки запуска/остановки отображаются в строке статуса.
Start/Stop записи сохраняют существующие track/master PreparedGraph/VST3 instances,
когда chains и device config неизменны. Не выполняется новый probe/load/restore плагинов.
После stop/join callbacks материализуется take, выполняется один command/Undo и
обновляется playback graph с теми же processors. Clip-only Undo/Redo и grouped move
также сохраняют runtime instances; plugin/track/device edits используют прежний rebuild.
Не требуется сериализовать plugin state ради clip-only recording commit; Save/capture
по-прежнему читает состояние действующих processors. Schema 13 без изменений.
Desktop очищает красный recording preview синхронно после завершения Stop, вместо
ожидания timer; лишняя пауза загрузки VST3 устранена. Stop/join driver, audio file flush
и обработка большого take остаются необходимы; нулевая hardware latency не обещается.
ЛКМ в пустой Arrange-области + drag выделяет audio/MIDI-клипы рамкой. ПКМ рамку
больше не создаёт. Обычный клик без drag сохраняет seek по пустой области.
ЛКМ на выбранном клипе сохраняет группу и перемещает её целиком. Preview локальный;
release выполняет Application::move_clips одной атомарной command/Undo. Общий
sample shift сохраняет относительные позиции, MIDI ticks вычисляются Timeline;
snap по якорному клипу, общая граница timeline, track shift сохраняет относительные
номера дорожек. Несовместимые destination tracks отклоняются атомарно. Escape отменяет.
Source `6a38c765a2c884271a45628862664fa4f371a446`, только локально. Release без warnings; **113/113 CTest PASS (60.73 s)**;
packaged recording/clips/live/J3 PASS. Десять повторных Record/Stop в одной позиции
поверх существующих clips, stable insert_generation Start/Stop/Undo/Redo, immediate
preview clear PASS. MIDI/audio recording, monitor on/off, 4 workers и callback allocation 0 PASS.
Mixed group selection/move preview/commit, relative shift, one Undo/Redo, Escape и
atomic negative-start rejection PASS. Software previews selection/move 100%/150% просмотрены.
Физическая Komplete Kontrol/ASIO/VST3 и Windows mixed DPI для upd3 NOT RUN.
Пользователь подтвердил звук при переносе в upd1 fix1; в upd2 сообщил повторные отказы
Record и задержку red → blue. Эти аппаратные жалобы остаются на ручной проверке,
не объявляются полностью закрытыми лишь по offline regression PASS.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd3-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `73C70F432A099AF12B854D484E4F418A9A6A8833240A2A2B597BDB52EB479359`. #68/#24/#65 открыты до соответствующей приёмки.
Следующий шаг: пользователь проверяет повторную запись в выбранной позиции,
Stop → blue clip, ЛКМ selection и grouped move. 0.2g/0.2h не начаты.

## Предыдущая поставка: 0.2f upd2

По семи новым требованиям пользователя выпущена **0.2f upd2**.
Убраны отдельные Play/Pause/Stop редактора; общий транспорт и Space сохранены.
ЛКМ по области соседнего MIDI-клипа в piano roll делает его активным в том же
редакторе. ЛКМ по audio/MIDI-клипу в Arrange переключает открытый редактор;
при закрытом редакторе только выбирает клип. Глобальная шкала, темные промежутки
и затемнённые неактивные MIDI-клипы сохранены.
Controllers open/closed — общая настройка MIDI-редактора; Attach/Detach — общая
настройка audio/MIDI-редактора. Оба флага сохраняются в локальный juce-view.json,
применяются ко всем клипам и после перезапуска. Старые settings без новых полей
совместимы: Controllers open, редактор detached. Project schema 13 не меняется.
Повторный Edit закрывает floating или attached редактор, следующий открывает его
с запомненным размещением. Смена клипа не создаёт второй параллельный редактор.
Удержание ПКМ и drag в Arrange выделяет пересекающиеся audio/MIDI-клипы рамкой;
не меняет transport/model. Delete/Backspace удаляет всю выбранную группу одной
shared Application command, один Undo восстанавливает клипы и IDs.
Record теперь может запускаться во время Play: device stop/join, фактическая позиция,
quiescent Pause, подготовка recorder/graph, Play и device start. Структурные изменения
по-прежнему запрещены при playback/recording. MIDI-only и совместная audio/MIDI
запись проверены из Stop и Play, с/без monitor, 4 processing workers, callback allocation 0.
Статус RECORDING виден текстом; ошибка автоматической остановки выводится в строку
статуса. Arm, MIDI input, hardware audio, loop off и linear append после существующих
MIDI-клипов остаются необходимы. Изменение лечит воспроизведённый отказ Play → Record;
причина конкретного аппаратного случая на скриншоте без runtime диагностики не доказана.
Пользователь по 0.2f upd1 fix1 подтвердил изменение звука при переносе и «всё остальное
работает», но сообщил повторный отказ записи при включённой R. Общая hardware
приёмка записи и текущих новых требований не объявляется пройденной.
Source `b2975843459c20685e9d20364787ee3159be386b`, только локально. Release без warnings; **113/113 CTest PASS (61.22 s)**;
packaged recording/clips/live/J3 PASS. UI регрессии: shared settings roundtrip,
ghost/Arrange activation, Edit close/reopen attached/detached, mixed clip rectangle,
Delete group/one Undo, отсутствие отдельных transport buttons PASS.
Software previews 100%/150% просмотрены. Физическая Komplete/MIDI/ASIO/VST3,
перезапуск пользовательского приложения и Windows mixed DPI для upd2 NOT RUN.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd2-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `31D3B7114017C018DF9516640EBAF5E3C1829E5D8404BB9FED248B3EC188D161`. #68/#24 открыты до приёмки.
Следующий шаг: пользователь проверяет семь изменений, особенно R → Play → Record
и R → Record из Stop. 0.2g/0.2h не начаты; остальные доработки после Stage 4.

## Предыдущая поставка: 0.2f upd1 fix1

По пяти замечаниям пользователя выпущена **0.2f upd1 fix1**.
Record сохраняет красный цвет при фокусе; после отказа UI показывает фактический
статус записи. На скриншоте обе R выключены: перед Record включить R нужной дорожки.
Автоматического Arm нет. Проверены отказ без Arm и успешная запись с Arm.
Piano roll использует глобальные ticks проекта. Play/Pause/Stop в редакторе и
Space управляют общим транспортом; белые плейхэды piano roll/контроллеров берут
позицию engine. Нажатие на линейку piano roll выполняет общий seek.
Все MIDI-клипы той же дорожки видны на общей шкале: активный светлее, остальные
затемнены и доступны только для просмотра; промежутки тёмные. Source offsets
сохраняются для нот/контроллеров и точных форм. Zoom/scroll обеих областей общий.
ПКМ больше не удаляет ноты. Двойной левый клик/Delete и общий Undo сохранены.
При вертикальном переносе ноты audition выключает старую высоту и включает новую;
отпускание мыши, Escape, скрытие и потеря фокуса выключают ноту. Один Undo на жест.
Source `68f1dc5968e89254213cfe48e899b877e1fadd1f`, только локально; schema 13 без изменений. Новые callback
allocation/locks/I/O не добавлены; используется существующая очередь audition.
Release без warnings; **113/113 CTest PASS (60.06 s)**; packaged recording/clips/live/J3 PASS.
Регрессии: отказ Record без Arm, real state UI, global timeline/Play/Pause,
ghost clips read-only, ПКМ без mutation, drag retune одной voice, Escape release PASS.
Software previews 100%/150% просмотрены. Физическая MIDI/ASIO/VST3 и mixed DPI
для fix1 NOT RUN; #68/#24 остаются открыты до пользовательской приёмки.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd1-fix1-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `D67EE9F15146E770F7A9A307FBC5F9A451854C36E1F7F31E357DA54C4D1C2090`.
0.2g/0.2h не начаты. Следующий шаг: пользователь проверяет пять исправлений,
затем продолжение Stage 4. Остальные доработки по прежней договорённости отложены.

## Предыдущая поставка: 0.2f upd1

По семи прямым запросам пользователя 2026-10-07 выполнена 0.2f upd1.
Controller lane под piano roll; CC/bend/pressure связаны прямыми отрезками,
Program и CC64/65/66 — ступенями. Горизонтальная шкала общая с piano roll.
Ctrl+wheel vertical zoom, Ctrl+Shift+wheel horizontal zoom, Shift+wheel scroll,
как в Arrange. Double left click удаляет ноту; empty grid создаёт, общий Undo.
MIDI take с удерживаемыми нотами виден до Stop: transient immutable-prefix preview,
atomic publication; UI conversion, callback allocation 0 проверен.
Edit открывает выбранный audio/MIDI clip; audio waveform и trim через shared Undo.
Attach/Detach перемещает тот же редактор в нижнюю панель вместо Mix и обратно;
selection/zoom сохранены. Mix/Arrange скрывают, Edit возвращает. Divider resize.
Controllers/Note details переключают нижнюю область. Schema 13, save/load,
Undo/recording commit и дискретный MIDI playback сохранены.
Source `b8d14d55c1e0e3dec0be1b43b7925e1bad958524`, локально; ветка `mrs/0.1q-fix3-processing-local`.
Release без warnings, **113/113 CTest PASS (60.63 s)**; packaged recording/clips/live/J3 PASS.
Concurrent preview/capture, open notes, отсутствие commit до Stop, zero callback allocations,
double-click Undo, zoom modifiers, audio trim Undo, MIDI/audio Attach/Detach и Mix/Edit PASS.
Software previews 100%/150% просмотрены: piano roll, CC1 линии, dock, waveform, recording.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-upd1-Editor-JUCE-ASIO-Windows-local`; EXE SHA256 `33CF213400C7297744A3628CB98CCBE712F63D4AA23B866698EFAAB2718B4E9E`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические
MIDI/ASIO/VST3 и Windows mixed DPI для upd1 NOT RUN. #68/#24 открыты до приёмки.
[Workflow](../EDITOR_WORKFLOW.md), [checklist](../MRS_STAGE_4F_CHECKLIST.md).
Общее откладывание остальных доработок сохраняется; эти семь запрошены отдельно.
0.2g/0.2h не начаты.

## Предыдущая поставка: 0.2f / Stage 4f

Piano roll → Controllers...: CC/sustain, pitch bend, channel/poly pressure и
Program Change. Выбор канала/CC/note, Add/Apply/Delete, графическая lane со snap,
перемещение позиции/значения одним Undo, Escape отменяет жест. Notes/IDs сохраняются;
общий SetMidiEvents, Undo/Redo, save/reopen; schema 13 без изменений.
Controller chase при seek/resume/loop; на границе клипа sustain off/bend center,
события начала следующего клипа применяются после сброса независимо от порядка clips.
Запись резервирует завершающие события: базовый raw limit 8144, worst case 8192 compiled.
Source `e8d714e41a9e0a46f849f0bbbcda30dfe7db12c8`, только локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings; **113/113 CTest PASS (59.31 s)**.
Packaged recording/clips/live/J3 PASS; controller previews 100%/150% просмотрены.
Проверены все типы/диапазоны, stable IDs, invalid input atomicity, Undo/serialization,
seek/loop/boundary ordering, callback allocation 0, полный recording event budget;
GUI add/apply/delete/drag/single Undo/Escape cancel.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2f-Controllers-JUCE-ASIO-Windows-local`; EXE SHA256 `9A3827B6BCF358E6946ABFDA39E69AA1D6081D904DDFA2321D239C8457A69142`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK.
Физические MIDI/ASIO/VST3 и Windows mixed DPI для 0.2f NOT RUN.
[Контракт](../MIDI_CONTROLLERS.md), [checklist](../MRS_STAGE_4F_CHECKLIST.md).
#68 открыт до пользовательской приёмки; #24 остаётся открыт. 0.2g/0.2h не начаты.
0.2e принята: «работает, приступай к 0.2f», #67 закрыт.
Дополнительные доработки после реализации всех пунктов Stage 4.

## Предыдущая принятая поставка: 0.2e / Stage 4e

Piano roll → Musical edit...: Quantize (grid/strength, start/length separately),
Transpose группы, Velocity и Length Set/Add/Scale. Scope Selected / Whole clip
явный; empty selection сообщает ошибку. Интервалы transpose сохраняются, pitch
0–127, velocity 1–127, length минимум 1 tick; clip/trim края учитываются.
Одна Apply — один общий Undo; no-op без history. ID/channel/controller events
сохраняются. EditMidiNotes/transform_midi_notes общие для Studio/Live; UI — Studio.
Schema 13 и audio callback не изменены. Editing после Pause/Stop.
Source `abd77b09f3edb2045873cf33e19d291095eda26b`, только локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings, **113/113 CTest PASS (58.96 s)**.
Packaged midi recording/clips/live/J3 PASS; musical dialog 100%/150% просмотрен.
Тесты: strength 0/50/100, targets, selection/isolation, transpose limits/intervals,
velocity/duration clamp, trim edges, IDs/controllers, Undo/Redo/serialization,
GUI transpose/whole-clip scope/invalid input/empty selection/no-op.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2e-Musical-edit-JUCE-ASIO-Windows-local`; EXE SHA256 `5371EFAA368955563AFCC84184E95CE573F7C8E2C720CB6178C0D891E607FE13`.
FEATURE READY WITH MANUAL CHECK. Физические ASIO/VST3 и Windows mixed DPI
для 0.2e не запускались. [Контракт](../MIDI_MUSICAL_EDIT.md), [checklist](../MRS_STAGE_4E_CHECKLIST.md).
#67 закрыт: пользователь «работает, приступай к 0.2f» подтвердил приёмку 0.2e и запросил 0.2f.
Дополнительные доработки отложены до реализации всех пунктов Stage 4.

## Принята 0.2d; порядок продолжения

Пользователь: «Да, все работает». 0.2d принята, #66 закрыт. Первоначальный запрос
0.2f исправлен: «0.2e начинаем, я перепутал»; 0.2f затем реализована по новому запросу после приёмки 0.2e. Пользователь
указал: «Все доработки будем делать когда все пункты stage 4 будут реализованы».
После приёмки 0.2e продолжать базовые 0.2f/0.2g/0.2h, без дополнительных upd.
Точные аппаратные режимы при приёмке 0.2d не перечислены; новых измерений нет.

## Предыдущая принятая поставка: 0.2d / Stage 4d

Piano roll: клавиатура/ruler, zoom/scroll, snap, создание/удаление и групповое
выделение/перемещение/длина/velocity, copy/paste, audition. Один жест — один
общий Undo; stable IDs/проект/события контроллеров сохраняются. Точная note form
остаётся. Editing и audition после Pause/Stop. Отдельная bounded UI SPSC для
preview MIDI; driver bridge не получает второго producer, audition не записывается.
Семь transport hit regions одинаковые: 42×30 logical px, gap 3, icon-only.
Source `92ac0f2fceea2a79bdd7d77873a199ee87fe7c45`, локально, ветка `mrs/0.1q-fix3-processing-local`.
Full Release PASS без warnings; **113/113 CTest PASS, 59.81 s**.
Packaged recording/clips/live/J3 PASS. Piano roll/Arrange/Mix software previews
100%/150% просмотрены. Checked callback host allocation 0; Workers 1/2/4,
stale generation, invalid track, audition overflow/panic и exclusion из recording.
GUI: snapped move/group single Undo, fresh paste IDs, delete, Escape, resize,
velocity и pitch/clip edge clamp. Схема 13 без изменений, читает 1–12.
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2d-Piano-roll-JUCE-ASIO-Windows-local`; EXE SHA256 `BED650A86669056C81F1F5B471BE257D10BC925BBA1DAB6C917751C52202582D`.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические ASIO/VST3
и Windows mixed DPI для 0.2d не запускались. [Пользовательская проверка](../MRS_STAGE_4D_CHECKLIST.md),
[архитектура/ограничения](../PIANO_ROLL.md). #66 закрыт после приёмки «Да, все работает»; 0.2e реализована следующим запросом.

## Пользовательская приёмка 0.2c fix1

Пользователь: «Вроде все работает» и «Поехали 0.2d». На присланном скриншоте
два MIDI take, connected Komplete Kontrol и Komplete Audio ASIO. Визуальный фикс
и показанный рабочий сценарий приняты; равные transport bounds включены в 0.2d.
Отдельные sustain/bend, joint recording, unplug и полный 4c hardware checklist
не перечислены; #65 остаётся открытым для этих проверок. #24/#54/#16 не закрываются.

## Предыдущая поставка: 0.2c fix1

По запросу пользователя: transport buttons рисуют только SVG без фона/рамки;
области нажатия, layout, tooltips и accessibility names сохранены. Hover/press,
keyboard focus и toggle отражаются цветом glyph. Названия треков/каналов, dB,
кнопки и insert names в Strip используют общий с VST3 tree системный шрифт 13 px
(ранее Noto Sans 13 pt). Остальная типографика не изменена.
Source `d3da9f739915103e9952a641c154b167cf436cbf`, только локально. Release GUI build PASS без предупреждений;
packaged J3 PASS (MIDI 4a/4b/4c, transport/Arm/Record/Stop/Undo/Redo).
Software Arrange/Mix previews 100%/150% просмотрены. Полный CTest не повторялся:
113/113 PASS относится к базовой 0.2c. Физические MIDI/ASIO и Windows mixed DPI
для косметического фикса не запускались; визуальная приёмка впоследствии получена (см. выше).
Пакет `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2c-fix1-UI-JUCE-ASIO-Windows-local`; EXE SHA256 `DA313BC3E7A1E176B26DC60AD16ACC93BC67947C3E9A4A671E46563A53CAC236`.
0.2c/#65 ещё не принята целиком; 4d не начат, schema 13 и engine без изменений.

## База фикса: 0.2c / Stage 4c

По прямому запросу пользователя после приёмки 0.2b реализованы MIDI recording
и лёгкое обновление transport SVG. 0.2c пока не принята пользователем; #65/#24 открыты.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2c-MIDI-recording-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`; scanner рядом. Предыдущие пакеты сохранены.
- EXE SHA256: `E168C92CF6D1E826C9B279882F094D7F2B77AB3A12B9E5C705F0C4C27D18CE24`.
- Source `f5247ede6775657f6067109047b7786af2f849b8`, только локально,
  ветка `mrs/0.1q-fix3-processing-local`. Принятая компактная UI-база сохранена.
- Instrument Arm R, live capture с независимым monitor, notes/velocity/channel,
  CC/sustain/pitch bend/program/pressure. WinMM timestamp → transport samples →
  PPQ ticks по общей tempo map. Открытые ноты/педаль закрываются при завершении.
  Общая audio/MIDI сессия — один Undo; save/reopen и контроллеры поддерживаются.
- Линейная запись: новая instrument track либо курсор после всех её MIDI clips;
  Loop/overdub/punch не включены. Только MIDI — Record без WAV chooser, совместная
  запись выбирает WAV для audio. Offline clock не записывает. Seek/loop/structural
  edits/save/device changes во время записи запрещены. UI close/project change
  завершает запись до предложения Save. Unsaved MIDI не имеет crash journal.
- Core schema **13**, читает 1–12; старые builds не читают 13. Использовать копии
  проектов. Event IDs/trim/split/duplicate/Undo сохранены; clip controller chase
  и boundary release добавлены. Controller lanes — 4f, piano roll — 4d.
- Семь transport buttons используют предоставленные SVG из `assets/ui/icons/transport`,
  встроенные в BinaryData. Размер glyph 20 logical px, прежние hit regions/layout,
  tooltips/accessibility names сохранены. Остальные SVG не включают новые функции.
- Cached configure/full Release PASS; финальные **113/113 CTest, 58.60 s**.
  Packaged midi_recording/midi_clips/midi_live PASS; hidden J3 exit 0.
  Software previews 100%/150% просмотрены. В проверенных RT callbacks host allocation 0.
  Проверены Workers 1/2/4 + mixed Process Buffer, input loss/overflow, timestamp
  clamp, controller chase, live event order, raw audio и сохранение MIDI.
- FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. Физические A49/ASIO,
  SWAM и Windows mixed DPI не запускались в этом чате. Timestamp arithmetic измерен
  synthetic-тестом (100 ms/48 kHz → 4800 samples); WinMM jitter/driver latency не
  калиброваны. Подробнее: [MIDI_RECORDING](../MIDI_RECORDING.md).
- Следующий шаг: [checklist 0.2c](../MRS_STAGE_4C_CHECKLIST.md): Arm → Record → игра →
  Stop → MIDI take → Play, sustain/bend, Undo/save/open, joint audio/MIDI и unplug.
  4d не начинать автоматически. Chord/Arranger — Stage 5; metronome/precount — 4h.
  P4/#54/#16 и отдельная multi-instance VSTi идея остаются самостоятельными задачами.

## Предыдущая принятая поставка: 0.2b / Stage 4b

По запросу «Поехали дальше, работаем над 0.2b» реализованы MIDI-клипы и playback.
Пользователь принял 0.2b: «Тест пройден, идем дальше. Работаем над 0.2c». #64 закрыт; #24 открыт. Точные аппаратные режимы не перечислены. 4c реализован по следующему запросу; текущая поставка описана выше.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2b-MIDI-clips-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`; scanner рядом. Прежние версии сохранены.
- EXE SHA256: `2418BB4F9D31E5B816EB97327820007578BC05D61B7B89D4C6B0943486A219F6`.
- Source `1c26828606c19d1f298417f3d1edf7821b7e4a67`, текущая локальная ветка
  `mrs/0.1q-fix3-processing-local`. Включает принятую UI-базу 0.2a upd1 fix1.
- Clip/note IDs, PPQ 960, pitch/start/length/velocity/channel; создание и простой
  note form, move/trim/split/duplicate/delete, Undo/Redo, Save/Save As/reopen.
  Общая tempo map, tick-сетка и Transport → Project tempo. Это минимальный ввод
  нот; полный piano roll — 4d, MIDI recording — 4c.
- Playback: block-relative offsets, chasing после seek/resume, Pause/Stop/loop,
  overlap policy и bounded overflow. MIDI clips остаются device-owned с Workers
  и Process Buffer; перед заменой графа обработчики останавливаются/присоединяются.
  В проверенных новых callback paths host allocation 0. RT SAFE WITH MANUAL CHECK.
- Core schema **12**, читает 1–11 с пустым MIDI payload. Старые сборки не читают
  12: работать на копиях проектов. Audio asset persistence пропускает MIDI clips.
- Cached configure/full Release PASS. Финальные **112/112 CTest, 58.30 s**.
  Packaged `midi_clips`, `midi_live`, hidden `--j3-smoke` PASS/exit 0.
  Software Arrange/note form previews 100%/150% проверены; физический DPI/ASIO,
  Komplete Kontrol/SWAM и длительная матрица не запускались в этом чате.
  FEATURE READY WITH MANUAL CHECK. Подробности/границы: [MIDI_CLIPS](../MIDI_CLIPS.md).
- Следующий шаг: пользовательский [checklist 0.2b](../MRS_STAGE_4B_CHECKLIST.md):
  instrument → Edit → Create MIDI clip at cursor → Add note → Play;
  редактирование после Pause/Stop, операции над клипом, tempo/loop/save/reopen,
  ASIO и Process Buffer Off/1024/4096 с Workers 1/2/4.
  4c не начинать автоматически. Chord/Arranger — Stage 5; metronome/precount — 4h.
  P4/#54/#16 и полный checklist 4a этой поставкой не закрываются.

## Предыдущая принятая визуальная база: 0.2a upd1 fix1

**Принята пользователем 2026-10-06**: «Отлично, задокументируй, чтобы другой чат
мог продолжать с этого билда». Это приёмка поставленной дизайн-доработки;
полный MIDI checklist и P4 этой репликой не закрываются.

- Пакет: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.2a-upd1-fix1-Mixer-JUCE-ASIO-Windows-local`.
  Запуск `Moon River Studio JUCE.exe`, scanner рядом. Предыдущие пакеты сохранены.
- EXE SHA256: `DC5DD5DE8191055E1E1B666A051D85EC5D1EC231C16D52C1D3EC993FD9E50009`.
  Пакет содержит licenses, README, VALIDATION, SHA256.json и smoke previews.
  Не переписывать выпущенный пакет для новой записи о приёмке.
- Принятая визуальная база: Noto Sans 13 pt, как заголовок проекта, для кнопок,
  меню, подписей треков/микшера и timeline; длинные button labels имеют ellipsis
  без автоматического уменьшения шрифта. Тёмная серо-синяя палитра, выделенный
  header/selected state. Обычные и Master strips 86 logical px с шагом 88 px;
  ручка вертикального фейдера 16 px, meter/fader сближены. Транспорт центрирован,
  CPU расположен слева внизу. Ещё не реализованные функции не имитируются.
- Исходники в существующем checkout, ветка `mrs/0.1q-fix3-processing-local`.
  HEAD `38e89e738b5b494b90d56ba0093c45d4ffbc6891` **не включает UI-доработку**:
  на момент той поставки она была в рабочем дереве. В 0.2b она сохранена и включена в source commit. Исторические изменения:
  `apps/studio-juce/src/Controls.h`, `apps/studio-juce/src/Desktop.cpp`,
  `apps/studio-desktop/include/mrs/version.hpp` и документацию.
  Не сбрасывать checkout и не заменять его remote-кодом.
- Проверки: локальная Release-сборка MoonRiverStudioJuce PASS; packaged
  `--j3-smoke --fixture mrs_vst3_fixture.vst3` exit 0 после финального изменения
  шрифта. Software preview 150% финального пакета просмотрен; 100%/150%
  проверялись в ходе итерации. CTest J3 1/1 PASS (2.36 s) относится к предыдущей
  итерации 116 px. Полная suite для финальной косметической версии не повторялась.
  Физический DPI, screen-reader и реальный ASIO не проверялись в этом чате.
- Audio engine, routing, Undo, schema 11 и plugin lifecycle не менялись;
  исправления 0.2a fix1 входят в этот исторический пакет. Текущая поставка — 0.2b выше.

## Точное состояние этапов

| Область | Состояние |
|---|---|
| P1 / [#51](https://github.com/vladleng/MR-Studio/issues/51), 0.1r | Принят 2026-10-05, issue закрыт. При 128 frames регулярные щелчки исчезли, редкие остаются |
| P2 / [#52](https://github.com/vladleng/MR-Studio/issues/52), 0.1s | Принят 2026-10-06, issue закрыт. Первый native whole-graph anticipation slice |
| P3 / [#53](https://github.com/vladleng/MR-Studio/issues/53), 0.1t | Принят 2026-10-06: «P3 вроде стабилен. Переходим дальше». Issue закрыт для поставленного mixed slice; длительная matrix остаётся P4/#16 |
| 0.1t fix1, редакторы плагинов | Принят пользователем 2026-10-06: «теперь с плагинами все как надо». Bypass, порядок окон и Pin подтверждены |
| P4 / [#54](https://github.com/vladleng/MR-Studio/issues/54) | Принят и закрыт 2026-10-09: #113 без сбоев, #114 допустимые щелчки при высокой нагрузке; причина43+43 не установлена. Полная измерительная матрица не заявлена |
| Parent Stage 3 / #23, длительная matrix #16 | Не закрыты этой приёмкой |
| 3e2 / 3e3 | Безопасные переходы/dynamic latency и recovery/isolation остаются отдельными задачами |
| 4c / [#65](https://github.com/vladleng/MR-Studio/issues/65), 0.2c | Реализован локально, 113/113 CTest/package PASS; пользовательская приёмка ожидается |
| 4b / [#64](https://github.com/vladleng/MR-Studio/issues/64), 0.2b | Принят: «Тест пройден, идем дальше»; #64 закрыт. Режимы аппаратного теста не перечислены |
| 4a / [#63](https://github.com/vladleng/MR-Studio/issues/63), 0.2a fix1 | Fix1 принят 2026-10-06: «Все работает». Полный checklist 4a отдельно не подтверждён; #63 открыт |

В пользовательской переписке «0.1c показывает хорошие результаты» было принято
как подтверждение тогдашней 0.1s/P2; не возвращай roadmap к старой 0.1c.

## Предыдущая функциональная поставка: 0.2a fix1

- **0.2a fix1**, 2026-10-06. Пользователь подтвердил загрузку SWAM Alto Flute 3,
  MIDI connected и звук через Komplete Audio ASIO / B 128 / Workers 2 после
  отключения Process Buffer («DSP»). После поставки fix1 пользователь сообщил
  «Все работает» — fix1 принят. Конкретные режимы повторной проверки не перечислены;
  полный checklist 4a отдельно не подтверждён.
- Устранена причина тишины с Process Buffer: live MIDI flags не копируются в
  ahead producer graph. Инструмент/downstream остаются device-owned; producer
  больше не отклоняет свой граф по anticipation_safe и не выставляет этот fault.
- MIDI status сохраняет имя назначенного порта/канал и показывает audio disconnected
  отдельно от Off. Длинный текст имеет ellipsis с читаемым шрифтом и полный tooltip.
  Offline clock не выводит звук и не даёт hardware CPU/latency; ASIO подключать явно.
- Открытие plugin editor без runtime готовит offline graph после Stop; отсутствие
  runtime больше не выдаётся за отсутствие GUI. Native и generic parameter windows
  owned by DAW HWND; editor generation обновляется после lazy prepare.
- Source `8e2c7d48f7a8857469a791ea4ddb18721983894b` (основной fix `0899d1c`),
  та же локальная ветка. Configure/Release прошли. **111/111 CTest, 58.10 s**;
  после финальной правки шрифта **4/4 focused, 4.42 s**. Packaged J3 и MIDI — exit 0.
  Первый GUI test падал из-за отсутствующего DAW peer после J3 cleanup; fixture
  добавляет scoped peer, повторный общий прогон зелёный. Snapshots 100%/150% просмотрены.
- MIDI regression: live Notes/Play/panic при Workers 2/4 × Process 1024/4096,
  host callback allocation 0. GUI: disconnected native view, HWND owner обеих
  панелей, сохранённый MIDI port/status. Установленный SWAM в этих tests не запускался;
  физические MIDI/ASIO устройства не открывались.
- Пакет `Builds/MR-Studio-0.2a-fix1-MIDI-JUCE-ASIO-Windows-local`; старый 0.2a сохранён.
  EXE SHA256 `F857D4BD6BB8C6BE95BA2B151E012A18EEAA03966EC83924F752C38CDAC52D90`.
  Проверены совпадение Release/пакета и SHA256 manifest. Core schema остаётся 11.
- Следующий шаг: продолжение MIDI по следующему запросу пользователя; оставшийся
  [checklist](../MRS_STAGE_4A_CHECKLIST.md) не отмечать пройденным без свидетельства.
  #63/#24 остаются открытыми, 4b не начат. Приёмка fix1 записана в #63.
- Уточнение о многопоточности: Workers распараллеливает независимые каналы,
  включая instrument tracks. Один вызов DSP экземпляра плагина имеет одного
  владельца; host не делит его между workers. Live MIDI остаётся device-domain,
  Process Buffer не вычисляет его заранее. Внутренний multicore SWAM не подтверждён.
- Fix1 docs-only GitHub main `e08b5ce77dcd476c54cb81ab24d09b6f9db5f474`
  опубликован с `[skip ci]`, remote SHA проверен; #63 обновлён и открыт.
  Source-ветка не отправлялась, Actions не запускались. Пакет заморожен с docs
  `88a0fae`, manifest 118 файлов проверен; запись синхронизации пакет не меняет.

## Исходная поставка 0.2a

- **0.2a / Stage 4a**: MIDI input Windows, instrument tracks, VST3 event input,
  monitoring при Stop/Play, общий mixer/buses/Master/PDC, panic и missing status.
- Пакет: `Builds/MR-Studio-0.2a-MIDI-input-JUCE-ASIO-Windows-local` в папке чатов.
  Запуск: `Moon River Studio JUCE.exe`; scanner рядом. Старые пакеты сохранены.
- Source: `ee55afc9384c99695c8e582d51da60cabc681616`, ветка
  `mrs/0.1q-fix3-processing-local`, только локально.
- Cached configure/Release build прошли. Финальный **111/111 CTest, 57.15 s**;
  packaged hidden J3 с MIDI smoke — **exit 0**. MIDI snapshots 100%/150% просмотрены.
  SHA256 EXE: `654675B8BB474F3E617B9FF829B7C129E1683D5C9AAA810157E0E343116D402F`.
  EXE совпадает с Release; package SHA256 manifest включает файлы/логи/документы.
- `midi_live`: decoder, Notes/velocity/sustain/bend, controller burst/release,
  Stop/panic/overflow/stale generation, short blocks, bus/gain/PDC, save/open,
  Undo/Redo и missing plugin/port. В проверенных host callbacks allocation 0.
  Первый общий прогон: 109/111, два parallel-теста падали из-за stack overflow;
  MIDI buffers перенесены в heap на stopped prepare, финальный прогон весь зелёный.
- Core schema **11** читает 1–10; MIDI input/channel/monitor и instrument/state
  сохраняются. Старые сборки не читают 11 — проверять на копиях проектов.
- Basic live MIDI имеет offset 0 следующего callback; sample-accurate timestamp
  recording не реализован. Controllers зависят от IMidiMapping инструмента.
  Структура цепи и MIDI input/channel/monitor меняются после Pause/Stop.
- Физические MIDI/ASIO устройства тестами не открывались. [Workflow/RT contract](../MIDI_LIVE_INPUT.md),
  [пользовательский checklist](../MRS_STAGE_4A_CHECKLIST.md). #63/#24 открыты;
  приёмка пользователя ещё не получена. P4/#54/#16 этим выпуском не закрываются.

## Предыдущая принятая поставка — 0.1u

- Пакет: `Builds/MR-Studio-0.1u-P4-profiling-JUCE-ASIO-Windows-local` в папке чатов.
- Запуск: `Moon River Studio JUCE.exe`; `mrs_vst3_scan.exe` рядом.
- Source commit: `ef11071bd221e30f822c2fd09e4689bfbf0b98ad` (только локально).
- SHA256 EXE: `3B75C8CF1886CC983544254171D6B9499D515E7CA0500F193BE50CF874DBCBA9`.
- Cached configure/full Release прошли. **110/110 CTest, 57.29 s**;
  финальные profiling/J3 проверки — 2/2, 1.71 s. Packaged hidden J3 — exit 0.
- 112 канал-конфигураций OFF + 112 ON: совпадают checksum; worker timeout 0,
  Late 14/13. Playback/mixed — 210 OFF + 210 ON, 67 200 точных PCM-блоков;
  producer/disk/worker misses 0, Late 13/13. Все выбросы сохранены в CSV.
- По минуте streamed mixed raw recording и playback при 48 kHz / Device 128,
  Workers 4, profiling ON: по 22 540 точных блоков, Late/U/D/timeouts 0.
  Запись: 2 885 120 mono raw frames, каждый равен исходному входу, fault 0.
  Это paced synthetic callbacks; физический ASIO не открывался.
- Панель/CSV проверены, software snapshots 100%/150% просмотрены. Пакет имеет
  SHA256 manifest; EXE совпадает с Release. Старые 0.1t/fix1 и другие версии сохранены.
- [Методика](../ENGINE_PROFILING.md), [замеры и ограничения](../ENGINE_PROFILING_BENCHMARK.md).
  Результаты не доказывают Fender parity, общий CPU speedup или отсутствие щелчков.

## Существенные границы реализации

P1 обрабатывает независимые каналы параллельно и сводит результаты детерминированно;
зависимые buses/Master ждут входы. P2 заранее готовит подходящий native playback.
P3 разделяет независимый native playback и live/shared DSP через ограниченную
PCM-очередь; фактические DSP/PDC/source-cursor имеют по одному владельцу.

Process Buffer по умолчанию Off. Для mixed нужны Workers >= 2 и подходящие
prerecorded native-каналы; Process 1024/4096 не меньше Device Buffer. VST3,
live/shared buses и Master обрабатываются напрямую. VST3 anticipation не реализован;
TH-U/Xvox/ONE-only граф не получает обещания нового ускорения сверх P1.
Mon — оценка driver I/O + fixed PDC, не измеренная акустическая задержка.
Недостающий playback PCM не останавливает live/capture/device time; восстановление
может обрезать tails. Seamless history/crossfade и native crash/hang isolation не готовы.

0.1t fix1: редактор — owned top-level окно Windows над MR Studio; Pin сохраняет
его при открытии другого плагина, закрытие одного не закрывает остальные.
Host bypass по-прежнему после Pause/Stop: PDC-граф пересобирается, native child
редактора переподключается в том же JUCE-окне. Pin — состояние открытого окна,
не проекта. Structural/device/project changes и Undo/Redo могут закрывать редакторы.

0.1u P4: Transport -> Engine profiling... включает timing каналов/insert/Master,
worker job balance и отдельные device/ahead banks, экспорт CSV из UI. По умолчанию
OFF; закрытие отключает сбор. Timing добавляет overhead. Dependency cost estimate
исключает source/reduction/queue/scheduler wait и не равен полной critical-path
wall time. Сумма пересекающихся jobs не равна process CPU. Engine counters сбрасываются
при stopped prepare, processor counters — при пересоздании PreparedGraph. Снимки
приблизительны, project schema/Undo не менялись. Полная семантика — в ENGINE_PROFILING.

## Что читать по задаче

- Общий смысл продукта: [PROJECT_VISION](../PROJECT_VISION.md), [ARCHITECTURE](../ARCHITECTURE.md),
  [DEVELOPMENT_TRACKS](../DEVELOPMENT_TRACKS.md). Их исторические статусы не заменяют снимок выше.
- Движок: [план](../ENGINE_PERFORMANCE_PLAN.md), [reliability](../PROCESSING_RELIABILITY.md),
  [P1](../ENGINE_PARALLEL_PROCESSING.md), [P2](../ENGINE_ANTICIPATIVE_PROCESSING.md),
  [P3](../ENGINE_MIXED_PROCESSING.md), [P4](../ENGINE_PROFILING.md).
- Плагины: [редакторы/fix1](../ENGINE_PLUGIN_EDITORS.md), [VST3](../VST3_HOSTING.md),
  [пресеты](../PLUGIN_PRESETS.md).
- UI: `mr-studio-ui-design-system/SKILL.md`, [JUCE J3](../JUCE_J3.md).
- Проверки: `mr-studio-build-test/SKILL.md` и [локальные команды](../LOCAL_DEVELOPMENT.md).

Карта исходников: `apps/studio-juce/src/` — актуальный UI; `apps/studio-desktop/`
— Application/services и исторический Win32 UI; `core/audio/` — engine/worker/ahead/mixed;
`core/processing/` — processor graph, VST3, native DSP, PDC; `core/src/` — model,
commands, serialization; `tests/` — регрессии. JUCE GUI smoke находится также
в `apps/studio-juce/src/Smoke.cpp`.

## Пользовательский ASIO CSV: Test 01

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 01.csv`.
Только анализ, не пользовательская приёмка P4 и не подтверждение записи.
Komplete Audio ASIO Driver, 48 kHz, B 256, Workers 4, Process 256, profiling ON.
Callback load p50/p95/p99 17/24/28%, max 1.8651 ms при deadline 5.3333 ms;
Late/XR/disk/worker timeout 0. Но Ahead underruns **17 212**, invalidations 5,
buffered 0, producer max 0.4337 ms. Проверить starvation/учёт очереди отдельно:
metrics накоплены за engine session, timing cells — только пока profiling ON;
по CSV нельзя вычислить текущую частоту underrun или акустические последствия.
7 709 device timing calls соответствуют 41.1147 s обработанных frames; это не
известная длительность всей сессии/не гарантия непрерывной игры или записи.

Средние device channel jobs: Voc 0.8046 ms, GTR 0.5794 ms, PB 0.4983 ms,
Keys 0.0067 ms; времена пересекаются, не складывать в callback load.
Xvox Pro 0.6044 ms, TH-U 0.3725 ms, Xrack Pro 0.4860 ms;
две Ambiente 0.1867/0.1917 ms. ONE имеет 0 measured calls; пользователь
после Test 01 подтвердил, что ONE был в bypass.
Следующий полезный контроль — свежий session CSV с Process Off / 1024 при
неизменной остальной нагрузке; текущие данные не являются 128-frame проверкой.
Исходный CSV и пользовательский проект не изменялись. Code/build/package без
изменений; #54 не закрывать по этому снимку.

## Пользовательский ASIO CSV: Test 02

2026-10-06: получен `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 02.csv`.
Пользователь называет режим «выключенным DSP»; CSV показывает Process Buffer Off,
Workers 4 и активный processor DSP, включая ONE. Komplete Audio ASIO Driver,
48 kHz / Device 256. Callback load p50/p95/p99 24/34/37%, max 2.3907 ms
при deadline 5.3333 ms. Late/XR/disk/worker timeout 0. Ahead counters 0 ожидаемы
при отключённом producer и не подтверждают исправление Test 01 queue starvation.

ONE: 385 вызовов, mean 0.4243 ms, max 0.8427 ms. Prepared graph PDC 2465 samples
= 51.3542 ms; это fixed compensation, отдельно от driver I/O и Process Buffer,
не акустическая latency и не пропущенный deadline. Device Voc/GTR/PB mean
0.6197/0.5394/0.4274 ms. Timing frames 98 560 = 2.0533 s обработки при profiling;
полная длительность session из CSV неизвестна. Это короткий profiling sample,
не длительная ASIO приёмка. Прямое сравнение с Test 01 не изолирует Process:
одновременно изменились bypass ONE и Process, окна сбора тоже различаются.

Для чистого сравнения держать ONE и остальные plugin states одинаковыми,
сравнить Process Off / 1024 свежими counters, дать более длинный profiling window.
P4 остаётся открыт. Исходные CSV/проект/код/билд/пакет не изменены; только анализ
и локальная запись контекста.

## Пользовательский ASIO CSV: Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 256 / Workers 4 / Process 1024,
profiling ON. ONE активен: 9 410 measured calls. PDC прежний: 2465 samples.
Callback p50/p95/p99 25/33/37%, max 2.7451 ms (51.47% deadline 5.3333 ms).
Late/XR/disk/worker timeouts и Ahead underruns/invalidations **0**;
queued 1024 frames в момент snapshot, producer max 0.4354 ms.
Timing содержит 2 408 960 frames = 50.1867 s обработки при profiling; это не
доказательство wall duration всей сессии, записи или непрерывного транспорта.

С Test 02 (Process Off, ONE active): p50 24->25%, p95 34->33%, p99 37->37%,
max 2.3907->2.7451 ms. Ускорение callback не установлено; окна сбора различаются
(~2.05 vs ~50.19 processed s), max нельзя трактовать как доказанное ухудшение.
ONE mean 0.4676 ms, max 1.2369 ms. Основные VST3 inserts остаются device-owned;
producer timings каналов малы, поэтому выраженного выигрыша anticipation тут
не видно. Test 01 Process 256 имел 17 212 накопленных underruns и ONE bypass;
при 1024 текущие counters чистые, но это не доказательство найденной/исправленной
причины прежних misses. Снимок поддерживает работоспособность очереди 1024 на
этой нагрузке. #54 остаётся открыт без явной приёмки и более широкой matrix.
CSV/проект/код/билд/пакет не изменены; результаты сохранены только локально.

## Пользовательский ASIO CSV: Device 128 / Process 1024

2026-10-06: `C:/Users/Vladislav/Documents/MR Studio/Projects/Test 128 dsp 1024.csv`.
Komplete Audio ASIO Driver, 48 kHz / Device 128 / Workers 4 / Process 1024,
profiling ON. ONE 0 measured calls, PDC 0; соответствует выбранному live bypass.
Callback p50/p95/p99 16/27/32%, max 1.4593 ms при deadline 2.6667 ms (54.72%).
Late/XR/disk/worker timeouts 0. Ahead buffered 1024, producer max 0.6036 ms;
**Ahead underruns 43 и invalidations 43**. Совпадение totals не устанавливает
причину или связь каждого события. В mixed code rebase вызывают как command/mix
revision, так и head.sequence > producer sequence после отставания; поэтому
нельзя автоматически объявить конкретную причину. Пользователь ответил «нет»
на вопрос о щелчках/потере playback и Pause/Seek/изменениях параметров: слышимых
проблем и указанных действий не было. Не списывать 43 события на ручной транспорт.
Для диагностики проверить producer scheduling/queue starvation и корректность
счётчиков, в том числе вне активного playback; причины CSV сам по себе не даёт.

Timing содержит 80 135 device calls / 10 257 280 frames = 213.6933 s обработки
(~3 min 34 s), не полную wall duration сессии или доказательство 5-min continuous
playing/записи. В producer 80 167 calls: может содержать заранее queued frames.
Host plugin PDC теперь 0; driver/converter latency остаётся, acoustic не измерена.
Субъективный live/playback контроль без слышимых проблем подтверждён пользователем.
Пользователь затем уточнил: в самом мультитреке была небольшая пауза.
Вероятно, речь о тишине/разрыве в материале; это не подтверждение нажатия Pause.
Не приписывать пропуски этому месту: CSV содержит только totals, без времени
событий. Тишина может скрыть слышимые последствия недостающего playback PCM,
но сама по себе при непрерывном транспорте не доказывает нормальность underrun.
43 очередных события остаются диагностическим пунктом P4; record acceptance
и полный scope #54/#16 этим CSV/ответом не подтверждены.
User CSV/проект/код/билд/пакет не изменены; локально сохранён только анализ.

## Приёмка 0.1u и переход к MIDI — 2026-10-06

Пользователь согласился принять поставленный software slice 0.1u, оставить
#54 открытым для оставшихся проверок и перейти к MIDI в следующем чате:
«Да, давай, хочу в следующем чате уже заняться миди, а P4 буду в процессе тестировать».
Это приёмка сборки/профилирования и разрешение сменить фокус разработки;
не подтверждение полной sustained recording/transport/reconnect matrix.
43 Ahead underrun/invalidation остаются диагностическим пунктом, причины не
установлены. Live 128 / Workers 4 / Process 1024 / ONE bypass без слышимых
проблем принят в сообщённом пользователем объёме; сведения о паузе в материале
сохранены выше. Более широкие #16, 3e2/3e3 не закрыты. Выпущенный пакет и его
manifest не переписываются после новой пользовательской приёмки.

## Продолжение и обновление этого файла

2026-10-06: по запросу пользователя Stage 4 / MIDI / #24 разбит на **4a–4h**:
0.2a MIDI-вход/VST3-инструменты; 0.2b клипы/playback; 0.2c запись;
0.2d piano roll; 0.2e quantize/transpose; 0.2f CC/PC;
0.2g внешний MIDI; **4h / 0.2h — метроном и precount для audio/MIDI записи**.
Порядок уточнён 2026-10-08: сначала 4h, затем 4g; итоговая проверка после обоих.
По последнему уточнению пользователя
Arranger Track, Chord Track и остальная музыкальная структура остаются в Stage 5 / #25.
Доработки — updN, исправления — fixN;
независимые счётчики сбрасываются при переходе к новой букве.
План и критерии: [MRS_STAGE_4_PLAN](../MRS_STAGE_4_PLAN.md), схема — VERSIONING.
GitHub: parent #24 обновлён; созданы issues #63–#70 для 4a–4h соответственно.
Stage 5 / #25 сохраняет Chord/Arranger и музыкальную структуру; его scope не менялся.
План, roadmap, VERSIONING и этот контекст опубликованы отдельным docs-only
коммитом `dcd2dad1b5dbd5dd95199d8071af0daf18955e69` в GitHub main с `[skip ci]`;
remote SHA проверен. Локальная source-ветка не отправлялась, код/пакет не менялись.
Планирование завершено, по запросу «Приступай к 0.2a» реализован 4a и выпущен
локальный пакет выше. Следующий шаг — пользовательская игра через MIDI/ASIO:
проверить клавиатуру, выбранный канал, sustain/bend, Stop/panic/reconnect,
save/open и прежние audio/plugin editors. Прочитать MIDI_LIVE_INPUT и
MRS_STAGE_4A_CHECKLIST. Не закрывать #63 без подтверждения физической приёмки.
4b / 0.2b реализован по следующему запросу пользователя; текущая поставка описана выше. 4c / 0.2c автоматически не начинать.
Chord/Arranger остаются Stage 5; метроном/precount — 4h. MIDI-клипы/recording,
piano roll, external MIDI в 0.2a отсутствуют. Собственный synth не поставляется;
нужен VST3-инструмент. Callback ingress/worker ownership не менять на UI note pump.

GitHub #63/#24 обновлены результатами 0.2a и остаются открытыми. Шесть выбранных
docs опубликованы в main отдельным docs-only коммитом
`5d1908936a3494d53df93fd8d72d14f6b4f5c735` с `[skip ci]`; remote SHA проверен.
Remote-only time-stretch roadmap сохранён. Source-ветка не отправлялась, Actions
не запускались. Пакет заморожен с docs `7207ad6` и проверенным manifest 114 файлов;
эта запись синхронизации не переписывает выпущенный пакет.

P4 теперь остаётся проверкой пользователя параллельно разработке MIDI. Не
навязывай завершение #54 как условие начала MIDI и не продолжай engine fixes
самостоятельно вместо MIDI. При новом CSV/аудио-дефекте анализируй сообщение
и фиксируй результаты; по запросу исправления возвращайся к нужной части P4.
Осталось: выяснить 43 queue events, подтвердить запись и sustained transport/
reconnect/state checks. #54/#16 не закрывать автоматически; фиксировать точный
объём будущей приёмки и реальные durations, без vendor/project optimizations.

После содержательной работы обновляй этот снимок: дату, задачу, подтверждённый
результат, незавершённое, ветку/source commit, пакет/проверки, приёмку и следующий шаг.
При передаче незавершённого кода укажи файлы/ошибку/последнюю команду и безопасное
продолжение. Не копируй весь журнал чата и не запускай старые patch-скрипты.

## Decision: instrument parallelism in P4

Recorded 2026-10-06 after the user's Omnisphere piano comparison at 48 kHz / 128 frames.

P4 / #54 now also owns **Instrument Parallelism / multi-instance VSTi sharding**. Existing
P1 already parallelizes independent tracks/nodes, but one VST3 instrument instance remains
one host scheduling node. The new goal is to let one logical instrument track optionally use
2/4 synchronized hidden instances, distribute MIDI note ownership, process them on existing
workers, deterministically sum audio, then run the shared post-instrument FX chain once.

Initial policy is opt-in/capability-gated with Single Instance fallback; do not force this on
mono/legato/sequenced/random/global-state instruments. Never make scheduling depend on the
Omnisphere name/vendor. Omnisphere piano/high-polyphony is only the first representative
hardware benchmark.

No implementation was started by this documentation update. Code/build/test work remains
local; GitHub receives issues/docs only.

## Синхронизация поставки 0.2b

GitHub #64/#24 обновлены и открыты. Пять выбранных docs опубликованы с [skip ci];
remote-only решение о multi-instance VSTi / P4 сохранено в контексте отдельной записью.
Source-ветка не отправлялась, Actions не запускались. Пакет заморожен: 29 файлов,
SHA256 manifest проверен. Последующая запись синхронизации пакет не меняет.

## Синхронизация поставки 0.2c

GitHub #64 закрыт по приёмке 0.2b; #65/#24 обновлены и открыты. Шесть выбранных
docs опубликованы отдельным docs-only main commit `0d1720e0d7c2bab5bcb79a06c4465156cade6bd9`
с [skip ci], remote SHA проверен. Remote-only P4/multi-instance VSTi контекст сохранён.
Source-ветка не отправлялась, Actions не запускались. Пакет заморожен с docs
`4a87c66`: manifest 48 файлов проверен, EXE совпадает с Release. Эта запись пакет не меняет.
