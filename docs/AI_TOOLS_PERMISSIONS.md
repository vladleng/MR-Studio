# AI tool contracts / permissions — #161/#162

2026-10-13. Пользователь: «Хорошо, приступай к #161/#162».
База source5beabc6/docs c5fe3a4; checkout clean, AGENTS/PROJECT_CONTEXT/AI_INTEGRATION
GitHub main сверены с локальными. Parent [#26](https://github.com/vladleng/MR-Studio/issues/26).
Использованы feature implementation, realtime safety, build/test и UI design system.

## Доступный объём и закрытые gates

Реализован foundation для READ + SUGGEST: закрытый registry, JSON schema/decoder,
типизированные calls/results, подготовка существующих Core commands, централизованные
permission checks, UI и preferences. Реальное Apply project/selection изменений
**не включено**. Оно требует preview/confirmation #163, atomic Undo #164 и MIDI
contract #165. Настройка EDIT/AUTO не открывает эти gates.

Разрешённые read calls действительно проходят через `Application::ai_tool_query` →
`ai_query` и существующий frozen query path. Marker/MIDI предложения компилируются
в те же AddMarker/UpdateMarker/EditMidiNotes, которые использует Application/UI,
но не исполняются. Это не заменяет проверку фактической mutation/Undo/SaveOpen в
#163–#165. Human DAW commands остаются обычными UI commands и не ограничиваются
AI permission mode. Parent и child issues не принимаются автоматически.

## Registry и protocol

| Tool | Аргументы / путь |
|---|---|
| query_project | Query name, IDs, cursor/limit, selection/tick filters → Application query |
| add_marker | name, tick → подготовленный AddMarker / metadata path |
| rename_marker | marker_id, name → UpdateMarker с captured expected value |
| set_selection | typed entities, optional sample range → proposal, UI apply deferred |
| transpose_notes | clip_id, явные note_ids<=256, semitones -24..24 → EditMidiNotes |
| set_note_velocity | clip_id, явные note_ids<=256, velocity1..127 → EditMidiNotes |

Unknown tools/версии/операции дают явный отказ. Shell, произвольные имена функций,
code evaluation, device/transport/routing/destructive file tools не зарегистрированы.
Модель не может передавать mode/permissions/confirmed/ready flags.

Envelope v1: version, request_id (uint64 decimal string), operation_id<=128bytes,
context_key (project/session IDs, project+graph revisions), tool, arguments.
Schema draft2020-12, closed oneOf для6 tools; все объекты additionalProperties=false.
API выдаёт schema из registry; independently parsed Python JSON проверяет структуру.
Request ID связывает ответ с host-issued request, не является свободной authority модели.
Provider adapter #167 собирает/проверяет envelope вместе с host response metadata.

Strict decoder:<=32KiB input, depth<=8,<=2048nodes,<=32fields/object,<=256array items,
strings<=4096bytes, canonical decimal integers (без fraction/exponent), uint64 counters
как strings. Duplicate/unknown keys, wrong types, trailing data, invalid UTF-8,
unpaired UTF-16 surrogate, NUL identifiers/names и invalid bounds отклоняются.
Unicode escapes/surrogate pairs нормализуются в UTF-8. Schema bounds проверяются
decoder, domain IDs/ranges проверяются повторно против текущего ProjectState.

Marker names<=4096bytes, tick0..10^12. Selection использует existing Context validation.
MIDI clips/notes должны существовать; notes должны пересекать видимое source-offset
окно. Не выполняется скрытое редактирование вне clip. Pitch overflow отвергается,
не маскируется clamping. IDs/channels/unselected notes сохраняются будущим Core command.
Generation/replacement партий, CC edits и batches не добавлены: это #165/#164.

## Permission matrix

| Режим | Read | Suggest | Apply сейчас |
|---|---|---|---|
| READ | да | нет | отказ |
| SUGGEST (default) | да | да, без изменений | отказ |
| EDIT | да | да | confirmation/preview/Undo gates закрыты |
| AUTO | да | да | только explicit tool bits; preview/Undo/MIDI gates закрыты |

Disabled access запрещает новый context/view/query/snapshot/request/tool call.
Stored plugin parameters отдельно требуют user flag и caller request; один bool
ReadPolicy не может повысить permission ранее захваченного view. Unknown action —
default deny. Authorization проверяется при каждой доставке/подготовке/Apply attempt;
plan.apply_decision — информационный результат подготовки, не reusable approval.

Recording/count-in блокируют Apply; Live performance (Live + play/record/count)
блокирует AI изменения. Reads/suggestions могут работать во время playback. Даже
после будущей общей activation transport/routing/device/files останутся unavailable
до отдельных tool/policy contracts. Runtime state читается заново на Apply attempt,
не берётся из старого transport observation snapshot.

ExecutionGates — trusted host contract для следующих slices. Application сейчас
передаёт закрытые gates; нет setter/model JSON поля для обхода. EDIT approval должен
быть привязан к точным immutable ToolCall/targets/arguments/context/policy revision,
а не к слову «подтверждено» в ответе модели. Этот UI confirm/diff появится в #163.

## Receipts, revocation и replay

Application регистрирует host request receipt: request ID, ContextKey, runtime permission
revision, weak reference на **тот же** Snapshot. Подмена ID, другого snapshot pointer,
revision или проекта отклоняется. До128 receipts и128 proposal operation records/session;
при заполнении busy, без eviction старых IDs. Snapshot blobs не удерживаются receipt.
Policy change/project replace очищают receipts/operations и меняют authority revision.

Prepared ToolPlan immutable, хранит canonical Call, policy revision, affected count,
apply decision и private compiled Core command. Repeated identical operation_id/call
возвращает тот же plan с duplicate=true, не генерирует новый marker ID. Другие аргументы
под тем же operation ID — invalid_input. Это foundation idempotency для proposals;
applied/rollback ledger и фактический exactly-once mutation проверяются в #164.

Application Apply принимает только свой зарегистрированный plan pointer, проверяет
текущие key/policy/runtime и затем закрытые gates. Raw Core-created plan не является
approval. После edit/Undo/Redo старый key rejected; после policy change старый plan
rejected независимо от одинаковой project revision. Network retry не повторяет mutation.

Permission revision host-only, не в exported JSON. Захваченные views/snapshots имеют
revocable ReadAccess: изменения policy, project switch и shutdown инвалидируют новые
reads и submits с прежним grant. Query проверяет grant до и после serialization.
Project edits сохраняют detached immutable data, но Application rejects stale key.
Старый public JSON/уже отправленные provider данные нельзя «отозвать из памяти»;
revocation блокирует последующие host operations и доставку позднего ответа.

Policy change отменяет worker jobs; Result содержит host policy revision, UI delivery
отбрасывает прежние ответы даже при неизменном ContextKey. Cancellation кооперативна
по #157, network adapter должен обеспечить finite interruptible I/O и bounded callbacks.

## UI и persistence

JUCE menu **AI: READ/SUGGEST/EDIT/AUTO/OFF → Permissions...**. Panel показывает mode,
enabled flag, отдельно saved plugin parameters, explicit AUTO allowlist, Apply permissions
и Revoke access. EDIT/AUTO помечены locked; текущий режим и lock status видимы текстом.
Human choice применяется через Application policy setter, меняет grant/cancel, но не
ProjectStore/Undo. Панель использует текущую Theme, Segoe/Noto font conventions,
existing EditorWindow ownership/focus. Title area сдвинута, чтобы новый AI menu не
перекрывал project caption. Global redesign и AI chat/provider UI не добавлены.

Policy хранится в existing Preferences, config writer v8/reader1–8. Legacy1–7 получают
SUGGEST/enabled/saved-parameters=false/AUTO mask0. Unknown modes/masks/flags rejected.
Credentials, receipts, approvals и runtime grants не сохраняются в project/snapshot.
Schema21/archive1 не изменены. Старые binaries с config reader<=7 не читают v8;
validation использует testing=true и не меняет реальный user config. Accepted0.3h upd2
package/manifest/shortcut сохраняются; user-facing AI prototype package — #167.

## Layers / realtime

| Слой | Результат |
|---|---|
| UI | AFFECTED: mode menu, settings panel, software100/150 snapshots |
| Application/domain | AFFECTED: permission setter, receipts, proposal/query/apply gate |
| Core commands | REUSED: подготовка existing commands, execution deferred |
| Project/persistence/Undo | NOT AFFECTED: нет AI writes, schema21/archive1 |
| User preferences | AFFECTED: v8, legacy defaults |
| Engine/DSP/RT | NOT AFFECTED: NON-RT parser/authorization, existing state observation |
| Studio/Live | SHARED Application; runtime performance guard |
| Tests/docs | AFFECTED |

Mutex/atomic/string/command allocation только control/worker paths. Audio callback
не читает permissions и не ждёт AI. Human playback/recording/device paths не изменены.
RT SAFE WITH MANUAL CHECK для границы; HTTP/physical ASIO/DPI gate остаётся в #167.

## Validation и следующий шаг

ai_registry/ai_policy/ai_authorization покрывают strict JSON/schema bounds/unknown fields/
Unicode, request/snapshot binding, same operation replay/conflict, Core command names,
mode matrix, future readiness gates, Live/record guards, readonly history/dirty,
revoke/mode-switch/cancel/late delivery, legacy preferences, wrong-thread и shutdown.
Прежние6 AI suites сохраняются, revocation-priority lifetime test обновлён.
juce_ai_permissions: read mode, explicit AUTO flags, visible mode, revoke, no project
mutation, control bounds, PNG100/150 inspected. Physical mixed-DPI/ASIO/network NOT RUN.

Release configure/full build PASS; final CTest140/140 PASS99.94s, focused AI9/9
PASS0.55s, GUI1/1 PASS0.17s. Independent Python JSON schema parser PASS:6 closed tools,
no model mode/confirmation fields. Source `f7b78e379d5d9cbecf9c69339498d564c28eff0e`.
Software PNG100/150 inspected; physical mixed-DPI/ASIO/network/provider NOT RUN.
Логи `build/asio-local/validation/ai-tools-permissions`. Initial MSVC shadow/string-view
UI conversions и test create_midi_clip signature/revocation expectation исправлены.
Следующий пользовательский тестовый slice — #167: provider/AI panel READ+SUGGEST.
#163/#164/#165 нужны до включения EDIT/AUTO. #161/#162/#26 остаются open до приёмки;
проверки фактической permitted write/preview/approval/exactly-once Apply не заявлены.
