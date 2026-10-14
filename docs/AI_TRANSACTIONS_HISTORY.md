# AI transactions / history — #164, 2026-10-13

**ACCEPTED2026-10-13:** пользователь «#164 принимаю». #164 closed completed,
parent#26:1/11 completed. Принят описанный transaction/history+chat UX объём0.3k;
Application/GUI MIDI Apply остаётся #165. Source/build/package/manifest/shortcut
и прежние результаты проверок сохранены, новых build/tests/paid calls нет.
Подробная аппаратная ASIO/physical DPI матрица приёмкой не предоставлена.

Пользователь поручил #164 и попросил опустить Send/Cancel к нижнему краю AI tab.
В этот билд входят ранее запрошенные лента чата, очистка composer и Enter=Send,
Shift+Enter=newline. Предыдущая база: local source69e8e4c, package0.3j.

## Контракт операции

Trusted host `ai::ProjectEdit` композирует1–8 Core ICommand. Каждая команда
исполняется и валидируется на приватном candidate во время preflight.
Сохраняются immutable before/after snapshots и label `AI proposal (N commands)`.
Отказ любой команды не публикует промежуточный результат. Apply проверяет точный
prior Project и копирует уже проверенный after; повторного исполнения tools нет.
Preview хранит этот же ProjectEdit, поэтому diff и исполняемый результат совпадают.

Application по-прежнему проверяет registered preview/context/session/policy/Live,
manual confirmation и replay ledger. Marker Apply использует existing metadata
validator, ProjectDocument validation и staged ProjectStore transaction. Commit
готовит одну запись общей истории; unfinished transaction откатывает модель,
revision, label, обе ветки Undo/Redo без публикации. No-op сохраняет revision/Redo.
Redo берёт stored after из общей истории; model/network/tool callback не вызываются.
Ручные и AI edits чередуются в одном стеке; новая ручная правка после Undo удаляет
ветку Redo по существующей политике. Ledger не позволяет повторный Apply после Undo.
Отзыв AI permission не отбирает обычную Undo/Redo уже применённого результата.

Dirty policy остаётся revision-based: Undo к сохранённым значениям всё ещё dirty,
Save сбрасывает dirty; Open очищает историю. Это обычный контракт MR Studio,
не новая AI политика. Save/Open сохраняет применённый Project; chat, preview,
request IDs, keys, drafts и ledger не сериализуются. Schema21/archive1/config8 прежние.

## Границы #165

Общая композиция проверена на marker+MIDI transpose+track gain группе с точными
Undo/Redo/serialization, но это Core fixture, а не открытие новых model tools.
Application/GUI Apply по-прежнему исполняет только marker add/rename. MIDI Apply
и его engine/runtime publication остаются #165; parameter tools не добавлены.
Selection по-прежнему unsupported для preview. Полный parent #26 не принят.

## Chat и layout

Последовательная лента `You / Assistant / System / Connection` сохраняет сообщения
при следующих запросах и переключении Browser tabs. Лента ограничена96 entries/
256KiB UTF-8; один entry отображает до32768 characters. Старые entries удаляются
при лимите; max32 calls/session прежний. В0.3k это была только display history:
на каждый запрос отправляется текущий prompt+bounded snapshot, без всей переписки.
Лента transient и не сохраняется в project или credentials. С0.3l upd2 отдельный
bounded conversation context описан ниже и заменяет прежний stateless provider path.

После принятия worker request пользовательский текст добавляется в ленту, composer
очищается вместе с локальным editor Undo. До успешного submit draft и старый preview
сохраняются. Во время pending можно писать следующий draft: повторный Enter его
не отправляет/не очищает; ответ также не меняет draft. Provider/worker errors и
Cancel/invalidation добавляются в ленту, без retry. Enter=Send, Shift+Enter=newline;
typing guard сохраняет DAW shortcuts. Composer/Send/Cancel привязаны к низу;
бывшая пустая область используется diff, нижний margin8logicalpx.

## Влияние и проверка

UI, AI/Core command composition, Application/history integration, tests/docs:
AFFECTED. Shared Studio/Live authority: REUSED. Project model/file format and
audio engine/DSP/device/callback/routing: NOT AFFECTED. Durable chat/history export:
NOT APPLICABLE. MIDI Apply/runtime publication: DEFERRED #165.

`ai_transactions` проверяет mixed typed group, exact state/one Undo/Redo, counted
command executed once, preflight late failure, stale prior state, abandonment/
post-commit rollback, revision/label/both history branches, no-op/Redo preservation,
serialization, manual/AI interleaving, permission revoke then Redo, dirty and Save/Open.
Existing ai_preview safety suite сохраняется. GUI smoke: consecutive messages,
Enter/ShiftEnter, pre-submit failure draft, pending next draft, reply ordering,
post-async empty composer, redacted HTTP400, cancellation, docked tab reuse,
collapsed/expanded Connection at192×570/404×720/620×720, bottom anchor,100/150 images.
Первая проверка обнаружила Shift+Enter handling JUCE; исправлено. При visual review
обнаружено восстановление старого composer text асинхронным JUCE Value notification:
перед clear синхронизируется text Value; отдельная post-async regression добавлена.
Итоговые build/test/package/source evidence: [PROJECT_CONTEXT](PROJECT_CONTEXT.md).
Пользователь принял #164: «#164 принимаю». Подробные physical DPI/ASIO измерения
не предоставлены, новых аппаратных результатов не заявляем. Source/build/package local, docs/issues
GitHub отдельно, без source push/PR/merge/Actions.

## Follow-up context / 0.3l upd2 — 2026-10-14

Provider context is separate from display transcript: last8 completed HTTP200 user/
assistant text pairs,16KiB UTF8 total. Earlier assistant text shortened at2048chars
with explicit notice; discard oldest whole pairs at budget. No usage/model metadata,
connection/system errors, raw tool arguments or earlier snapshots. Current snapshot
sent once on each request and takes precedence over historical facts/IDs.
Values copied into immutable worker closure, no access to live UI in worker.
Memory-only, not Save/Open/project schema/history/credentials. Reset on changed
project session/access revocation/key removal/provider base or model change.
HTTP errors/cancelled requests are not completed turns. Existing proposal ledger,
permissions/review/stale/Apply/history remain authoritative; text cannot bypass them.
GUI follow-up/UTF8/no telemetry/session+revocation reset and budgets PASS; final
affected14/14 CTest PASS4.68s. Exact delivery in PROJECT_CONTEXT; real provider
conversation behavior remains manual check. No #164 acceptance status change.
