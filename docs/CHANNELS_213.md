# Каналы Tracks / Mixer — #213

Объём: [#213](https://github.com/vladleng/MR-Studio/issues/213), parent #209.
Пользователь подтвердил единый порядок Tracks и Mixer 2026-10-17.
Локальная реализация; пользовательская приёмка ожидается.

## Порядок и идентичность

Обе панели используют `Project::tracks`. Drag названия или свободной области
переставляет канал; интерактивные дочерние controls не начинают этот drag.
Порог 5 px отделяет drag от клика; double-click названия открывает rename.
Успешная перестановка — одна команда Undo; прежняя позиция не создаёт history.
Drop откладывается до выхода из JUCE drag callback и проверяет Store/revision.
Drop вне области не применяется. Master не переносится и остаётся справа последним.

Audio, instrument, MIDI, bus и FX return перемещаются в общем плоском списке.
`folder` остаётся прежним: это не reparent и не drag самих папок. Routing,
sends, clips, automation и slot IDs не изменяются. Фактические объекты Strip
переставляются в обеих панелях; открытые редакторы не закрываются.

Перестановка/её Undo/Redo требуют остановленного транспорта и отсутствия записи.
Существующая граница stop/quiesce/reprepare пересобирает индексы mixer/MIDI/PDC,
удерживая `PreparedGraph` по channel ID. DSP instances и insert generation
сохраняются; panic отпускает held MIDI notes перед сбросом поколения MIDI.
Аудио callback не изменён. Это не обещание бесшовной перестановки при playback.

## Цвет

Справа Track header и внизу Mixer strip — поле цвета с меню по ЛКМ.
Обе проекции используют `Track::color` и общую палитру `ObjectPalette::clips`,
расширенную с 24 до 32 цветов. Default снимает custom RGB; существующие цвета
клипов не меняются. Master не является Track и сохраняет принятое оформление.

Цвет — project metadata: одна команда, Undo/Redo, без rebuild DSP.
Schema 27 добавляет `TRACKCOLOR present rgb`; archive container остаётся 1.
Версии 1–26 открываются без custom channel color. RGB проверяется в модели и
десериализации; существующие clip colors/fades и MIDI данные сохраняются.

## Органы управления

У instrument channel первый реальный slot остаётся инструментом, но в FX list
не рисуется. Кнопка под Sends и piano icon открывают тот же `openInsert` по ID;
повторное нажатие поднимает существующее окно. Это одна instance/один тракт.
FX insertion/drop переводит видимый индекс в реальный с учётом скрытого slot 0.
Пустой канал предлагает выбор просканированного инструмента; отсутствие каталога
и ошибки загрузки показываются явно через существующие host states.

Под инструментом — существующий MIDI input/channel selector, включая missing
input, внешние outputs, Panic/Reconnect. Arm/monitor и port IDs не сбрасываются
перестановкой. Отдельного native MR instrument в данном объёме нет; native FX
editor и synthetic VST3 instrument проверяются как разные host cases.

При наличии instrument каналов Mixer резервирует дополнительную высоту для этой
секции, включая минимальное окно приложения 1200×700; travel фейдера остаётся 145 px.

M/S/O (O = Arm recording) получают фон как I; действия сохранены. Активность
выражена glyph/цветом, hover/pressed меняют фон, keyboard focus имеет индикатор.
Glyph добавления FX уменьшен до 13 px SVG host box; hit area сохранена 20×19.
Существующая фейдерная геометрия и compact gain #215 сохранены.

## Проверка

Адресные model/legacy/persistence, hosting/lifetime, MIDI и GUI проверки;
локальные результаты и commit — в Issue. Для новой раскладки — software scale
100/125/150/200 и minimum/nominal previews. Физические ASIO/слух/DPI не заявляются
без пользовательского/аппаратного прогона. Full CTest не является gate этого этапа.