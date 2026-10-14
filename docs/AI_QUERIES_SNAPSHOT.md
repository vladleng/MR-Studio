# AI Query API и snapshot v1 — #159/#160

## Актуальная приёмка — 2026-10-14

Пользователь: «#158 - принят», «#160 - принят». #160 closed completed в текущей
0.3m upd4; source66b712f, прежние build/tests/package evidence сохранены.
#159: «на первом скрине» — screenshot показывает36 authored chords, первые16,
sections Verse[15360,61680)/Chorus[61680,107520) и явное предупреждение об усечённом
снимке/неполном списке. Это подтверждает disclosure ограниченного контекста;
не доказывает точность всего Query API или dirty/Undo/no-freeze сценарии.
#159 остаётся open до явной приёмки; parent#26 open7/11. Без новых build/tests,
paid calls или hardware measurements. Старые записи о приёмке ниже исторические.

2026-10-13. Пользователь: «Понял, переходи к #159/#160».
Parent [#26](https://github.com/vladleng/MR-Studio/issues/26).
База: локальный worker/context source0359a72, docs40ab35b, принятый package0.3h upd2.
GitHub main AGENTS/PROJECT_CONTEXT/AI_INTEGRATION сверены с локальными перед реализацией.
Используются действующие mr-feature-implementation, mr-realtime-audio-safety,
mr-studio-build-test. Этот slice не подключает provider/UI/tools и не принимает parent.

## Дополнение #161/#162 — централизованные permissions

Текущий Application capture содержит ReadAccess с policy epoch и host grant на
saved plugin parameters. Caller ReadPolicy не может повысить этот grant. Read/query/
snapshot проверяют разрешение до и после сериализации; Worker не принимает revoked
snapshot. Mode/revoke/project switch/shutdown отзывают прежние grants, отменяют jobs
и блокируют late results. Immutable данные сохраняются, но новый анализ старого view
после этих событий permission_denied. Описание lifetime первого slice ниже относится
к прежнему контракту и дополнено этим ограничением. Project edits/Undo/Redo сохраняют
immutable view, current-key guard предотвращает применение stale proposals.
[Tool API / permission authority и UI](AI_TOOLS_PERMISSIONS.md).

## Безопасная граница чтения

Application предоставляет `ai_read_view`, `ai_query`, `ai_snapshot` всем workspace/Live.
Захват выполняется на application thread; wrong-thread вызов отклоняется до обращения
к services. Captured ReadView удерживает существующие `shared_ptr<const Project>` и
`shared_ptr<const GraphState>`, копирует Context/ограниченные Live descriptors. Нет копии
полного ProjectDocument, plugin blobs или IR samples. Модель/graph закрыты private;
публичные результаты содержат только JSON allowlist и идентификаторы.

Captured ReadView допускает background queries без Application/engine/plugin calls.
Все страницы одного view и snapshot используют один ContextKey и одну revision.
Правки/Undo/Redo/открытие нового проекта не разрушают прежнее immutable состояние;
оно остаётся доступным для анализа уже сделанного снимка. Новый запрос Application
с прежним key получает stale_context. Код в audio callbacks не изменён и не знает AI.

Контекст использует project ID, session ID, project revision, graph revision. Graph
identity сверяется с документом. Transport — отдельное наблюдение, не revision проекта;
playback не инвалидирует read snapshot. Его временная позиция не является precondition
для будущего Apply: этот контракт относится к #161/#162.

## Запросы / scopes

QueryKind и versioned JSON Page покрывают:

- tracks/folders, clips, MIDI notes/channel events;
- chords, sections, markers, tempo/meter segments;
- mixer (включая Master), outputs/sends/hardware channel indices;
- insert chains и allowlisted сохранённые параметры;
- graph nodes/edges/outputs/MIDI routes и logical MIDI port descriptors;
- project-owned Live section/patch bindings, marker actions, patch IDs/names.

Automation, audio analysis, current Live performance state — unsupported. Наличие
сохранённых Live metadata не означает работающий Live executor. Current DSP/editor
state/physical MIDI port keys не читаются. Mixer/plugin данные — authored project/graph
state, а не текущий meter/preview или plugin editor draft.

Фильтры `id`, `track`, `clip`, `plugin`, `master`, `selected_only`, `samples`, `ticks`
применяются к соответствующему типу данных. Не относящийся к запросу фильтр отклоняется
invalid_input; он не игнорируется молча. `id` — stable entity ID, не имя/позиция списка.
Отсутствующий ID/parent получает not_found. Для MIDI требуется конкретный `clip`;
audio clip в MIDI query — unsupported. Пустой корректный filtered результат — обычная
пустая Page. Selection проверяется Context API, временная и не сохраняется в проект.
Выбранный track включает его clips/plugins/MIDI; folder selection не раскрывает subtree
автоматически. Для range-only selection temporal queries используют selection sample
range; temporal map queries возвращают действующий сегмент, включая точку до range start.

Ranges полуоткрытые. Samples проектные, Tick — quarter-note ticks PPQ960, channels0..15.
MIDI `source_start_ticks`/`source_length_ticks` сохраняют authored coordinates; project
start/end показывают **видимую часть** после source offset/trim. Скрытые notes/CC за
границами окна clip не возвращаются. CC/program/pressure/pitch bend/poly pressure сохраняют
существующие MidiKind ordinals2..6 и data1/data2. Audio clips возвращают sample range и
source offset, без media paths. Tempo в tick, meter в one-based bar и производном tick;
преобразование использует существующий Timeline, не новый musical-time алгоритм.

## Plugins / privacy

Метаданные insert chain отделены от параметров. Query plugin_parameters/graph_parameters
по умолчанию получает permission_denied; `ReadPolicy.stored_plugin_parameters=true`
явно разрешает **только сохранённые значения**. Это локальный read-policy entry point;
полная READ/SUGGEST/EDIT/AUTO policy и UI consent относятся к #162/#167.

VST3 insert values помечены normalized_value/authored_project_state. Native inserts
возвращают сохранённые controls с semantic name/unit (linear/dB/Hz/ms/ratio/boolean).
Native descriptor_index — индекс DTO, не processor parameter ID для mutation. Graph
parameters используют имеющиеся parameter IDs/raw stored values и authored_graph_state;
без предположения, что все native значения нормированы0..1.

Нет вызовов capture_insert, getState, DSP parameter getter, editor или device из query.
Снимок исключает plugin_path/processor_id (в graph он может быть путём), component/
controller blobs, IR assets/references, clip source, физические device keys, paths,
unknown archive extensions, произвольные Live notes и secrets. Названия проекта/дорожек/
плагинов, IDs и логические port names — разрешённые authored metadata. Автоматическая
цензура произвольного пользовательского текста не заявлена. API keys здесь не существуют.

## Пагинация и бюджеты

Page содержит source_items, scanned, returned, next_cursor, has_more, byte_limited,
query name, effective scope, cursor и ContextKey. `source_items` — размер исходной
коллекции, а не число совпадений. Cursor — индекс исходной коллекции **в frozen view**;
его нужно продолжать с тем же query/filter/view. При переносе cursor на новую редакцию
Application отклоняет прежний ContextKey. Scan-limited пустая страница может иметь
has_more=true; это не not_found и не конец результата.

Defaults:64 rows,512 scanned,64KiB/Page. Максимум256 rows/4096 scanned/1MiB per Page.
Размер проверяется для полного JSON envelope перед добавлением строки. Непоместившаяся
строка не потребляет cursor, byte_limited/has_more позволяют продолжить с большим budget.
Если единственная строка/envelope не помещается — response_too_large, без тихого обрезания
имени/объекта. UTF-8 проверяется, controls/quotes/backslashes экранируются.

Allowlisted текстовые поля<=4096bytes, без тихого substring. Capture limits:4096 tempo/meter points,65536 элементов каждой project entity collection;
4096 Live bindings/actions/patches/ports, name<=4096bytes. Большие metadata отклоняются
response_too_large. ProjectStore IDs могут требовать линейного поиска в ограниченной
коллекции; тяжёлые массовые reads следует делать через frozen ReadView на background thread.
Query scan и JSON output ограничены; абсолютный UI latency deadline не заявлен.

SnapshotBudget default256KiB bytes/256KiB conservative token upper bound; максимум1MiB.
До32 явных Queries, их порядок сохраняется. Снимок минимален по переданным query/scopes;
MIDI notes конкретного clip нужно запросить отдельно. Timing maps передаются отдельными
tempos/meters pages; запрос только tracks не включает их автоматически.

Snapshot JSON содержит snapshot_version1, Context, requested_query_names/count,
emitted_queries, pages и truncated. Page pagination и пропущенные по общему budget
queries явно видны; caller может продолжить на том же frozen view. Token upper bound
равен UTF-8 bytes сериализованного body — консервативный лимит, **не точный tokenizer**.
Prompt/system/provider framing и фактический tokenizer/cost accounting — #167.
Envelope/существенный Context не обрезается; слишком маленький budget даёт error.

## Immutable serialization / worker invalidation

Snapshot создаётся только ReadView, все его поля const; `shared_ptr<const Snapshot>`
не содержит указателей на модель/engine/plugins. JSON key revisions — decimal strings,
чтобы избежать потери uint64 в double-based clients. Samples/ticks сохраняют точные
существующие целочисленные диапазоны. Floating values сериализуются через to_chars,
не зависят от locale. Повторный snapshot одного frozen view/request даёт одинаковый JSON.
Неверный UTF-8/nonfinite value отклоняется, повреждённый JSON не выдаётся.

Request может удерживать Snapshot; Worker проверяет key и общий budget prompt+JSON.
Application использует captured Context из Snapshot, сохраняя его transport/selection
observation, и проверяет key против текущей модели. Project/GraphStore subscriptions
отменяют jobs сразу при commit/Undo/Redo/graph edits; replace и shutdown сохраняют
прежний cancel/join lifecycle. Даже если backend отвечает поздно, result key guard
отбрасывает ответ. Отмена остаётся кооперативной по контракту #157; arbitrary C++ tasks
не прерываются принудительно. Apply/mutations в этом slice отсутствуют.

## Влияние на слои

| Слой | Классификация |
|---|---|
| Application/domain/query | AFFECTED: frozen view, pages, snapshot, invalidation subscriptions |
| Shared Project/GraphStore | NOT AFFECTED: существующие immutable states/revisions |
| Commands/Undo | NOT AFFECTED: только read; будущие AI transactions #161/#164 |
| Persistence/project files | NOT AFFECTED: schema21/archive1 без миграции; snapshot не archive |
| UI | DEFERRED #167; текущая GUI — regression smoke |
| Audio/DSP/RT | NOT AFFECTED: NON-RT capture/JSON/query, нет callback dependency |
| Studio/Live | SHARED CORE / одна Application |
| Tests/docs | AFFECTED |

RT status: RT SAFE WITH MANUAL CHECK для текущей границы чтения. Offline playback во
время mock request проверяется; physical ASIO с HTTP/provider — NOT RUN, gate #167.
Новая user-facing сборка не выпускается для внутренних API; package0.3h upd2/ярлык сохранены.

## Проверки / продолжение

CTest ai_queries/ai_snapshot/ai_lifetime плюс прежние3 ai suites: fixtures и связи,
time boundaries, offset MIDI, native/VST3/graph stored parameters и denied policy,
unsupported/not_found/invalid/stale errors,2000tracks/budgets/cursors, UTF-8/control
escaping, secret/path exclusion, deterministic JSON, detached lifetime, concurrent
query/edit, dirty/history, revision cancellation/Undo/Redo/project switch/graph edits.
Независимый Python json parser проверяет21 pages экспортированного fixture, Unicode,
control chars, exact counts/keys, MIDI coordinates и Live port references.

Release configure/full build PASS; final CTest136/136 PASS140.83s, включая все6 AI
suite и GUI/audio/MIDI/VST3/persistence/history. Source `5beabc6ab7ca49f8284d860978894f14336e5302`,
ветка `mrs/0.2g-fix1-editor-space-local`. Independent Python JSON PASS21pages/14290bytes.
Предыдущий full136 PASS101.54s до final scoped/privacy guards; focused6 PASS0.48s до них.
Данные вне scope пропускаются перед сериализацией; unused VST3 class_id native inserts/
nodes исключён. Проверены scoped read при чужом invalid UTF-8 и native unused secret fields.
FEATURE READY WITH MANUAL CHECK для API boundary; parent #26 FEATURE INCOMPLETE,
HTTP/ASIO/permissions/UI gates остаются в следующих slices.
Логи `build/asio-local/validation/ai-queries-snapshot`. Исправлены test fixtures:
слишком широкая проверка слова source (цепляла parameter_source), transport stop/poll;
MSVC /WX class/struct forward declaration приведено к одному виду. При перекрытии двух
build процессов возник C1041/PDB conflict; после их завершения build и CTest выполнены
последовательно — PASS, без изменений build flags. Логи сохраняются.
#159/#160 остаются open до приёмки. Следующий блок — #161/#162, Command/Tool contracts
и permissions; provider/UI prototype #167 начинается после этих prerequisites.
