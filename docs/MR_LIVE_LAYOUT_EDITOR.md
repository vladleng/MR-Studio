# MR Live — браузерный редактор и согласование интерфейса

Статус: согласованный план, реализация редактора и финальная раскладка ещё не приняты.
Tracking: [#28 UI Foundation](https://github.com/vladleng/MR-Studio/issues/28), первый этап [#272 Online Layout Editor](https://github.com/vladleng/MR-Studio/issues/272).
Решения пользователя: 24 октября 2026.

## 1. Два представления шоу

**MR Live** — основной show-интерфейс подготовки и работы с setlist, песнями, playback/stems, патчами, автоматизацией и routing внутри Moon River Studio.

**MR Live Performance Mode** — последующее отдельное направление: концертный экран, для которого пользователь заранее выбирает нужные блоки, их размеры и расположение. Его реализация следует после основного MR Live. Общая архитектура блоков должна позволить добавить этот target без нового редактора.

Оба представления используют один ShowDocument, Project Model, Audio Engine, Transport и GraphStore. Вход в show — создание/открытие .mrlive через Files; песни — ссылки на .mrsproject. Смена представления сама по себе не перезагружает DSP и не создаёт второй аудиотракт.

## 2. Порядок разработки

1. #272: расширить MR Design Lab онлайн-редактором блочных макетов MR Live и принять его.
2. #28: собрать варианты, выбрать и явно утвердить основной интерфейс MR Live: состав/иерархию панелей, размеры, типографику, icons, transport/toolbar и поведение при resize.
3. #28/#29: реализовать утверждённые JUCE-компоненты и layout loader, затем bind к реальным shared services; fixtures сохраняются как тестовые данные.
4. #30–#33: show/setlist, playback/click/cue, переходы, patches и hardware control в собственных согласованных этапах.
5. Отдельная последующая issue MR Live Performance Mode: выбор видимых блоков, раскладки, вход/выход из концертного представления и сохранение настроек. Этот документ фиксирует направление, а не объявляет его реализованным.
6. #35: репетиционная и концертная надёжность.

Приёмка редактора, утверждение макета, рабочая JUCE-интеграция и концертная приёмка — разные результаты.

## 3. Развитие существующего MR Design Lab

База — tools/design-lab, принятый Track Header пилот #242 и единая MR UDS registry assets/ui/mr-uds/. MR Live добавляется как workspace/target существующего инструмента; принятый Track Header продолжает работать.

#249 — родитель развития Design Lab; #247 — общий layout contract; #246 — опциональный будущий Browser Bridge; #248 — Pages sync/QA. #272 согласует общий контракт с #247, но не ждёт полного переноса Mixer и native editors. #246 не является условием автономного редактора: первый обмен — JSON export/import.

Публикация — отдельный путь в существующем GitHub Pages, без замены текущего Design Lab. В issue реализации зафиксировать фактический проверенный URL, source revision и deployment revision. Сайт должен открываться по HTTPS с корректными относительными путями к manifest/SVG. Не объявлять будущий URL опубликованным без проверки.

## 4. Интерфейс редактора v1

- Палитра зарегистрированных блоков и поиск.
- Canvas с выбором viewport, zoom, grid/snap/guides.
- Drag/drop, move/resize, числовое редактирование bounds, alignment, порядок слоёв.
- Дерево слоёв: выбор, hide/show и возврат невидимого блока.
- Inspector: min/max, anchors/docking, padding/gap, font family/size/weight, выравнивание, MR UDS tokens, semantic SVG IDs.
- Настройка состава и расположения транспорта/toolbar через зарегистрированные controls с устойчивыми binding IDs.
- Undo/Redo для редактирования, reset, именованные layouts, browser autosave/recovery, export/import.
- Preview mode без редакторских handles, fixtures разных состояний; preview не исполняет реальные audio/MIDI-команды.

При resize окна задаются constraints и явные правила overflow/scroll/collapse, а не простое масштабирование единственного screenshot. Конкретные breakpoints и эталонные viewport выбираются при реализации/приёмке. Browser zoom и UI scale не подменяют проверку физического DPI в JUCE.

## 5. Каталог блоков

| Block type | Preview / назначение |
| --- | --- |
| show.setlist | Список песен, current/next, длинные названия |
| song.header | Название, tempo/meter, состояние песни |
| song.timeline | Секции, markers, patches и playback области |
| playback.stems | Свернутый playback / раскрытая группа stems |
| input.patches | Живые inputs, список/состояния patches |
| mixer.channels | Уровни, pan, mute/solo, meter placeholders |
| control.transport | Play/Pause/Stop, song navigation, time/bar/beat |
| control.toolbar | Настраиваемый набор зарегистрированных actions |
| musical.chords | Moving strip, current/next chords |
| musical.sections | Current/next section |
| info.cues | Подсказки, markers, notes |
| info.lyrics | Текст песни |
| system.status | Device/MIDI/load/preflight indicators |

Это визуальный каталог: наличие блока не означает реализацию соответствующего runtime workflow. Tuner и другие widgets добавляются через registry по необходимости.

Патчи в целевом MR Live могут размещаться свободно по музыкальной сетке; section boundaries не являются единственным способом их задания (#138). Редактор иллюстрирует это fixtures. Модель исполнения/сохранения свободных patch events уточняется отдельно в #32/#33; layout JSON её не заменяет.

## 6. Расширяемость

Block registry содержит type ID, block version, property descriptors, defaults, allowed token/asset/binding references, min/max constraints и browser preview renderer. Canvas/inspector работают с descriptors; новый block type не требует переписывать общий редактор.

Разделить registry, document/schema validation, layout resolution, canvas rendering, inspector, history/persistence и fixture data. Конкретные модули/файлы выбираются по фактическому состоянию tools/design-lab. Сохранить совместимость принятых Track Header layouts; не переименовывать их schema под MR Live.

Registry — код репозитория, а не механизм исполнения JavaScript из импортированного документа.

## 7. Контракт документа

Versioned JSON содержит:
- schema ID/version и layout target (mr-live-main; будущий mr-live-performance);
- layout ID/name, registry version;
- viewport/profile/container geometry и resize constraints в logical pixels;
- стабильные block instance IDs и type/version;
- положение, размеры, порядок, visibility, padding/gap;
- typography и MR UDS token overrides в разрешённых пределах;
- semantic asset IDs и action/data binding IDs.

Layout хранит представление. Audio assets, project/plugin state, DSP parameters и show transport position принадлежат shared project/show services. В JSON нет executable code, arbitrary external URLs, произвольной SVG-разметки или credentials.

Задать bounded file size/block count/nesting/text и finite numeric ranges. Проверять уникальность IDs, ссылки, type/version и geometry. Неподдерживаемый формат отклонять с понятной ошибкой; миграции поддерживаемых старых версий явные. Неизвестные block types нельзя молча терять. Ошибка импорта не заменяет последний валидный документ.

localStorage — восстанавливаемый browser draft, не долговременный файл и не автоматический GitHub save. Для переноса между устройствами нужен export/import. Хранилище рабочих раскладок внутри .mrlive определяется в отдельном runtime этапе с versioning/compatibility; v1 редактора не меняет формат пользовательских шоу.

## 8. Переход в JUCE

После утверждения макета реализовать native mapping block type → JUCE component и semantic binding → существующий service/command. Rendering, hit areas и constraints используют один согласованный layout contract; browser и native должны согласованно разрешать геометрию, typography и assets.

Экспорт из браузера сам по себе не меняет приложение. Утверждённый default layout включается в обычную сборку; внешние overrides/hot reload — только явный dev workflow с last-known-good. Browser Bridge остаётся отдельной #246.

Реальный Transport единственный; browser fixtures не переносятся в runtime как второй clock. Парсинг/загрузка layout, font/SVG preparation, file/network I/O выполняются вне audio callback; GUI updates — на message thread.

## 9. Приёмка

### Редактор #272
- Проверенный опубликованный URL.
- Сборка экрана из блоков, настройка размеров, шрифта, SVG, transport/toolbar без ручной правки JSON.
- Move/resize/snap/hidden layers/history/длинные подписи/empty states работают в реальном браузере по HTTP.
- Export/import и reload сохраняют раскладку; invalid/unsupported input сохраняет last-known-good.
- Расширяемость проверена добавлением fixture block через registry.
- Принятый Track Header editor работает; SVG references берутся из единой MR UDS.
- Есть пример layout и инструкция, пользовательская приёмка редактора.

### Основной интерфейс #28
- Пользователь утвердил конкретный JSON/layout revision и эталонные viewport.
- Определены состав панелей, transport/toolbar, resizing, typography и icons.
- Native реализация проверяется отдельно; фиктивные данные и ещё отсутствующие действия обозначены явно.
- Runtime #29 использует общие Project/Transport/Engine и сохраняет accepted behavior.

Статический редактор требует адресных schema/history/roundtrip проверок и browser smoke; DAW build и полный CTest ради документации/статического прототипа не нужны. JUCE функциональность и аудио проверяются по затронутому риску и AGENTS.md. Текущий документ не заявляет новых PASS, аппаратных замеров или готовности к концерту.

## 10. Каналы и sub-patches — browser demo, решение 2026-10-24

Пользователь определил четыре типа каналов: **Instrument**, **Player** (живой вход с обработкой), **Playback** (подготовленное воспроизведение), **Audio** (запись). В browser demo канал — общая сущность шоу; каждый item имеет соответствующий трек по `channelId`. Создание/переименование/удаление канала согласованно обновляет timeline, панель каналов и микшер. Audio предоставляет demo arm/monitor без реального захвата аудио. Это UI/model прототип, не изменение native TrackKind, Audio Engine или `.mrlive`.

Патч принадлежит каналу; выбор основного патча и его вариации хранится на треке конкретного item. **Sub-patch** — именованный набор overrides некоторых параметров основного патча. Он наследует тот же инструмент и ту же цепочку плагинов, не содержит собственных engine/plugin fields и не означает переключение инструмента/пересборку цепочки. Эффективные значения: параметры основного патча + overrides sub-patch; Base возвращает исходные значения. Прототип демонстрирует volume/pan/tone/mix; реальные plugin parameter IDs, automation, smoothing и RT-safe исполнение определяются при runtime integration отдельно.

Demo show schema v2 содержит channels, channel patches/sub-patches и item tracks с устойчивыми ID. Явная миграция browser v1 добавляет начальные каналы/треки, сохраняя items/folders/setlists. Layout JSON остаётся независимым. Проверки demo подтверждают ссылки, сохранение/Undo и неизменность описания инструмента/цепочки при выборе sub-patch; они не доказывают реальное DSP переключение без разрыва аудио.

## 11. Item workspace и панели — уточнение 2026-10-24

По решению пользователя Inputs / patches упраздняется как рабочий блок. Громкость, панорама и управление каналами сосредоточены в микшере, открываемом иконкой. Browser открывается другой иконкой и следует структуре MR Studio (FX, Files, MR Library) без AI assistant. В browser demo это каталожные fixtures; реальный доступ к диску/сканирование/hosting сюда не входят. Inputs остаётся скрытым legacy-блоком. Mixer / meters доступен в палитре конструктора и слоях как перемещаемый/растягиваемый блок; иконка также открывает самостоятельную runtime-preview панель. Полосы каналов имеют компактную фиксированную ширину и горизонтальную прокрутку. Оба представления используют один channel gain/pan/M/S; выбор item/patch/sub-patch не меняет общий фейдер канала Instrument, Player или Playback. Перекомпоновка сохранённого пользовательского layout предлагается отдельным preset, без тихого сдвига всех custom bounds.

Весь набор треков текущего item находится в одном сворачиваемом контейнере. Патчи — прямоугольные блоки с тематической иконкой; sub-patches — более плоские вложенные полосы (например Rhythm/Solo). Имена и semantic color roles сохраняются в demo show. Выбор strip сохраняет тот же base patch/engine description и меняет только parameter overrides. `patchOrder` хранит набор/порядок отображаемых патч-блоков на треке item; это не подменяет будущие musical-time patch events из #138/#32/#33. Цветовая маркировка не заменяет подписи и индикацию выбранного состояния.

## 12. Музыкальная геометрия item — уточнение пользователя 2026-10-24

Playback/Backing — дорожка аудиофайла, без назначаемых патчей. В прототипе отображаются filename и demo waveform; общая громкость этого канала доступна в Playback / stems и синхронизирована с микшером. File picker сохраняет только имя, без чтения/декодирования/воспроизведения аудио. Старые Playback patches при миграции остаются в archivedPatches, а их track assignments очищаются.

Таймлайн показывает ровно все entries активного setlist слева направо и в его порядке, включая повторы item. Каждый occurrence — свой контейнер, ссылающийся на общие данные item. Над всеми треками внутри item располагается дорожка song sections (куплеты/припевы и т. п.). Parts имеют имя, цвет, start/length в тактах, свободно перемещаются и растягиваются; перекрытия допустимы.

Патчи Instrument/Player имеют временные границы в item и handles слева/справа. Sub-patches расположены горизонтально внутри base patch, с относительными start/length, move/resize в его пределах. Изменение длины родителя пропорционально масштабирует вложенную геометрию. Во время жеста меняется только preview; завершение — одна undoable запись вне audio callback. В Preview Ctrl+Z / Ctrl+Shift+Z работает с demo-show history. Начальная длина item вычисляется из metadata duration/tempo/meter (BPM четвертями); эти данные не считаются измеренными характеристиками файла.

Browser demo show schema v3 содержит item bars/sections и track media/spans/sub-spans, с explicit migration v1/v2, bounds/reference validation, local draft и JSON roundtrip. Это сохраняемая музыкальная геометрия прототипа; реальный transport/event scheduling/DSP и native .mrlive не реализуются этой правкой.


### Стыковка и библиотека items

Патчи одного трека и sub-patches внутри патча не перекрываются. Перетаскивание тела ограничено соседями; растягивание общей границы одновременно меняет размер обоих соседей, с пропорциональным масштабированием вложенных sub-spans. Один жест сохраняется одним Undo. Числовые правки также проверяются; перекрывающийся импорт отклоняется. Старые локальные v3 drafts с перекрытиями восстанавливаются с упаковкой таких полос без удаления ID; новые blocks размещаются в свободном интервале либо делят самый длинный существующий блок.

Кнопка Browser в Setlist открывает библиотеку items справа рядом с блоком, сохраняя одновременно видимый setlist. Дерево папок раскрывается на месте. Drag из библиотеки на строку setlist вставляет ссылку перед этой строкой, на свободную область — в конец; исходный item остаётся в своей папке. Drag внутри setlist меняет порядок. FX/Files/MR Library — отдельный Browser по иконке toolbar.
