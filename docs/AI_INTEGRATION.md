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
