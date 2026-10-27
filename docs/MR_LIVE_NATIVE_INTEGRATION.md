# MR Live — передача принятого интерфейса в MR Studio

Актуальная сверка native реализации и задач на 2026-10-27: [матрица покрытия](MR_LIVE_INTERFACE_AUDIT.md). MR Live.exe запускается напрямую; Home Studio — дополнительный вход. Исторический статус браузерной передачи ниже не означает отсутствие уже принятого native runtime.

Уточнение пользователя: **Playback / stems** содержит общий Playback level выбранного item и индивидуальные уровни его stems. **Mixer** содержит постоянный общий фейдер каждого канала на всё show, неизменный при смене item/patch. Требуемый item Playback group gain (#292) отличается от существующего show-wide `nativeLiveAudio.gain`; это целевой контракт, а не уже завершённый binding. Mixer — #279, folds — #280.

Контракт этапа 0.8.6 (#279/#280/#292/#286): изменения полей item context применяются непосредственно к show с Undo, без Apply. Notes всегда видны под подзадачами. Сохранение `.mrlive` на диск остаётся явным; название show и `*` несохранённого состояния отображаются в верхней панели/заголовке окна. `Save for All` публикует версию item для Browser и не является кнопкой применения полей. `items[].playbackGroupGain` — индивидуальный общий уровень item (по умолчанию 0 dB для прежних show), native source row `gain` — stem, `channels[].gain/pan/mute/solo` — постоянный канальный mix. Прежний `nativeLiveAudio.gain` сохраняется как дополнительный совместимый show trim, не переписывая исходные уровни; Cue/Click остаются независимыми от item Playback group. Общий Core `Track::Mix` ограничивает суммарный gain: превышение его диапазона отклоняется до изменения show, без скрытого clamp. Входы/процессоры Instrument/Player/Audio по-прежнему относятся к #32; наличие их strip не доказывает готовый DSP binding. Приёмка этого этапа отслеживается в Issues.

Совместимость панорамы: `channels[].pan` в show/editor и Browser snapshots хранится в процентах **−100…100**, включая значения около центра ±1. `channelPan()` переводит его в Core scalar **−1…1**; normalized API `setLiveChannelMix`/preview сохраняет обратно `pan * 100`. Открытие прежнего `.mrlive` не переписывает его и не сбрасывает значения. Проверка процентов как normalized scalar в build 336 была регрессией `Invalid channel mix`.

Статус на 2026-10-25: **браузерный этап интерфейса завершён и принят пользователем; следующий этап — native JUCE UI и подключение реального SHARED Core MR Studio**. Завершение макетирования не означает готовность аудио/runtime.

Tracking: [#14 roadmap](https://github.com/vladleng/MR-Studio/issues/14), [#272 редактор](https://github.com/vladleng/MR-Studio/issues/272), [#28 native UI](https://github.com/vladleng/MR-Studio/issues/28), [#29 shared integration](https://github.com/vladleng/MR-Studio/issues/29), [#274 главная страница и Live Projects](https://github.com/vladleng/MR-Studio/issues/274), [#30 show lifecycle](https://github.com/vladleng/MR-Studio/issues/30). Исходники прототипа — PR #273, тематическая ветка `codex/mr-live-layout-editor`; merge в main не выполнен.

## Решение пользователя — самостоятельное приложение MR Live (2026-10-25)

Это уточнение имеет приоритет над прежними описаниями Live как workspace обычного Studio-проекта.

- MR Live — отдельное приложение `MR Live.exe`, запускаемое с Home MR Studio, со своим show-документом `.mrlive` и собственным workflow по принятому онлайн-редактору.
- В MR Live нет кнопки Studio, перехода в Arrange/Edit/Mix или открытия `.mrsproject` через item. Общий Core — повторное использование кода Engine/ASIO/Transport/Project Model/GraphStore/MIDI/plugin host, а не перенос Studio workflow в show.
- Studio → Live: пользователь планирует отдельную будущую функцию экспорта как Playback. Прямое открытие Studio-песни в Live не заменяет этот экспорт.
- Для пока не реализованных модулей пользователь разрешил статичные изображения-заглушки из принятого интерфейса (например `live_tuner.svg`); это не работающие показания или DSP.
- Принятый layout v5 и поведение онлайн-редактора сохраняются как reference переноса. Создавать новый макет или придумывать переходы нельзя.

## Принятый артефакт

Оригинальный файл пользователя сохранён без изменения байтов: [mr-live-layout-v5.json](../assets/ui/mr-uds/layouts/mr-live-layout-v5.json).

- SHA-256: `c95a3dd3258fa4d79085233eb98e4444903bfb3377e707eb591548f5ba9cb2f1`.
- `schema: mr-live-layout`, schema/registry version 1, target `mr-live-main`, viewport 1920×1080, 10 записей блоков.
- Это ревизия макета v5, а не schema version 5. JSON прошёл текущую bounded layout validation с каноническим manifest assets.
- Отдельные Header, Channels и Timeline; Setlist, Playback / stems и Status слева. Mixer скрыт до открытия. Старые `input.patches` и `control.transport` оставлены как совместимые legacy-записи и не должны возвращаться в native UI как дополнительные видимые панели.
- Тот же файл используется как `tools/design-lab/live/user-layout.json`; редактор: https://vladleng.github.io/MR-Studio/tools/design-lab/live/ .

Файл содержит геометрию, typography/tokens, semantic assets/bindings и видимость блоков. **Он не содержит audio/plugin state, содержимое setlists/items и Browser, настройки элементов кнопки item или задачи органайзера.** Эти данные прототипа находятся в demo show (`performance.itemCardLayout`, `itemTasks`, `itemWorkflow`, `library` и др.). Экспорт demo show передаётся отдельно от v5; не считать пользовательские данные извлечёнными из layout JSON. Публичные начальные настройки интерфейса находятся в `assets/ui/mr-uds/layouts/mr-live-interface-defaults-v1.json` (handoff d9e75b2). При импорте сохранённые настройки show имеют приоритет над этими defaults; пользовательский экспорт не включать в репозиторий или CI. Для native интеграции переносить согласованное поведение по спецификации и исходникам, а реальные документы — через model/commands/persistence MR Studio.

## Что завершено в браузерном этапе

Реализованы редактор блоков, import/export, responsive preview, Undo/Redo, канонические assets и опубликованный интерактивный макет. Основной workflow включает Setlist/Browser с папками и перетаскиванием, разделители выступлений, общий набор каналов, timeline items/sections/chords, patches/sub-patches и Playback clips, микшер и отдельный gain Playback внутри item, arm/record/takes/comp как демо-состояние.

Кнопки items настраиваются в конструкторе; Save for All сохраняет содержимое в Browser, точка показывает отличия. Статус и приоритет общие и синхронизируются без Save for All. В ПКМ-карточке — вложенный органайзер с прогрессом веток и компактные действия; его данные также общие. Органайзер в кнопках скрывается иконкой рядом с Browser. Browser items без шапки закрывается ЛКМ снаружи, кроме области Setlist. Подробный контракт: [MR_LIVE_LAYOUT_EDITOR.md](MR_LIVE_LAYOUT_EDITOR.md).

Это **модель поведения и визуальная приёмка**, а не работающие Audio Engine/ASIO/plugins/запись аудио. Browser waveform/tuner/meters/recording — иллюстрации либо симуляция; не переносить фиктивные значения в production UI.

## Следующий этап и вход в MR Live

1. **#274 — главная страница и браузер проектов.** Кнопка «Новый проект MR Live» создаёт show `.mrlive` и запускает отдельное приложение MR Live. В браузере проектов доступны вкладки обычных проектов и MR Live; переключение вкладки меняет список/тип документов, не backend. Предусмотреть открытие существующих Live projects, recent projects, сохранение/reopen и защиту несохранённых изменений.
2. **#28 — native UI по v5.** Общие MR UDS/JUCE components, layout/resize, фиксированные Header/Channels, синхронная прокрутка, настоящие focus/drag/keyboard и Undo/commands. JSON — reference/loader input, пока native loader не реализован; не утверждать, что браузерный JS автоматически исполняется в MR Studio.
3. **#29 — реальные bindings.** Project Model, Transport, musical context, Chord/Arranger/sections, GraphStore/channels/mixer, Playback, meters и запись через существующие shared services. Один Audio Engine, ASIO/device layer, Transport, Project Model, MIDI engine и plugin host. Без browser simulation как production backend и без второго аудиотракта.
4. **#30–#33 — профильные workflows.** Show persistence/refs/preload/preflight/switch safety, Playback/click/cue, patches/sub-patches и MIDI/control в соответствующих задачах. `.mrlive` хранит собственный show; будущий экспорт Studio → Live как Playback определяется отдельно. Общие библиотеки не вводят новый backend.

Performance Mode остаётся последующим отдельным направлением. Загрузка макета/SVG/fonts, сериализация и организация задач выполняются вне audio callback. Plugin switching и sub-patch parameter changes должны сохранять существующие lifetime/PDC/recording contracts.

## Проверка и критерии передачи

- Последний полный профиль прототипа перед handoff: 53 Node tests PASS, адресные HTTP Chromium smoke (органайзер, карточки, Browser/Setlist, сохранение и внеоконные клики) PASS. Макет v5 дополнительно проверен парсером и SHA-256; эта документационная передача не требует native сборки.
- Native интеграция выполняется поэтапно в PR #275; текущая реализованная граница описана ниже, полная native-приёмка остаётся открытой. Аппаратный ASIO, слуховая проверка, физический mixed-DPI, реальные плагины, audio recording/takes и концертная надёжность не подтверждены браузерными тестами.
- Приёмка следующего этапа: из Home создать/открыть Live show, переключить вкладки проектов без регрессии обычных проектов; показать v5 в native UI; управлять теми же Project/Transport/Engine; сохранить/reopen модель и проверить адресные model/command/persistence/native GUI tests. Публиковать точную source/build revision и отдельный результат аппаратных проверок, если они выполнялись.

## Native application/document implementation boundary (#274, #28, #29)

`MRLiveJuce` is a dedicated JUCE application target with its own entry/window,
identity and UI settings. It shares the existing host source files and Core
libraries with Studio. Home dispatches `.mrlive` creation/opening to `MR Live.exe`
without replacing the current Studio project. Studio releases its audio device
when launching Live; no new audio backend implementation is introduced.

The initial authoring document uses UTF-8 JSON `.mrlive`, schema `mr-live-show`,
version 2. `interface` stores show authoring data imported from browser show v3:
items, folders, setlists/dividers, saved Browser versions, workflow, recursive
tasks and card settings. Saved settings override embedded public defaults.
The development v2 container also has legacy `projects`, `active`, `itemProjects`
fields from the rejected workspace prototype. Dedicated MR Live accepts empty
legacy references only; it never opens a Studio song through them. Their presence
is not an accepted Studio↔Live workflow or a specification for future show audio
persistence. The user-provided browser export has no such project bindings.

Setlist/Browser/card/organizer authoring has show Undo/Redo and dirty tracking.
Save always enforces `.mrlive`, preserves metadata history and atomically replaces
the document. Browser simulation data remains opaque metadata and does not create
real audio, plugins or recording takes. The initial metadata import does not
claim completed Live audio integration.

The shared Core adapter still has development compatibility tests for old native
reference containers; those tests are not an exposed product workflow. Separate
application checks cover process entry, application identity, native show
save/reopen, Home dispatch, rejection of `.mrsproject`, blocked Studio workspace
shortcuts and absence of a Studio transition in the Live interface.

Full native Timeline, Playback, patches, MIDI, Live audio persistence and lifecycle
remain in #28/#29/#30–#33. Studio export to Live as Playback is a subsequent,
explicit workflow. Software previews do not certify physical ASIO/MIDI, hearing,
vendor-plugin behavior or mixed-DPI. Native visual acceptance remains open.

## Полный комплект передачи интерфейса

Для следующего чата передаются совместно:

1. Оригинальный `assets/ui/mr-uds/layouts/mr-live-layout-v5.json` — принятые блоки/геометрия.
2. `assets/ui/mr-uds/layouts/mr-live-interface-defaults-v1.json` — отдельно выгруженные исходные настройки кнопки item (revision 2), статусы/priority, переключатель органайзера и описание его необязательного шаблона. Это defaults из исходников, **не пользовательские данные браузера**.
3. Весь `tools/design-lab/live/` — работающая логика/модели/валидаторы/тесты; canonical assets `assets/ui/mr-uds/` и `assets/ui/icons/transport/`.
4. Этот handoff, `MR_LIVE_LAYOUT_EDITOR.md`, `LIVE_WORKFLOW.md`, `AGENTS.md` и GUI skill.

Этого комплекта достаточно для продолжения native реализации интерфейса и его согласованного поведения. Для точного восстановления пользовательских items, папок, собственных координат полей кнопки и задач нужен дополнительный **пользовательский demo show JSON**: в своём браузере открыть Files → сохранение demo show либо ПКМ Setlist → «Экспорт demo show» и приложить скачанный файл. Он хранится в localStorage пользователя (`mr-live-demo-show-v1`); среда разработки не имеет доступа к этому localStorage. Пока такого файла нет, не заменять его тестовыми данными и не заявлять восстановление пользовательского show.

## Первый runtime-срез #29: item Playback

В native show `interface.performance.nativePlayback[itemId]` хранит `file`, `gain` (dB, −60…+12), `mute`, `solo`, а также опциональные `trimStart`/`trimEnd` в секундах исходного WAV. Отсутствующие границы означают полный исходник. Это опциональное дополнение native show v2; browser export/layout v5 не изменяются. В памяти путь абсолютный, в `.mrlive` — относительно каталога show; Save as пересчитывает ссылку, не копируя исходный WAV. Состояние входит в show Undo/Redo. Ссылка на WAV и настройки Playback входят в itemVersion: добавление/замена аудио или изменение mix отмечает расхождение с Browser; Save for All обновляет saved version. Save show сохраняет и рабочие изменения, и Browser version, не подменяя Save for All. Относительные пути пересчитываются в обоих состояниях при Save as. Multistem mapping остаётся последующим срезом.

На первом этапе поддерживается один WAV на item в существующей панели Playback / stems. Рабочий Project создаётся штатным Core из этой ссылки, без открытия/создания `.mrsproject`; декодирование/ресэмплинг выполняет существующий WAV import worker, публикацию — `Application::poll`. Play/Pause/Stop и счётчик секунд используют общий EngineTransport; gain/Mute/Solo — SetTrackMix/shared mixer; peak и waveform берутся из Core, не из браузерной симуляции. Несвязанные browser timeline clips/patches этим автоматически не подключаются.

Основная Playback lane текущего item отображает Core-клип: настоящее имя WAV, длину и cached waveform. Координаты используют общий time map; клик в lane выполняет seek. Границы можно перетаскивать (одна show Undo-транзакция при отпускании), либо задать точно в полях In/Out (секунды) панели Playback и нажать Trim. Trim использует штатный Core `trim_clip`, сохраняет позицию на timeline и source offset, не меняет WAV и допускает обратное расширение до исходных границ. Trim разрешён после Pause/Stop; границы проверяются до записи show history, включая Undo/Redo. Невалидный диапазон относительно заменённого внешнего WAV блокирует воспроизведение до повторного подключения файла. Замена WAV сбрасывает trim, сохраняя mix. Trim входит в Browser saved version и Save for All.

Колонка первого Playback-канала управляет теми же gain/Mute/Solo, что и левая панель: общий Core mixer, show Undo/Redo и persistence. Остальные типы каналов пока не получают фиктивных audio bindings. Несколько независимых Playback-каналов и stem routing принадлежат следующему этапу #31. Неактивные items сохраняют реальные waveform через отдельный display cache, описанный ниже; это не предзагрузка playback.

Текущий handoff описан в разделе предзагрузки ниже: подготовка не останавливает текущий item, готовый playback заменяется с сохранением открытого устройства. Ошибка подготовки нового item сохраняет текущую модель/звук. Физические ASIO/MIDI/слух проверяются отдельно.

Плейхэд основного timeline использует общий компонент Studio `PlayheadLine` и наблюдаемую позицию Core через `VisualTransport`, без отдельного UI clock. Samples переводятся в координаты выбранного item по его темпу/размеру, сохранённым scale и lead. При выборе другого item показывается его начало; не подключённый к Playback item не отображает фиктивный плейхэд. Эти display bindings не означают готовую интеграцию секций/patches или time-stretch.

## Browser focus и музыкальные bindings

ЛКМ Browser активирует item и показывает его на timeline. Повторный выбор текущего item возвращает фокус без остановки аудио. Item вне активного setlist показывается отдельным Browser preview без автоматического добавления reference в setlist.

Tempo/meter, sections и authored chords выбранного item проецируются в общий Project через Core SetMusicalData. Следующая/предыдущая секция, section loop, ruler seek и section click используют общий MusicalTimeline/EngineTransport. Current/next section и current chord читаются из shared context; elapsed/remaining — из фактических samples/длины PCM. Изменение музыкальных данных требует Stop и выключенного loop. Show Undo отделён от истории автоматически создаваемого runtime Project.

Browser допускает перекрытия authored spans. Core использует одну музыкальную полосу: runtime range предыдущего события заканчивается на старте следующего; при одинаковом старте остаётся последнее. Авторские границы в show/saved versions не переписываются. Это правило действует только при проекции, не является time-stretch или редакторским изменением данных.

## Прямой запуск MR Live

`MR Live.exe` запускается самостоятельно из каталога runtime, без запуска MR Studio и окна выбора проекта. Без аргументов открывает последний успешно открытый/сохранённый `.mrlive`; при первом запуске — чистый пустой untitled show. Options → Open last show on startup отключает восстановление, чтобы всегда начинать с пустого show. Последний путь и режим хранятся в отдельных Live view settings, вне show. Unsaved changes сами не восстанавливаются: на закрытии/смене документа действует обычная защита сохранения.

`MR Live.exe "путь к show.mrlive"` и `--open-show "путь"` открывают указанный show приоритетно. Для Windows Open with можно выбрать `MR Live.exe`; автоматическая регистрация file association в этой поставке не выполняется. `--empty-show` принудительно открывает пустой show, `--new-show "путь"` сохраняет совместимость с созданием через Home. Повторный запуск без аргументов только фокусирует уже открытое рабочее окно; перед явной сменой show проверяются несохранённые изменения. Отсутствующий/повреждённый последний документ оставляет пустое рабочее окно с неблокирующим сообщением, без перезаписи файла.

Files → New MR Live show создаёт пустой show сразу, без предварительного Save dialog. Первый Save запрашивает `.mrlive` path; первый edit включает dirty/close protection, Undo до исходного пустого состояния снимает dirty. Общий Core и независимый Live workflow сохраняются.

## Multistem и физические выходы (#31)

Native show v2 сохраняет прежний `nativePlayback[itemId]` как основной WAV и добавляет опциональный массив `stems` (до 31 дополнительного WAV). Каждый дополнительный stem имеет `id` общего Playback-канала, `file`, `gain`, `mute`, `solo`, опциональные `trimStart`/`trimEnd` и `outputs`. Основной WAV использует первый Playback-канал; дополнительные используют остальные свободные Playback-каналы или создают канал с соответствующими tracks всех items. Формат старых single-WAV show остаётся валидным. Удаление stem удаляет media binding, не физический WAV или общий канал.

`outputs` — пустой массив/отсутствие для Master, один физический индекс для mono или два для stereo, zero-based. Физические выходы сначала включаются в Audio settings. Маршрутизация использует общий Core SetHardwareOutput и его PDC/graph lifecycle, без второго тракта; direct output не дублируется в Master. Изменение маршрута требует Pause/Stop. Недоступный сохранённый route блокирует Play с сообщением; автоматически переводить его на Master нельзя. Пользователь может явно выбрать Master или включить нужные выходы. Предзагрузка/переход без dropouts этим не заявляются.

Playback/stems → `+` добавляет несколько WAV одним выбором; selector выбирает stem для WAV replacement/trim/output, `-` удаляет дополнительный stem с Undo. На основной timeline каждый привязанный Playback-канал показывает свой реальный waveform/имя/длительность; его gain/Mute/Solo управляют соответствующим Core track. Все WAV импортируются одним существующим background batch без target track: отдельные clips имеют общий start sample 0 и один EngineTransport. Trim сохраняет исходное соответствие sample→source; никаких отдельных UI clocks для stems нет. Рабочая длина item/remaining учитывает самый длинный клип.

Все binding/mix/trim/routes входят в show Undo/Redo, Browser item content и Save for All; relative paths/Save as пересчитываются для каждого stem и saved Browser version. Перед публикацией проверяется весь набор trim/routes. Shared Core import отменяется при смене item; prepare/decode остаются вне callback. Предзагрузка и seamless transitions остаются дальнейшим объёмом #30/#31.

Для `[ci live]` оба адресных smoke выполняются через `MR Live.exe`; отдельное приложение MR Studio и его Home smoke в этом scope не запускаются. Обычный profile сохраняет прежнее покрытие Studio entry. В scope live собираются MRLiveJuce и адресные mrs_audio_tests / mrs_live_preload_tests (только audio_metronome / desktop_live_preload), упаковывается отдельный MR Live runtime без Studio EXE; обычный profile сохраняет combined package. В manifest указан application, verifier проверяет соответствующий набор EXE.

## Click, Cue и общий Playback level (#31)

`performance.nativeLiveAudio` — опциональные show-настройки: `gain` (-60..12 dB) и `click` (`enabled`, `accent`, `level` 0..100, `outputs`). Отсутствие сохраняет старое поведение: 0 dB, Click off. UI: Options → Playback / Click / Cue; общий graphite context с пользовательской прозрачностью. Изменения входят в show Undo/Redo и `.mrlive` v2, без миграции и без записи в Studio preferences.

Click использует существующий Core Metronome и tempo/meter текущего item, общий seek/loop/Transport. Можно запускать item с Click без WAV. `ClickSettings` хранит физические выходы в Application; перед публикацией в bounded control queue и подготовкой RenderGraph они переводятся в индексы выбранного stream. Пустой маршрут следует Master; mono/stereo direct route не проходит через Master и не дублируется в нём. Та же маршрутизация действует для существующего count-in Core; Live count-in UI этим не добавлен. Смена route требует Pause/Stop; недоступный сохранённый выход блокирует Play и выключает Click до явного исправления/смены устройства. В callback добавлено только ограниченное смешивание и чтение scalar control, без alloc/free/locks/I/O/UI.

WAV-stem может иметь опциональный `cue: true` (кнопка Cue в Playback/stems). Это готовая пользовательская cue-аудиодорожка в том же batch, с общим sample 0/Transport, своими gain/Mute/Solo/trim и `outputs`; генерация речи/новый cue editor не входят в этап. Общий Playback gain умножает gain музыкальных tracks через существующий Core mixer, включая direct outputs, и не меняет Cue/Click. Поэтому он не реализован через Master gain. Solo пока остаётся общим стандартным Solo Core, без отдельной cue solo-группы. Флаг Cue входит в Browser saved item/Save for All; общий gain и Click — настройки show.

## Waveform неактивных items

Видимые связанные WAV получают display-only cache реальных Core Waveform в исходном sample rate, включая trim и длину на timeline. Кэш существует независимо от рабочего Project и не исчезает при выборе другого item. Один отменяемый background worker использует штатные open_wav/Waveform; PCM после расчёта освобождается, peak pyramids ограничены Core. Не более 64 уникальных видимых источников за раз, сохранение ранее просмотренных до вытеснения; изменение размера/mtime WAV инвалидирует изображение. В paint нет файлового I/O или декодирования; отсутствующий/невалидный WAV показывает ошибку. Это только визуальный кэш, не preload всего show, а runtime использует отдельную подготовку из следующего раздела.

## Предзагрузка следующего item и безопасный manual handoff (#30/#31)

Application.prepare_playback переиспользует PreparedWavImport: тот же WAV inspect/decode/resample, один batch, без публикации в текущий Project. Job не зависит от revision текущего item; mix/Undo и сам playback не отменяют подготовку. Native controller хранит до трёх наборов (текущий, следующий, ранее выбранный), автоматически готовит следующий валидный item активного setlist, пропуская dividers. Transient entry cursor различает повторные ссылки на один item; Setlist/timeline передают entry ID, Browser reselect сохраняет текущую позицию. Cursor ещё не является сохранением позиции/recovery show. Один набор удерживает до 128 MiB декодированного PCM; остальное остаётся file-backed. Отменённые workers удерживаются Core до завершения, максимум шесть jobs, без join в обработчике выбора. Закрытие Application отменяет и присоединяет workers вне callback.

Перед ручной активацией проверяются исходные size/mtime, sample rate, все trim и физические routes, Click и модель. Изменившийся WAV требует повторного выбора/явного Choose WAV, который сбрасывает даже same-path cache. Невалидная подготовка не переключает выбранный item и не останавливает текущую композицию; Status показывает ошибку. Esc отменяет ожидающее переключение. Новый выбор заменяет pending request; завершение старого worker не публикует устаревший item. Preload/status — transient runtime, не изменяют `.mrlive`, Undo, saved dots.

Готовый набор передаётся через install_playback в тот же Application / AudioEngine / GraphStore и общий render builder. Весь graph/asset setup выполняется вне callback. Действующий callback владеет прежними immutable assets, пока новый граф готовится; затем выполняется stop → quiesce → prepare → start **с тем же открытым аудиоустройством**, без close/open, смены backend или второго Engine. Project/Transport/musical projection меняются согласованно; runtime Project получает чистую историю, пользовательский show Undo остаётся прежним. До успешного старта сохранены старые модели/graph/assets; отказ старта откатывает и возобновляет прежний playback. Assets и временные resampled WAV освобождаются вне callback только после ухода старого графа.

Это срез WAV/Click/Cue: активные инструменты, inserts/hosted plugins/ARA не заменяются этим handoff; их lifecycle относится к #32/#33. Ручной выбор начинает новый item в sample 0 и Stopped, без неявного autoplay. Options → Previous item / Next item использует тот же путь; Status показывает следующий item и readiness его media. Callback не получает нового alloc/free/locks/I/O. Стресс-проверка fake device проверяет многократные переходы, PCM/routes/tempo, сохранение одного Engine/одного open и rollback; её время не является физическим ASIO latency.

После явного Disconnect редактирование/загрузка WAV остаются доступны: install_playback включает штатные Core Offline clock, если открытого устройства нет. Это не переоткрытие существующего ASIO handle; физические routes по-прежнему валидируются, недоступные выходы не downmix в Master.

Runtime preload и устранение повторного импорта реализованы, но **stop/start остаётся границей ручного handoff**. Автоматический sample-boundary переход на конце item, no-dropout во время непрерывного воспроизведения, per-song end policy и физический ASIO stress этим не заявляются и остаются последующим объёмом #30/#31.
