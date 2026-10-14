# AI Foundation: worker и Project Context v1

## User acceptance — 2026-10-14

Explicit user: «#167 / #157: принимаю». #157 closed completed in delivered scope
(current0.3m upd1/source4c1d5ba). Existing live text/proposal/model/usage and local
mock/lifetime/permissions/history/audio regression evidence retained; no new detailed
ASIO/recording/error/DPI measurements supplied. Other6 child issues and parent#26
remain open; actual native completion5/11 (#164/#165/#166/#167/#157).
Split/Merge G/Duplicate D also confirmed working. Next maintenance build requested:
gradual grid subdivisions and Smart Snap; [current context](PROJECT_CONTEXT.md).
Older awaiting-acceptance records below are historical and superseded by this statement.


Актуальное дополнение 2026-10-13: #159/#160 реализуют entity_queries и snapshots;
подробности и текущие limits/serialization/invalidation — [AI_QUERIES_SNAPSHOT.md](AI_QUERIES_SNAPSHOT.md).
Описанные ниже unsupported query/snapshot статусы относятся к исходному #157/#158 slice.

2026-10-13. Parent [#26](https://github.com/vladleng/MR-Studio/issues/26),
первый локальный slice [#157](https://github.com/vladleng/MR-Studio/issues/157) /
[#158](https://github.com/vladleng/MR-Studio/issues/158).
Пользователь поручил: «Изучи AGENTS.md и PROJECT_CONTEXT.md и приступаем к #26».
Это начало реализации; parent и дочерние задачи не приняты автоматически.

## Контракт и границы

`mrs::ai` — отдельная библиотека C++20 поверх shared Core. Application предоставляет
`ai_context`, `request_ai`, `cancel_ai`, `take_ai_result` всем workspace, включая Live.
Эти entry points разрешены только на application thread; wrong-thread вызов отклоняется
до чтения ProjectStore. Сам worker не получает engine, transport, plugin или UI pointers.
Task обязан работать только с переданными значениями и JobControl; захват Application,
UI или изменяемых services запрещён контрактом adapter.

Context v1 независим от schema21/archive1 и содержит:

- stable project ID, runtime session ID, project revision и graph revision;
- title/artist, sample rate, PPQ960, tempo/meter map, количества сущностей;
- наблюдённый TransportState и явно переданную Selection;
- capabilities: summary/timing/selection доступны; entity_queries/tools/analysis/
  live_performance пока unsupported.

Сущности адресуются typed EntityRef (track/folder/clip/chord/section/marker) и stable ID,
не именем/индексом. Selection проверяет ссылки, дубли и диапазон; максимум256 объектов.
Selection и sample range временные, не проектные данные, не Undo и не persistence.
Никакой выбор UI не угадывается. Неопределённые возможности не изображаются пустыми
реализованными queries. Детальные entity DTO/queries относятся к #159, полный согласованный
снимок document/graph/live metadata — к #160.

## Единицы и согласованность

Samples и Tick — signed64, начало0. Sample range полуоткрытый [start,end).
Sample0..2^52, Tick0..10^12. PPQ960 — ticks четвертной ноты; denominator meter определяет
display beat. Bars/beats one-based; tempo changes заданы tick, meter changes — bar.
`Timeline` — существующий единственный алгоритм преобразования samples/ticks/positions;
seconds = samples/sample_rate. В Context нет второго алгоритма musical time.

Чтение происходит последовательно на control thread, где владеют ProjectStore/GraphStore;
worker получает value DTO, без mutable/engine/plugin references. Transport — наблюдение:
playhead продолжает движение и не инвалидирует revision проекта.
Для transport-dependent Tools нужны отдельные preconditions в #161/#162.

ContextKey = project ID + session ID + project revision + graph revision.
Каждая замена проекта получает новый session ID, в том числе повторное открытие той же
сохранённой версии. Отмена прежних jobs и сравнение полного ContextKey предотвращают
доставку ответа от закрытого проекта, прежнего открытия или устаревшей редакции.
Application заново строит контекст запроса из своей модели, проверяя присланный key;
caller не может подменить title/time map/counts в job. Изменение модели/Undo/Redo меняет
revision, old-context запрос получает stale_context; поздний ответ отбрасывается.

Title/artist<=4096bytes, tempos/meters<=4096, stable ID<=128bytes. Неограниченные
plugin blobs, media paths, IR samples и raw audio в Context не включены.

## Worker lifetime, очередь и ошибки

Application создаёт worker лениво при первом запросе. Обычная работа без AI не запускает
AI thread и не обращается в сеть. Одна фоновая очередь, один backend invocation на job.
Retries0: повторные платные запросы и tool mutation не выполняются автоматически.

Default:8 outstanding (pending + executing + completed-undrained), input<=1MiB,
output<=1MiB; настраиваемые пределы ограничены64jobs/16MiB. Worker возвращает busy
при заполнении, invalid_input для ошибочного payload/timeout, unsupported для иной
версии API, cancelled/timeout/response_too_large/backend_failure/shutdown.
Request IDs монотонны внутри worker; ответ несёт request ID и ContextKey.
Error output пуст, oversized allocation освобождается перед постановкой результата.

Deadline начинается при submit, включая время в очереди; допустимый timeout1ms..5min.
Queued cancelled/expired jobs не вызывают backend. Cancel после завершения, но до
delivery, также подавляет output. Результаты забираются control/UI thread через take;
worker не вызывает UI callbacks. Неактивный UI не вызывает рост completion queue.
Stop callbacks вызываются без worker mutex, чтобы не создавать reentrant deadlock.

Shutdown: остановить audio callbacks обычным Application lifecycle, запросить отмену AI,
join worker, затем освободить остальное runtime state. Detached threads отсутствуют.
Shutdown выполняется owner thread, не внутри Task и не одновременно из нескольких threads.

**Cancellation кооперативная.** Backend обязан регулярно проверять JobControl stop/deadline
и ограничивать чтение JobControl.output_bytes. Произвольный зависший C++ call безопасно
прервать нельзя; worker не заявляет такую гарантию. Сетевой adapter #167 обязан обеспечить
конечные interruptible connect/read timeouts и bounded response до выделения больших
буферов. Текущий slice использует mock Tasks; HTTP/JSON provider ещё не подключён.

## Влияние на слои / RT review

| Слой | Классификация / результат |
|---|---|
| Application/domain | AFFECTED: read context, lazy worker, session/revision delivery guards |
| Shared model | NOT AFFECTED: существующий ProjectStore, value projection |
| UI | DEFERRED: AI panel/connection prototype #167; обычная GUI проверяется регрессией |
| Commands/permissions | DEFERRED #161/#162; worker не выполняет mutations |
| Undo/Redo | NOT AFFECTED; чтение/jobs ничего не редактируют |
| Persistence/project files | NOT AFFECTED; schema21/archive1, AI session transient |
| Engine/DSP/callback/routing | NOT AFFECTED; отсутствует AI dependency/call на audio path |
| Notifications | AFFECTED: control-side explicit drain, без worker UI calls |
| Studio/Live | SHARED CORE / одна Application |
| Tests/docs | AFFECTED: ai_context/worker/application и regression |

Все новые пути NON-RT. Mutex/condition_variable/string/vector используются только
worker/control thread. Audio callback не знает о worker и не ждёт его. Application
закрывает device перед worker join; replace отменяет AI вне callback. RT status:
RT SAFE WITH MANUAL CHECK для foundation boundary; реальный ASIO с HTTP запросом
не измерен и не заменяется offline PASS. Полный snapshot/audio analysis в #160/#166
потребует своей проверки thread ownership.

## Проверки и продолжение

Логи: `build/asio-local/validation/ai-foundation`. Новые suites покрывают empty/timing/
unsupported capabilities/selection/stable IDs/read-only, bounded queue/completions,
oversize/exception/cancel/queued expiry/timeout/reentrant stop callback/100jobs/shutdown,
shared workspace context, смену проекта/тот жеID+revision, stale edits, wrong-thread
read и продолжение offline playback во время медленного job.

Release configure/full build PASS; final CTest133/133 PASS97.44s, включая новые3 AI
suite и обычные GUI smoke. Initial focused3 PASS2.48s; первый full133 PASS112.96s до
final version guard. На пересборке во время ещё работающего GUI smoke возник LNK1104;
после завершения smoke повторены build и full tests последовательно — PASS.
Source `0359a72bc0d578a922047c0daad1ccf66d757897`, ветка `mrs/0.2g-fix1-editor-space-local`.
Фактическое начало реализации проверено против GitHub main/docs и принятых prerequisite
#15/#17/#18/#19. FEATURE READY WITH MANUAL CHECK для foundation boundary, parent #26
FEATURE INCOMPLETE: остальные queries/snapshot/tools/permissions/preview/Undo/MIDI/
analysis/provider slices ещё не реализованы. #157/#158 остаются open до приёмки.
Hardware ASIO/network/balance smoke NOT RUN: сеть/ключи не подключены, adapter/UI #167
ещё не реализован. Отдельный пользовательский package для невидимого foundation
не выпускается; принятый0.3h upd2 и ярлык сохраняются. После этого slice следующий
блок #159/#160: полноценные query DTO и согласованный snapshot; затем tools/permissions.
