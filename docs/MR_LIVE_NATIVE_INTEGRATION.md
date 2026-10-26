# MR Live — передача принятого интерфейса в MR Studio

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

В native show `interface.performance.nativePlayback[itemId]` хранит `file`, `gain` (dB, −60…+12), `mute`, `solo`. Это опциональное дополнение native show v2; browser export/layout v5 не изменяются. В памяти путь абсолютный, в `.mrlive` — относительно каталога show; Save as пересчитывает ссылку, не копируя исходный WAV. Состояние входит в show Undo/Redo. Ссылка на WAV и настройки Playback входят в itemVersion: добавление/замена аудио или изменение mix отмечает расхождение с Browser; Save for All обновляет saved version. Save show сохраняет и рабочие изменения, и Browser version, не подменяя Save for All. Относительные пути пересчитываются в обоих состояниях при Save as. Multistem mapping остаётся последующим срезом.

На первом этапе поддерживается один WAV на item в существующей панели Playback / stems. Рабочий Project создаётся штатным Core из этой ссылки, без открытия/создания `.mrsproject`; декодирование/ресэмплинг выполняет существующий WAV import worker, публикацию — `Application::poll`. Play/Pause/Stop и счётчик секунд используют общий EngineTransport; gain/Mute/Solo — SetTrackMix/shared mixer; peak и waveform берутся из Core, не из браузерной симуляции. Несвязанные browser timeline clips/patches этим автоматически не подключаются.

Смена item останавливает старый runtime и отменяет его незавершённый импорт штатным lifecycle Core. Ошибка/отсутствие файла блокирует Play и показывает причину. Переключение на этом этапе не является seamless/preloaded: безопасная концертная предзагрузка/переходы принадлежат #30/#31. Физические ASIO/MIDI/слух проверяются отдельно.

Плейхэд основного timeline использует общий компонент Studio `PlayheadLine` и наблюдаемую позицию Core через `VisualTransport`, без отдельного UI clock. Samples переводятся в координаты выбранного item по его темпу/размеру, сохранённым scale и lead. При выборе другого item показывается его начало; не подключённый к Playback item не отображает фиктивный плейхэд. Эти display bindings не означают готовую интеграцию секций/patches или time-stretch.

## Browser focus и музыкальные bindings

ЛКМ Browser активирует item и показывает его на timeline. Повторный выбор текущего item возвращает фокус без остановки аудио. Item вне активного setlist показывается отдельным Browser preview без автоматического добавления reference в setlist.

Tempo/meter, sections и authored chords выбранного item проецируются в общий Project через Core SetMusicalData. Следующая/предыдущая секция, section loop, ruler seek и section click используют общий MusicalTimeline/EngineTransport. Current/next section и current chord читаются из shared context; elapsed/remaining — из фактических samples/длины PCM. Изменение музыкальных данных требует Stop и выключенного loop. Show Undo отделён от истории автоматически создаваемого runtime Project.

Browser допускает перекрытия authored spans. Core использует одну музыкальную полосу: runtime range предыдущего события заканчивается на старте следующего; при одинаковом старте остаётся последнее. Авторские границы в show/saved versions не переписываются. Это правило действует только при проекции, не является time-stretch или редакторским изменением данных.
