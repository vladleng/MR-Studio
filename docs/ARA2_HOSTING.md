# MR Studio — ARA2 Hosting: архитектура и план реализации

**Статус на 2026-10-18:** P1/#177 и P2/#178 приняты пользователем; P3/#179 реализован локально и ожидает ручной приёмки с ограничениями. P4/#180 и P5/#181 не начаты. Актуальные проверки/совместимость — в соответствующих Issues; этот файл задаёт контракт.
**Tracking:** [#88 — ARA 2 Интеграция](https://github.com/vladleng/MR-Studio/issues/88), родительский FEATURES [#78](https://github.com/vladleng/MR-Studio/issues/78).
**Платформа первого этапа:** Windows x64, JUCE/C++, ASIO, существующий VST3 host.
**Принцип:** ARA2 — расширение хостинга внешних редакторов, а не новый аудиодвижок и не универсальный API управления параметрами плагина.

## 1. Цель и границы

Позволить открывать выбранный аудиоклип MR Studio во внешнем ARA2-совместимом редакторе (например, Vovious, Melodyne, RePitch — **только после отдельного compatibility test**), редактировать его в контексте проекта и сохранять/восстанавливать результат.

Первый вертикальный сценарий: выбрать вокальный WAV-клип → открыть ARA-редактор → передать материал/позицию → отредактировать интонацию в интерфейсе плагина → услышать ровно один обработанный сигнал в Arrange/Mix → сохранить проект → повторно открыть и получить тот же результат. Изменения **не разрушают исходный медиафайл**.

ARA2 **не** предоставляет хосту стандартных команд «передвинь ноту №3», «исправь фальшь» или API извлечения всех внутренних blobs/нот плагина. Эти действия зависят от vendor-specific API/экспортируемых данных/автоматизируемых параметров. Не обещать их приёмкой ARA Host.

**Не входит в первую версию:** собственный MR Pitch [#73](https://github.com/vladleng/MR-Studio/issues/73), нейросетевой анализ/вокодер, гарантированная коррекция нот через ИИ, автоматическое управление GUI стороннего плагина, поддержка всех ARA vendors, Live low-latency ARA workflow, макросы для обхода внутренних функций плагинов, перенос Harma Waves Core.

## 2. Основания и рабочие связи

- JUCE [ARA support](https://github.com/juce-framework/JUCE/blob/master/docs/ARA.md) — справка по демонстрационному AudioPluginHost. Фактический MR host использует существующий direct Steinberg VST3 SDK 3.8.1, JUCE 9.0.3 — GUI. `MRS_BUILD_ARA` opt-in/default OFF и pinned внешний SDK; `juce_set_ara_sdk_path` настраивает SDK, но production `JUCE_PLUGINHOST_ARA`/второй JUCE host не включаются.
- [Celemony ARA_SDK](https://github.com/Celemony/ARA_SDK), [ARA_Examples](https://github.com/Celemony/ARA_Examples): зафиксировать **публичный tagged release** SDK и совместимость с фактически установленной в локальном checkout версией JUCE. По документации JUCE базовый пример использует releases/2.3.0; перед сборкой сверить актуальную поддерживаемую комбинацию, не следовать слепо HEAD.
- [VST3_HOSTING.md](VST3_HOSTING.md): переиспользовать обнаружение/загрузку VST3, жизненный цикл editor/state, но не считать обычный VST3 insert эквивалентом ARA instance. Изолированный сканер и существующие VST3 регрессии сохраняются.
- [AUDIO_ARRANGEMENT.md](AUDIO_ARRANGEMENT.md), [DATA_MODEL.md](DATA_MODEL.md), [PERSISTENCE.md](PERSISTENCE.md): Media Asset/Clip/Timeline/Project являются источником истины; уже принятые trim/split/move/copy/loop, Undo/Redo и Save/Open не ломать.
- [AI_INTEGRATION.md](AI_INTEGRATION.md), [AI_TOOLS_PERMISSIONS.md](AI_TOOLS_PERMISSIONS.md): использовать принятые Query/Tool API, Preview, identity/revision guards и READ/SUGGEST/EDIT/AUTO, без нового AI транспорта и без доступа из audio callback.
- [PROCESSING_RELIABILITY.md](PROCESSING_RELIABILITY.md), [ENGINE_PERFORMANCE_PLAN.md](ENGINE_PERFORMANCE_PLAN.md): RT-аудиопоток не ждёт сеть, диск, GUI или построение ARA document.
- [MR Pitch #73](https://github.com/vladleng/MR-Studio/issues/73) — отдельный **нативный** редактор, не зависит от ARA Host. [Harma Waves #115](https://github.com/vladleng/MR-Studio/issues/115) — отдельный native MIDI core; будущая самостоятельная VST3/ARA оболочка не равна MR Studio ARA Host.

## 3. Граница ответственности и модель

Управление идёт через существующие **UI → Application/Commands → Project Model → Engine/Plugin Hosting**. ARA adapter подключается к существующему аудиографу; отдельного «ARA DAW Engine» не создавать.

### 3.1 Модель, которую требуется уточнить на первом slice

- **Source / Media Asset:** стабильный ID исходного разрешённого аудиофайла, метаданные (sample rate, channel count, sample count) и revision/generation; безопасное чтение с ограниченным lifetime, без вмешательства в raw recording. Не подменять source на «обработанный» файл.
- **Clip / Audio Modification / Playback Region:** соотнести каждый clip instance, source offset, длину, позицию на таймлайне, срезы/копии/loop/reverse (последнее только если поддержано) с поддерживаемой ARA-моделью; определить явные правила split, copy, trim, move, duplicate, undo/reopen. Не смешивать оригинальные media samples с time-placement клипа.
- **ARA Document Controller / Editor / Playback Renderer:** жизненный цикл и ownership по правилам SDK/плагина. В одном проекте могут быть несколько клипов, источников и экземпляров; выбрать модель владения после аудита SDK и JUCE API, зафиксировать создание/изменение/удаление объектов, thread-affinity и порядок teardown.
- **Session Plugin Binding:** привязка к plugin ID/vendor/version и ARA instance; нельзя предполагать, что каждый VST3 поддерживает ARA. Для vendor без поддержки — обычный VST3 insert, а не фиктивный ARA.
- **Render path:** ровно один актуальный обработанный поток для активного региона, согласованный с транспортом, границами клипов, crossfade/trim и mixer routing. Избегать двойного звука dry+ARA; не помещать новую работу по управлению моделью в RT callback.

**Аудит перед кодом:** сопоставить реальные локальные классы Clip/Media/Plugin Graph/Archive с сущностями ARA SDK; зафиксировать таблицу «MR Studio ↔ ARA» и список недостающих API. Номера классов, схема архива и CMake targets уточняются по локальному checkout, а не выводятся из устаревшего GitHub source.

### 3.2 Аудиодоступ, транспорт и модели содержимого

Хост реализует согласованное чтение исходного PCM с правильной длительностью/каналами/форматом; background reading и cache должны уважать состояния проекта и ownership. Инициализация ARA/audio readers, синхронизация состояний и потенциально долгий анализ **вне audio callback**. Нельзя предполагать, что все callbacks ARA допустимы из любого потока: применять thread/edit-session rules используемой версии SDK.

Изменения позиции клипа, trim, split, move, undo/redo и Save/Open приводят к последовательным ARA model updates на управляющем потоке. Передача музыкального контекста (tempo, meter, key/scale, chord) и других optional capabilities — **по feature negotiation** и только при наличии корректного mapping. Отсутствие capability не делает базовый ARA hosting неработоспособным.

Transport start/stop/seek/loop и latency/PDC проходят через общий SHARED Engine. При выключении ARA или отсутствующем плагине предусмотреть детерминированный dry/fallback path и видимую причину, не потеряв сохранённое состояние и ссылку на плагин.

## 4. UX первой версии

**Локальный контракт P3/#179 (2026-10-18):** обработанный звук готовится на остановке на joined workers и подменяет PCM в существующем Engine один раз. Native editor меняет ARA/VST3 state; уведомления помечают звук устаревшим. Update playback или новый MR Play обновляет его перед стартом. Во время playback звучит последний подготовленный вариант с явным статусом. Vendor-owned audition остаётся неподдержанным. Native Play/Stop/Seek/cycle requests проходят через bounded queue в общий Core transport (Stop редактора = MR Pause с сохранением позиции). Editor получает coherent Engine position/Playing/PPQ/tempo/meter/loop snapshots через VST3 ProcessContext в коротком 64-frame process block на отдельном non-RT worker: входы тихие, весь выход отбрасывается, второго слышимого тракта нет. Буферы подготовлены на UI/control thread. Zero-frame flush недостаточен для редакторов, читающих playhead в audio callback. Worker join-ится до model edit/PCM preparation/teardown. Ошибка process block диагностируется отдельно без уничтожения готового PCM. Discovery изолирован и cancellable; in-process произвольный зависший vendor call нельзя прервать кнопкой Cancel. Save/Open/archive и долговечный Undo binding относятся к P4; P3 binding временный, об этом есть предупреждение. Missing/incompatible отклоняются при discovery/attach; сохранённые placeholders/relink — P4. Лицензия и коммерческая совместимость не выводятся из наличия ARA factory.

Завершённый ARA discovery сохраняет совместимые классы в machine-local `ara-catalog.json`; при новом запуске меню читает его без повторной загрузки модулей/сканирования. После установки/обновления/удаления нужен явный Rescan; выбор одного файла дополняет каталог, отмена сохраняет прошлый. Missing module виден disabled, attach повторно проверяет actual factory/class. Это metadata cache, не project/plugin state. ARA host toolbar использует общий EditorWindow/Theme и power/pin presentation обычных plugin editors; Pin временный до P4. Progress сохраняет active/focus при открытии/закрытии, чтобы Space работал после Update playback; обновление обычного insert-host не закрывает независимый ARA editor.

- Контекстное меню аудиоклипа: **Edit with ARA → [поддерживаемые ARA-плагины]**; опционально отдельная вкладка редактора клипа. Не добавлять в обычный VST3 insert скрытую ARA-семантику.
- Видимые статусы: normal / analysing / ready / offline / missing plugin / incompatible / failed; предупреждения о несохранённых изменениях и об отсутствии плагина.
- Кнопки/операции: Open editor, Bypass, Remove safely, при необходимости Re-analyse/Retry (если поддерживается), no destructive «Apply» без отдельного подтверждения.
- Не обещать GUI-интеграцию у каждого производителя: native plugin window может иметь ограничения/свой threading. Проверить focus, DPI, teardown/reopen и переключение проектов.
- Undo/Redo хостовых изменений клипа гарантируется контрактом MRS; undo истории внутренних нот/редакций плагина — только в пределах фактической поддержки стороннего плагина. При необходимости тестировать host restore checkpoint, но не объявлять его гарантией.

## 5. Persistence и восстановление

- Сохранять понятную MR Studio модель ссылок audio source/clip/region + идентичность плагина + его ARA document/archive state/opaque data **в поддерживаемом SDK формате**; проверить порядок сериализации, возможность восстановить reader/model до editor/renderer, пересоздание после restart.
- Проект должен открываться даже когда ARA-плагин удалён/недоступен/сменил версию: placeholder/fallback, сохранение неизвестных opaque bytes и возможность relink без потери данных.
- Для несовместимых версий или невалидных данных — явная диагностика и безопасный откат; не повреждать проект. Миграция схемы только после проверки необходимости и backward compatibility.
- Разделять: состояние MRS (Undo/Redo, graph/clip model, Save/Open) и приватные edits/Undo плагина. Никаких заявлений об автоматическом восстановлении его внутренней Undo-истории.

## 6. MR AI: доступные команды и ограничения

**Сначала Host ARA2 без ИИ; интеграция с ИИ — последний slice.** Команды реализовать как новую allowlisted группу существующего MR Command/Tool API.

Предлагаемые действия:
- READ: `list_ara_capabilities`, `query_ara_binding`, `get_ara_status` для выбранного клипа и поддерживаемых экземпляров;
- SUGGEST: `propose_open_in_ara`, `propose_ara_bypass`, `propose_ara_reanalyse` при объявленной поддержке плагина;
- EDIT: после разрешений + user preview/confirmation выполнить host-level attach/open/bypass/detach; обеспечить application command/Undo там, где операция меняет проект;
- AUTO: **не разрешён по умолчанию**, не расширять текущий allowlist незаметно.

Любая мутация использует подтверждённые host-created IDs, project/session/clip revisions, stale-context validation, permissions и Apply/Undo, как у текущего AI Foundation. Модель не получает arbitrary C++/VST3/ARA pointers, file paths, raw audio или секреты провайдера. Отправка PCM/notes/ARA state внешнему ИИ только с отдельным согласием, если когда-либо понадобится.

**Принципиальное ограничение:** ARA2 не является универсальным интерфейсом для изменения pitch blobs. AI-инструкция «исправь третью ноту на 20 cents» не должна заявляться работающей без отдельно подтверждённого vendor API / разрешённого parameter mapping / собственного MR Pitch.

## 7. План исполнения (ровно пять slices)

### P1 — ARA2 Host Foundation
1. Аудит локальных JUCE/VST3/CMake и SDK/version/license, матрица supported features; pin SDK; условная сборочная опция.
2. Проверка factory/capability и безопасного instantiation ARA extension; lifetime document/renderer/editor и capability diagnostics.
3. На синтетическом ARA TestPlugIn/MiniHost проверить безопасную загрузку, закрытие, отказ и отсутствие регрессии обычного VST3 hosting.

**Gate:** локальный build + smoke с тестовым ARA плагином; без claim о clip-editing. При отсутствии ARA — прежнее поведение.

### P2 — Audio Clip Integration
1. Source ↔ Clip/region mapping, корректная передача аудиосэмплов, позиция/offset/trim, mono/stereo и sample rate.
2. Transport/seek/loop, порядок model update в рабочем потоке, обработанный единожды сигнал в graph + PDC.
3. Split/move/duplicate/trim/undo/redo и переход между клипами; feature negotiation для tempo/meter/key.

**Gate:** детерминированный synthetic clip suite и однократный корректный playback, no audio callback alloc/lock/disk/GUI.

### P3 — Plugin Hosting / UX / Compatibility
1. Контекст Edit with ARA, управление статусами и окнами редактора, безопасный fallback при отказе.
2. Матрица ARA TestPlugIn + installed/licensed Vovious/Melodyne/RePitch **по доступности**, UI focus/DPI, plugin disabled/missing/version mismatch.
3. Регрессия обычного VST3, отказ unsupported plugin не меняет проект.

**Gate:** end-to-end edit/listen по тестовому плагину; коммерческие vendors отмечать **not tested** до физической проверки.

### P4 — Project State / Undo / Recovery
1. Сериализация и восстановление ARA state; соблюдение ownership/archive и версий.
2. Save/Close/Open, копии/несколько регионов, Undo/Redo clip edits, missing vendor, damaged state, crash/recovery.
3. Regression legacy schema и внешних медиа; если нужна миграция, описать schema upgrade и rollback.

**Gate:** тот же восстановленный звук/положение/состояние после перезапуска и безопасное открытие без плагина.

### P5 — AI Tool Integration
1. Только безопасные Host ARA tool queries/actions поверх текущих MR AI permissions, Preview/Apply, Undo и revision guards.
2. Assistant может выбрать/открыть подходящий ARA-плагин и показать статус; не притворяется редактором внутренних нот.
3. Тесты READ/SUGGEST, EDIT confirmed, AUTO denied, stale/expired/unknown plugin, project-switch, callback isolation.

**Gate:** реальные user scenarios с host-level операциями; перечень unsupported voice-edit commands видим и проверен.

## 8. Общая test / acceptance матрица

- **Functionality:** один/два mono/stereo клипа, разные offsets и sample rates, split/trim/copy/move, timeline seek/loop, bypass и fallbacks, несколько ARA-bindings.
- **Safety:** crash/timeout/incompatible plugin, disabled ARA, device restart, project close/reopen, delete source/track, long session, no UAF/dangling reader.
- **Persistence:** Save/Open/Undo/Redo, missing vendor, intact legacy project, no silent write to source WAV.
- **Audio:** no double signal, no unexpected gain change, latency/PDC, xruns/max callback time measured, no disk/network/ARA model mutation on callback.
- **UX:** DPI, editor focus, clip selection, clear unsupported/error state.
- **AI:** capabilities exposed truthfully; tool only through approved Application actions; confirmations and policy enforcement.

Каждый этап закрывается только после **локальной сборки/регрессии, ручной пользовательской приёмки своего объёма и синхронизации документации/issue**. Не подменять developer-unit pass физической ASIO/плагиновой проверкой.

## 9. Ограничения выполнения для Codex

- Перед работой прочитать [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md) и этот документ; проверять локальный checkout, git status и ветку. Реальные source commits могут быть новее публичного main.
- Код, SDK, локальная сборка, тесты и user packages — **только локально**. В GitHub — docs/issues-only commit с [skip ci]; не запускать Actions, не публиковать исходники, не делать PR/merge без прямого указания.
- Не изменять структуру другого native editor или Harma Waves/Live для ARA2. Режим Live использует общий Core; низкая задержка для ARA offline editing не является обязательным первым gate.
- После P1 остановиться и запросить пользовательскую приёмку; не начинать P2–P5 автоматически.
- При недостатке SDK, лицензированного vendor plugin или hardware ASIO ясно писать **blocked / not tested**, а не выдавать предполагаемую совместимость за факт.
