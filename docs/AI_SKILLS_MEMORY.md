# Skills and Memory — спецификация и handoff #168

2026-10-14. **PLANNED / NOT IMPLEMENTED**. Прямой запрос пользователя: создать
«[MRS AI] Skills and Memory», sub-issues и всю информацию для следующего чата.
[Parent #168](https://github.com/vladleng/MR-Studio/issues/168).

Все7 дочерних issues созданы. Native parent/sub-issue links пользователь добавляет
сам: «Я сам сделаю sub-issues». Список ниже до этого не означает native relationships;
следующий чат проверяет actual parent links, а не предполагает их по checklist.

## Результат для пользователя

После сохранения/закрытия и повторного открытия проекта AI восстанавливает его
подтверждённые цели, решения и предпочтения. Свежий snapshot описывает фактический
проект; память хранит намерения/решения и не переопределяет факты модели.
Пользователь видит, редактирует и очищает сохранённую память и историю.

Пользователь создаёт Skill для частой задачи, задаёт параметры, сохраняет в библиотеку
и повторно запускает на выбранном контексте. Пример: «создай пустой MIDI-клип на
выбранной instrument-дорожке, начало и длительность в тактах» либо marker workflow.
Skill — инструкции/данные; модель выдаёт только существующие typed tool calls.
Будущие Mixing/Mastering workflows не обещать до реализации соответствующих Tools.

## База и ограничения

**Номер следующей delivery — 0.4**, по уточнению пользователя2026-10-14:
«билд ... после уже сделанного #26 ... должен иметь номер0.4». Старый принятый
0.3m upd4 не переименовывать/перепаковывать;0.4 ещё не собрана. Обновить source
version/title/new package/PDB/manifest/shortcut согласованно при следующем выпуске.

- Исходники: C:/Users/Vladislav/Documents/GitHub/MR-Studio; рабочая папка чатов:
  C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio. Не клонировать старый main.
- Source: `66b712fa2a66b6c8e3d2c1627e6caacf167c4348`, ветка
  `mrs/0.2g-fix1-editor-space-local`. Проверить actual HEAD/status перед работой:
  docs commits и работа другого чата могут быть новее.
- Принятая сборка0.3m upd4:
  `Builds/MR-Studio-0.3m-upd4-Grid-Shift-Drag-JUCE-ASIO-Windows-local`.
  GUI SHA256 `3A0964BD615FBA1BA21BC2C30FC4421796A86487F0B20EB8159E5B1551C86BC0`;
 13runtime files/12hashes, symbols build/asio-local/deliveries/0.3m-upd4-grid-shift-drag.
- #26 native children11/11 приняты; parent остаётся open до финального аудита.
  Memory/Skills — отдельный новый объём, не часть прежней приёмки.
- Последние affected6/6 CTest PASS91.01s вupd4; full145 последний раз PASS160.58s
  вupd2. Не выдавать эти старые проверки за validation новых функций.
- Schema21/archive1/preferences8 в текущей базе; storage architecture ещё выбрать.
- Текущий Conversation ограничен8turns/16KiB и памятью процесса; он не является
  долговременной ProjectMemory. Reset/session/revoke и UI feed изучить в коде.
- ProxyAPI/Luna default, one call/message,32/session, no retry. AUTO сейчас требует
  ручного Apply. Autonomous AUTO, сложный orchestrator, model routing и monthly cost
  monitor вне #168. Отдельные Codex SKILL.md не равны пользовательским MRS Skills.

## Подзадачи и порядок

| Slice | Issue | Работа |
|---|---|---|
| SM01 | [#170](https://github.com/vladleng/MR-Studio/issues/170) | Контракт и хранение памяти проекта |
| SM02 | [#171](https://github.com/vladleng/MR-Studio/issues/171) | Восстановление контекста и обновление памяти |
| SM03 | [#172](https://github.com/vladleng/MR-Studio/issues/172) | UI памяти: просмотр, правка, забывание и история |
| SM04 | [#173](https://github.com/vladleng/MR-Studio/issues/173) | Skill Registry/Loader и формат |
| SM05 | [#174](https://github.com/vladleng/MR-Studio/issues/174) | Редактор и библиотека пользовательских Skills |
| SM06 | [#175](https://github.com/vladleng/MR-Studio/issues/175) | Запуск Skills с памятью и безопасным Tool API |
| SM07 | [#176](https://github.com/vladleng/MR-Studio/issues/176) | Интеграция, совместимость и итоговая приёмка |

Memory MVP: SM01→SM02/SM03; затем Skill MVP: SM04→SM05→SM06; завершение SM07.
SM02 и SM03 координировать по контрактуSM01, не выпускать пустой UI вместо persistence.
Для каждого slice отдельные source checkpoint/проверки/пакет при необходимости и
пользовательская приёмка; не принимать следующие задачи автоматически.

## Контракт Memory

Разделить confirmed memory, ограниченный локальный transcript и derived summaries.
Запись памяти: stable ID, project ID, schema/version, memory revision, type
(goal/preference/decision/note), текст, source/provenance, timestamp, optional stable
entity refs; status suggested/confirmed/obsolete. Project revision при создании —
источник проверки, а не запрет памяти пережить любое редактирование.

Авторитет: musical model для состояния проекта; confirmed store для намерений.
AI не подтверждает свои предположения сам. Предложенная запись проходит локальное
review; отклонённая команда не становится «выполненным решением». Локальная ручная
правка памяти доступна без провайдера. Разрешения на память отделить от права
менять музыку; READ/SUGGEST никогда не открывают музыкальные writes.

Хранилище выбирается вSM01 после аудита existing Save/Open/SaveAs/archive extensions:
portable sidecar против archive extension, один owner, атомарные write/replace,
schema/defaults/forward compatibility. Нельзя молча менять project format.
Явно определить unnamed/unsaved projects, SaveAs(copy/fork), move/rename, identical
project IDs в copies, concurrent session writes, missing/read-only/corrupt storage,
backup recovery и limits. Не смешивать память разных проектов по имени или пути.

Memory и transcript edits требуют описанной Undo/dirty/save policy; не засорять
musical Undo незаметным журналированием. При sidecar UI должен честно показывать
unsaved/error/recovered состояние; память не должна теряться при обычном Save/Open.
Рекомендуется начать с user-confirmed notes и bounded history, без автоматических
оплаченных summarization calls. Exact retention/input byte/token limits утвердить
вSM01/SM02, проверять fixtures; нынешние бюджеты не увеличивать молча.

При открытии показать restored summary, время/источник и включение в запрос.
Память/история — private local data; провайдер получает только выбранный bounded
контекст в рамках действующего разрешения. Не сохранять API keys/headers, raw snapshots,
plugin blobs, filesystem/device secrets или audio. Очистка удаляет соответствующий
summary/history/cache, отменяет pending jobs и защищает от late-result resurrection;
она не может отозвать данные, уже отправленные провайдеру. Обычные пользовательские
имена/заметки не считать автоматически очищенными от чувствительных данных.

## Контракт Skills

Versioned immutable Skill: ID, semantic format version, content revision, name,
description, instructions, typed parameter schema/defaults, declared Tools/capabilities.
Local user library с bounded loader/atomic persistence; проект выбирает ID+version.
При missing/conflicting version показать проблему, без тихой замены новой версией.
Portable embedded copy/import/export можно добавить только с явным контрактом.

Создание вручную и «сохрани это как Skill» дают editable draft с review. Шаблоны только
для существующих marker/MIDI Tools. Нет executable scripts, arbitrary function names,
shell, recursive file includes, remote dependencies и автоматического повышения mode.
Declared tool subset ограничивает вызовы, но не выдаёт permissions.

Run: selection+parameters→validate→fresh snapshot+selected Skill+relevant memory→
existing worker/provider→typed proposal→host validation→Preview→manual Apply→Undo.
Отправлять один выбранный Skill, не всю библиотеку/историю. Привязать immutable request
к skill/memory revisions плюс existing ContextKey/session/policy/operation identity.
Если во время запроса меняются Skill/Memory/project/policy, отменить/пометить результат
устаревшим; новый запуск получает новые данные. Runtime recording/count-in/Live guards
и stopped/loop-off MIDI requirements сохраняются.

## Влияние на слои

| Слой | План |
|---|---|
| Application/shared model | AFFECTED: memory/Skill stores, context composition, revisions |
| UI | AFFECTED: Browser AI memory/history/library/editor/run states |
| Persistence/project lifecycle | AFFECTED: storage/identity/SaveAs/restart/compatibility |
| Commands/Undo | AFFECTED: явно выбрать memory-edit policy; musical writes reuse |
| Provider/worker | AFFECTED: bounded context + cancellation/receipts, existing network |
| Engine/DSP/audio callback | NOT AFFECTED intended; I/O/JSON/LLM только NON-RT |
| Studio/Live | SHARED CORE; Live runtime restrictions unchanged |
| Tests/docs | AFFECTED |
| Automation/routing/plugins/model escalation | DEFERRED |

Если реализация касается RT/playback lifecycle, прочитать realtime skill и
PROCESSING_RELIABILITY; не менять callback ради памяти.

## Где продолжать в коде

- apps/studio-juce/src/AiAssistant.cpp — conversation, UI feed, begin/timerCallback,
  preview/renderDiff/updatePreview, stop/reset/revoke/session paths.
- apps/studio-juce/src/AiProvider.h/.cpp — Conversation, bounded serialization,
  provider config, usage/errors; secrets хранятся отдельно.
- apps/studio-desktop/src/application.cpp и её заголовок — Application,
  request receipts/policy/preview/transactions, project open/replace/snapshot.
- core/include/mrs/ai_query.hpp, core/src/ai_query.cpp — immutable query/snapshot.
- core/include/mrs/ai_tools.hpp, core/src/ai_tools.cpp — registry/tool/schema/policy.
- tests/ai_tests.cpp, ai_query_tests.cpp, ai_tool_tests.cpp; JUCE Smoke.cpp —
  nearest regression patterns. Actual persistence/serialization paths найти rg,
  не угадывать место для нового store.

## Следующий чат: чтение и первый шаг

1. Прочитать AGENTS.md, PROJECT_CONTEXT.md, LOCAL_BUILD_POLICY.md, этот документ,
   AI_INTEGRATION§16, AI_FOUNDATION_CONTRACT, AI_QUERIES_SNAPSHOT,
   AI_TOOLS_PERMISSIONS, AI_PREVIEW_BROWSER, AI_TRANSACTIONS_HISTORY, AI_MIDI_CONTRACT.
2. Сверить GitHub main docs и fresh issues#168/#170–176 с локальной копией; сохранить
   чужие edits/remote plans. Проверить current branch/status/log; source main отстаёт.
3. Начать только #170/SM01, если пользователь поручает продолжать #168.
   Аудит project identity/SaveOpen/SaveAs/conversation reset/persistence, затем короткий
   ADR storage+Undo+limits и реализация вертикального slice. Не начинать всё сразу.
4. Применить mr-feature-implementation; для UI mr-studio-ui-design-system, после кода
   mr-studio-build-test; mr-realtime-audio-safety при касании RT. Skills в рабочей папке
   .codex/skills. Discover actual CMake presets/targets/tests, старые implement scripts
   не запускать. Нет paid requests без необходимости пользовательской проверки.
5. Проверки storage/restart/SaveAs/two projects/corrupt/budget/late cancellation,
   Skill schema/versions/permissions, preview/diff/history/SaveOpen, GUI editing/100–150,
   no audio callback dependency. После финальных изменений запускать нужные тесты.
6. Для user delivery новый local package с symbols/hash manifest, прежние сборки сохранить.
   Обновить PROJECT_CONTEXT/этот документ/issues после каждого meaningful checkpoint.
   GitHub docs-only [skip ci] atop fresh main; без source push/PR/merge/Actions.

## Итоговая приёмка #168

Память переживает restart/Open и изолирована между проектами; её можно посмотреть,
поправить и очистить. Пользователь создаёт Skill, перезапускает приложение, выбирает
его версию/параметры и выполняет marker/MIDI workflow с правильным diff/Undo/SaveOpen.
Cancel/Reject/stale/revoke/missing/corrupt/oversize сохраняют целостность; usage/limits
и privacy visible. Все семь children accepted, локальные checks PASS и ограничения
записаны; parent закрывается только после отдельной итоговой приёмки.

