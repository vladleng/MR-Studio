# AI API prototype #167 — 2026-10-13

По запросу «Отлично, приступай к #167». Локальный прототип READ/SUGGEST;
приёмка реального API и ASIO ожидается. EDIT/AUTO project Apply остаётся закрыт
до #163–#165. UI и сеть не исполняют Core commands напрямую.

## Пользовательский путь

AI menu → Assistant. Provider — описательное имя; HTTPS base URL и model ID
редактируются. Defaults: ProxyAPI, `https://api.proxyapi.ru/v1`,
`openai/gpt-5.6-luna`. Ввести ключ в password field → Save connection → Test / models.
Модель должна быть в списке; это не доказывает поддержку tools или качество ответа.
Ввести сообщение → Send. Cancel отменяет job; повторный платный запрос автоматом
не выполняется. Permissions открывает shared policy panel; revoke отменяет запрос.
Delete key удаляет credential для текущего base URL и отменяет активный job.

Каждое сообщение независимо: разговорная история не отправляется. Передаётся
bounded snapshot: tracks/clips/chords/sections/markers/mixer/plugin metadata,
по24 entities/page, 4096bytes/page, общий16KiB conservative token/byte budget.
Большие проекты могут быть truncated; это не полный архив. Paths/media/plugin blobs,
credentials и host permissions отсутствуют. Saved plugin parameters в этот
начальный набор scopes не входят. Project names/text считаются untrusted data.

READ не объявляет write tools. SUGGEST/EDIT/AUTO объявляют только5 suggestion tools;
все mutations остаются locked. Ответ содержит текст либо структурированные
proposals: host присваивает request/operation ID и captured ContextKey, strict
decoder и Application.ai_propose повторно проверяют args/permissions/revision.
Ответы при изменении проекта/policy discard. Предложения не сохраняются в проект,
не меняют Undo/dirty; кнопки Apply нет. Preview/approval/atomic Apply — будущий scope.

## Provider contract и лимиты

OpenAI-compatible GET `/models`, POST `/chat/completions`, non-streaming.
Один model call/message, tool-loop budget1 (нет дополнительных query round trips),
max_completion_tokens1024, prompt<=8192 UTF8bytes, response<=256KiB,
до8 proposals, до32 requests/panel session. В Core нет model/provider dependency.
Tool schemas формируются из existing closed registry; parallel_tool_calls=false.
Provider rejecting parameter/tool support returns error; silent model fallback нет.

60s job deadline, WinHTTP resolve/connect/send5s/receive30s, async completion wait
10ms с JobControl. Cancel/deadline закрывает request и ждёт HANDLE_CLOSING перед
освобождением callback context. Нет detach/thread callback в UI, сеть вне RT.
HTTPS/TLS OS validation, redirects disabled, response read bounded. Transport
injectable для deterministic mocks. Нет balance/price lookup или paid retries.
401/402/403/404/429/5xx отображаются как HTTP errors с советом проверить ключ,
баланс, model и лимиты; body/headers ошибки не экспортируются в UI/logs.
Success strings проходят key redaction; пользовательские prompts/snapshots не логируются.
Usage отдаётся ровно как provider вернул (включая nested reasoning/cached categories
при наличии), model и finish_reason видны. Стоимость неизвестна: актуальный tariff
не получен, точный monetary estimate не выдумывается. Monthly Usage Monitor,
Skills/Orchestrator/model routing остаются архитектурным планом из AI_INTEGRATION§16.

Provider docs verified2026-10-13: [единый API](https://www.proxyapi.ru/docs/api-overview),
[model IDs и GET models](https://proxyapi.ru/docs/models).
Доступность именно Luna и tool capabilities подтверждаются только пользовательским
live completion; документация не заменяет такой тест.

## Хранение и совместимость

Nonsecret provider/base/model/max_tokens JSON — userApplicationData/MR Studio/
ai-provider.json. Windows Credential Manager generic credential
`MRStudio.AI.<base URL>`: ключ связан с адресом, другой URL не получает старый ключ.
Password field очищается после Save/Send; secret остаётся только в credential/worker
memory на время запроса. Нет secret в project/config/log/snapshot/source.
Crash dumps по природе могут содержать память процесса; автоматической отправки нет.
Schema21/archive1 прежние, permission Preferences v8 из #162. Старый config reader<=7
не читает v8; предыдущие packages сохранены, но возврат к ним не гарантирует чтение
новых user preferences. Тесты не меняют реальные user settings/keys/projects.
Credential smoke создаёт UUID fixture credential `.invalid`, удаляет его после теста.

## Слои и проверка

UI/settings: AFFECTED. Application/query/tool policy: REUSED. Model/Core commands/
Undo/project persistence: NOT AFFECTED (read/propose only). Engine/DSP/RT: NOT AFFECTED.
Studio/Live: shared Application. Device/performance paths не изменены.
Build/GUI mocks/docs/package: AFFECTED. Physical API/ASIO/DPI: MANUAL CHECK.

`juce_ai_assistant`: protocol/errors/usage/redaction/models, cancel/deadline before
transport, Credential Manager roundtrip/isolation/delete, real Application worker
→ UI delivery (Cyrillic), typed marker proposal/no mutation, pending revoke.
PNG100/150 software renders inspected. Full/targeted results и hashes — PROJECT_CONTEXT.
Initial syntax/redaction constness and UTF8 mock conversion errors исправлены.

## Ручная приёмка

1. Ввести ключ в приложении, Save, Test/models: выбранная модель доступна.
2. На пользовательском проекте Send вопрос: реальный ответ, model/usage зафиксировать
   без ключа; проверить, что проект/Undo не изменились. Предложение marker — validated,
   changes locked. Model/tool support failure не считается PASS.
3. Cancel и Permissions/revoke во время запроса, открыть другой проект: поздний ответ
   не показывается, панель остаётся отзывчивой. Проверить 401/402/429 по доступности.
4. Во время ASIO playback сделать запрос: звук не прерывается; CPU/B/XR/Late/D
   измерить отдельно. Offline CTest не заменяет этот hardware scenario.

#167/#26 остаются open до live/API/ASIO приёмки. Никаких source push/Actions.
