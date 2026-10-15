# MR Cab 2.0 — Universal IR Collections: архитектура и продуктовый контракт

## Уточнение пользователя, 2026-10-14 — Legacy Cab compatibility

Пользователь: «Не нужна совместимость со старым Cab». Эта инструкция заменяет
прежние требования ниже о сохранении старого Cab ID/embedded state/presets,
миграции и legacy Cab regression/parity как обязательной приёмке MR Cab2.0.
Старый DSP можно переиспользовать после аудита; обязательного legacy adapter нет.
Общая совместимость других функций проекта и MR Reverb сохраняются; неподдерживаемый
старый Cab показывать явно, без подмены IR. #23/#128 не переоткрывать.
P1/#183 audit завершён, ждёт приёмки; #184 не начат. [Отчёт](MR_CAB_AUDIT_183.md).


**Статус:** design/backlog, реализация НЕ начата; 2026-10-14.
**Parent issue:** [#182](https://github.com/vladleng/MR-Studio/issues/182), направление [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111).
**План:** [MR_CAB_ROADMAP.md](MR_CAB_ROADMAP.md). **Codex handoff:** [MR_CAB_CODEX_HANDOFF.md](MR_CAB_CODEX_HANDOFF.md).
**Принятые основы:** инфраструктура/Legacy Cab IR — закрытая [#23](https://github.com/vladleng/MR-Studio/issues/23), [CAB_IR.md](CAB_IR.md); MR Reverb — закрытая [#128](https://github.com/vladleng/MR-Studio/issues/128), [MR_REVERB.md](MR_REVERB.md), [REVERB_LIBRARY_IMPLEMENTATION.md](REVERB_LIBRARY_IMPLEMENTATION.md), [REVERB_EDITOR_IMPLEMENTATION.md](REVERB_EDITOR_IMPLEMENTATION.md).

## 1. Цель и первый реальный workflow

Создать один расширяемый нативный **MR Cab** для загрузки и быстрого переключения **реальных WAV кабинетов** из комплекта производителя. Важно: library import не делает новые IR, он классифицирует внешние WAV и сохраняет метаданные. **Не разрабатывать отдельные жестко привязанные «Celestion engine» и «TONEX engine».**

**Первый acceptance набор:** локально уже скачанная пользователем коллекция Celestion (ориентир из скриншота: Vox V412 BN 412 C, папки Shure SM57, Royer R-121, Sennheiser MD421, Neumann TLM 107 Room, готовые WAV варианты 121+57, 421+57, 421+121, All, Hi-Gn/Lo-Gn и All+Room Mono/Stereo). На скриншоте показан только верхний уровень; **подпапки, точные filenames, количества, sample rates и license не проверены**. Codex сначала должен просканировать фактическую папку на локальном ПК после указания пользователя/обнаружения доступного пути; без переноса содержимого в GitHub/CI.

Пользовательский путь: открыть MR Cab → выбрать корень коллекций → импортировать папку комплекта → подтверждение обнаруженных кабинетов/вариантов → выбрать **Cabinet → Mic/Mix → Voicing → Room → WAV variant** в компактном редакторе → сохранить свой preset → сохранить проект и открыть вновь. Режим Advanced позже предложит самостоятельное смешивание **компонентных** IR, но Simple по умолчанию воспроизводит один выбранный заводской WAV.

После Celestion: WAV-наборы TONEX и любые разумно организованные third-party/user collections. Под «TONEX совместимостью» здесь понимаются **только стандартные WAV IR, которые пользователь законно получил/экспортировал и может использовать**. Proprietary Tone Models (.txm), presets (.txp), DSP декодирование закрытых контейнеров и neural captures в scope НЕ входят.

## 2. Основные сущности и разница с пресетами

- **IRFile:** физический WAV, внешний абсолютный путь только локально, relative-path относительно library root, длительность/каналы/sample rate/битность, точная проверенная file identity (размер+mtime для scan; более сильный digest в worker при применении), compatibility status. Не дублировать аудиоданные в global catalog.
- **Collection:** логический набор из указанной папки, с origin type (Celestion profile, generic, TONEX-WAV profile), manifest schema version, user-overrides и идентификаторами. Это **не архив с авторскими WAV**.
- **Cabinet:** идентифицируемый кабинет/модель внутри коллекции, label (не выдавать автоматически юридически значимое название производителя), группировка вариантов.
- **IRVariant:** ссылка ровно на один WAV или корректно подтверждённую stereo-pair/quad mapping; фасеты: `mic`, `mic_position`, `mix`, `voicing`, `room`, `rate`, `length`, `channel_mode`, plus free-form tags. Fасеты optional: generic package не обязан иметь Hi/Lo или Room. `mix_type`: **premixed** vs **component**; `room_included` — boolean/unknown (не вычислять по аудио, только явная метка или подтверждённое распознавание); `confidence/source`: filename / folder / manual.
- **CabPreset:** авторская настройка MR Cab — reference к Collection/Cabinet/IRVariant + output/mix, EQ low/high cut, polarity и, в Advanced, явный список компонентов, уровни, delay/polarity, optional routing. **Preset — не копия заводского WAV**. Пресет должен иметь версию и миграцию; external dependency validity показывать.
- **Factory preset:** только собственный/CC0/лицензированный IR или metadata-only template, требующий установленной пользовательской IRCollection; отсутствие нужного IR не должно маскироваться под готовый звук. Не помещать коммерческие Celestion/TONEX WAV в исходники, тестовые fixtures, бинарник или дистрибутив.
- **Patch Slots:** будущие Pre-FX / Amp / Cab / Post-FX слоты и метаданные возможностей; текущий продукт — **Cab**. Новые Amp/Neural/Pedal DSP — отдельные будущие issues.

Разделение метаданных:
1. App-global каталог/корни/ручные соответствия — user preferences и versioned manifest (может переиндексироваться).
2. Project/preset — стабильная identity выбранного варианта и настройки конкретной инстанции; при отсутствии библиотеки не удалять ссылку.
3. Render/cache — подготовленный runtime IR kernel; не source of truth и может быть пересоздан.

**Сохранить обратную совместимость** прежнего Cab IR: его embedded kernel/state и ограничения не переписывать под внешние коллекции. На первом этапе новая функция может быть отдельным native Cab v2 insert с явным совместимым adapter/upgrade путем; реальную стратегию выбирать после аудита локального project serialization. Нельзя молча заменить legacy node/ID.

## 3. Импорт коллекций

### 3.1 Pipeline

```text
User folder(s) → Async headers-only scanner (with cancellation/generation)
               → Proposed cabinet/grouping metadata
               → Provider profile / generic tokenization
               → Ambiguities & manual mapping preview
               → Versioned collection manifest (metadata + path references)
               → Browser snapshot → Selected variant → Prepared IR loader → SHARED audio graph
```

Использовать готовые ReverbLibrary/Loader и native editor UI где действительно применимо: `ReverbLibrary` реализует folder scan, immutable snapshot, bounded workers, root-rel identity и missing/relink, `ReverbLoader` — worker prepare + atomic publication, stale generation guard/80 ms crossfade. Проверить фактический **локальный** код — опубликованный исходник может отставать; общую утилиту извлекать аккуратно, не менять согласованное поведение MR Reverb. Коллекционный индекс должен быть отдельным metadata layer поверх shared WAV services. Не плодить второй CPU-heavy convolution decoder, но сохранить разную аудио-семантику Reverb/Cab.

### 3.2 Celestion recognition profile v1

Нормализовать имя/регистр Unicode, расширение, разделители пробел/дефис/underscore, `Hi-Gn`/`Lo-Gn`, `121+57`, `421+57`, `421+121`, `All`, `Room Mono`/`Room Stereo` (все — **примеры**). Папки микрофонов могут содержать дополнительные позиции/длительности/частоты, определять только по реальным данным. Пары `121+57` и `421+121` — готовые mix WAV, **не** команда загрузить два WAV. Тег `Hi-Gn`/ `Lo-Gn` — **voicing производителя**, не усиление gain в DSP; не корректировать уровень автоматически.

Для готового `All+Room` ставить `room_included=true` и не применять вторую Room-свёртку. Для отдельных `Neumann ... Room` и микрофонных IR определять семантику по расположению и при необходимости просить ручное подтверждение. В случае неуверенного парсинга показать raw filename + `Unclassified`, разрешить `Edit mapping`; никогда не выдавать выдуманный WAV за существующий.

**Наличие комбинации** = реально найденный совместимый вариант с подтверждённой metadata, а не декартово произведение facet-значений. Кнопки/селекторы показывают только реально доступные значения или явно disabled комбинации. Если выбор одного facet лишает доступности текущий вариант, интерфейс предлагает ближайший **явный** доступный вариант (с preview), без тихой автоподмены звучания. Никаких автоматических смешиваний для заполнения пустот.

Одинаковые смысловые IR с частотами 44.1/48/96k и разной длиной — отдельные physical assets с одной семантикой; выбор файла по целевой sample rate/настройкам качества детерминирован и прозрачен, не смешивать rates как независимые кабинеты. Строго проверять фактическую длину/supported formats и ресэмплинг; 1s legacy Cab limit не менять без явной задачи/тестов.

### 3.3 TONEX WAV / generic profiles — после Celestion acceptance

Перенести общую модель без обязательных vendor fields. Если у TONEX WAV имена/структура не дают однозначно определить кабинет/микрофон/позицию, показывать importer wizard с ручным mapping, возможностью сохранить profile/template локально. Не предполагать единой структуры всех наборов TONEX. Запрещён implicit импорт любых proprietary TONEX models; .txm/.txp → explicit unsupported.

## 3.4 Утверждённая политика частот и длительности IR (2026-10-14)

**Это согласованный baseline для MR Cab 2.0 и первой пользовательской библиотеки Celestion**, а не утверждение о том, что уже созданный конвертер или автоматический selector работает.

| Вариант на диске | Роль | Политика |
|---|---|---|
| **48.0 kHz / 200 ms** | **Default Cab**: close-mic, обычные заводские mic/mix IR, Studio Mix/Record и потенциально Live | Основная рабочая коллекция Celestion |
| **48.0 kHz / 500 ms** | **Room / room-included готовые WAV**, хвосты с полезными поздними отражениями; A/B тесты с тем же вариантом 200 ms | Оставить в рабочей коллекции, прежде всего для Room; не считать HQ автоматически |
| 44.1 kHz / 200 ms и 500 ms | Проекты 44.1 kHz, если хочется применять точно совпадающий по частоте файл; 500 ms при необходимости Room | Опционально, иначе ресэмплинг с 48 kHz |
| 96.0 kHz / 200 ms и 500 ms | Проекты 96 kHz и quality/CPU comparison; 500 ms — Room/исследование | Не требуется для первой рабочей коллекции, сохранять в исходном архиве |
| 88.2 kHz / 200 ms и 500 ms | Проекты 88.2 kHz и экспериментальный A/B | Дополнительный формат, в MVP хранить в архиве, не обязательный к установке |

### Правила выбора и хранения

1. **Рабочая копия Celestion:** достаточно основной ветки `48.0 kHz/200 ms` для обычных кабинетов плюс `48.0 kHz/500 ms` для Room и небольшого набора matched A/B. **Полный оригинальный комплект сохранить отдельно без удаления/перекодирования** до проверки работы importer и прослушивания. Если пользователь оставляет обе длительности для одних и тех же микрофонов, не индексировать их как разные модели кабинетов.
2. **Подбор при воспроизведении:** обработка всегда на sample rate проекта/движка. Если подходящий `IRVariant` (тот же Cabinet/Mic/Mix/Voicing/Room/Length) содержит файл с *точно совпадающим* rate, предпочесть его. Если нет — асинхронно подготовить ресэмплированный IR из **того же смыслового варианта**, явно отражать source file rate и project rate. Нельзя выбирать другой микрофон/кабинет/room/длительность, лишь чтобы совпала частота. Rate conversion не делать в audio callback.
3. **Длительность — осознанный вариант, не качество:** простой Cab по умолчанию выбирает **200 ms**; если выбран `Room` / `room_included`, **предпочитать 500 ms, только когда такой реальный файл есть**, с сохранением пользовательского override. Другую длительность нельзя подставлять молча после reopen, чтобы не изменить звучание пресета; если exact choice отсутствует — показать fallback/предупреждение или потребовать подтверждения. `500 ms` не «всегда лучше», `96 kHz` не «обязательное HQ».
4. **Room:** готовый `All+Room` — **один** WAV без повторного Room. 500 ms полезно только если фактический файл содержит поздние отражения. Название `Room` не доказывает, что хвост длиннее 200 ms; опционально сравнить tail-energy/прослушивание на реальном наборе.
5. **DSP и производительность:** при 48 kHz 200 ms ≈ **9 600 сэмплов/канал**, 500 ms ≈ **24 000** (в **2,5 раза** больше samples); при 96 kHz соответственно 19 200/48 000. Это не значит 2,5-кратную CPU нагрузку: FFT/partitioned convolution/число mic IR/стереоканалов важны. Подготовка/память обычно растут с объёмом IR, реальный CPU/peak измерять на 1/2/4/8 экземплярах/микрофонах и буферах, не обещать производительность без тестов.
6. **Latency:** 200/500 ms — **длительность импульсного отклика**, не обязательная сквозная задержка. Для Cab low-latency processing цель — не добавлять задержку, пропорциональную длине IR; фактическую алгоритмическую latency/PDC подтвердить на выбранном движке/режимах. Нельзя объявлять постоянную нулевую задержку без теста нового pipeline.
7. **Manifest:** сохранять `original_rate_hz`, `source_duration_samples`/`duration_ms`, `chosen_length_profile`, `source_identity`, `prepared_project_rate_hz` (runtime metadata) и точный выбранный variant. Preserve `200/500 ms` как альтернативы одного артикула, не как независимые коллекции, при одинаковых остальных фасетах.
8. **Приёмка:** на конкретных WAV Celestion измерить сравнимые варианты 48k/200 и 48k/500, room/close mic и exact-rate/resample; тестировать clickless IR swap, resampling correctness, SaveOpen, fallback, CPU/RAM/memory/headroom и RT callback; оригиналы не публиковать. Только после базового Celestion gate переносить эти правила на TONEX WAV/generic.

## 3.5 Full Combo IR — решение 2026-10-14

**Additive feature #196:** [Full Combo IR specification](MR_CAB_FULL_COMBO_IR.md), логический parent [#182](https://github.com/vladleng/MR-Studio/issues/182). Текущий MR Cab P3 [#185](https://github.com/vladleng/MR-Studio/issues/185) **принят/закрыт**, менять его существующий конвольвер, Celestion presets/UX и исходные IR не нужно.

В рамках **того же** MR Cab разрешить два разных библиотечных типа WAV: `Cabinet IR` (реальный speaker/cab/mic) и `Full Combo IR` (линейная запись полной цепочки Polytone/Fender и др., **если source цепочка подтверждена**), плюс безопасный `unknown`. Один Full Combo WAV → один existing convolver, без второго Cab IR/Room. Внешние VST3/host inserts автоматически не обходить. Навигация может иметь фильтр All/Cabinet/Full Combo/Unknown и metadata `amp_model`, `capture_settings`, `cabinet_included`, `mic`, `provenance`; unknown нельзя объявлять combo по одному имени.

WAV IR **не** является реальной нелинейной моделью преампа/комбо; NAM/Neural Combo (отдельная технология/движок) **не входит** в scope #196. Размер/частота generic Combo WAV не ограничены Celestion default `48 kHz / 200/500 ms`: выбирать реальные physical source rate/duration, prepared off-RT. Persist `ir_role` и manual overrides, не ломать Celestion #185 SaveOpen/Undo. Импорт generic WAV координировать с #187, не писать новый второй parser. Реализация только после отдельного запроса пользователя.

## 3.6 MR Rig product layer (2026-10-15)

[#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197) будет **продуктовым UI-слоем и новым RigPreset** поверх текущего универсального MR Cab IR backend, а не новым дублирующим конвольвером. Сохранить существующую #185 принятую функциональность и отдельный #196 Full Combo WAV (локально ready, не accepted), #187 generic WAV, #186 Advanced mic. **#182 остаётся Cab/IR технологией**; MR Rig не превращает кабинетный Celestion в модель Fender/Polytone amp.

В Rig: curated `IR Models` (кабинеты/линейные full combo, честно размеченные), future `NAM Models` с самостоятельным нейросетевым runtime, `My Rigs` whole-chain presets; фото и категории — метаданные, Import Own IR вторично. UI PreFX/Core/PostFX, реальные педали отдельно. Роли должны быть явными: cabinet-only vs full-combo WAV и full-rig vs amp-only NAM. Ограничение WAV linear response остаётся. Не добавлять второй Cab/Room к Full Combo или NAM Full Rig; не встраивать чужие WAV/фото в пакеты. Все детали [MR_RIG_ARCHITECTURE.md](MR_RIG_ARCHITECTURE.md), [MR_RIG_LIBRARY_PRESETS.md](MR_RIG_LIBRARY_PRESETS.md), [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md). **P0 #198** решит migration/plugin identity после аудита local code; эти docs не меняют project schema или DSP.

## 4. GUI и поведение

**Основной стиль:** MR Reverb/JUCE Theme, graphite flat/compact, reusable native editor shell и компоненты `ReverbBrowser`; минимум декоративных рамок. Не выдумывать второй дизайн-язык.

**Simple (MVP):** library tree/search → Cabinet selector → Mic/Mix → optional Voicing → optional Room → active WAV identity/status → Mix, HP/LP, Gain, Polarity, Bypass, Favorites/Save Preset. У варианта может не быть Room/Voicing: соответствующие controls скрыты, а не заполняются искусственными values. Простой режим всегда означает выбор **одного** готового WAV (или достоверной stereo/pair mapping). Изменение facet и кнопка переключения не провоцируют пользовательские файловые диалоги.

**Advanced (после Simple):** независимые mic channels при наличии **компонентных** IR: gain, delay/time alignment, polarity, pan если supported channel layout; selectable Room IR only if it actually exists and is semantically **separate**. При смешивании синхронных LTI каналов разрешён precompute weighted composite FIR вне RT; компенсация времени, фазы, gain обязаны быть измерены/видимы. Premixed `All`/ `All+Room` не раскрывать как выдуманные mic stems. Если принцип математики/маршрутизации не позволяет простую свёртку — использовать честный multi-convolver с бюджетом или пометить unsupported. Избегать double Room и комбинирования разных sample rate без подтверждённой стратегии.

**Transition UX:** `Loading` не выключает текущий IR; активация только полного проверенного prepared kernel; clickless/crossfade с фиксированными заранее подготовленными буферами. Invalid WAV/нехватка памяти/отмена — сохранять старый звук, выбранный идентификатор и объяснять ошибку. `Missing` на reopening — визуально различать want-to-relink identity и active/fallback audio. Не подбирать по одному названию без точной проверки identity.

**Preset UX:** разделять factory (owned/bundled or templates), user presets и сохранённые варианты из импортированной коллекции. Пресет — не IR collection. Favoriting metadata не меняет audio project. Simple/Advanced переключение не переписывает аудио и не уничтожает сохранённые настройки.

## 5. DSP и realtime границы

```text
Input → [future Pre FX] → [future Amp/Preamp] → MR Cab DSP → [future Post FX] → SHARED Graph
                                            ↑
                           Prepared IR / optional composite FIR
                                            ↑
                      off-thread decoder + resampler + collection loader
```

- **Не создавать отдельный звуковой движок** для Live; Stream/Studio используют SHARED. Для Studio Mix более высокий буфер приемлем; Live/record low-latency пригодность по измерениям, а не навязанное требование всем функциям.
- Контроль DSP: native insert node, single audio owner, no locks/alloc/fs/UI/network/kernel destruction in callback, safe graph/loader lifetime, no double signal/dry+processed duplication, корректный PDC/loop/seek/reset/transport; raw recording unaffected.
- Возможность многоканальных IR только при явной route mapping (моно, stereo diagonal, true stereo with LL/LR/RL/RR proof), не выдавать L/R split за true stereo и не менять track channels без согласования.
- Реиспользование DSP Cab IR vs MR Reverb решить после локального профилирования. Reverb поддерживает длинные IR, Cab short IR; общий decoder/cache не означает одинаковые настройки, CPU, лимиты или window sizes.
- Тестировать моно/стерео, actual sample rate/length, разных buffer sizes, invalid/corrupt/silent, off-thread cancellation during playback, repeated fast selection, multi instance, memory bound and any required resource caps.
- Bypass/compare должен честно показывать разницу и не выдавать при выборе другого WAV усиление через обходящую цепочку; измерять matching levels при A/B.

## 6. Persistence, history, safety and licensing

- Сохранять стабильный collection/cabinet/variant ID + file identity/hash/path reference + controls, overrides и versioned state; правильно работать с длинными/кириллическими Windows paths и перемещёнными коллекциями.
- Atomic Apply / Undo/Redo, Save/Close/Open; при missing collection/preset не терять metadata/состояние/legacy old state; migration version только после локального аудита. Оригинальный WAV не менять.
- App-wide external catalog/preferences и project-bound selection state — **разные владельцы**. При переиндексации защищать old project reference от случайного переназначения на одноимённый файл.
- Права: внешние Celestion и TONEX WAV остаются у пользователя; не публиковать и не копировать в samples/tests/docs attachments/issue logs. При отсутствии пользовательского доступа используем **синтетические/собственные WAV** в автоматических тестах, отдельный opt-in real Celestion smoke на ПК пользователя.
- Desktop logs, indices, manifests и пресеты не должны тайно содержать raw PCM или пересылать его сети. Любое будущее отправление ИИ — только по отдельному согласию.
- Только локальный код, билд, тесты, релизы; GitHub — документация/issues [skip ci]. Пересоздавать закрытые #23/#128 не нужно.

## 7. Acceptance — этапы и итоговая проверка

**Celestion:**
1. Inventory выбранной пользователем коллекции: реальные подсчёты + список найденных Mic/Mix/Voicing/Room и неподдерживаемых файлов (не заявлять неподтверждённое).
2. Выбрать 121+57, 421+57, 421+121, All, Hi/Lo и Room **если файлы реально присутствуют**; услышать нужный WAV, никакого ненамеренного двойного Room/суммы нескольких IR.
3. Copy/Move коллекции, missing/relink, sample-rate selection, multiple preset variants; сохранение/reopen и Undo/Redo без незаметной подмены.
4. Compare reference single-WAV convolution at matched gains; measured RT callback max, RAM/CPU, clickless rapid switching.

**Другие WAV:** импорт TONEX WAV/generic после Celestion accepted; ручная карта неоднозначных имен; не зависит от Celestion tokens. Unsupported Tone Models честно отклоняются.

**Regression:** MR Reverb editor/browser/DSP, Legacy Cab IR embedded state and projects, VST3, common SHARED, raw recording, ASIO safety, JUCE DPI, missing plugins, project schema versions.

## 8. Дальнейшая эволюция

Будущие модули MR Guitar Rig: Gate/Compressor/Overdrive (Pre FX), classic Amp/Preamp/Power Amp, optional open NAM runtime, Post EQ/Delay/MR Reverb. Сейчас только **предусмотреть API/slots/capability contracts** и визуальное место; audio DSP этих будущих компонентов — новые отдельные issues после согласования. Не смешивать разработку Cab Collections с Neural Amp Modeler, TONEX Tone Models и ARA2.

**Приоритет пользователя:** довести Celestion на локально скачанных WAV до приемлемого UX/звучания, затем на других коллекциях расширять parser/manifest/UI. Реализация P1 начинается только по отдельному запросу и останавливается после каждого принятого slice.
