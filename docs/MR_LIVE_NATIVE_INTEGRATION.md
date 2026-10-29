# MR Live — передача принятого интерфейса в MR Studio

## Show-owned channel bindings (#302 A2)

### Уточнение типов каналов и выбора плагинов — 29.10.2026

Целевой контракт пользователя (реализация и приёмка отслеживаются в #302 и #281):

| Тип канала | Источник и обработка |
| --- | --- |
| Instrument | MIDI device/port + channel/Omni → VSTi → audio FX → выход. Физический audio input здесь не предлагается. |
| Player | Живой mono/stereo audio input (гитара, микрофон, внешний инструмент) → audio FX → выход. MIDI/VSTi не предлагаются. |
| Audio | Самостоятельная дорожка аудиозаписи с audio input, Monitor/Arm и audio FX; без VSTi. Реальный capture остаётся #284. |
| Playback | Подготовленные WAV/stems; не live input и не VSTi. |

Instrument и Player имеют дочернюю запись по #284; это не делает их одним типом. Общие channel mix, routing, patch/subpatch и один Core сохраняются. VSTi здесь означает инструмент в формате VST3, а не поддержку старого формата VST2.

В контекстных селекторах канала и Mixer и в Browser использовать общий catalog с классификацией Instrument/Audio Effect по metadata конкретного plugin class ID. Выбор VSTi (создать/заменить инструмент) отделён от добавления post-instrument FX. Player/Audio показывают только audio effects. Внутри каждого подходящего раздела: **папка вендора → плагины**, вендоры и названия плагинов сортируются по алфавиту без учёта регистра; стабильный tie-breaker — class ID/path. Пустой vendor попадает в «Без вендора». Неизвестный тип не считать эффектом: показать отдельно как недоступный для назначения с предложением обновить scan metadata. Открытие меню не сканирует и не загружает плагины.

Те же ограничения действуют при drag/drop, через команды назначения и при активации сохранённого профиля. Старые show с audio-профилем на Instrument не переназначать и не удалять молча: сохранить descriptor, показать конфликт и предложить явное Undoable преобразование Instrument → Player с сохранением channel ID, mix, chain и item/patch связей. Открытие show не активирует плагины/Monitor. Срез исходников после Windows #378 (коммиты `[skip ci]`): Live channel header и Mixer используют общий read-only catalog view с раздельными VSTi/Audio FX меню, папками вендоров, стабильной сортировкой и точным `class ID` для выбора. Unknown class metadata недоступны для нового назначения; backend проверяет роли и тип конкретного класса при назначении/активации. Физический audio input на Instrument теперь запрещён для новых назначений. Старый audio-профиль Instrument остаётся в `.mrlive` и требует явной Undoable команды `Convert to Player`, которая сохраняет ID, mixer state, цепочку и item/patch связи. Это **SOURCE ONLY, после #378 не компилировалось и на Windows/ASIO/MIDI/VST3 не проверено**. Отдельный runtime Browser FX/Files/MR Library, drag/drop из него и полная A2-приёмка остаются задачами #281/#302.

Native channel headers and Mixer embed Input, Output, FX/VSTi and explicit Monitor controls. Audio channels select named mono/stereo device ports; Instrument channels can select a shared Core MIDI port/channel and a VST3 instrument followed by effects. Plugin editors use the existing host. Channel settings have no separate configuration window or Save/Load action: ordinary `.mrlive` Save captures every active channel's component/controller state and retains inactive/missing descriptors. Open restores descriptors only, without loading processors or enabling audio/MIDI monitoring. An explicit Monitor action validates the saved device, input pair, named outputs, MIDI port and modules before activation. **Open editor** in a channel's VST3 slot is also an explicit activation request: if the Core track is currently inert, prepare the channel and VST3 host with Monitor still OFF, then attach the vendor GUI; a missing module/device or stale slot refuses the request without changing the saved show. The option must not be disabled merely because a saved channel has not yet been monitored. Hardware/vendor editor crashes remain a separate acceptance gate; Windows diagnostics for MR Live are under `%APPDATA%\\MRLive\\logs`.

Show channel IDs map to stable Core track IDs. `Application::update_live_channels` prepares one candidate graph off callback, retains unchanged processor instances, and publishes model/device changes through a rollback transaction. `PlaybackLayout::live_tracks` carries all active channel IDs across stopped manual WAV/stem/empty-item handoffs, including Monitor OFF channels. WAV sources remain first in the runtime track order. Playing item switches and staged/automatic handoffs with active channel runtimes remain guarded by #301. No additional Engine, ASIO device owner, MIDI bridge or VST3 host is introduced.

Limits remain explicit: 64 Live input channels, the existing 128 mixer-track budget and 32 inserts per Core project, at most eight inserts per channel. Saved A1 audio profiles remain readable; additive stereo/output names prevent new bindings from silently following renamed ports. MIDI profiles use channel profile version 2; older builds reject that profile rather than interpreting MIDI as audio. Missing hardware/modules leave the show configuration available for explicit reselection. Audio and physical MIDI/VSTi acceptance are separate gates; source implementation and synthetic tests do not constitute Windows/hardware acceptance.


## #32b2 / #299: два уровня совместной работы Live Input + WAV

**Этап #300 (источники реализованы, Windows build и физический ASIO ещё NOT TESTED):** к `Application::PlaybackLayout` добавлена явная ссылка `live_track` на уже подготовленный живой audio channel. Если Playback **остановлен**, `install_playback` останавливает callback и workers до повторного использования `PreparedGraph` VST3; добавляет тот же monitor track (stable Core ID, physical input, hardware outputs и effects) после WAV stems в единственный runtime Project, сохраняет ссылку на существующий host insert, затем подготавливает общий RenderGraph и запускает прежнее устройство. WAV/stems/Click/Cue остаются источниками общего графа. При возврате к пустому item WAV исчезает, Live Input остаётся. Функция `Save Show` сохраняет только `.mrlive`, а не временный Core-проект; региональные Playback правки используют первые source tracks и не могут переписать последний Live track. Добавлен GUI smoke с fake input, WAV, native Insert, audio output comparison, blocked active item change и повторным manual selection.

**Ограничение #300:** на время ручного переключения Playback требуется **Stop**; device quiesce неизбежно прерывает живой мониторинг на границе ручного перехода. Это **не бесшовная смена композиций** и не гарантирует непрерывность хвостов во время самой перестройки; только *тот же экземпляр* VST3 и параметры остаются после неё. Автоматический next-item staging с активным Live Input по-прежнему выключен. Нельзя переиспользовать mutable VST3 graph одновременно в исходном и staged graph.

**Этап #301 (не реализован):** RT-safe separation persistent/live processing from replaceable WAV/stem state; immutable ahead-prepared WAV graph + one live plugin owner, bounded graph publication, safe tail/lifetime/PDC and smooth automatic playback transition while audio running. Это требует целевых RT-тестов и пользовательской физической приёмки ASIO/VST3/128 frames. #299 остаётся открытой до #301; #289 — отдельные patch/subpatch переключения.

## Исторический срез #32b1 / #298 — show-owned Live Input profiles (2026-10-29)

Workflow ниже относится к A1; действующий контракт A2 описан в начале документа.

Пользователь принял одноканальный Live Input/VST3 pilot (#297). В самостоятельном `.mrlive` теперь предусмотрено необязательное `channels[].liveInput` (version 1): физический input index + точная метка разъёма, mono/stereo, routing, Core Track::Mix, список VST3 (class ID/path/name, bypass, параметры, component/controller state). Данные ограничены по размеру и валидируются на чтении; старые show без профилей продолжают открываться. Live profile — состояние шоу, а не ссылка на `.mrsproject` и не самостоятельный Engine.

Действия `Options → Live Input (pilot)`: выбрать show-канал Instrument/Player/Audio, включить один временный hardware monitor, добавить VST3 через общий host, `Save to Show`, сохранить сам `.mrlive`. Для восстановления: открыть show, подключить ASIO с прежним физическим именем input, выбрать show-канал и нажать `Load from Show`. Загруженный Core track **начинает с Monitor OFF**: явное `Enable Monitor` обязательно. Никакого автооткрытия VST3/ASIO или автозвука при запуске show. Отсутствие плагина или смена input-label должны выдавать ошибку без подмены сохранённого профиля. Пилотный проект/track transient; сохранение `.mrlive` не должно создавать Studio song. Поддержан один активный input; пункт #300 допускает **только ручную** смену items с остановленным Playback при активном мониторинге, автоматическая подготовка WAV-only graph по-прежнему блокируется.

Этап сохранения #298 принят пользователем на Windows build 371. Совместная работа через один граф при остановленном Playback развивается в #300; непрерывный автоматический handoff — #301. Перенос параметров при смене патчей — #289, запись — #284, physical ASIO/vendor plugin/128 frames — отдельная Windows приёмка. Актуальные PASS/FAIL/NOT TESTED см. #298; не считать source commits тестом без Windows build.

Актуальная сверка native реализации и задач на 2026-10-27: [матрица покрытия](MR_LIVE_INTERFACE_AUDIT.md). MR Live.exe запускается напрямую; Home Studio — дополнительный вход. Исторический статус браузерной передачи ниже не означает отсутствие уже принятого native runtime.

Уточнение пользователя: **Playback / stems** содержит общий Playback level выбранного item и индивидуальные уровни его stems. **Mixer** содержит постоянный общий фейдер каждого канала на всё show, неизменный при смене item/patch. Требуемый item Playback group gain (#292) отличается от существующего show-wide `nativeLiveAudio.gain`; это целевой контракт, а не уже завершённый binding. Mixer — #279, folds — #280.

Контракт этапа 0.8.6 (#279/#280/#292/#286): изменения полей item context применяются непосредственно к show с Undo, без Apply. Notes всегда видны под подзадачами. Сохранение `.mrlive` на диск остаётся явным; название show и `*` несохранённого состояния отображаются в верхней панели/заголовке окна. `Save for All` публикует версию item для Browser и не является кнопкой применения полей. `items[].playbackGroupGain` — индивидуальный общий уровень item (по умолчанию 0 dB для прежних show), native source row `gain` — stem, `channels[].gain/pan/mute/solo` — постоянный канальный mix. Прежний `nativeLiveAudio.gain` сохраняется как дополнительный совместимый show trim, не переписывая исходные уровни; Cue/Click остаются независимыми от item Playback group. Общий Core `Track::Mix` ограничивает суммарный gain: превышение его диапазона отклоняется до изменения show, без скрытого clamp. Входы/процессоры Instrument/Player/Audio по-прежнему относятся к #32; наличие их strip не доказывает готовый DSP binding. Приёмка этого этапа отслеживается в Issues.

Совместимость панорамы: `channels[].pan` в show/editor и Browser snapshots хранится в процентах **−100…100**, включая значения около центра ±1. `channelPan()` переводит его в Core scalar **−1…1**; normalized API `setLiveChannelMix`/preview сохраняет обратно `pan * 100`. Открытие прежнего `.mrlive` не переписывает его и не сбрасывает значения. Проверка процентов как normalized scalar в build 336 была регрессией `Invalid channel mix`.

Уточнения интерфейса после build 337:
- Меню Setlist использует ту же owned context surface с 90% opacity, что и item. ПКМ передаёт координаты события до смены фокуса; кнопка «…» — нижний край собственной кнопки. Позиция переводится из screen в координаты окна и ограничивается его границами.
- Mixer — нижняя панель от левого края **сетки таймлайна**, на всю её ширину; открытие резервирует высоту под панель.
- Playback на таймлайне — контейнер. Его раскрытие (`performance.stemsExpanded`) вставляет дочерние строки непосредственно под ним, включая первичный WAV; сворачивание скрывает эти строки. В родительской строке отображается среднее реальных peak envelopes всех её источников, с учётом sample rate, source offset и trim. Это визуальное представление, не новый audio mix/backend.
- **Playback / stems** всегда показывает список всех стемов текущего item: узкая волна и отдельный узкий горизонтальный фейдер на строке; общий item Playback level — сверху. Сворачивание дорожки не скрывает список. Файл/trim/route/Cue доступны через «…» соответствующего стема. Изменение фейдера адресует item/stem по стабильным IDs и не переключает выбранный stem.
- Это перенос представления над существующими nativePlayback/channels: IDs, источники, уровни, routing и Core lifecycle не пересоздаются. Раскладка дочерних строк — view projection, она не меняет item content dirty.

Контракт native этапа 0.8.7 (#282, уточнения #279/#286), принятого в build 347; дополнения Sections/Chord для 0.8.8:
- Sections и Chord Track редактируются двойным кликом, контекстными свойствами/цветом и drag по общей сетке в четверть такта. Sections используют шаблоны Intro/Verse/Chorus/Bridge/Solo/Outro; аккорды — Add/Edit/Duplicate/Remove, Enter/Delete и Alt-drag copy с новым ID. С 0.8.8 новые правки обеих полос запрещают перекрытия. Свободные края используют bracket cursor; общий стык — двустороннюю стрелку и атомарный resize обоих соседей с минимумом 1/4 такта. Ранее сохранённые authored пересечения не переписываются при загрузке; Core projection по-прежнему умеет прочитать старый show.
- Внутренний origin раскрывает lead-in, `−/+` меняют только scale, правая граница — только `performance.arrangement[itemId].length`. Явный конец больше не расширяется обратно по WAV. Сокращение скрывает события/клипы, сохраняя исходные bars и source ranges. Ruler, разметка, patches, waveform и playhead используют один origin/scale. Исполнение end policies подключено в исходниках второго среза #287; native/ASIO приёмка этого среза ещё впереди.
- Drag меняет только view preview; release — одна show Undo transaction, Escape/потеря фокуса — отмена. Редактирование активной музыкальной разметки сохраняет guard Stop + выключенный section loop; изменение представления не публикует музыкальную модель каждый кадр. Отдельного транспорта нет.
- В Mixer остаётся общий Playback strip, без отдельных strips его stems. Этот постоянный channel mix действует на все музыкальные stems; индивидуальные item/stem уровни остаются в Playback / stems. Прежние дополнительные channel trims сохраняются в модели. Cue не получает общий музыкальный Playback multiplier; Click независим. Group preview/cancel используют ту же очередь Core mixer, без новых операций в callback.
- Notes: Enter вставляет новую строку; текст применяется без Save/Apply через компактное coalesced Undo-действие. Изменение Notes не пересоздаёт дерево задач; сохранение `.mrlive` на диск остаётся явным.

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

ЛКМ Browser активирует и фокусирует item только если он уже находится в активном setlist. Выбор item вне setlist остаётся выбором в библиотеке: не меняет активное воспроизведение и не создаёт временный item на timeline. Для появления на timeline требуется явное добавление в setlist. Повторный выбор текущего item возвращает фокус без остановки аудио.

Tempo/meter, sections и authored chords выбранного item проецируются в общий Project через Core SetMusicalData. Следующая/предыдущая секция, section loop, ruler seek и section click используют общий MusicalTimeline/EngineTransport. Current/next section и current chord читаются из shared context; elapsed/remaining — из фактических samples/длины PCM. Изменение музыкальных данных требует Stop и выключенного loop. Show Undo отделён от истории автоматически создаваемого runtime Project.

Browser допускает перекрытия authored spans. Core использует одну музыкальную полосу: runtime range предыдущего события заканчивается на старте следующего; при одинаковом старте остаётся последнее. Авторские границы в show/saved versions не переписываются. Это правило действует только при проекции, не является time-stretch или редакторским изменением данных.

## Контракт Instrument на Timeline и Mixer — 0.8.9-fix1 (#302/#281)

В `MR Live` общая модель show-канала и один VST3 host не меняются, но
контекстные органы управления разделены по назначению:

- **Timeline / Instrument:** `MIDI Input` показывает *имя реально подключённого*
  порта; если сохранённый порт не найден — `Missing MIDI`, если не выбран —
  `MIDI Off`. Второе поле — `MIDI Channel` (`Omni` или 1–16) **вместо**
  маршрута `Master`. Третье поле выбирает/заменяет **только VST3 Instrument**
  в первом insert slot, позволяет открыть его редактор.
- **Mixer / Instrument:** содержит **физический выход / Master** для сохранения
  ранее реализованной маршрутизации и **Audio FX после VSTi**. Выбор
  инструмента не дублируется, MIDI Input/Channel на этой компактной полосе
  не повторяются. FX и состояния сохраняются в общем `.mrlive` show.
- **Player/Audio:** сохраняют прежние физические input/output и Audio FX;
  выходы/маршрутизация AudioEngine не изменяются. Перемещение Instrument VSTi
  в другой FX-slot запрещено как прежде. Выбор MIDI-порта не обнуляет ранее
  заданный MIDI Channel.

Метка `fix1` применяется к **Live runtime artifact / README / manifest label**
после версии `0.8.9`, а не к PE FileVersion/публичной версии продукта. Это
сборка с source fixes классов VST3 cache, editor-open и значений MIDI no-input
(`Core=-2`, сохранённый `.mrlive=-1`), ожидающая приёмки на Windows. Не считать
`CI PASS` подтверждением открытия реальных сторонних GUI или отсутствия crash.

## Разделение ARA2 между MR Studio и MR Live (#303; 0.8.9)

**MR Studio** остаётся ARA2 host при `MRS_BUILD_ARA=ON`; GUI Studio, ARA scanner,
ARA editor и профильные ARA-тесты сохраняются. **MR Live** — самостоятельное приложение
без ARA2 (без `MRS_HAS_ARA`, ARA host/library, scanner и ARA-функций интерфейса),
но с прежними общими Core, ASIO, VST3/VSTi, MIDI, Transport и .mrlive persistence.
Это граница *сборочной цели*, а не выключатель ARA в Studio-проекте и не второй
AudioEngine/plugin host.

При сборке Studio с ARA ON `MRLiveJuce` линкует отдельную ARA-free компиляцию
того же `mrs_desktop` (`mrs_desktop_live`) для исключения транзитивного
`mrs_ara`. Общие аудио/плагин библиотеки не дублируются. Если глобальный
`MRS_BUILD_ARA=OFF`, штатный `mrs_desktop` уже ARA-free и используется напрямую.
`LiveMain.cpp` содержит compile-time guard на случай утечки `MRS_HAS_ARA`.

Только для адресного профиля `checks=live` Actions принудительно устанавливает
ARA OFF **до подготовки SDK/CMake**: не скачивает ARA SDK, не собирает ARA
fixture/scanner. Профиль Studio и полная интеграция сохраняют обычный ARA ON
по умолчанию; явный ARA OFF для Studio остаётся диагностическим режимом.
Отдельный `MR-Live-0.8.9-Windows` пакет имеет `ara=false` и не включает
`mrs_ara_scan.exe` или notices ARA SDK, что проверяет `Verify-Package.ps1`.
Combined Studio+Live package при ARA ON по-прежнему включает ARA scanner
**для Studio**; находящийся рядом `MR Live.exe` не использует его.

Это исходники этапа #303, не заявление о Windows build PASS или аппаратной
приёмке. `#302` (Live channels A2) и `#281` (plugin browser) отслеживаются
отдельно. Не путать ARA с VST3-инструментами и эффектами реального времени.

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

## Монтаж Playback-регионов (0.8.8, #283)

`performance.clips[trackId]` (`id/start/length/offset` в тактах) проецируется в несколько настоящих Core clips на одном stem track. Native-only source без browser track хранит такие же опциональные `regions` в своём nativePlayback binding. Отсутствующее поле означает прежний полный source window; явный пустой массив означает удалённые регионы и не восстанавливается автоматически. Сохранение `.mrlive` и item saved version включает эту разметку. Замена WAV сбрасывает нарезку только заменяемого источника; Undo возвращает её вместе с прежней ссылкой.

Pointer перемещает регион без изменения source offset. Постоянный Scissors режет по сетке 1/4 такта; следующий разрез сохраняет накопленный offset. Обрезка края работает в обоих инструментах, левый край меняет start/offset совместно. Context properties/Delete и Restore original region доступны на source lane; Enter/Delete работают для выделенного региона. Restore возвращает полный исходник, WAV не переписывается. Сокращение item только скрывает регионы. Для составного Playback сначала раскрываются stems; одна source lane доступна и в свёрнутом контейнере. Секундные source-window controls в sidebar отключены при явной нарезке; для неё используются края и свойства региона.

Preview не меняет Model; release — одна Show Undo transaction; Escape/blur отменяют жест, stale baseline не коммитится. Изменение активного аудиомонтажа требует Pause/Stop и отсутствия записи. Core preflight проверяет все регионы/источники до Show Undo, существующие бюджеты 128 voices / 32 streamed voices / 256 MiB read-ahead сохраняются. Перекрывающиеся аудиорегионы суммирует общий Core mixer; запрет перекрытия музыкальной разметки не распространяется на аудио.

Prepared media cache по-прежнему зависит от WAV, а layout регионов применяется отдельно при активации следующего item. Редактирование текущего layout использует существующий Core command/graph publication, не открывает второй device/engine и не добавляет операций в audio callback. Транзиентные source descriptors сохраняют возможность восстановления после удаления всех регионов. Отсутствующий WAV и выход за его реальный extent остаются ошибкой; явное исправление разметки повторяет активацию без скрытого переписывания проекта.

## Режимы исполнения: authoring и подготовка policy (#287, первый срез)

Native хранит те же `performance.modes[targetId] = normal/skip/stop/loop`, что принятый редактор. Target — ID item или его секции; отсутствующее значение означает normal. Режим задаётся в контекстном меню Header item / секции и item в Setlist, показывается canonical transport glyph. Изменение — одна Show Undo transaction, входит в item saved content / Save for All и `.mrlive`. Normal удаляет лишний override. Старые файлы без modes/options читаются без миграционного переписывания; устаревшие ID не исполняются, новые изменения по удалённому target запрещены. Неверный mode/тип опции отклоняется до изменения show.

Подготовка policy использует настоящий Core Timeline и нормализованные Core sections. Явный arrangement.length имеет приоритет над исходными bars и полной WAV extent; без него новый пустой item занимает 16 тактов, а после загрузки WAV/stems длительность заменяется фактическим концом самого длинного Playback clip (с учётом trim/regions), с прежним пределом 4096 тактов. Это может как увеличить, так и уменьшить пустую длину; Core заканчивает Playback на точном sample конца аудио, без округления вверх до полного такта. Прежние явно заданные bars пустых items и ручной arrangement.length сохраняются. Сокращение item не удаляет скрытые sections/clips. Незавершённый section stop/loop не перекрывает жёсткий конец item; section skip обрезается им. Валидный ручной loop внутри item имеет приоритет над section modes. При естественном завершении section loop/stop имеет приоритет над входом в соседнюю skip-секцию; явный seek различается с прохождением границы. Seek внутрь skip описывается переходом на его конец; seek за уже завершённую stop/loop секцию не вызывает устаревшее действие. Item skip описывает переход к следующему occurrence, stop — остановку на конце, loop — возврат к музыкальному нулю, normal — next-item intent на конце. backToStart и linked stopRecord сохраняются раздельно; политика сама не включает capture.

**Принятый первый срез (build 352)** добавил authoring и чистую policy. Второй срез подключает её к реальному AudioEngine; его исходники требуют отдельной native приёмки.

### Исполнение и автоматический handoff (#287, второй срез)

RenderGraph содержит immutable PerformancePlan и уникальный runtime token. AudioEngine делит callback на chunks по tempo/meter, ручному loop и performance boundary. Skip/stop/loop и конец item исполняются на sample boundary. Natural stop/loop секции выигрывает у следующей skip-секции; явный seek не повторяет завершённую границу. Ручной loop имеет приоритет над секциями. Явный конец item ограничивает исполнение; implicit extent следует фактическому концу WAV/regions без округления вверх до полного такта. Back to start — сохраняемая настройка Options: Stop возвращает в начало item, иначе удерживает позицию. Применение режимов активного item требует Pause/Stop; запись отдельно не включается.

Native controller готовит следующий нескрытый режимом skip entry активного setlist, сохраняя entry ID для повторных ссылок. Общие WAV preparation и render factory строят следующий набор заранее. AudioEngine::PreparedTransition удерживает готовые graph/DSP buffers, PDC, channel workers, MIDI schedulers, metronome и streams. Callback принимает raw pointer через bounded atomic mailbox и меняет подготовленные контейнеры swap; outgoing state остаётся в control-owned handle до release acknowledgement и уничтожается вне callback. Один AudioEngine и тот же device handle продолжают работу без stop/start/open. UI poll принимает соответствующий ProjectStore/EngineTransport/MusicalTimeline и assets после подтверждения; он не задаёт время переключения.

Control/mix сообщения помечены item token; устаревшие seek/loop/mix не попадают в новый item. Show edits, меняющие следующий entry/media/mix/музыкальные данные, отзывают подготовленный следующий граф до публикации изменений. Обычный Play/seek внутри текущего item не делает готовый следующий граф устаревшим. Stop/Pause остаются общими для устройства даже во время смены token. Для file-backed skip/loop destinations заранее читаются immutable страницы; общий лимит остаётся 32 streamed voices / 256 MiB, с учётом резервирования jump pages.

Ограничения: автоматический handoff сейчас для подготовленного WAV Playback/Cue (включая stems и Click) со стабильным device/master routing. Live inputs, VoiceRenderer, hosted editor runtimes, external MIDI и capture не принимаются этим handoff; patch/MIDI и запись остаются отдельными issues. Если следующий граф ещё не подготовлен/был отменён, Core удерживает natural-end request и ждёт на границе. При готовности следующий граф принимает audio callback: normal продолжает воспроизведение, item «доиграть и остановиться» выбирает следующий entry в sample 0 / Stopped и ждёт Play. Это действует и для задержанной подготовки; UI timer не посылает команду autoplay. Ручные Stop/Pause/seek отменяют ожидающее автоматическое продолжение. Section stop удерживает позицию внутри текущего item. Последний entry остаётся на конце; автоматического wrap setlist нет. Будущие crossfade/tails и физический no-dropout не подтверждаются синтетическими PCM тестами. stopRecord сохраняется, но capture по нему не объявляется реализованным. Ahead/Mixed speculation для performance graphs отключена; устройство остаётся владельцем исполнения. MR Studio graphs без policy сохраняют прежний путь.

### Выбор и реальный device input profile

Для dedicated Playback projection включённые физические ASIO inputs не создают monitor routes у каналов без Monitor/Arm. Это не ослабляет guard на настоящий input processing: реальные monitor/armed routes остаются и не допускаются prepared WAV handoff. Stage не меняет applied mixer revision текущего item. Runtime acknowledgement обновляет frontend revision baseline (без ложного Studio Save song gate) и сбрасывает display interpolation по новому item token.

Отложенный single-click сетлиста принадлежит самому списку и удерживает entry ID; repaint/rebuild карточек не теряет клик. При double-click/drag, новом явном выборе или смене show/setlist ожидание отменяется по navigation revision; сама потеря фокуса/rebuild не отменяет уже принятый клик. Drag guard использует elapsed time от optional timestamp и остаётся корректным при большом Windows uptime и wraparound uint32, включая отсутствие предшествующего drag. Timeline layout сохраняет видимый selected/browsed entry как scroll anchor при асинхронной подгрузке waveform extent, чтобы расширение предыдущих items не уводило выбор за пределы экрана.

## Source preflight и безопасная перепривязка (#288, первый срез)

Options → Show / source check открывает принятый owned graphite context (90% opacity). Явная фоновая проверка охватывает WAV/stems всех items каталога, фактический header/размер/mtime, resample extent общего Core, trim/region source bounds и сохранённые outputs относительно текущих enabled outputs; отдельно проверяются маршруты Click. Stale size/mtime сравниваются с retained preparation stamps, когда такой cache существует. Проверка не меняет show/Undo, активный ProjectStore или звук. Положительный результат означает проверку источника, а не готовность DSP: decoding/stream/DSP preparation и устройство остаются отдельными runtime gates. Offline clock / неработающее audio показываются явно; аппаратная сертификация не заявляется.

Choose WAV / Reload source сначала проверяют совместимость файла в worker и лишь затем публикуют новую ссылку одним Show Undo. Trim, regions, channel и mix сохраняются; несовместимый/короткий/повреждённый файл отклоняется без изменения bindings. Use Master — явное Undoable исправление физического маршрута. Неактивный item исправляется при продолжающемся текущем playback; для активного item и глобального Click нужен Pause/Stop, для recording действует guard. Старый next graph отзывается перед изменением его данных. Повторные проверки bounded (до четырёх retained jobs), закрытие контекста отменяет inspection без join в GUI handler; завершение выбранной перепривязки обрабатывается Desktop вне callback и не зависит от срока жизни popup. Session epoch/fingerprint отбрасывает результаты для другого/изменённого show или устройства. Worker не обращается к GUI/Core models.

Native Status различает media ready, готовый следующий graph и ожидание на конце item. Autosave, восстановление несохранённых данных/позиции, interrupted capture и input/plugin/MIDI preflight остаются последующими срезами; этот экран не объявляет их реализованными.

## Предзагрузка следующего item и безопасный manual handoff (#30/#31)

Application.prepare_playback переиспользует PreparedWavImport: тот же WAV inspect/decode/resample, один batch, без публикации в текущий Project. Job не зависит от revision текущего item; mix/Undo и сам playback не отменяют подготовку. Native controller хранит до трёх наборов (текущий, следующий, ранее выбранный), автоматически готовит следующий валидный item активного setlist, пропуская dividers. Transient entry cursor различает повторные ссылки на один item; Setlist/timeline передают entry ID, Browser reselect сохраняет текущую позицию. Cursor ещё не является сохранением позиции/recovery show. Один набор удерживает до 128 MiB декодированного PCM; остальное остаётся file-backed. Отменённые workers удерживаются Core до завершения, максимум шесть jobs, без join в обработчике выбора. Закрытие Application отменяет и присоединяет workers вне callback.

Перед ручной активацией проверяются исходные size/mtime, sample rate, все trim и физические routes, Click и модель. Изменившийся WAV требует повторного выбора/явного Choose WAV, который сбрасывает даже same-path cache. Невалидная подготовка не переключает выбранный item и не останавливает текущую композицию; Status показывает ошибку. Esc отменяет ожидающее переключение. Новый выбор заменяет pending request; завершение старого worker не публикует устаревший item. Preload/status — transient runtime, не изменяют `.mrlive`, Undo, saved dots.

Готовый набор передаётся через install_playback в тот же Application / AudioEngine / GraphStore и общий render builder. Весь graph/asset setup выполняется вне callback. Действующий callback владеет прежними immutable assets, пока новый граф готовится; затем выполняется stop → quiesce → prepare → start **с тем же открытым аудиоустройством**, без close/open, смены backend или второго Engine. Project/Transport/musical projection меняются согласованно; runtime Project получает чистую историю, пользовательский show Undo остаётся прежним. До успешного старта сохранены старые модели/graph/assets; отказ старта откатывает и возобновляет прежний playback. Assets и временные resampled WAV освобождаются вне callback только после ухода старого графа.

Это срез WAV/Click/Cue: активные инструменты, inserts/hosted plugins/ARA не заменяются этим handoff; их lifecycle относится к #32/#33. Ручной выбор начинает новый item в sample 0 и Stopped, без неявного autoplay. Options → Previous item / Next item использует тот же путь; Status показывает следующий item и readiness его media. Callback не получает нового alloc/free/locks/I/O. Стресс-проверка fake device проверяет многократные переходы, PCM/routes/tempo, сохранение одного Engine/одного open и rollback; её время не является физическим ASIO latency.

После явного Disconnect редактирование/загрузка WAV остаются доступны: install_playback включает штатные Core Offline clock, если открытого устройства нет. Это не переоткрытие существующего ASIO handle; физические routes по-прежнему валидируются, недоступные выходы не downmix в Master.

Runtime preload и устранение повторного импорта реализованы, но **stop/start остаётся границей ручного handoff**. Автоматический sample-boundary переход на конце item, no-dropout во время непрерывного воспроизведения, per-song end policy и физический ASIO stress этим не заявляются и остаются последующим объёмом #30/#31.


## #302 — штатные Live Channels: исходный срез A1 (29.10.2026)

**Статус: SOURCE ONLY / NOT TESTED.** Начат перенос общих механизмов MR Studio на каналы MR Live. В native Mixer у каждого show-owned non-Playback channel появился `Input / FX` с выбранным channel ID. Он открывает существующий `LiveInputPilotPanel` с полями: физический вход текущего ASIO устройства, mono/stereo pair, route Master/direct active output(s), VST3 audio FX, monitor и явный Save to Show/Load from Show. Используются `Application::set_track_input`, `set_track_monitoring`, `set_hardware_output`, `set_inserts`, `capture_insert`, существующая сериализация `channels[].liveInput`. Второй audio/backend/plugin host не добавлен, WAV-guard и #300 stopped handoff не отключены. Новый smoke проверяет control IDs/Mixer action и shared Core stereo input + outputs profile.

**Ограничение A1 (заменено A2 выше):** это ещё **один активный runtime audio input track**, не полноценные несколько одновременно работающих каналов. До следующего среза физический вход или stereo mode можно менять только после Disable; смена канала при активной цепочке запрещена. Stereo требует реально доступную пару ASIO входов. Переназначение hardware route/новый insert производится вне активного Playback. Сохранение профиля явно инициируется пользователем; открытие show не включает monitoring автоматически. Ни fake GUI-контролы, ни тест mock driver не доказывают hardware PASS.

**План после A1 (реализованную часть A2 см. выше; приёмка отдельно):**
1. Реальная show-owned mapping `channelId → Core Track ID` для **нескольких** каналов: управление созданием/удалением, mono/stereo vendor input и сохранение с безопасным lifecycle, включая смешанный WAV/stems graph. Убрать одноканальный pilot только после проверки lifetime/rollback и аппаратной приёмки.
2. Per-channel plugin rack, FX Browser #281, routing/meters и глобальный Mixer #279 с независимым channel mix.
3. Раздельный VSTi/MIDI путь через shared instrument hosting и MIDI Core #18/#24: hardware/device mapping и VSTi нужно проверить, не выдавать audio FX panel за готовый инструмент.
4. Приёмка на Windows ASIO 48 kHz / 128, vendor VST3, затем hardware MIDI (отдельно). Не запускать Windows Actions и не объявлять PASS без проверки. #289 Patch/Sub-patch зависит от принятых штатных channel bindings, #301 seamless межпесенных переходов optional.
