# Roadmap аудита и оптимизации MR Studio / MR Live

**Дата снимка:** 21.11.2026  
**Основа:** локально присланные `AGENTS(1).md` (проектный), `AGENTS(2).md` (глобальный), `skills(2).zip` (36 Skills), `docs.zip` (238 Markdown + 4 вспомогательных файла) и подготовленные в этой беседе обновления.  
**Система разработки:** локальный Windows checkout + DeepSeek Harness (DSH) + DSH Task Manager. GitHub Issues/PR/Actions — исторические ссылки, не текущий трекер задач.  
**Статус:** план аудита; **не** подтверждение, что правки уже установлены в локальном репозитории.

> **Как пользоваться.** Выполняй этапы по одному; `[x]` — только подтверждённое действие соответствующего описания (например, «редакция подготовлена»), `[ ]` — ещё не подтверждено. Для списка документов `[ ]` означает «индивидуальная проверка ещё не закрыта», а **не** «обязательно переписать». При передаче этой дорожной карты в DSH фиксируй основные контрольные точки в Task Manager. Не добавляй этот файл в авто-загрузку `AGENTS.md` / hooks / Skills: он предназначен для управляемого аудита, а не как постоянный контекст каждого запуска.

## Что считаем успехом

- Сохранены технические инварианты: **один Shared Core**, RT/audio-callback safety, ARA2 для Studio / без ARA2 для Live, JUCE/host lifetime, Undo/Redo, persistence, корректная независимая версия Studio/Live.
- Действующий запрос на **редизайн GUI** может изменять палитру, компоновку, геометрию и компоненты в пределах задачи; прежний MR UDS — база для согласованности, а не запрет на развитие.
- Глобальный AGENTS содержит общие правила, проектный — только особенности MR, Skills — адресные процессы, `docs/` — долгосрочные технические контракты, Task Manager — требования, статус и приёмку.
- По умолчанию нет каскада `ds-mode → ds-plan-loop → ds-verify-loop → ds-review-loop` и нет принудительного чтения целых больших документов.
- Никакие задачи, версии, PR, Actions или локальные исходники не меняются автоматически вследствие этого roadmap.
- При равных сценариях измерено сокращение лишних прочтений/контекста **без ухудшения корректности** и без роста стоимости успешной задачи.

## Этап 0. Синхронизировать уже подготовленные редакции (P0)

- [x] Подготовлен новый сокращённый `dsh-task-manager-workflow/SKILL.md` с маршрутом `tasks_create` и расширенным описанием применения.
- [x] Подготовлена более гибкая редакция `mr-unified-gui/SKILL.md`.
- [x] Подготовлены согласованные `mr-studio-ui-design-system/SKILL.md`, `ds-design/SKILL.md` и проектный `AGENTS.md` под свободный редизайн.
- [x] Подготовлены `mr-studio-build-test/SKILL.md` и его оптимизированный `references/command-discovery.md` плюс согласованный проектный `AGENTS.md`.
- [x] Подготовлены оптимизированные `docs/LOCAL_BUILD_POLICY.md` и `docs/MR_UNIFIED_DESIGN_SYSTEM.md`.
- [ ] Сверить подготовленные пакеты с **актуальными локальными** файлами, а не заменять вслепую; проверить отличия после подготовки архивов.
- [ ] Объединить последовательные редакции **одного** `AGENTS.md` (GUI → Build/Test) без потери более новых локальных правил; глобальный `AGENTS.md` не изменять без отдельной причины.
- [ ] Применить согласованный набор одним адресным diff/локальным docs-only коммитом **после проверки пользователем**; не делать merge/push/Actions автоматически.
- [ ] Проверить пути `SKILL.md`, frontmatter `name`/`description`, существование references/assets, перекрёстные ссылки и отсутствие дублирующих версий Skill.
- [ ] Уточнить, какой из глобальных и проектных AGENTS фактически подхватывает DSH (включая импорт `CLAUDE.md`, `GEMINI.md`, hooks); убрать двойную автоматическую загрузку, если подтверждена.

**Готово, когда:** новые редакции реально находятся в локальном checkout, `git diff` ограничен согласованными файлами, Skill discovery работает и локальные правила не противоречат друг другу.

## Этап 1. Измерить исходные затраты контекста (P0)

- [ ] Выбрать 4–5 типовых задач: небольшая правка Studio GUI; редизайн MR Live; багфикc с RED/GREEN; изменение аудиографа/RT; документационная правка. Использовать безопасные read-only/учебные прогоны, без лишних правок.
- [ ] Для каждого прогона записать: модель/effort/провайдер, входные/выходные токены, cached tokens (если доступны), число чтений файлов/Skills, вызовы субагентов, число повторных тестов, время и результат.
- [ ] Разделять **размер файлов на диске** и **реально загруженные токены**. Не выдавать оценку по символам за подтверждённую стоимость.
- [ ] Проверить автозагрузку `SESSION_CONTEXT.md`, `docs/START_HERE.md`, `ds-help`, `ds-mode`, проектных Skills и повторные чтения документации.
- [ ] Сохранить короткую baseline-таблицу результатов вне постоянно загружаемых инструкций (резюме контрольной точки — в DSH Task Manager).

**Готово, когда:** есть воспроизводимая база для сравнения «до/после» при неизменных модели, настройках и сопоставимом сценарии.

## Этап 2. Оставшиеся профильные MR Skills (P0–P1)

- [ ] **`mr-feature-implementation`** — убрать повторы глобального AGENTS/Task Manager; оставить вертикальный путь Model → Commands → Engine → UI, Undo/persistence и адресное подключение references. Проверить, что обычная небольшая правка не вызывает весь набор Skill.
- [ ] **`mr-plugin-development`** — отделить общие требования host/editor/preset lifetime от MR Cab/IR/WAV-браузера; Cab-специфику вынести в reference **только при реальной необходимости**, не создавать новый Skill на каждый плагин.
- [ ] **`mr-realtime-audio-safety`** — сохранить строгие RT-запреты, lifetime, callback/xrun; сократить повторный отчётный/Git-процесс и проверить, что Skill включается лишь для RT-affected кода.
- [ ] Проверить все references/assets трёх Skills; не удалять шаблоны и чек-листы, пока не доказана их ненужность.
- [ ] Проверить разделение вызовов: визуальный GUI → `mr-unified-gui`; JUCE implementation → `mr-studio-ui-design-system` **по необходимости**; RT → `mr-realtime-audio-safety`; сборка → `mr-studio-build-test`; статус DSH → `dsh-task-manager-workflow` только при операции с журналом.

**Готово, когда:** каждый Skill отвечает за свой слой, не вызывает соседние без повода и сохраняет поведение safety-critical проверок.

## Этап 3. Универсальные ds-* Skills: маршрутизация, не массовое удаление (P1)

- [ ] Разобрать `ds-help`/`ds-mode` как источники каскадов; не делать `ds-mode` обязательным в каждой coding-сессии, если глобальный AGENTS уже задаёт процесс.
- [ ] Проверить `ds-principles`, `ds-plan-loop`, `ds-verify-loop`, `ds-tdd`, `ds-review-loop`, `ds-blueprint`, `ds-blast-radius` на дубли с глобальными правилами и проектными Skills; разрешить адресный вызов сложных задач.
- [ ] Сопоставить `ds-ledger` и `ds-context-reset` с DSH Task Manager; не создавать `PLAN.md`, `PROGRESS.md`, TSV-журналы в репозитории автоматически.
- [ ] `ds-swarm`, `ds-best-of-n`, `ds-jury`: установить явные условия запуска и согласовать с ограничением **один активный субагент**, без несанкционированной дополнительной платной модели.
- [ ] `ds-pr`, `ds-pr-comments`, `ds-ready`, `ds-revive`, `ds-ship`, `ds-watch-pr`, `ds-adopt`: исключить из автоматического маршрута локального MR; **не** удалять глобально и не отменять явно разрешённые сценарии других проектов.
- [ ] `ds-design`, `ds-design-audit`: использовать применительно к native JUCE с поправкой на назначение; не навязывать web-only процедуры и старую палитру.
- [ ] Остальные `ds-*`: классифицировать как полезный по явному запросу / устаревший / не нужен MR; проверить frontmatter `disable-model-invocation`, описания-триггеры и связанные scripts без правки поведения других проектов.

**Готово, когда:** для типовых MR-задач агент сам не открывает посторонние PR/multi-agent/ledger Skills.

## Этап 4. GUI-документы (P0–P1)

- [ ] `docs/MR_UI_KIT.md` — разграничить обязательные контракты параметров/gestures/binding и свободные визуальные решения; убрать дубли с новым MR UDS.
- [ ] `docs/MR_UI_KIT_ROADMAP.md` — удалить **нормативность** исторических GitHub стадий; убрать безусловный маршрут чтения нескольких документов и обязательные Gallery/DPI-проходы для простой правки.
- [ ] `docs/MR_UI_GALLERY.md`, `docs/MR_UI_GALLERY_193.md`, `docs/MR_UI_TOKENS_191.md`, `docs/MR_UI_CONTROLS_192.md` — определить актуальную техническую ценность и статус исторических результатов.
- [ ] Проверить `MR_FADER_SYSTEM.md`, `MR_SVG_RUNTIME.md`, `NATIVE_GUI_206_INVENTORY.md`, `MR_TRACK_HEADER_JSON_242.md`, `UI_UX_CONCEPT.md` и все GUI related handoff/audit на противоречие свободному редизайну.
- [ ] Сохранить семантические состояния, цветовые/контрастные обозначения по смыслу, SVG catalog/manifest и правила runtime; не превращать прежние цвета и геометрию в неоспоримый стандарт.
- [ ] Сверить не-Markdown ресурсы `docs/design/` и реальные `assets/ui/mr-uds/` **по локальному checkout**; не запускать генераторы и не менять assets без отдельной задачи.

**Готово, когда:** запрос «переделать интерфейс» не блокируется старым визуальным контрактом, а binding/host/Undo остаются защищёнными.

## Этап 5. Навигация, локальная сборка и версии (P0–P1)

- [ ] `docs/START_HERE.md` + `docs/README.md` + `docs/PROJECT_CONTEXT.md` — оставить короткие индексы, не три конкурирующих автозагрузчика документов.
- [ ] `docs/SESSION_CONTEXT.md` — проверить полезность Codex hook в DSH; не дублировать AGENTS/DSH Task Manager и не считать памятку статусом задач.
- [ ] Сверить оптимизированный `LOCAL_BUILD_POLICY.md` с **реальными** `Build-Local.ps1`, CMake presets, `tools/ci/sdk-lock.json` и локальными Windows окружениями; команды из одного Markdown без исполнения не подтверждены.
- [ ] `docs/LOCAL_DEVELOPMENT.md`, `docs/LOCAL_WORKSPACE.md`, `docs/CLOUD_DEVELOPMENT.md` — чётко пометить исторические/справочные части и убрать ложный сигнал о действующем GitHub-first процессе. Не удалять полезные hardware/SDK факты.
- [ ] `docs/VERSIONING.md` — сохранить отдельным компактным контрактом `0.I.S` / `-fixN` / `-UpdN` с независимыми Studio/Live; не дублировать весь текст в AGENTS или Skills.
- [ ] `docs/ROADMAP.md`, `IMPLEMENTATION_STATUS.md`, `DEVELOPMENT_TRACKS.md` — оставить направления/ссылки; текущие стадии и приёмку — в DSH Task Manager.

**Готово, когда:** одна точка входа, одна политика полномочий и один набор реальных команд сборки; ссылки не ведут к противоречивому процессу.

## Этап 6. Крупные архитектурные контракты (P1)

- [ ] `docs/MR_LIVE_NATIVE_INTEGRATION.md` — разделить **действующие границы `.mrlive`/show/channel/patch/Shared Core** и историю сборок/Issue; не переносить неутверждённые планы в статус implemented.
- [ ] `docs/MR_LIVE_LAYOUT_EDITOR.md`, `MR_LIVE_INTERFACE_AUDIT.md`, `MR_LIVE_EDIT_ARCHITECTURE.md` — определить, что относится к browser prototype, что к native contract, а что уже только исторический отчёт.
- [ ] `docs/PROCESSING_RELIABILITY.md` — выделить обязательные RT/graph/Undo гарантии, отделить датированные Fix/Upd отчёты; сверить с `AUDIO_ENGINE.md` и `ENGINE_PERFORMANCE_PLAN.md`.
- [ ] `docs/ARCHITECTURE.md`, `CORE_CONTRACTS.md`, `DATA_MODEL.md`, `PERSISTENCE.md`, `VST3_HOSTING.md`, `ARA2_HOSTING.md` — проверить владельцев контракта и ссылки; **не удалять** уникальные технические инварианты ради процента сокращения.
- [ ] `docs/STEM_SEPARATION.md` — выделить модель jobs/leases/PCM/stems/recovery, отделить историю #333+ и старые версии; сравнить с актуальным локальным кодом и задачами DSH.
- [ ] `docs/AUDIO_TO_MIDI.md` и `docs/Audio to midi.md` — объединить только после проверки уникальных UX/модельных требований; сохранить различие реализации и черновых решений.

**Готово, когда:** у каждой подсистемы есть понятный действующий контракт и адресные секции; исторические сведения не выдаются за текущий статус.

## Этап 7. Остальная документация и специальные плагины (P1–P2)

- [ ] Проверить MR Rig/Cab/Reverb/Harma Waves спецификации по цепочке owner → implementation → tests, не открывая целые папки при одной задаче.
- [ ] Пометить архивные MR Cab2.0 планы (например, `MR_CAB_ROADMAP.md`) как историю, если локальные задачи/код подтверждают superseded-статус; не удалять технические требования, используемые MR Rig.
- [ ] Проверить старые `STAGE_*`, `MRS_*`, `*_UPD*`, `*_FIX*`, `*_CHECKLIST*`, `*_HANDOFF*`, `*_AUDIT*` на уникальные требования и входящие ссылки; действие: **оставить / обновить / объединить / историзировать**, а не «удалить по названию».
- [ ] Перед перемещением/удалением любого документа: найти реальные входящие ссылки в `AGENTS.md`, Skills, других docs и локальном коде; перенести уникальные контракты; обновить ссылки, проверить итоговый diff.
- [ ] Не переписывать сразу все 238 файлов. Для документов низкого риска, которые не загружаются в типовых задачах, достаточно короткой проверки и классификации.

**Готово, когда:** все файлы из реестра ниже обработаны решением; архивная история не попадает в стандартный путь агента.

## Этап 8. Финальная проверка и измерение (P0 перед завершением)

- [ ] Повторно измерить те же задачи, модель, параметры и условия кэширования; сравнить **фактически прочитанные** токены, число вызовов, время и успешность, не только размеры Markdown.
- [ ] Проверить отрицательные сценарии: простой GUI-файл не вызывает RT/CTest/PR; редизайн не блокируется MR UDS; RT-багфикc не теряет RED/GREEN; MR Live не вызывает Studio-only тяжёлый тест без причины.
- [ ] Проверить обнаружение всех активных Skills, ссылки relative paths, отсутствие битых links, циклических обязательных чтений и неоднозначных источников истины.
- [ ] Проверить `git diff --check`, список файлов, ссылки и актуальность локальных инструкций. Docs/Skills-only изменения **не требуют** приложения или полных тестов без технической причины.
- [ ] Подвести итог: исходные и конечные затраты; список сохранённых контрактов; что архивировано; что `NOT TESTED`; какие вопросы требуют пользовательской приёмки.
- [ ] Только после пользовательской проверки отметить согласованный этап в DSH Task Manager; Git push/merge/релиз — отдельно по проектным разрешениям.

**Готово, когда:** измерена реальная экономия и подтверждена сохранность ключевых операций, а не только уменьшение числа символов.

---

## Приложение A. Контрольный список всех Skills из `skills(2).zip`

В архиве **36** `SKILL.md`: 29 `ds-*`, 6 `mr-*` и 1 `dsh-task-manager-workflow`. Вторая строка у отмеченных «подготовлена редакция» **не** означает, что локальная версия уже обновлена. Все чекбоксы остаются открытыми до сверки с локальной установкой. Универсальные `ds-*` не удалять из DSH глобально лишь потому, что они не нужны MR.

### Готовые редакции (ещё проверить внедрение в локальный проект) — 5

- [ ] `.agents/skills/dsh-task-manager-workflow/SKILL.md` — подготовлена редакция; сверить и внедрить
- [ ] `.agents/skills/mr-unified-gui/SKILL.md` — подготовлена редакция; сверить и внедрить
- [ ] `.agents/skills/mr-studio-ui-design-system/SKILL.md` — подготовлена редакция; сверить и внедрить
- [ ] `.agents/skills/mr-studio-build-test/SKILL.md` — подготовлена редакция; сверить и внедрить
- [ ] `.agents/skills/ds-design/SKILL.md` — подготовлена редакция; сверить и внедрить

### Профильные MR Skills — следующий аудит — 3

- [ ] `.agents/skills/mr-feature-implementation/SKILL.md`
- [ ] `.agents/skills/mr-plugin-development/SKILL.md`
- [ ] `.agents/skills/mr-realtime-audio-safety/SKILL.md`

### Универсальные ds-* — маршрутизация и сокращение повторов — 18

- [ ] `.agents/skills/ds-mode/SKILL.md` — исключить обязательное включение
- [ ] `.agents/skills/ds-help/SKILL.md`
- [ ] `.agents/skills/ds-principles/SKILL.md`
- [ ] `.agents/skills/ds-blueprint/SKILL.md`
- [ ] `.agents/skills/ds-plan-loop/SKILL.md`
- [ ] `.agents/skills/ds-verify-loop/SKILL.md`
- [ ] `.agents/skills/ds-tdd/SKILL.md`
- [ ] `.agents/skills/ds-blast-radius/SKILL.md`
- [ ] `.agents/skills/ds-review-loop/SKILL.md`
- [ ] `.agents/skills/ds-design-audit/SKILL.md`
- [ ] `.agents/skills/ds-unstick/SKILL.md`
- [ ] `.agents/skills/ds-lessons/SKILL.md`
- [ ] `.agents/skills/ds-sharpen/SKILL.md`
- [ ] `.agents/skills/ds-explain/SKILL.md`
- [ ] `.agents/skills/ds-catchup/SKILL.md`
- [ ] `.agents/skills/ds-unslop/SKILL.md`
- [ ] `.agents/skills/ds-context-reset/SKILL.md`
- [ ] `.agents/skills/ds-ledger/SKILL.md`

### Ресурсоёмкие/многоагентные — только по явному смысловому требованию — 3

- [ ] `.agents/skills/ds-swarm/SKILL.md`
- [ ] `.agents/skills/ds-best-of-n/SKILL.md`
- [ ] `.agents/skills/ds-jury/SKILL.md`

### PR/remote-процесс — исключить из автоматического маршрута локального MR — 7

- [ ] `.agents/skills/ds-pr/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-pr-comments/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-ready/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-revive/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-ship/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-watch-pr/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов
- [ ] `.agents/skills/ds-adopt/SKILL.md` — отключить автоматический вызов для MR, сохранить для иных проектов

**Дополнительные материалы Skills:** проверить при просмотре **соответствующего** Skill все `references/`, `assets/`, `agents/`, `scripts/`; отдельная сверка required/optional важнее переноса файлов ради компактности.

### Вспомогательные файлы Skills — отдельный контроль

Перечень из того же архива, чтобы не потерять зависимости при замене SKILL.md:

- [ ] `skills/deepseek-stack-LICENSE` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/ds-watch-pr/scripts/watch_pr.py` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/assets/feature-acceptance-checklist.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/assets/feature-spec-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/assets/feature-test-matrix.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/references/layer-impact-map.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/references/persistence-and-compatibility.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-feature-implementation/references/vertical-feature-example.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-plugin-development/agents/openai.yaml` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-realtime-audio-safety/assets/rt-code-review-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-realtime-audio-safety/assets/rt-validation-matrix.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-realtime-audio-safety/references/ownership-and-atomics.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-realtime-audio-safety/references/plugin-host-realtime-safety.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-realtime-audio-safety/references/rt-hazard-checklist.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-build-test/assets/build-report-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-build-test/assets/stage-acceptance-checklist.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-build-test/assets/validation-matrix-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-build-test/references/command-discovery.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-build-test/references/windows-juce-validation.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/assets/component-redesign-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/assets/ui-audit-template.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/references/juce-ui-rules.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/references/project-map.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/references/reference-intake-policy.md` — проверить актуальность, ссылки и необходимость загрузки
- [ ] `.agents/skills/mr-studio-ui-design-system/references/token-schema.md` — проверить актуальность, ссылки и необходимость загрузки

## Приложение B. Полный реестр документации из `docs.zip`

**Правило отметки:** для каждого файла зафиксировать решение `KEEP` (действующий контракт), `EDIT` (нужно актуализировать), `MERGE` (перенос уникального в канонический), `HISTORY` (исторический, не авточитать). Историзация/удаление — **только** после проверки кода и входящих ссылок. Распределение по темам предварительное, по именам файлов: назначение каждого уточняется при аудите. Не переносить этот реестр целиком в инструкции агента.

### Навигация, локальная разработка, версии и контекст — 15 документов

- [ ] `docs/CLOUD_DEVELOPMENT.md`
- [ ] `docs/DEVELOPMENT_TRACKS.md`
- [ ] `docs/IMPLEMENTATION_STATUS.md` — P1: не дублировать DSH статус
- [ ] `docs/LOCAL_BUILD_POLICY.md` — подготовлена оптимизированная редакция; применение в локальном checkout не подтверждено
- [ ] `docs/LOCAL_DEVELOPMENT.md` — P1: сверить со скриптами и локальной политикой
- [ ] `docs/LOCAL_WORKSPACE.md` — P0: в архиве сохранён исторический текст о GitHub
- [ ] `docs/MOON_RIVER_STUDIO_VISION.md`
- [ ] `docs/ONLINE_DEVELOPMENT_0_6_UPD1.md`
- [ ] `docs/PROJECT_CONTEXT.md`
- [ ] `docs/PROJECT_VISION.md`
- [ ] `docs/README.md`
- [ ] `docs/ROADMAP.md` — P1: не дублировать DSH задачи
- [ ] `docs/SESSION_CONTEXT.md` — P0: проверить автоматические hooks и повторы
- [ ] `docs/START_HERE.md` — P0: не инициировать автоматическое чтение цепочек
- [ ] `docs/VERSIONING.md` — P0: сохранить отдельный канон версий

### GUI, UI Kit, редакторы и интерфейсные контракты — 30 документов

- [ ] `docs/BROWSER_FONT_CARDS_UPD1.md`
- [ ] `docs/INTERFACE_212.md`
- [ ] `docs/MIXER_LAYOUT_UPD3.md`
- [ ] `docs/MIXER_UI_UPD7.md`
- [ ] `docs/MR_CONTROL_BINDING_194.md`
- [ ] `docs/MR_DAW_205_P2.md`
- [ ] `docs/MR_DAW_205_P3.md`
- [ ] `docs/MR_DAW_205_P4.md`
- [ ] `docs/MR_DAW_205_P6.md`
- [ ] `docs/MR_DAW_SHELL_205_P1.md`
- [ ] `docs/MR_DAW_UI_INVENTORY_205_P0.md`
- [ ] `docs/MR_FADER_SYSTEM.md`
- [ ] `docs/MR_LIBRARY_BROWSER.md`
- [ ] `docs/MR_LIBRARY_PREVIEW.md`
- [ ] `docs/MR_LIBRARY_SMART_SEARCH.md`
- [ ] `docs/MR_SVG_RUNTIME.md`
- [ ] `docs/MR_TRACK_HEADER_JSON_242.md`
- [ ] `docs/MR_UI_CONTROLS_192.md`
- [ ] `docs/MR_UI_GALLERY.md` — P1: проверить обязательность Gallery/DPI
- [ ] `docs/MR_UI_GALLERY_193.md`
- [ ] `docs/MR_UI_KIT.md` — P0: согласовать с гибким редизайном
- [ ] `docs/MR_UI_KIT_AUDIT_190.md`
- [ ] `docs/MR_UI_KIT_CODEX_HANDOFF.md`
- [ ] `docs/MR_UI_KIT_ROADMAP.md` — P0: устаревшая маршрутизация и проверки
- [ ] `docs/MR_UI_TOKENS_191.md`
- [ ] `docs/MR_UNIFIED_DESIGN_SYSTEM.md` — подготовлена оптимизированная редакция; применение не подтверждено
- [ ] `docs/NATIVE_EFFECT_EDITOR_UNIFICATION.md`
- [ ] `docs/NATIVE_GUI_206_INVENTORY.md`
- [ ] `docs/UI_REFERENCE_UPD9.md`
- [ ] `docs/UI_UX_CONCEPT.md`

### MR Live и обмен с Studio — 8 документов

- [ ] `docs/LIVE_WORKFLOW.md`
- [ ] `docs/MR_LIVE_EDIT_ARCHITECTURE.md`
- [ ] `docs/MR_LIVE_INTERFACE_AUDIT.md`
- [ ] `docs/MR_LIVE_LAYOUT_EDITOR.md` — P1: отделить браузерный прототип от нативного контракта
- [ ] `docs/MR_LIVE_NATIVE_INTEGRATION.md` — P0: отделить действующие контракты от истории
- [ ] `docs/Patches.md`
- [ ] `docs/STUDIO_ONE_SHOW_IMPORT.md`
- [ ] `docs/STUDIO_PRO_INTEGRATION.md`

### Shared Core, аудиодвижок, ARA, VST3, RT — 41 документов

- [ ] `docs/ARA2_AI_TOOLS_181.md`
- [ ] `docs/ARA2_CLIP_INTEGRATION_178.md`
- [ ] `docs/ARA2_EDITOR_AUDITION_88.md`
- [ ] `docs/ARA2_EDITOR_UX_179.md`
- [ ] `docs/ARA2_FOUNDATION_177.md`
- [ ] `docs/ARA2_HOSTING.md`
- [ ] `docs/ARA2_INCREMENTAL_PLAYBACK_88.md`
- [ ] `docs/ARA2_LIVE_PLAYBACK_88.md`
- [ ] `docs/ARA2_SHARED_DOCUMENTS_88.md`
- [ ] `docs/ARA2_STATE_180.md`
- [ ] `docs/ARCHITECTURE.md`
- [ ] `docs/ASIO_DEPENDENCIES.md`
- [ ] `docs/AUDIO_ARRANGEMENT.md`
- [ ] `docs/AUDIO_CORE.md`
- [ ] `docs/AUDIO_ENGINE.md`
- [ ] `docs/BUSES.md`
- [ ] `docs/CORE_CONTRACTS.md`
- [ ] `docs/CRASH_DIAGNOSTICS.md`
- [ ] `docs/CRASH_NOTICE_FIX5.md`
- [ ] `docs/DATA_MODEL.md`
- [ ] `docs/DESKTOP_FOUNDATION.md`
- [ ] `docs/ENGINE_ANTICIPATIVE_PROCESSING.md`
- [ ] `docs/ENGINE_INSTRUMENT_PARALLELISM.md`
- [ ] `docs/ENGINE_MIXED_PROCESSING.md`
- [ ] `docs/ENGINE_P4_ACCEPTANCE.md`
- [ ] `docs/ENGINE_PARALLEL_PROCESSING.md`
- [ ] `docs/ENGINE_PERFORMANCE_PLAN.md` — P1: только нужные разделы при RT
- [ ] `docs/ENGINE_PLUGIN_EDITORS.md`
- [ ] `docs/ENGINE_PROFILING.md`
- [ ] `docs/ENGINE_PROFILING_BENCHMARK.md`
- [ ] `docs/HARDWARE_OUTPUTS.md`
- [ ] `docs/INPUT_ROUTING.md`
- [ ] `docs/PERSISTENCE.md`
- [ ] `docs/PRERECORD_96.md`
- [ ] `docs/PRE_SPATIAL_ROUTING.md`
- [ ] `docs/PROCESSING_RELIABILITY.md` — P0: выделить RT-контракт
- [ ] `docs/RECORDING.md`
- [ ] `docs/SENDS.md`
- [ ] `docs/TIME_STRETCH.md`
- [ ] `docs/TIME_STRETCH_API.md`
- [ ] `docs/VST3_HOSTING.md`

### AI, stem separation и Audio-to-MIDI — 13 документов

- [ ] `docs/AI_ANALYSIS_CONTRACT.md`
- [ ] `docs/AI_API_PROTOTYPE.md`
- [ ] `docs/AI_FOUNDATION_CONTRACT.md`
- [ ] `docs/AI_INTEGRATION.md`
- [ ] `docs/AI_MIDI_CONTRACT.md`
- [ ] `docs/AI_PREVIEW_BROWSER.md`
- [ ] `docs/AI_QUERIES_SNAPSHOT.md`
- [ ] `docs/AI_SKILLS_MEMORY.md`
- [ ] `docs/AI_TOOLS_PERMISSIONS.md`
- [ ] `docs/AI_TRANSACTIONS_HISTORY.md`
- [ ] `docs/AUDIO_TO_MIDI.md` — P1: сравнить с одноимённым черновиком
- [ ] `docs/Audio to midi.md` — P1: проверить уникальные UX-решения перед консолидацией
- [ ] `docs/STEM_SEPARATION.md` — P0: отделить рабочую архитектуру от старых Issue/версий

### Нативные плагины, MR Rig, Cab, Reverb, Harma Waves — 43 документов

- [ ] `docs/ACOUSTIC_SPACE_PROTOTYPE.md`
- [ ] `docs/CAB_IR.md`
- [ ] `docs/DSP_MODELING.md`
- [ ] `docs/HARMA_WAVES_CODEX_HANDOFF.md`
- [ ] `docs/HARMA_WAVES_NATIVE.md`
- [ ] `docs/HARMA_WAVES_ROADMAP.md`
- [ ] `docs/MR_CAB_AUDIT_183.md`
- [ ] `docs/MR_CAB_BROWSER_STATE_UPD1_196.md`
- [ ] `docs/MR_CAB_CODEX_HANDOFF.md`
- [ ] `docs/MR_CAB_COLLECTIONS.md`
- [ ] `docs/MR_CAB_FULL_COMBO_196_DELIVERY.md`
- [ ] `docs/MR_CAB_FULL_COMBO_IR.md`
- [ ] `docs/MR_CAB_MANIFEST_184.md`
- [ ] `docs/MR_CAB_ROADMAP.md` — P1: архивный статус, не считать актуальным планом
- [ ] `docs/MR_CAB_SIMPLE_185.md`
- [ ] `docs/MR_CAB_UI_UPD1_185.md`
- [ ] `docs/MR_CAB_WINDOW_UPD2_196.md`
- [ ] `docs/MR_REVERB.md`
- [ ] `docs/MR_REVERB_GUI_207.md`
- [ ] `docs/MR_REVERB_KIT_PILOT_195.md`
- [ ] `docs/MR_RIG_ARCHITECTURE.md`
- [ ] `docs/MR_RIG_AUDIO_PEDALS.md`
- [ ] `docs/MR_RIG_AUDIT_198.md`
- [ ] `docs/MR_RIG_CHAINS_201.md`
- [ ] `docs/MR_RIG_CODEX_HANDOFF.md` — P1: оценить актуальность локального handoff
- [ ] `docs/MR_RIG_GUI.md`
- [ ] `docs/MR_RIG_LIBRARY_199.md`
- [ ] `docs/MR_RIG_LIBRARY_PRESETS.md`
- [ ] `docs/MR_RIG_PEDALS_202.md`
- [ ] `docs/MR_RIG_PEDAL_CHARACTER.md`
- [ ] `docs/MR_RIG_ROADMAP.md`
- [ ] `docs/MR_RIG_SHELL_200.md`
- [ ] `docs/MR_SATURATOR.md`
- [ ] `docs/MR_STRIP_LINK.md`
- [ ] `docs/NATIVE_INSERTS.md`
- [ ] `docs/PLUGIN_PRESETS.md`
- [ ] `docs/PLUGIN_WORKFLOW_210.md`
- [ ] `docs/REVERB_ACTIVE_CPU.md`
- [ ] `docs/REVERB_EDITOR_IMPLEMENTATION.md`
- [ ] `docs/REVERB_GATE_MIX.md`
- [ ] `docs/REVERB_LIBRARY_IMPLEMENTATION.md`
- [ ] `docs/REVERB_OPTIMIZATION.md`
- [ ] `docs/REVERB_VALIDATION.md`

### Музыкальное редактирование, каналы, MIDI и проекты — 61 документов

- [ ] `docs/ARRANGER_EDITOR_80.md`
- [ ] `docs/ARRANGE_AUDIO_UPD7.md`
- [ ] `docs/ARRANGE_FINAL_UPD8.md`
- [ ] `docs/BYPASS_CPU_UPD5.md`
- [ ] `docs/CHANNELS_213.md`
- [ ] `docs/CHORD_TRACK_79.md`
- [ ] `docs/CLIP_COPY_UPD1.md`
- [ ] `docs/CLIP_WORKFLOW_211.md`
- [ ] `docs/COMPACT_GAIN_215.md`
- [ ] `docs/CONTROLLER_SCROLL_100.md`
- [ ] `docs/EDITING_BACKLOG_150.md`
- [ ] `docs/EDITOR_SPACE_FIX1.md`
- [ ] `docs/EDITOR_SPACE_FIX2.md`
- [ ] `docs/EDITOR_WORKFLOW.md`
- [ ] `docs/HOME_MIXER_FIX6.md`
- [ ] `docs/INSERT_LOADING_SEND_BYPASS_UPD8.md`
- [ ] `docs/INSTRUMENT_DROP_98.md`
- [ ] `docs/JUCE_FIX1.md`
- [ ] `docs/JUCE_FIX2.md`
- [ ] `docs/JUCE_FIX3.md`
- [ ] `docs/JUCE_J1.md`
- [ ] `docs/JUCE_J2.md`
- [ ] `docs/JUCE_J3.md`
- [ ] `docs/JUCE_MIGRATION.md`
- [ ] `docs/JUCE_UPD2.md`
- [ ] `docs/MARKER_TRACK_82.md`
- [ ] `docs/METER_MAP_84.md`
- [ ] `docs/METRONOME_FIX1.md`
- [ ] `docs/METRONOME_PRECOUNT.md`
- [ ] `docs/MIDI_CLIPS.md`
- [ ] `docs/MIDI_CONTROLLERS.md`
- [ ] `docs/MIDI_EDITING_UPD1.md`
- [ ] `docs/MIDI_EXTERNAL.md`
- [ ] `docs/MIDI_INSTRUMENT_DROP_UPD2.md`
- [ ] `docs/MIDI_LIVE_INPUT.md`
- [ ] `docs/MIDI_MUSICAL_EDIT.md`
- [ ] `docs/MIDI_PROCESSOR_GRAPH.md`
- [ ] `docs/MIDI_RECORDING.md`
- [ ] `docs/MIXER.md`
- [ ] `docs/MIXER_BYPASS_UPD2.md`
- [ ] `docs/MIXER_IR_INSERTS_UPD6.md`
- [ ] `docs/MIXER_SENDS_95.md`
- [ ] `docs/MUSICAL_CONTEXT_85.md`
- [ ] `docs/MUSICAL_LANE_GESTURES_UPD2.md`
- [ ] `docs/MUSICAL_TIMELINE.md`
- [ ] `docs/NAVIGATION_86.md`
- [ ] `docs/NOTE_EDITOR_LAYOUT_99.md`
- [ ] `docs/PIANO_ROLL.md`
- [ ] `docs/PIANO_ROLL_214.md`
- [ ] `docs/POPUP_MIXER_FIX7.md`
- [ ] `docs/PRESET_LIBRARY_UPD1.md`
- [ ] `docs/PROJECT_CHANNEL_MENUS_UPD14.md`
- [ ] `docs/PROJECT_FOLDERS.md`
- [ ] `docs/PROJECT_HOME.md`
- [ ] `docs/SECTION_COLORS_BOUNDS_81.md`
- [ ] `docs/SMART_SNAP_UPD2.md`
- [ ] `docs/STATE_SNAPSHOT_FIX3.md`
- [ ] `docs/STUDIO_EDIT_RESPONSE_341.md`
- [ ] `docs/TEMPO_MAP_83.md`
- [ ] `docs/TRACK_FOLDERS.md`
- [ ] `docs/TRACK_RENAME_97.md`

### Исторические этапы, Fix/Upd, отчёты и handoff — 27 документов

- [ ] `docs/MRS_0_1H_FIX1.md`
- [ ] `docs/MRS_0_1H_FIX2.md`
- [ ] `docs/MRS_0_1H_FIX3.md`
- [ ] `docs/MRS_0_1I_UPD1.md`
- [ ] `docs/MRS_PROJECT_FOLDERS_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_1D_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_2D_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_2E_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_3C_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_3_PLAN.md`
- [ ] `docs/MRS_STAGE_4A_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4B_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4C_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4D_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4E_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4F_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4G_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4H_CHECKLIST.md`
- [ ] `docs/MRS_STAGE_4_PLAN.md`
- [ ] `docs/RELEASE_0_2G.md`
- [ ] `docs/SHARED_STAGE_0_CHECKLIST.md`
- [ ] `docs/SHARED_STAGE_1_CHECKLIST.md`
- [ ] `docs/SHARED_STAGE_2_CHECKLIST.md`
- [ ] `docs/SHARED_STAGE_3_CHECKLIST.md`
- [ ] `docs/SHARED_STAGE_4_CHECKLIST.md`
- [ ] `docs/STAGE_5_PLAN.md`
- [ ] `docs/STAGE_5_RISK_REVIEW.md`

### Вспомогательные файлы в `docs.zip` (не Markdown)

Эти четыре ресурса не входят в счётчик 238 Markdown-файлов. Не переписывать SVG/JSON/DOCX в ходе текстового аудита без отдельной задачи; проверить, что ссылки на них остаются валидными.

- [ ] `docs/design/MR_REVERB_GUI.svg` — сохранность ссылки и референса.
- [ ] `docs/design/MR_Studio_Audio_to_MIDI_UX_TZ_v0.1.docx` — проверить, является ли действующим ТЗ или историческим; при необходимости сохранить уникальные UX-решения в действующем контракте.
- [ ] `docs/design/MR_UNIFIED_STYLE_REFERENCE.svg` — reference, не обязательная палитра при редизайне.
- [ ] `docs/design/MR_UNIFIED_TOKENS.json` — reference tokens, не универсальный запрет на изменение дизайна.

## Шаблон отчёта после каждого завершённого этапа

```text
Этап:
Затронутые файлы:
KEEP / EDIT / MERGE / HISTORY:
Удалённые дубли / разорванные цепочки чтения:
Сохранённые контракты:
Проверка ссылок / scope diff:
Реальные проверки PASS / FAIL / NOT TESTED:
Оценка контекста (факт, если измерено):
Нужно согласовать / следующий этап:
```

**Итоговый критерий:** ни один Skill или документ не исчезает без решения о его назначении; каждое обновление минимально и проверено, а экономия токенов оценивается по фактическим прогонам DSH.
