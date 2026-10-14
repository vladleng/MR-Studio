# Reference theme — 0.3c upd9 / #145

## Button typography — 0.3c upd13 / #146

Запрос: подписи New project/Open/Refresh и подобных кнопок крупнее и шире.
Theme::getTextButtonFont: Segoe UI14 logical, для compact height<=22 —13,
horizontal scale1.04. Mixer buttons исключены: прежние12/scale1 по уточнению пользователя. Численные значения визуальный кандидат по образцу.
Font menu/browser12 не меняется; команды/размеры hit areas/семантические
состояния/цвета/DSP/schema18 прежние. StripFont кнопки дорожек тоже сохраняют12.
Source `04f97e6`; Release/GUI6/6 PASS (65.09s), packaged J3 PASS, home100/150
и mixer previews просмотрены. Logs `validation/button-font` внутри build.
Package `Builds/MR-Studio-0.3c-upd13-Button-Font-JUCE-ASIO-Windows-local`,
13 files / 12 hashes verified. READY WITH MANUAL CHECK; #146 open.

## Menu typography — 0.3c upd12 / #146

Upd11 принят: «Это принимаю», #145 closed completed. Новая просьба —
шрифт выпадающих меню по размеру как в VST3 browser. JUCE V4 default
row round(12*1.3)=15px, paint inset2px -> maxFontHeight10px; glyph уменьшался.
Theme минимум22px для обычных пунктов, separator высота прежняя.
PresetMenuTheme использует Theme::channelFont (Segoe UI12 logical) и ту же
минимальную строку. Цвета/состояния/commands/схема18/DSP прежние.
Smoke menu preview использует default item height (без explicit22), чтобы
проверить исправленный путь. Release/GUI CTest6/6 PASS (43.97s); packaged J3
fixture smoke PASS (exit0). Fresh default/preset popup software100/150
previews просмотрены: `build/asio-local/validation/menu-font/packaged-smoke`.
Source `5b11598ee420fe9f5a2e821bc8793c312d58cdb2`;
compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd12-Menu-Font-JUCE-ASIO-Windows-local`.
GUI SHA256 `98819CD7DF028C1A87EE0F7DDB25A451EDD2BB51BD466794E1AC5FF0312C40D1`.
Symbols `build/asio-local/deliveries/0.3c-upd12-menu-font`.
READY WITH MANUAL CHECK, #146 open до визуальной приёмки.
Прежние packages сохраняются.

## Follow-up — 0.3c upd11, 2026-10-11

Пользователь оставил цвет общей области upd10: «Хорошо, этот цвет оставляем».
Уточнил вложенные браузеры: дерево папок и список импульсов должны иметь
тёмный фон, как VST3/Files. Только TreeView/ListBox возвращены к surface
#212830 внутри card #2D353E. Кнопки/Arrange/font/radius upd10 сохранены.
Release и две Reverb GUI проверки PASS (1.36s); packaged browser/editor smoke
PASS (exit0), fresh software100/150 previews просмотрены. #145 open,
READY WITH MANUAL CHECK — визуальная приёмка ожидается.
Source `8f5e56e1355d710ab4aa98d376bb2933d0384489`;
package `Builds/MR-Studio-0.3c-upd11-IR-Browser-JUCE-ASIO-Windows-local`,
compact13 files / 12 hashes verified. GUI SHA256
`DF84930A5875F6307685E2433B6A4DBAEEC2354717380C8DBFC5E3AB472BD1D7`.
Logs/previews `build/asio-local/validation/ir-browser-surfaces`; symbols
`build/asio-local/deliveries/0.3c-upd11-ir-browser`. Physical mixed-DPI не
измерялся, DSP/schema18 прежние; без push/merge/Actions, старые пакеты сохранены.

## Follow-up — 0.3c upd10, 2026-10-11

Пользователь по двум свежим скриншотам указал, что зоны сливаются.
IR browser card MR Reverb (вместе с tree/list) теперь #2D353E, прежний цвет
кнопок; внешний фон редактора #212830 сохранён. Нейтральная заливка всех
кнопок #2F3841: RGB прежнего #2D353E умножены на 1.05 и округлены.
Arrange grid canvas #262E37 — немного светлее исходного #212830; chord/marker
lanes используют тот же canvas. Шрифты, геометрия, рамки и семантические
selection/record/focus цвета сохранены. `contentSurface` — отдельная роль в
существующей Theme, не альтернативная тема. Model/DSP/schema18 не меняются.
Local Release PASS, GUI CTest6/6 PASS (69.27s), logs
`build/asio-local/validation/theme-contrast`. Packaged J3 и Reverb browser smoke
PASS (exit0), fresh software100/150 browser/Arrange previews просмотрены в
`packaged-smoke`. #145 open, READY WITH MANUAL CHECK; physical mixed-DPI
не измерялся, требуется пользовательская визуальная приёмка.
Source `b042800098093969e0a65c82640dc6a5bc1ad312`; compact13 files / 12 hashes:
`Builds/MR-Studio-0.3c-upd10-Theme-Contrast-JUCE-ASIO-Windows-local`.
GUI SHA256 `67E195B5D029D7B79CC24D3F4527D6D38B00A5C9868832CE1204B2584993AA18`.
Symbols `build/asio-local/deliveries/0.3c-upd10-theme-contrast`.
upd9 и более ранние пакеты сохраняются.

2026-10-11. Пользователь принял upd8/#137: «Тесты прошли приемку».
Новый запрос — стиль bitmap `codex-clipboard-787b7e4f-e50f-4b48-9eff-8c262178e5a9.png`:
фон mixer channels/track headers, окантовка/скругления, мелкий шрифт, разделители,
native/host plugin editors, VST3/Files text, audio/MIDI clip и marker edges.

## MR UI Kit / Gallery — future extension (2026-10-14)

Новая инфраструктурная задача [#189 MR UI Kit + Gallery](https://github.com/vladleng/MR-Studio/issues/189), [спецификация](MR_UI_KIT.md), [интерактивный конструктор](MR_UI_GALLERY.md), [этапы](MR_UI_KIT_ROADMAP.md). **Этот документ остаётся каноническим источником принятых цветов/шрифтов и исключений #145/#146**, не заменяется будущим Kit. На первом этапе новый дизайн не применяется глобально; экспериментальные изменения действуют только в scoped preview Gallery. Code/build/test не выполнялись.

## Общие роли

Владелец — существующий `ui::Theme` / `Controls.h`, отдельной темы нет.

| Роль | Значение | Основание |
|---|---|---|
| Фон channel/editor/canvas | #212830 | sampled bitmap RGB33/40/48 |
| Окантовка | #3D444D, 1 logical px | sampled edge RGB61/68/77, тонкая линия образца |
| Разделители | #343C45, 1 logical px | визуальное приближение линии образца |
| Raised control | #2D353E | приближение поднятой кнопки образца |
| Body text | Segoe UI, regular, 12 logical JUCE units | Windows UI аналог по плотности; точный face bitmap не доказывает |
| Радиус | 6 logical px | геометрическое приближение углов образца |

Направление явно заказано пользователем; численные font/radius/divider значения
пока PROVISIONAL до визуальной приёмки. Прежний stripFont11 заменяется прямым
запросом. Общий шрифт используется в channel controls, browser names/Files rows,
editor controls/labels/preset selectors и clip/marker labels. Заголовки Windows
остаются native. Внутренний HWND GUI сторонних VST3 принадлежит vendor и не
перекрашивается; стилизуются host chrome и native MR controls.

## Границы

UI/Theme AFFECTED; model/engine/commands/Undo/persistence/audio NOT AFFECTED;
schema18 прежняя. Меняются только paint/font/look-and-feel, не routing, DSP,
editor lifetime, hit/drag geometry, clip spacing, send gestures, mixer divider.
Blue selection/focus/power, record red и marker warning остаются различимыми.
Clip colors и alpha.72 сохранены; selected border использует accent, обычный —
outline. Marker pill имеет round border, исходный tick/flag и coincident count
сохранены. Подключение/загрузка сторонних plugins не меняется.

## Проверка

Local Release PASS; GUI CTest6/6 PASS (53.40s), включая J2/J3/reverb
browser/editor. Logs: `build/asio-local/validation/reference-theme`.
Packaged J3 fixture smoke PASS (exit0). Fresh software100/150 previews
просмотрены: mixer, Arrange audio/MIDI clips, markers, native EQ, host editor,
VST3/Files. Evidence: `validation/reference-theme/packaged-smoke` внутри build.
Исправлен унаследованный цвет waveform после
смены цвета текста; waveform остаётся голубым. Визуальные assertions старой
заливки и точки на углу ghost clip обновлены под новый дизайн, проверки
жестов/команд сохранены.
Более общий suite не обязателен для косметического изменения; physical
mixed-DPI/manual font acceptance пока не проверены. Package upd9 отдельный,
upd8/прежние packages сохраняются. #145 открыт до пользовательской приёмки.
Без push/merge/Actions; документация только локально, #25/Stage5 не начинать.

Delivery: **READY WITH MANUAL CHECK**, визуальная приёмка #145 ожидается.
Source `fd07877b690cc836c177aa2a6f6bc2f2300e3e4e`.
Package `Builds/MR-Studio-0.3c-upd9-Reference-Theme-JUCE-ASIO-Windows-local`
в рабочей папке чатов, 13 files / 12 payload hashes verified.
GUI SHA256 `8B70743328CFF54B178C61F6E4E2A31505A6030A791ABFFA59582A9F26D67926`.
Symbols: `build/asio-local/deliveries/0.3c-upd9-reference-theme`.
Accepted upd8 GUI SHA256 `D4D49B452A3A7974047FD42D2CD2C2F565119732CEFACC2E25D0F2CECA18D527`
проверен и не изменён. Более общий CTest126 не повторялся для paint/font-only.


2026-10-11: **0.3c upd13 принят** — «Принимаю,тест пройден».
#146 closed completed. Пакет, SHA256 manifest и исходники не изменены при приёмке;
без пересборки/push/merge/Actions. Дальнейшие изменения только по новому запросу.
