# Актуальный контракт / решение пользователя — 2026-10-19

P1–P5/#177–181 приняты и закрыты; общий документ, live playback, editor audition и workspace приняты в проверенных сценариях. Пользователь подтвердил Save/Open смешанного проекта с тремя разными ARA-приложениями. Это заменяет прежние ожидания этих проверок в исторических разделах ниже; полная vendor/physical ASIO/DPI матрица не заявляется. Текущий статус и ограничения — [#88](https://github.com/vladleng/MR-Studio/issues/88).

**Update playback** — резервная команда **ПКМ audio clip → ARA editor → Update playback**; отдельной кнопки в toolbar редактора нет (локальный `618c3b5`). Автоматический live ARA, PCM fallback, preparation/focus/Space сохраняют существующий контракт.

**Новый обязательный этап [#240 — Render and Restore](https://github.com/vladleng/MR-Studio/issues/240), реализация не начата:** ARA как Event FX должна обратимо фиксироваться в аудио отдельного event, с оригиналом и durable архивом правок для Restore после Save/Open. Render/Restore — команды ПКМ и иконка восстановимого rendered event, host Undo/Redo, без track/bus/master FX и без молчаливой замены shared source copies. Это отличается от автоматического PCM cache и Remove. Точный scope/DoD — в #240; #88 остаётся открытым до отдельной приёмки.

Baseline GUI J2/J3 вынесен в самостоятельную [#239](https://github.com/vladleng/MR-Studio/issues/239) вне ARA. Дрожание границ vendor GUI при resize не дорабатывать по решению пользователя; host flicker fix принят. Vovious internal track selection не объявляется универсально исправленным.

Остальные разделы ниже сохраняют архитектурные решения и исторические checkpoint-данные; актуальный статус приёмки определяется Issues и блоком выше.

# Follow-up 2026-10-19 — ARA editor workspace

Локальный `b67256a`, готов к ручной проверке: resize, Attach/Detach в общий editor slot, Full screen/Restore (иконка и F11). Attach расположен справа сверху рядом с Pin. Сохраняются тот же AraPanel/native HWND и ARA Session; view/DSP не пересоздаются. Resizable vendor получает checkSizeConstraint/onSize; fixed-size GUI сохраняет размер с прокруткой контейнера. Full screen из docked режима возвращается в dock; Pin/follow-selection и Space работают в обоих режимах. Геометрия/Attach — UI session-only, без новой project schema/archive/Undo. Исправлен возврат native focus после Cancel preparation, когда Windows уже активировала другое окно MR; внешнее приложение не перехватывается.

Release build PASS;12 уникальных адресных CTest PASS, финальный ARA/J1/native-editors3/3 PASS. J2 FAIL на прежнем baseline `reorder synchronizes both views` (#178/#88), общий GUI gate не green. MR chrome100/150% просмотрен; physical ASIO/mixed-DPI и реальный vendor resize/dock NOT TESTED. Ожидается отдельная ручная приёмка окна; #181 не начат.

Предыдущий `92e5c06` принят пользователем в проверенном Vovious audition сценарии: «Прослушка нот в редакторе работает». VocAlign переключение подтверждено; Vovious меняет заголовок окна, но отображение активной дорожки остаётся наблюдаемой особенностью плагина, универсальное исправление не заявляется. Пользователь разрешил следующий пункт.

# Follow-up 2026-10-19 — EditorRenderer audition

Локальный `92e5c06`: preview направлен в Master с отдельным device owner, без второго clip playback. Raw recording подавляет preview; close/reopen, safe gate/lifetime и isolated preview fault проверены. Клик передаёт SDK object selection с `timeRange=NULL` для уточнения поведения Vovious; Pin сохраняется. Release build и25 уникальных адресных CTest PASS; финальный ARA/GUI7/7 PASS. Vovious preview принят пользователем в проверенном сценарии; выбор внутренней дорожки Vovious остаётся наблюдаемой особенностью плагина. Physical ASIO/DPI отдельно не подтверждены. [Контракт](ARA2_EDITOR_AUDITION_88.md). Resize/Attach/Full screen реализованы локально b67256a и ждут отдельной приёмки; #181 не начат.

# MR Studio — ARA2 Hosting: архитектура и план реализации

**Follow-up 2026-10-19 — локальный `96e93d8`, принят пользователем («теперь все работает и результат слышен сразу»):** для zero-latency mono/stereo экземпляров реализован прямой ARA PlaybackRenderer на каждый region общего документа. Content edits, включая NULL range, больше не запускают PCM preparation в live mode. Статус `Ready - live ARA`, `live=N`, `frames=0`. Ненулевая latency/unsupported setup или более32 regions/document сохраняют компенсированный PCM fallback.16 уникальных адресных PASS + ARA-OFF4/4 PASS; generated Vovious/RePitch/VocAlign live-vs-PCM probes дают max difference=0. Мгновенный слышимый результат после правки подтверждён пользователем. Полная vendor/physical ASIO/RT матрица не заявляется. [Текущий контракт](ARA2_LIVE_PLAYBACK_88.md).

**Статус на 2026-10-18:** P1/#177, P2/#178 и текущий объём P3/#179 приняты пользователем и закрыты; ограничения проверок сохраняются. Drag/drop follow-up #88 принят пользователем (`96be919`: «Все перетаскивается»). Idle preparation/delayed progress и удаление подтверждения дропа реализованы локально (`315353d`), адресные16/16 PASS; ручная проверка выявила focus/drop/transient-render сбои, исправляемые `306d42c`; полный бесшовный playback ещё не достигнут. Resize/Attach/Full screen реализованы локально b67256a, приёмка ожидается. P4/#180 принят пользователем и закрыт (`46139b5`), сохранение нот трёх экземпляров Vovious после перезагрузки проекта и раздельные Undo/Redo подтверждены; P5/#181 не начат. Актуальные проверки/совместимость — в соответствующих Issues; этот файл задаёт контракт.
**Shared-document follow-up (локальный `86d92e3`, принят пользователем 2026-10-18: «Проверки пройдены, все работает»):** новые source bindings одного плагина объединяются в общий project-local document_id/Session/editor с отдельными track sequences; формат schema29. Legacy28 документы остаются раздельными. Контракт и границы — [ARA2_SHARED_DOCUMENTS_88.md](ARA2_SHARED_DOCUMENTS_88.md); проверки/приёмка — #88. Остальной UX backlog и #181 не начаты.

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

- Follow selection / Pin — follow-up #88 (2026-10-19): одиночный ЛКМ по bound audio clip переключает уже открытый незакреплённый ARA-редактор на его region/track. Общий документ переиспользует окно и native HWND; другой plugin/document открывает свой редактор и закрывает прежний незакреплённый. Pin удерживает текущий клип/окно и блокирует автоматическое переключение закреплённого документа. Остальные pinned окна сохраняются; если открыты только pinned окна или ARA-окон нет, простой выбор не создаёт окно. Unbound/MIDI, right-click, knife, trim/copy drag и Shift-снятие выделения не запускают follow. Явный Open/double-click остаётся явным выбором, включая pinned документ. Действие отложено до завершения одиночного клика, сверяет project identity/selectedClip/Pin, сохраняет focus аранжировки и работает при Play. Binding/model/Undo/persistence/audio graph не меняются; Pin session-only. Локальная реализация `c5ad745`; Release GUI build, `juce_ara_editor` и `juce_j1_smoke` PASS (2/2). Синтетический тест покрывает общий HWND, Pin, смену plugin/document, Play, focus, unchanged model/Undo и закрытые окна. Реальная vendor-приёмка этого пункта ожидается; результаты — #88.


**Исторический контракт P3/#179 (2026-10-18), сохраняется для PCM fallback:** обработанный звук готовится на остановке на joined workers и подменяет PCM в существующем Engine один раз. Native editor меняет ARA/VST3 state; уведомления помечают звук устаревшим. Update playback или новый MR Play обновляет его перед стартом. Во время playback звучит последний подготовленный вариант с явным статусом. Vendor-owned audition остаётся неподдержанным. Native Play/Stop/Seek/cycle requests проходят через bounded queue в общий Core transport (Stop редактора = MR Pause с сохранением позиции). Editor получает coherent Engine position/Playing/PPQ/tempo/meter/loop snapshots через VST3 ProcessContext в коротком 64-frame process block на отдельном non-RT worker: входы тихие, весь выход отбрасывается, второго слышимого тракта нет. Буферы подготовлены на UI/control thread. Zero-frame flush недостаточен для редакторов, читающих playhead в audio callback. Worker join-ится до model edit/PCM preparation/teardown. Ошибка process block диагностируется отдельно без уничтожения готового PCM. Discovery изолирован и cancellable; in-process произвольный зависший vendor call нельзя прервать кнопкой Cancel. С #180 binding и ARA/companion state сохраняются в Project schema28; attach/bypass/detach/relink имеют host Undo. Missing/incompatible/corrupt state сохраняется opaque для Retry/relink; подробный контракт и лимиты — [ARA2_STATE_180.md](ARA2_STATE_180.md). Ручная приёмка P4 подтверждена 2026-10-18. Лицензия и коммерческая совместимость не выводятся из наличия ARA factory.

Завершённый ARA discovery сохраняет совместимые классы в machine-local `ara-catalog.json`; при новом запуске меню читает его без повторной загрузки модулей/сканирования. После установки/обновления/удаления нужен явный Rescan; выбор одного файла дополняет каталог, отмена сохраняет прошлый. Missing module виден disabled, attach повторно проверяет actual factory/class. Это metadata cache, не project/plugin state. ARA host toolbar использует общий EditorWindow/Theme и power/pin presentation обычных plugin editors; Pin действует только в текущей session; HWND/UI-положение не являются ARA document archive. Progress сохраняет active/focus при открытии/закрытии, чтобы Space работал после Update playback; обновление обычного insert-host не закрывает независимый ARA editor.

- Контекстное меню аудиоклипа: первым идёт **ARA editor** со всеми host/discovery/recovery командами, затем **ARA 2** только со списком совместимых плагинов. Текущий plugin отмечен; готовая привязка не заменяется через список. Attach/Relink доступен для unbound/unavailable/failed source с повторной проверкой в обработчике popup. Меню после discovery имеет ту же структуру.
- Double-click follow-up #88 (локальный `633c3ca`, принят пользователем 2026-10-18): двойной ЛКМ по bound audio clip открывает/поднимает общее ARA-окно и выбирает region/track. Unbound audio сохраняет Audio editor, MIDI — piano roll; knife/right button не запускают double-click editor. Открытие не создаёт Model/Undo edit. Не добавлять в обычный VST3 insert скрытую ARA-семантику.
- Drag/drop follow-up #88 (локальный `96be919`, принят пользователем 2026-10-18: «Все перетаскивается»; подтверждение дропа удалено follow-up `315353d` по прямому запросу пользователя): browser содержит группу **ARA 2** из persistent capability catalog. Перенос на audio clip подключает точный module/class через прежнюю host-команду и открывает общий editor; ordinary VST3 leaf на audio clip сначала проходит isolated capability check, а существующие insert/instrument targets сохраняют прежнюю семантику. ARA-only drop на MIDI/пустое место ничего не создаёт. Применение отложено до возврата JUCE drag callback и разрешено на остановке; project store/revision, clip и drag generation повторно проверяются после scan. Source binding применяется сразу ко всем occurrences исходного аудио без диалога подтверждения — прямое решение пользователя от 2026-10-18. Тот же plugin/class поднимает существующее окно, другой требует Remove; unavailable binding/state не заменяется молча. Missing/unsupported class не создаёт binding. Host Undo/Save/Open/shared document используются без изменения схемы. Кнопка и команда **Remove** снимают ARA со source/all copies с сохранением оригинального WAV; отделение окна этим действием не выполняется.
- Видимые статусы: normal / analysing / ready / offline / missing plugin / incompatible / failed; предупреждения о несохранённых изменениях и об отсутствии плагина.
- Кнопки/операции: Open editor, Bypass, Remove safely, при необходимости Re-analyse/Retry (если поддерживается), no destructive «Apply» без отдельного подтверждения.
- Не обещать GUI-интеграцию у каждого производителя: native plugin window может иметь ограничения/свой threading. Проверить focus, DPI, teardown/reopen и переключение проектов.
- Undo/Redo хостовых изменений клипа гарантируется контрактом MRS; undo истории внутренних нот/редакций плагина — только в пределах фактической поддержки стороннего плагина. При необходимости тестировать host restore checkpoint, но не объявлять его гарантией.

**Подготовка воспроизведения — follow-up #88 (локальный `315353d`, 2026-10-18; приёмка ожидается):** после 600 ms стабильной content/model revision на остановке, без mouse gesture/modal UI и после окончания analysis, host заранее готовит PCM. Play переиспользует готовые caches; при dirty state обновляет только затронутые documents без общей invalidation. Явный Update playback сохраняет принудительное обновление для vendors без надёжных notifications. Progress появляется после 350 ms ожидания; короткая подготовка не создаёт окна и не меняет focus. Joined PCM workers и thread-affine model/vendor control сохраняются, во время ожидания обрабатываются только сообщения progress HWND. Это stopped/control pre-preparation, не обещание полностью неблокирующего vendor API; произвольный зависший vendor call остаётся непрерываемым. Cancel сохраняет предыдущие подходящие PCM caches и не запускает отменённый Play. Автоматическая попытка для той же revision после cancel/failure не повторяется; новая правка или явный Update/Play допускает повтор. Схема проекта, plugin/host Undo, RT callback и единственный слышимый тракт не меняются.

**Ручная проверка preparation — исправления #88 (`306d42c`, 2026-10-18; приёмка ожидается):** private document state и notifications с `signal scope remains unchanged` меняют state/dirty revision, но не audio revision. VST3 endEdit завершает жест без повторной audio invalidation; restart учитывает audio-affecting flags. State сохраняется независимо от PCM readiness. Настоящий signal change во время рендера не публикует устаревший PCM: прежние подходящие caches сохраняются, статус остаётся pending; attach не откатывается как failed vendor из-за этого transient состояния. Закрытие активного ARA editor сначала активирует MR owner HWND, затем уничтожает native child; без безусловного foreground-stealing. Synthetic native/editor/cache/drop проверки прошли; коммерческая ручная проверка остаётся обязательной.

**Инкрементальная подготовка #88 — 2026-10-19 (локальный `6653349`, приёмка ожидается):** Session переиспользует immutable region PCM и не переотправляет неизменившуюся модель. Достоверные source/modification/playback time ranges объединяются и пересчитываются с ARA head/tail, VST3 tail, latency и защитным блоком; остальные regions сохраняют PCM. Неизвестный/невалидный range, global parameter/restart/analysis update, structural change, infinite tail и explicit Update сохраняют консервативный full fallback. Dirty ranges/cache generation коммитятся только после проверки audio revision; отмена/race не публикуют частичный результат. Synthetic 120-frame signal edit:1197 DSP frames вместо440051, результат эквивалентен full render. Release/16 адресных PASS; это не vendor приёмка. [Подробный контракт](ARA2_INCREMENTAL_PLAYBACK_88.md). Статус при Play явно показывает ARA ON / ARA BYPASSED; disabled toolbar не меняет bypass model.

**Ограничения после `96e93d8`:** direct playback включается только при нулевой VST3 latency, mono/stereo, до32 regions/document и успешной инициализации realtime экземпляров. Для остальных случаев остаются stopped PCM preparation и возможный progress; live PDC для ненулевой latency не реализована. Structural/VST parameter/restart transitions используют stopped preparation. Обычные ARA content edits в live mode используют общий документ без PCM render и без ожидания точного диапазона. Persistent editor DSP инициализируется независимо от bake(), включая случай без открытия GUI; исправлен выявленный RePitch2.0.0 teardown crash неподготовленного экземпляра. Editor audition подключён отдельным follow-up `92e5c06`: постоянный EditorRenderer обрабатывается в device callback и смешивается перед Master; редкий worker сохранён только для warm-up. Vovious preview принят в проверенном пользователем сценарии. Resize/Attach/Full screen реализованы локально b67256a, приёмка ожидается; #181 не начат. Подробности RT ownership, failure behavior и проверок — [ARA2_LIVE_PLAYBACK_88.md](ARA2_LIVE_PLAYBACK_88.md). Ручная приёмка нового пути требуется отдельно.

**Lazy native editor sizing (follow-up #88, 2026-10-18):** view может возвращать 0×0 до attached(HWND). Хост подключает native view до окончательной проверки размеров, затем читает фактический размер; валидный pre-attach размер допустим как fallback. При отказе показывает этап ошибки отдельно от playback readiness и удаляет пустой native parent. Standalone attachment RePitch/VocAlign/Vovious проверен; открытие редакторов и текущий общий документ подтверждены пользователем; полная vendor/physical ASIO/DPI матрица не заявляется. Само исправление lazy editor sizing не меняло document ownership; общий документ для новых bindings добавлен отдельным follow-up `86d92e3`, см. [ARA2_SHARED_DOCUMENTS_88.md](ARA2_SHARED_DOCUMENTS_88.md).

## 5. Persistence и восстановление

**Контракт P4/#180 (2026-10-18):** stable source/modification persistent ID переживает Save As/media relink; source reference, plugin identity/version, ARA archive и companion controller/parameter state сохраняются вместе. Restore: document/companion → source/model/region graph → ARA archive в edit session → prepared live/PCM path. Readers1–27 мигрируют с пустыми ARA bindings; ARA-OFF сохраняет bytes opaque. Checksum/identity/version failures дают dry/missing fallback без удаления state; host relink/Undo используют checkpoints/rollback. Лимиты:8 bindings,1 MiB/blob,4 MiB ARA/project; сохранение/retirement ждёт завершения analysis. Подробности — [ARA2_STATE_180.md](ARA2_STATE_180.md). Реализация локальна; пользователь подтвердил сохранение нот трёх экземпляров Vovious после перезагрузки проекта и раздельные Undo/Redo. Mixed-vendor roundtrip и полная коммерческая state compatibility matrix остаются NOT TESTED.

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
