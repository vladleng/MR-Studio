# Moon River Studio — AI / ChatGPT Integration

## 1. Цель

Moon River Studio в будущем должен поддерживать AI-слой, который понимает структуру DAW-проекта и может анализировать или изменять проект через строго определённый API.

AI — не часть realtime Audio Engine и не должен влиять на стабильность playback/monitoring ни в production workspaces, ни в Live Mode.

## 2. Архитектура

```text
ChatGPT / OpenAI API
        |
        v
   AI Controller
        |
        v
 DAW Context + Tool API
        |
   +----+-----------------------------+
   |              |          |        |
Project Model   MIDI Model  Mixer   Analysis
```

Главный принцип:

> AI работает со структурированным состоянием проекта, а не управляет интерфейсом мышью и не пытается распознавать DAW по скриншотам.

## 3. Что AI должен уметь читать

Через Project Context API:

- project metadata;
- tracks/folders;
- audio/MIDI clips;
- MIDI notes, velocity, duration, CC;
- Chord Track;
- Arranger Track;
- tempo/meter map;
- markers;
- mixer state;
- routing;
- plugin chains;
- plugin parameters, когда это разрешено;
- automation;
- basic audio-analysis summaries;
- current selection/context пользователя;
- Live Mode context, если он активен: current section/chord/cue/setlist state.

## 4. Пример Tool API

```text
get_project_summary()
get_tracks()
get_track(trackId)
get_clip(clipId)
get_midi_clip(clipId)
get_chord_track()
get_arranger_sections()
get_mixer_state()
get_live_state()

create_midi_clip(...)
insert_midi_notes(...)
move_notes(...)
quantize(...)
transpose(...)

set_track_gain(...)
set_pan(...)
create_bus(...)
insert_plugin(...)
set_plugin_parameter(...)

create_marker(...)
create_section(...)
move_clip(...)
```

Все команды должны проходить через MRS Command/Undo system, а не менять внутренние объекты напрямую.

## 5. MIDI generation

AI может генерировать собственные MIDI-партии на основе контекста проекта.

Примеры:

- drums для выбранной секции;
- bass line по Chord Track;
- piano voicing/comping;
- strings;
- fills;
- reharmonized MIDI draft;
- вариация существующего MIDI clip.

Контекст генерации может включать:

```text
Tempo
Meter
Key / Chord Track
Arranger Section
Existing Bass
Existing Melody
Playing Range
Style/Profile
Density
Humanization
```

Результат — структурированный набор MIDI events, который MRS валидирует перед применением.

## 6. Анализ аранжировки и микса

В будущем AI может анализировать:

- register overlap;
- arrangement density;
- MIDI note distribution;
- duplicated voices;
- track activity by section;
- mixer levels;
- plugin chains;
- spectral/statistical summaries, подготовленные самой DAW.

Предпочтительно передавать подготовленные features/snapshots или выбранные offline-render fragments, а не неконтролируемый raw realtime stream.

## 7. Уровни разрешений

### READ
AI только читает контекст и отвечает.

### SUGGEST
AI предлагает изменения, но ничего не применяет.

### EDIT
AI формирует команды, которые пользователь подтверждает.

### AUTO
Разрешённый набор безопасных операций может выполняться автоматически.

Рекомендуемый старт: **READ + SUGGEST**.

## 8. Safety / Undo

Любое AI-изменение проекта должно:

- быть обычной MRS command;
- попадать в Undo/Redo;
- иметь diff/preview для сложных операций;
- не выполнять destructive file operations без явного подтверждения;
- не менять audio device/routing во время Live Mode без специальных разрешений;
- не влиять на realtime thread напрямую.

## 9. Realtime isolation

Строго запрещено:

```text
ASIO callback -> network/OpenAI request
ASIO callback -> JSON parsing
ASIO callback -> AI inference
```

Правильная схема:

```text
Audio Thread
     |
 atomic / lock-free state
     |
Application Core
     |
AI Worker / Network
```

Если AI недоступен, Moon River Studio и Live Mode продолжают работать без деградации audio path.

## 10. Voice control

Позднее AI layer может поддерживать голосовой интерфейс, например:

- "поставь loop на припев";
- "создай marker перед соло";
- "покажи только guitar tracks";
- "сделай копию этой MIDI-партии на октаву выше".

Такие команды также преобразуются в структурированные MRS commands.

В Live Mode набор разрешённых голосовых действий должен быть особенно ограниченным и предсказуемым.

## 11. Live Mode

AI может быть доступен и в Live Mode, но только как надстройка над тем же Project Context API.

Возможные безопасные use cases:

- показать/объяснить upcoming cue;
- найти нужную песню/setlist item;
- прочитать заметку;
- подготовить изменение во время rehearsal;
- диагностировать non-realtime issue.

Критические transport/routing/device действия в performance state требуют отдельной permission/safety policy.

## 12. Архитектурное требование уже сейчас

Даже если AI появится значительно позже, Project Model и Command architecture должны изначально иметь:

- stable object IDs;
- serializable project snapshots;
- query API;
- command API;
- validation;
- Undo/Redo;
- permissions/capabilities;
- понятные semantic names для track/clip/section/chord entities.

Это позволит добавить AI без переписывания DAW Core.


## 13. Декомпозиция AI Foundation — 2026-10-13

Parent: [#26](https://github.com/vladleng/MR-Studio/issues/26). На момент декомпозиции задачи — BACKLOG. Реализация начата отдельным поручением пользователя 2026-10-13; актуальный первый slice описан ниже. Исходные 11 чек-поинтов преобразованы в native GitHub sub-issues.

- [#158](https://github.com/vladleng/MR-Studio/issues/158) — Project Context API — контракт контекста проекта
- [#159](https://github.com/vladleng/MR-Studio/issues/159) — Query API — дорожки, клипы, аккорды, микшер, MIDI и плагины
- [#161](https://github.com/vladleng/MR-Studio/issues/161) — Command/Tool API — безопасные изменения проекта
- [#162](https://github.com/vladleng/MR-Studio/issues/162) — Permissions — READ / SUGGEST / EDIT / AUTO
- [#163](https://github.com/vladleng/MR-Studio/issues/163) — Preview / diff — просмотр и применение AI-предложений
- [#164](https://github.com/vladleng/MR-Studio/issues/164) — Undo / Redo — транзакции AI-изменений
- [#165](https://github.com/vladleng/MR-Studio/issues/165) — MIDI contract — генерация, замена и редактирование партий
- [#160](https://github.com/vladleng/MR-Studio/issues/160) — Project snapshot — согласованный снимок для AI
- [#166](https://github.com/vladleng/MR-Studio/issues/166) — Analysis hooks — контекст аранжировки и микса
- [#167](https://github.com/vladleng/MR-Studio/issues/167) — API prototype — ProxyAPI / Luna и настройки подключения
- [#157](https://github.com/vladleng/MR-Studio/issues/157) — AI worker — изоляция сети и AI от аудиопотока

Порядок: worker/context contracts → queries/snapshot → tools/permissions → READ + SUGGEST prototype; preview/Undo/MIDI до включения edits; analysis summaries и сквозная приёмка. Зависимости и критерии проверки указаны в каждой задаче.

Первый провайдер прототипа — ProxyAPI, базовый адрес `https://api.proxyapi.ru/v1`, выбранная модель `openai/gpt-5.6-luna`. Provider/base URL/model должны быть сменными; доступность, capabilities и фактические расходы проверяются при реализации. Секрет вводится в приложении и хранится защищённо вне project/source/logs/snapshot. Настройка ключа/баланса пользователем не подтверждает готовность интеграции.

Планирование не запускает новые feature stages автоматически и не меняет статус текущей сборки. Source/build/packages локально, GitHub Actions не использовать.

## 14. Начало реализации — worker/context v1, 2026-10-13

По прямому запросу «приступаем к #26» начат первый foundation slice #157/#158.
Prerequisites #15/#17/#18/#19 приняты/closed completed; Stage5/#25 также принят.
Локально реализованы `mrs::ai` Context v1 и bounded worker, Application read/request/
cancel/result APIs, session/revision guards и lazy lifetime. [Контракт, RT boundaries,
слои и проверки](AI_FOUNDATION_CONTRACT.md); [текущий результат](PROJECT_CONTEXT.md).
Чтение и jobs не меняют project/history/persistence: schema21/archive1 прежние.
Capabilities подробных queries/tools/analysis честно unsupported; HTTP/JSON/ключи,
чат UI и редактирование ещё не подключены. Используются mock Tasks; автоматических
платных retries нет. Cancel/deadline кооперативны, network adapter обязан обеспечить
конечные interruptible I/O и bounded read — этот критерий проверяется в #167.
Parent и sub-issues остаются open до соответствующей приёмки. Следующий блок:
#159/#160 (query DTO и согласованный snapshot), затем #161/#162. Пользовательский
пакет0.3h upd2/ярлык сохраняются; невидимый foundation не выдаётся как готовый AI.

## 15. Query API / Project Snapshot #159/#160 — 2026-10-13

По прямому запросу пользователя реализован следующий локальный slice: единый read-only
Application/query path, immutable ReadView, фильтры ID/selection/range, pagination и
allowlist JSON snapshot v1. Подробные сущности включают clips/MIDI, musical lanes/time,
mixer/routing/plugin chains, разрешённые authored parameters и сохранённые Live descriptors.
Неподдерживаемые automation/analysis/current Live state — unsupported. Snapshot содержит
один ContextKey, byte/token budgets, scope/cursors/truncation; project/graph edits,
Undo/Redo/switch отменяют worker jobs, stale replies не доставляются. DSP/editor capture,
пути/секреты/blobs/audio assets в экспорт не включены.
[Контракт и ограничения](AI_QUERIES_SNAPSHOT.md), [проверки/source](PROJECT_CONTEXT.md).
Это внутренняя подготовка: provider/chat UI ещё нет; первый пользовательский READ+SUGGEST
prototype остаётся #167 после #161/#162. Schema21/archive1/package0.3h upd2/ярлык прежние.
#159/#160 открыты до приёмки; parent #26 не завершён. Code/build/tests local, docs/issues
синхронизируются с GitHub отдельно, без source push/Actions.


## 16. Дополнение: Tools, Skills, модели и экономика AI API — 2026-10-13

**Статус: архитектурные идеи, не реализовано.** Дополнение к #26 и существующим 11 sub-issues; само по себе не запускает новую реализацию, не меняет порядок задач и не расширяет READ/SUGGEST до EDIT/AUTO.

### 16.1. AI Assistant / Orchestrator / Tools / Skills / Memory

- **AI Assistant (чат)** — понимает запрос пользователя и показывает результат.
- **AI Orchestrator** — подбирает разрешённые инструменты, подходящий Skill и модель, применяет лимиты расходов/времени и управляет ограниченным tool loop.
- **Tools** — типизированные функции MRS Context/Query/Command API: чтение треков/клипов/микшера, MIDI, управление параметрами MR Strip/Spatial/VST3, маркерами и маршрутизацией. Запись только через MRS Command, validation, permissions, Preview/Diff и Undo/Redo.
- **Skills** — версионированные инструкции и workflows на основе общих Tools (например MR Mixing, MR Guitar Mixing, MR Arrangement, MR MIDI & Harmony, MR Mastering, MR Live Setup). Skill не даёт дополнительных прав и не является произвольным исполняемым кодом; по возможности загружается только для текущей задачи.
- **Memory** — отдельная от контекста ChatGPT управляемая долговременная память о предпочтениях, рабочих профилях и подтверждённых решениях. OpenAI/ProxyAPI не переносит автоматически историю обычного аккаунта ChatGPT в MR Studio.

Логическая цепочка: **Chat UI → AI Orchestrator → Skills + Context/Tool API → Application Core → MRS Command/Undo → Project Model**. Музыкальные измерения выполняются локальными анализаторами; результаты передаются модели как компактные features. Никакого сетевого I/O, разбора JSON или inference в realtime audio thread.

### 16.2. Local-first управление и маршрутизация моделей

1. **Обычные детерминированные команды** (транспорт, Undo/Redo, действия меню, установленный параметр) выполняются локально без API, когда UI уже однозначно знает действие.
2. **Простые запросы естественным языком** (создать треки, добавить плагины, назначить уровни) — через экономичную модель и структурированный Tool API.
3. **Составные запросы** — один пакет проверенных действий / одна Undo-транзакция вместо множества самостоятельных вызовов; предварительная валидация каждого действия, единый Preview/Diff и откат при ошибке. Batch не обходит permissions.
4. **Звукорежиссёрские задачи** («сделай гитару теплее») — Skill + локальные DSP/спектральные summaries + предлагаемые настройки, а не попытка выполнять DSP внутри LLM.
5. **Сложная гармония, регармонизация, аранжировочная драматургия и неоднозначная диагностика** — опциональная более сильная модель с передачей минимального контекста и проверкой бюджета.

В прототипе #167 базовый вариант уже указан как ProxyAPI / openai/gpt-5.6-luna. Более сильная модель выбирается настройками, а не зашивается в Core. Смена провайдера, model IDs, доступность function calling/structured outputs и цены проверяются у провайдера при реализации. Автоматическое повышение класса модели отключаемое; при превышении бюджета или чувствительных действиях — подтверждение пользователя.

### 16.3. Что расходует токены API

- **Выходные токены**: не только текст чата, но и создаваемые моделью структурированные tool calls / аргументы функций, даже когда пользователь не видит JSON.
- **Входные токены**: инструкции, объявления tools и схем, содержимое выбранного Skill, история чата, Project Snapshot и ответы локальных инструментов, переданные обратно модели.
- **Локальное исполнение** команды, DSP/FFT/LUFS/MIDI-анализ и Undo/Redo **не расходуют API-токены** сами по себе. Токены расходуются, если результаты анализа или команды отправляются в LLM.
- Один запрос пользователя может включать несколько round trips tool loop; учитывать usage **всех** вызовов. Если провайдер предоставляет разбивку, учитывать input/output/cached/reasoning; иначе отмечать «нет данных».
- Не фиксировать в документации стоимость Luna/Sol как постоянную величину. Денежный расчёт — по текущему тарифу выбранного provider/model, с явной валютой и указанием оценок.

**Оптимизация:** компактные scoped snapshots и запросы по требованию; lazy loading Skills; не отправлять весь список Tools, если достаточно небольшого разрешённого поднабора; ограничение длины контекста, ответа, числа вызовов и бюджета; caching/dedup неизменившегося контекста, когда доступно; batch-команды и local-first shortcuts. Экономия не должна обходить валидацию, permission gates, stale-context guards и Undo.

### 16.4. AI Usage Monitor в MR Studio

Запланировать UI для прозрачной статистики:
- по запросу: дата, задача/Skill, модель/провайдер, число model calls и tool calls, input/output tokens, дополнительные usage категории при наличии;
- стоимость одного запроса, сессии, дня и месяца, с указанием тарифа/валюты и отличием фактической стоимости от оценки;
- лимиты на запрос/сессию/месяц, max tool-loop steps, max context/response, предупреждение перед дорогой операцией и подтверждение смены модели;
- отмена запроса, понятные ошибки лимитов, rate limits, 401/402/429; при недоступности API все локальные функции DAW продолжают работать;
- приватный локальный журнал без API-ключей, полных промптов, приватных снимков и аудиоданных по умолчанию; возможность очистить статистику.

При отсутствии usage или актуальной цены у провайдера показывать «неизвестно» или явно обозначенную оценку; не выдумывать точные расходы. Данные о цене не считать частью Project Snapshot.

### 16.5. Привязка к roadmap

- **#158/#159/#160** — компактный, защищённый и версионированный контекст; scoped queries и snapshot budgets.
- **#161/#162** — типизированные Tools, validation, permissions и контракт пакетных команд.
- **#163/#164** — Preview/Diff, транзакции, подтверждение, Undo/Redo и rollback.
- **#166** — локальные analysis summaries для музыкальных Skills.
- **#167** — первый READ+SUGGEST прототип: provider/model settings, usage capture, budget/tool-loop caps, безопасная модельная маршрутизация как дальнейшее расширение.
- **#157** — обязательная изоляция AI Worker/Network от audio callback, таймауты и отмена.
- **После foundation acceptance** — при необходимости отдельные задачи для Skill Registry/Loader, AI Orchestrator и полноценного AI Usage Monitor; не считать их выполненными или частью минимальных acceptance-критериев #167 без явного решения.

Приоритет: **безопасные Tools → ограниченные Skills → выбор моделей/стоимость → сложные автоматизированные workflows**. Для Live Mode — особенно строгие ограничения на автоматическое выполнение команд.

## 17. Tool API / Permissions #161/#162 — 2026-10-13

По прямому запросу пользователя реализованы6 закрытых typed tools и strict bounded
JSON/schema, ContextKey/request/operation guards и подготовка existing Core commands.
Application централизует READ/SUGGEST/EDIT/AUTO, explicit AUTO allowlist и saved-parameter
opt-in. Receipts привязаны к выданному snapshot и host policy epoch; смена режима,
revoke/project switch/shutdown отзывают grants и отменяют jobs. Repeat proposal возвращает
тот же plan, conflicting operation denied. Permissions не экспортируются в snapshot.
JUCE AI menu показывает текущий режим; Permissions panel применяет и отзывает доступ,
Preferences config8 читает legacy1–7 с безопасными defaults. Schema21/archive1 прежние.
[Контракт и ограничения](AI_TOOLS_PERMISSIONS.md), [source и проверки](PROJECT_CONTEXT.md).
Release build и140/140 CTest PASS; GUI100/150 software snapshots inspected.
Это READ+SUGGEST foundation: project Apply закрыт до preview/confirmation/Undo/MIDI
из #163–#165, проверки фактических writes/exactly-once mutation ещё не заявлены.
Следующий пользовательский prototype #167 (ProxyAPI / AI panel); package0.3h upd2 прежний.
Issues open до приёмки; source/build/tests local, docs/issues GitHub, без Actions/source push.

## 18. Первый API prototype #167 / 0.3i — 2026-10-13

По прямому запросу пользователя добавлены AI Assistant и сменные HTTPS provider/base/model,
Windows Credential Manager (endpoint-bound key), GET models/POST chat completions через
bounded async WinHTTP на existing worker. Snapshot-scoped READ/SUGGEST; typed proposals
проходят host context/request binding и Application validation без mutations.
Cancel/revoke/stale guards, byte/token/call limits; usage/model/finish видны, цена unknown.
Один call/message, без automatic retries/query loop/history; EDIT/AUTO locked до #163–#165.
[Контракт и ручная приёмка](AI_API_PROTOTYPE.md), [source/package/checks](PROJECT_CONTEXT.md).
Release build/full141 CTest PASS; final focused11 PASS после последних GUI/redirect guards.
0.3i package/packaged AI smoke exit0. Реальный Luna/model/tools/usage и ASIO с network
NOT RUN: READY WITH MANUAL CHECK, #167/#26 open. Monthly Monitor/Skills план§16 сохранён.
Code/build/package local; docs/issues GitHub, без source push/Actions.

## 19. Preview / Browser AI tab #163 / 0.3j — 2026-10-13

По прямому запросу пользователя Assistant перенесён во вкладку AI native Browser
рядом с VST3/Files; responsive layout и Connection collapse. Application строит
bounded immutable before/after diff из registered tools и текущего Project без мутации.
Marker groups Apply после explicit confirmation при EDIT/AUTO allowlist через shared
metadata path/staged transaction/одну Undo. Reject/no dirty; stale Refresh локально,
новый diff требует review; session/policy не переносится. Session-local replay ledger
предотвращает duplicate Apply/rebuild даже после Undo. READ/SUGGEST не пишут.
MIDI diff есть, Apply locked до #165; selection unsupported и не объявляется wire.
Mixed groups не применяются частично. Это supporting Undo для #163, не принятие #164.
[Контракт](AI_PREVIEW_BROWSER.md), [source/package/validation](PROJECT_CONTEXT.md).
Release build/full142 CTest PASS; final focused12 PASS3.92s после screenshot menu fixture/name escaping/session-limit message.
Package0.3j/packaged AI smoke exit0, Browser100/150 inspected; user API→Apply/Undo,
ASIO+network/physical DPI acceptance pending. #163/#26 open, source/build/package local,
docs/issues GitHub, без source push/Actions.

## 20. AI transactions/history #164 + chat UX / 0.3k — 2026-10-13

**Принято2026-10-13:** «#164 принимаю». #164 closed completed; native parent#26:
1/11 completed. #163/#26 остаются open; MIDI Application/GUI Apply остаётся #165.
Source/package/manifest/shortcut и прежние проверки сохранены; docs/issues-only
приёмка, новых build/tests/аппаратных измерений нет.

По прямому запросу пользователя #164: shared trusted ProjectEdit, preflight1–8
Core commands на private candidate, immutable exact before/after/label, одна общая
Undo/Redo без network/tool reexecution. Marker Application path сохраняет staged
rollback, confirmation/policy/session/Live/replay guards. Mixed metadata/MIDI/gain
composition проверена Core fixture; MIDI Application/GUI Apply остаётся #165.
Manual/AI history, dirty policy и Save/Open прежние; transient drafts/chat excluded.
В AI Browser добавлены sequential chat feed, composer clear after submit,
Enter=Send/ShiftEnter=newline, bottom composer/Send/Cancel по screenshot пользователя.
Лента bounded96entries/256KiB, не передаётся как conversation history провайдеру.
[Контракт](AI_TRANSACTIONS_HISTORY.md), [source/package/проверки](PROJECT_CONTEXT.md).
Release full build/full143 CTest PASS173.34s до final composer Value fix/version;
final build/focused13 PASS3.96s. Package0.3k/12hashes/packaged AI smoke exit0,
Browser100/150 inspected. Real provider Apply/Undo/Redo/SaveOpen, ASIO/physical DPI
подробная аппаратная матрица не предоставлена; #164 принят, #163/#26 open. Source/build/package local,
docs/issues GitHub, без source push/PR/merge/Actions.

## 21. MIDI draft / #165 / 0.3l — 2026-10-13

По запросу пользователя реализован closed MIDI draft JSON для INSERT нового clip
на existing instrument track, scoped REPLACE и EDIT по existing note/CC IDs.
Range conversion/source trim preservation, pitch/velocity/channel/CC/duration/
overflow/overlap/project+playback budgets validation; before/after, confirmation,
stopped/loop-off shared prepared MIDI transaction, exact one-Undo/Redo/rollback,
Save/Open. Existing transpose/velocity Apply также открыт. Runtime/plugins/routes/
MIDI generation сохраняются; callback/DSP не изменены. Human MIDI history сохраняет
прежний путь, AI history выбирает staged plan по центральному history name.
Selected track/clip+MIDI/tempo/meter context, explicit output budget64–4096(default1024),
whole-group truncation guard. Missing harmony/drum maps не выдумываются; map/pitches
и style/density пользователь может указать в сообщении. Marker badge fill alpha0.5,
caption/flag без изменений. [Контракт/границы/RT](AI_MIDI_CONTRACT.md),
[source/package/checks](PROJECT_CONTEXT.md). #165 open до пользовательской приёмки;
#164 accepted, native parent#26:1/11. Source/build/package local, docs/issues GitHub,
без source push/PR/merge/Actions. Actual provider musical quality/playback/ASIO/DPI
не подменяется synthetic fixture результатами.

## 22. Local analysis hooks #166 / 0.3m — 2026-10-14

Background summaries are now implemented on the existing AI worker: section note
activity/density/register/polyphonic ticks, authored mixer/routes and bounded dry
source RMS/peak. Source/range/units/confidence/partial/unavailable/stale are explicit.
Spectrum/VSTi/processed mix levels remain unavailable. Analysis button is local/no-key/
no-provider/read-only; ordinary messages receive summaries and a focused24KiB snapshot.
No raw audio upload/render/ML/stem separation. Cached by context/source identity and
stream size/mtime; cancellation/revocation and stale session delivery reused.
MIDI-only deletion/history avoids plugin capture/reload; stopped MIDI swaps retain
loop/timing/routing/generations. Marker badge25% opaque, caption/flag unchanged.
Rejected proposal reasons do not trigger paid retries; successful provider responses
with usage may be billed even when local validation rejects them (ProxyAPI rules
verified2026-10-14). Actual token savings/cost not invented. [Contract](AI_ANALYSIS_CONTRACT.md),
[current source/package/checks/limitations](PROJECT_CONTEXT.md). #166 remains open
for manual acceptance; #164/#165 accepted. Source/build/package local, docs/issues
GitHub, no source push/PR/merge/Actions.

## 23. #166 accepted and MIDI maintenance — 2026-10-14

User: «#166 принимаю». #166 closed completed; actual native parent#26 now3/11,
#164/#165/#166 accepted, other8 open; no acceptance of the entire Foundation.
Delivered0.3m/sourcec429d8d retained for acceptance; no new hardware measurements.
Requested next maintenance build0.3m upd1: coherent display grid on zoom out,
MIDI split runtime retention, Merge MIDI Clips (G), Duplicate (D).
[Contract and delivery](MIDI_EDITING_UPD1.md), [current context](PROJECT_CONTEXT.md).
No additional roadmap issue started; source/build/packages local, docs/issues GitHub.

## 24. #167/#157 explicit acceptance / Smart Snap — 2026-10-14

User: «#167 / #157: принимаю» and «Split, Merge G, Duplicate D — все работает».
Both issues closed completed; native parent#26 now5/11 (#164/#165/#166/#167/#157),
other6 and parent open. «Все вроде работает» general feedback, no blanket closure.
No new detailed hardware/recording/error/DPI measurements claimed. Next requested
maintenance0.3m upd2: gradual display subdivisions and zoom-dependent Smart Snap to
the visible Arrangement grid. [Contract](SMART_SNAP_UPD2.md), [delivery](PROJECT_CONTEXT.md).
No next roadmap issue started; source/build/packages local, docs/issues GitHub.
