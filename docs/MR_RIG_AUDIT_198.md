# MR Rig P0 / #198 — аудит принятого Cab и безопасного перехода

Дата: 2026-10-15. Статус: **принято пользователем**: «принимаю 198 и поехали 199».
Единственный активный parent — #197. #182 superseded; отдельные #186–188 не
продолжаются. Исторические #183/#184/#185/#196 — принятый фундамент, не новые задачи.
Сам аудит не реализовывал Rig, NAM, педали или библиотеку. После приёмки пользователь
отдельно разрешил #199; утверждён уточнённый путь без старых тестовых проектов/Convert.

## 1. Проверенная локальная база

- Checkout: `C:/Users/Vladislav/Documents/GitHub/MR-Studio`.
- Ветка `mrs/0.2g-fix1-editor-space-local`, HEAD
  `10fef43faea722737d76226f563b3be4a7852ad4`, рабочее дерево перед аудитом чистое.
- Принятый source: `ab66a2e34da2f8443df8bd5fef587b585ebffe22`;
  между ним и HEAD нет изменений в `apps/`, `core/`, `cmake/`, `CMakeLists.txt`.
- `application_version`: **0.4b upd3 Native Editors**. Пакет с тем же именем
  в проектном Builds: все **12 записей SHA256 проверены чтением**. GUI hash:
  `3E0DA57C3CE74FFF4656B8FD8F381705883F9D14A6E01DD50D338D7442C068BD`.
- Исторический лог upd3 подтверждает targeted **7/7 PASS, 47.28s**. Ранее принятые
  full149/149 и packaged smoke остаются прежними свидетельствами. Здесь ничего
  не пересобиралось и не запускалось; новых DSP/ASIO/listening/DPI результатов нет.
- GitHub API `/issues/197/sub_issues` действительно вернул **#198–204**, все open.
  #198 — первый этап P0; Markdown не использован как доказательство иерархии.

Изучены актуальные #197/#198 и handoff/roadmap; в каноне — Architecture §§2–4,
Library §§1/3–5, Audio/Pedals §§1–4, GUI §§2/6. Применены обновлённые проектные
`mr-plugin-development` и `mr-realtime-audio-safety`. Правила старого checkout
AGENTS не переопределяют обновлённый проектный AGENTS и текущий запрос пользователя.

## 2. Карта существующего кода

Пути ниже относительно checkout; это найденные реализации, не предлагаемые имена.

| Область | Файл / символ | Что реально есть |
|---|---|---|
| Состояние проекта | `core/include/mrs/core.hpp:60,97` — InsertKind, NativeInsert | cab_ir и cab_collection — разные enum values; IR controls, external ReverbState, cab IDs, cab_metadata, preset_name |
| Сериализация | `core/src/serialization.cpp:49–75` | schema23; CAB_SELECTION1/2; REVERB2; enum reader ограничен известными kind0..8 |
| Каталог | `core/audio/include/mrs/cab_collection.hpp` — CabAsset, CabMapping, CabManifest, CabCollection | Один внешний каталог, async scan, mapping overrides, фактические rate/channels/frames/digest; это не Rig library |
| Идентичность | `core/audio/src/cab_collection.cpp:25–37,67–76,126` | FNV1a64+bytes, asset/variant identity, роли cabinet/full_combo/unknown, exact-rate selector с сохранением длины |
| Построение узлов | `core/processing/src/model.cpp:123–136` — insert_graph | cab_ir → mrs.cab-ir; cab_collection → mrs.cab-v2; отдельные Gain/EQ/Reverb |
| Реестр DSP | `core/processing/src/native.cpp:166–174` — native_factory | mrs.cab-v2 = ReverbProcessor(true), mr_reverb, embedded Cab, Gain, Biquad, ChannelEq |
| Свёртка/обмен | `core/processing/src/reverb.cpp` — ReverbProcessor, ReverbKernel | prepare_kernel, stage_kernel, bounded switch/retired, wet controls, smoothing; текущий Cab core |
| Загрузка | `core/audio/src/reverb_library.cpp:97–165` — ReverbLoader::Impl | Worker decode/identity/SRC/kernel prep; cancellation/generation, previous active on failure, shutdown/join и graph lease |
| Граф / PDC | `core/include/mrs/processing.hpp`, `core/processing/src/graph.cpp` | IProcessor, PreparedGraph; ProcessBlock, queued params, latency/live/anticipation flags, bounded graph и bypass PDC |
| Команды / runtime | `core/include/mrs/core.hpp:371`, `apps/studio-desktop/src/application.cpp:647–738,1440–1475` | SetInserts, preview/commit/cancel, load_insert_preset; stop/quiesce, prepared affected chain, loader shutdown, Undo/Redo |
| Пресеты | `apps/studio-juce/src/PluginPreset.h:47–65` | MRS_PLUGIN_PRESET1 — сериализованный single-insert project; decode требует тот же InsertKind, remaps slot ID |
| Окно | `apps/studio-juce/src/Editors.cpp:212–231,302–313`; `Desktop.cpp`, `NativeEditorSpace.h` | Hidden configuration → final HWND owner/hooks → show once; native preset in place; retired paint guard; external VST lifetime отделён |
| Browser/preferences | `CabBrowser.h`, `Settings.h` | Фильтры отдельно от active source; bounded per-insert UI preferences, local manifest/root, favorites; нет ToneModel/RigPreset |
| Тема | `apps/studio-juce/src/Controls.h:40` — ui::Theme | Действующий LookAndFeel/контролы; #189 не блокирует первый Rig shell |

Критическое уточнение: **принятый Full Combo backend — mrs.cab-v2**, не
mrs.cab-ir. Старый embedded Cab не нуждается в новом compatibility adapter по
прежнему решению пользователя. Сохранить надо принятые Cab collection / Full Combo
проекты #185/#196; это отдельное требование от поддержки Legacy Cab #23.

## 3. Что можно переиспользовать и чего пока нет

Есть native Gain, HP/LP/single EQ, ChannelEq, Cab-mode convolver и IR Reverb.
У Reverb есть **wet-input gate**, но это не готовый самостоятельный Pre Gate:
его state, tail semantics и UI привязаны к Reverb. ChannelEq сглаживает параметры;
обычный Gain/Biquad не становятся автоматически безопасными плавными Rig knobs.

В native registry и проверенных processing/application sources **не найдены**
реализованные Drive/Saturator, Compressor, Chorus, музыкальный Delay, NAM runtime,
RigProcessor, ToneModel/RigPreset или module descriptors. `mrs/delay.hpp` используется
графом для компенсации; PDC delay нельзя выдавать за музыкальную педаль. Спецификация
«reuse Saturator» пока не подтверждена исходниками. Отдельного Freeze API по поиску
в core/desktop не найдено; Freeze compatibility не проверена и не объявляется готовой.

Graph limits: max_nodes32, внешняя цепь проекта — до8 inserts. Внутренние педали
не должны расходовать видимые inserts дорожки или создавать новый ProjectStore,
ASIO device, VST3 host либо независимый audible graph. Расширять лимиты без расчёта
памяти/очередей нельзя. Переиспользование IProcessor/подготовленных DSP не означает
повторный запуск внешнего host graph из callback.

## 4. Решение после уточнения пользователя

**Уточнение 2026-10-15:** «Старые проекты сохранять не нужно, они были только для тестов».
Требование обратной совместимости с тестовыми Cab-проектами снято. Предыдущая
рекомендация B с Convert/fallback отозвана в этой части.

Предлагаемый путь: один новый **MR Rig / mrs.rig** с чистым versioned RigState,
переиспользованием проверенного WAV DSP и одного editor lifecycle. Не нужны:
Cab→Rig Convert, compatibility wrapper, автоматическая миграция тестовых проектов,
сохранение старых ID ради их чтения, fallback Cab editor или old-schema acceptance
matrix как gate нового продукта. Существующие файлы проектов, WAV и принятые
бинарные пакеты при этом не удаляются и не переписываются.

Новый processor ID обозначает новую семантику контейнера Pre/Core/Post. Конкретные
enum/формат и schema выбираются при реализации, без бессмысленного переименования
внутреннего ReverbProcessor. Никаких ID или source изменений в P0 не выполнено.

Сохраняются инженерные требования: точная SourceAsset identity, честные роли,
безопасная загрузка/переключение, один audible core, команды/Undo, сохранение
**новых** RigPreset/проектов, один editor без переоткрытия. Input Trim, Core wet Gain,
Mix и Output Trim имеют независимую семантику; Input/Output trims по умолчанию unity.
Отказ от старых тестовых проектов не разрешает сбрасывать новые настройки Rig
при закрытии редактора и не превращает WAV в нелинейную amp-модель.

## 5. Адаптеры и границы данных

**Конкретный blocker простой обёртки:** PreparedGraph::stage_reverb_kernel,
collect_reverb_retired, reverb_switching, warm_reverb, tail_frames делают
`dynamic_cast<ReverbProcessor*>` непосредственно у узла (`graph.cpp:227–259`).
ReverbLoader хранит graph+node ID и пользуется этими методами. Узел RigProcessor
не пройдёт cast. Просто вложить ReverbProcessor и повторно вызвать loader нельзя.

Предлагается небольшой typed **IR kernel endpoint** (имя концептуальное):
stage/switching/activate/collect/tail, реализуемый существующим ReverbProcessor и
делегируемый RigProcessor выбранному WAV core. Сохранить current ownership,
graph lifetime/revision invalidation и shutdown/join; не хранить указатель на child,
который может уничтожиться при переключении NAM/WAV. Проверить все graph methods,
не только stage. Сам kernel/decode/SRC/crossfade переиспользуется, не переписывается.

Для #201 предлагается один native IProcessor-контейнер с заранее подготовленной
bounded последовательностью child native DSP; один host node/PDC identity. Child
parameter address = module instance ID + stable parameter ID. Текущий ParameterInfo
имеет диапазон/default/name/hidden, но не полные units/smoothing/position/capabilities;
нужен ModuleDescriptor adapter, а не контракт только на картинку педали.

Разделить три сущности:

- **SourceAsset**: external WAV identity/rate/frames/channels; позднее NAM format/arch.
  FNV1a64+bytes — текущая проверка изменений, не криптографическая гарантия прав/доверия.
- **ToneModel**: независимый стабильный modelId, source variants, роль/происхождение,
  title/tags/artwork/licence. Это глобальная curation metadata, не CabManifest row.
- **RigPreset**: project/preset snapshot модели/source и Pre/Post modules/controls,
  Input/Output trims/bypass. UI tab/search/selection — локальные per-insert preferences.

Не использовать абсолютный путь как modelId. Cab variant_id для classified assets
зависит от semantic mapping/tags; набор исключаемых capture fields ограничен.
Добавление artwork/genre в CabMapping.tags может изменить variant identity.
Поэтому обложка/теги живут в новой небольшой ModelLibrary, с мостом к неизменным
CabAsset references. Role adapter: existing `cabinet` → view `cabinet_only`,
`full_combo` → same, `unknown` → same; старые stored значения не переписывать.
NAM full_rig/amp_only/unknown — будущий отдельный typed capability, не tags-магия WAV.

Artwork не управляет DSP: missing image → fallback; missing source → явное Missing,
identity/настройки не теряются. Source relink проверяет bytes/digest и явный выбор;
нельзя заменять похожим файлом. Decoder/size/rights validation — вне RT. Фото
пользователя/лицензированное, без скачивания коммерческих фото или WAV/NAM в пакет.

## 6. Минимальные следующие этапы — предложение, не запуск

| Этап | Минимальный законченный результат и зависимости |
|---|---|
| P1 #199 | Небольшая versioned ModelLibrary, IR Models cards/tags/favorites/cover fallback, точные SourceAsset refs, optional Import My IR/manual role; NAM явно Planned, My Rigs без вымышленных сохранённых chains. Без vendor universal wizard/multi-mic |
| P2 #200 | Одно Rig window, центр model card, native inspector, WAV-only core; минимальный RigPreset с пустыми Pre/Post; native kind/endpoint и новый RigState без legacy migration. P1 accepted; #189 не blocker |
| P3 #201 | Реальные ordered native Pre/Post instances, parameter preview/commit, bypass/reorder/Undo/PDC; unsupported slots честно недоступны; не рисовать уже работающие несуществующие педали |
| P4 #202 | Малый согласованный DSP subset. EQ реально переиспользуем; Gate/Drive/Compressor/Chorus/Delay требуют самостоятельной реализации/адаптации. Reverb — реальный IR DSP с отдельным endpoint/state и tail policy |
| P5 #203 | Dedicated NAM loader/inference и проверка форматов/лицензии/поддерживаемых architectures. Full Rig без второго Cab; Amp Only с optional одним Cab; Unknown с явным routing choice |
| P6 #204 | Сквозные проекты/полные Rig presets, failures/relocation, Studio/record/Live budget и реальная пользовательская приёмка |

Это уточняет P1/P2 из roadmap: библиотека и shell — последовательные отдельные
приёмки, не обещание внедрить их вместе с цепью/педалями в P0.

## 7. RT, GUI и проверяемые риски

- **NON-RT:** library/artwork, validation, decode/SRC/kernel/NAM warm-up, graph compile,
  serialization, GUI. Loader mutex/control lock допустимы здесь; не вызывать loader
  snapshot/collect/prepare из аудиопотока.
- **RT-adjacent:** Application/runtime владеет graph/loaders, revisions и command
  transactions. Сохранять device stop → engine quiesce → worker shutdown/join →
  prepare/publish → restart. Structural changes пока требуют Pause/Stop; это не
  обещание live hot-reorder педалей или смены NAM backend без остановки.
- **RT-critical:** RigProcessor.process только bounded prepared data, без alloc/free,
  locks/I/O/UI. Один активный core, bounded transitions, retirement вне callback.
  Не клонировать тяжёлый NAM ради GUI или незамеренного crossfade.
- PDC уже учитывает latency bypassed processor. Rig должен сообщать суммарную
  алгоритмическую задержку модулей и правильный dry-delay; IR duration не latency.
  Изменение latency структурное; не обновлять report скрыто в середине playback.
- Live/record paths — device-sized blocks, Studio anticipation только при явно
  подтверждённой eligibility. Новый контейнер не объявлять anticipation_safe по
  аналогии с одним child. Raw recording оставить до Rig/PDC.
- UI сохраняет hidden-first/final owner/Space/same-window и защиту retired paint;
  asynchronous callbacks проверяют project/slot/generation. Пресет не создаёт
  второе окно и не сбрасывает filters/geometry. Physical mixed-DPI не доказан тестом.
- Headroom/gain/mix и metering определить по месту в цепи; WAV input trim даёт
  линейный уровень, не amp Drive. Для NAM input level может менять нелинейность.

Будущие проверки: (1) schema23 Cab → explicit Rig Convert → Undo/Redo, no source
substitution и сохранение nonunity wet Gain/Mix; (2) new preset/file project roundtrip
и отказ старого reader без silent loss; (3) failed/missing/relinked source + metadata;
(4) retained window/HWND/geometry, async callbacks и queued paint после removal;
(5) container vs equivalent serial DSP oracle, module order/bypass/tails/PDC;
(6) worker cancellation и backend changes при закрытии/Undo/rate/buffer restart;
(7) callback alloc/free probes, max time/xruns/RAM, 1/2/4 rigs ×128/256/512/1024,
48k и отдельно96k, Studio отдельно от recording/Live; (8) физические ASIO/listening/DPI.
До появления соответствующей реализации эти результаты **NOT TESTED**.

## Решение, переданное на приёмку

Утвердить уточнённую архитектуру: **один MR Rig / mrs.rig, общий WAV DSP,
чистый RigState, без поддержки/конверсии старых тестовых Cab-проектов**.
Условия Convert/fallback/old schema fixtures из первоначального предложения сняты.
Ни ID, ни schema в этом аудите не изменены.
Новых сборок, запуска приложения, тестов или обращений к аудиоустройству не было.
Статус RT: **NOT RT-VALIDATED для будущего Rig**; оценена архитектура и найдены
интеграционные риски, а не измерена безопасность ещё не существующего DSP.

Основания: [#198](https://github.com/vladleng/MR-Studio/issues/198),
[#197](https://github.com/vladleng/MR-Studio/issues/197), актуальные
[roadmap](MR_RIG_ROADMAP.md) и [handoff](MR_RIG_CODEX_HANDOFF.md) на GitHub main;
локальные реализации и принятый [upd3 audit](NATIVE_EFFECT_EDITOR_UNIFICATION.md).
