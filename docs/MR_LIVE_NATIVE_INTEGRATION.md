# MR Live — передача принятого интерфейса в MR Studio

Статус на 2026-10-25: **браузерный этап интерфейса завершён и принят пользователем; следующий этап — native JUCE UI и подключение реального SHARED Core MR Studio**. Завершение макетирования не означает готовность аудио/runtime.

Tracking: [#14 roadmap](https://github.com/vladleng/MR-Studio/issues/14), [#272 редактор](https://github.com/vladleng/MR-Studio/issues/272), [#28 native UI](https://github.com/vladleng/MR-Studio/issues/28), [#29 shared integration](https://github.com/vladleng/MR-Studio/issues/29), [#274 главная страница и Live Projects](https://github.com/vladleng/MR-Studio/issues/274), [#30 show lifecycle](https://github.com/vladleng/MR-Studio/issues/30). Исходники прототипа — PR #273, тематическая ветка `codex/mr-live-layout-editor`; merge в main не выполнен.

## Принятый артефакт

Оригинальный файл пользователя сохранён без изменения байтов: [mr-live-layout-v5.json](../assets/ui/mr-uds/layouts/mr-live-layout-v5.json).

- SHA-256: `c95a3dd3258fa4d79085233eb98e4444903bfb3377e707eb591548f5ba9cb2f1`.
- `schema: mr-live-layout`, schema/registry version 1, target `mr-live-main`, viewport 1920×1080, 10 записей блоков.
- Это ревизия макета v5, а не schema version 5. JSON прошёл текущую bounded layout validation с каноническим manifest assets.
- Отдельные Header, Channels и Timeline; Setlist, Playback / stems и Status слева. Mixer скрыт до открытия. Старые `input.patches` и `control.transport` оставлены как совместимые legacy-записи и не должны возвращаться в native UI как дополнительные видимые панели.
- Тот же файл используется как `tools/design-lab/live/user-layout.json`; редактор: https://vladleng.github.io/MR-Studio/tools/design-lab/live/ .

Файл содержит геометрию, typography/tokens, semantic assets/bindings и видимость блоков. **Он не содержит audio/plugin state, содержимое setlists/items и Browser, настройки элементов кнопки item или задачи органайзера.** Эти данные прототипа находятся в demo show (`performance.itemCardLayout`, `itemTasks`, `itemWorkflow`, `library` и др.). Пользователь не передавал экспорт demo show вместе с v5; не считать эти данные извлечёнными из layout JSON. Для native интеграции переносить согласованное поведение по спецификации и исходникам, а реальные документы — через model/commands/persistence MR Studio.

## Что завершено в браузерном этапе

Реализованы редактор блоков, import/export, responsive preview, Undo/Redo, канонические assets и опубликованный интерактивный макет. Основной workflow включает Setlist/Browser с папками и перетаскиванием, разделители выступлений, общий набор каналов, timeline items/sections/chords, patches/sub-patches и Playback clips, микшер и отдельный gain Playback внутри item, arm/record/takes/comp как демо-состояние.

Кнопки items настраиваются в конструкторе; Save for All сохраняет содержимое в Browser, точка показывает отличия. Статус и приоритет общие и синхронизируются без Save for All. В ПКМ-карточке — вложенный органайзер с прогрессом веток и компактные действия; его данные также общие. Органайзер в кнопках скрывается иконкой рядом с Browser. Browser items без шапки закрывается ЛКМ снаружи, кроме области Setlist. Подробный контракт: [MR_LIVE_LAYOUT_EDITOR.md](MR_LIVE_LAYOUT_EDITOR.md).

Это **модель поведения и визуальная приёмка**, а не работающие Audio Engine/ASIO/plugins/запись аудио. Browser waveform/tuner/meters/recording — иллюстрации либо симуляция; не переносить фиктивные значения в production UI.

## Следующий этап и вход в MR Live

1. **#274 — главная страница и браузер проектов.** Кнопка «Новый проект MR Live» создаёт show `.mrlive` и открывает MR Live workspace. В браузере проектов доступны вкладки обычных проектов и MR Live; переключение вкладки меняет список/тип документов, не backend. Предусмотреть открытие существующих Live projects, recent projects, сохранение/reopen и защиту несохранённых изменений.
2. **#28 — native UI по v5.** Общие MR UDS/JUCE components, layout/resize, фиксированные Header/Channels, синхронная прокрутка, настоящие focus/drag/keyboard и Undo/commands. JSON — reference/loader input, пока native loader не реализован; не утверждать, что браузерный JS автоматически исполняется в MR Studio.
3. **#29 — реальные bindings.** Project Model, Transport, musical context, Chord/Arranger/sections, GraphStore/channels/mixer, Playback, meters и запись через существующие shared services. Один Audio Engine, ASIO/device layer, Transport, Project Model, MIDI engine и plugin host. Без browser simulation как production backend и без второго аудиотракта.
4. **#30–#33 — профильные workflows.** Show persistence/refs/preload/preflight/switch safety, Playback/click/cue, patches/sub-patches и MIDI/control в соответствующих задачах. `.mrlive` хранит show policy и ссылки на `.mrsproject`; отдельная вкладка MR Live не вводит дублированный формат песни или новый engine.

Performance Mode остаётся последующим отдельным направлением. Загрузка макета/SVG/fonts, сериализация и организация задач выполняются вне audio callback. Plugin switching и sub-patch parameter changes должны сохранять существующие lifetime/PDC/recording contracts.

## Проверка и критерии передачи

- Последний полный профиль прототипа перед handoff: 53 Node tests PASS, адресные HTTP Chromium smoke (органайзер, карточки, Browser/Setlist, сохранение и внеоконные клики) PASS. Макет v5 дополнительно проверен парсером и SHA-256; эта документационная передача не требует native сборки.
- Native интеграция пока **NOT IMPLEMENTED / NOT TESTED**. Аппаратный ASIO, слуховая проверка, физический mixed-DPI, реальные плагины, audio recording/takes и концертная надёжность не подтверждены браузерными тестами.
- Приёмка следующего этапа: из Home создать/открыть Live show, переключить вкладки проектов без регрессии обычных проектов; показать v5 в native UI; управлять теми же Project/Transport/Engine; сохранить/reopen модель и проверить адресные model/command/persistence/native GUI tests. Публиковать точную source/build revision и отдельный результат аппаратных проверок, если они выполнялись.

## Полный комплект передачи интерфейса

Для следующего чата передаются совместно:

1. Оригинальный `assets/ui/mr-uds/layouts/mr-live-layout-v5.json` — принятые блоки/геометрия.
2. `assets/ui/mr-uds/layouts/mr-live-interface-defaults-v1.json` — отдельно выгруженные исходные настройки кнопки item (revision 2), статусы/priority, переключатель органайзера и описание его необязательного шаблона. Это defaults из исходников, **не пользовательские данные браузера**.
3. Весь `tools/design-lab/live/` — работающая логика/модели/валидаторы/тесты; canonical assets `assets/ui/mr-uds/` и `assets/ui/icons/transport/`.
4. Этот handoff, `MR_LIVE_LAYOUT_EDITOR.md`, `LIVE_WORKFLOW.md`, `AGENTS.md` и GUI skill.

Этого комплекта достаточно для продолжения native реализации интерфейса и его согласованного поведения. Для точного восстановления пользовательских items, папок, собственных координат полей кнопки и задач нужен дополнительный **пользовательский demo show JSON**: в своём браузере открыть Files → сохранение demo show либо ПКМ Setlist → «Экспорт demo show» и приложить скачанный файл. Он хранится в localStorage пользователя (`mr-live-demo-show-v1`); среда разработки не имеет доступа к этому localStorage. Пока такого файла нет, не заменять его тестовыми данными и не заявлять восстановление пользовательского show.
