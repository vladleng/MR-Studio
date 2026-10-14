# MR Cab 2.0 — Full Combo IR (Polytone / Fender / generic full-chain WAV)

**Дата решения:** 2026-10-14. **Статус:** реализовано локально в **0.4b Full Combo IR**, ожидает ручной приёмки #196. [Поставка/проверки/инструкция](MR_CAB_FULL_COMBO_196_DELIVERY.md).
**Issue:** [#196 — Full Combo IR](https://github.com/vladleng/MR-Studio/issues/196), логический parent [#182](https://github.com/vladleng/MR-Studio/issues/182) в [#111 Native Plugins](https://github.com/vladleng/MR-Studio/issues/111).
**Готовый baseline:** [#185 — Simple GUI, preset/state, smooth IR switching](https://github.com/vladleng/MR-Studio/issues/185), **accepted/closed 2026-10-14**, локальная версия 0.4a upd1; не открывать/переписывать.
**Параллельное развитие:** [#187 — generic WAV importer](https://github.com/vladleng/MR-Studio/issues/187) и [#188 — modular/QA](https://github.com/vladleng/MR-Studio/issues/188).
**Канон Cab:** [MR_CAB_COLLECTIONS.md](MR_CAB_COLLECTIONS.md); [MR_CAB_ROADMAP.md](MR_CAB_ROADMAP.md). **Рабочие правила:** [AGENTS.md](../AGENTS.md), [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).

## 1. Что нужно пользователю

Не «преамп + отдельный Celestion», а **записанный звук одного комбоусилителя** Polytone Mini-Brute или Fender Twin/Deluxe одним файлом:
- **Cabinet IR:** существующий режим — speaker/cab/mic IR, который можно ставить после отдельной amp-модели.
- **Full Combo IR:** WAV измеренного *полного пути* preamp/power amp/cab/speaker/mic (в той мере, в какой он был реально включён при измерении), применяемый **как один convolution processor без второго кабинета в MR Cab**. Использование одного Full Combo WAV полностью самодостаточно **только как линейная тембральная обработка**; не обещать полное поведение реального усилителя.

Имеющийся MR Cab с accepted Simple P3/#185 способен технически загружать WAV и применять один IR. **Новый объём работы — семантическая роль, адаптивная организация библиотеки, категории, сохранение метаданных и честный UX**, а не переписывание DSP.

### Физическое ограничение

Обычная линейная свёртка `y[n] = (h*x)[n]` воспроизводит один зафиксированный линейный частотно-фазовый отклик. Она **не воспроизводит** зависящий от сигнала перегруз, компрессию, transient behavior, работу ламп/транзисторов, tone-stack changes в реальном времени или спектральные изменения с изменением gain/уровня. Несколько IR с разными настройками — разные *статические снимки*, а не динамически управляемый preamp. Если производитель пишет «amp IR», нельзя автоматически считать, что capture включал весь кабинет либо preamp; источник этого утверждения — метаданные/документация/пользовательский mapping.

**NAM/Neural Combo** — отдельный processing backend в будущем, не тип файлов, который можно свёртывать. Не смешивать `.nam` с WAV, не заявлять поддержку NAM по наличию Full Combo в браузере.

## 2. Изменение модели коллекции и пресетов

### 2.1 Semantic role (ортогональна DSP)

Рекомендуемое поле `ir_role`:
- `cabinet` — измеренный кабинет/динамик/микрофон;
- `full_combo` — измеренный составной путь, отмеченный пользователем/доверенным профилем коллекции;
- `unknown` — ничего не предполагаем; пользователь может классифицировать позже.

Role *не* меняет физический алгоритм свёртки. Она определяет **расположение в библиотеке, labels, инструкции по подключению, предупреждения и пользовательский preset metadata**.

Возможные **опциональные** поля (не делать ни одно обязательным для arbitrary WAV): `vendor`, `amp_make`, `amp_model`, `combo_variant`, `capture_settings`, `speaker_info`, `mic`, `mic_position`, `cabinet_included` = `yes/no/unknown`, `room_included` = `yes/no/unknown`, `source_chain_notes`, `confidence/provenance`, `user_override`. `amp_make` / `amp_model` — **указатели на характер исходного измерения**, а не моделируемый динамический Amp DSP.

Presets/project store `ir_role`, validated identity of external WAV/variant, source selection, display labels/overrides, DSP parameters. Library relink/missing preserves role/notes; vendor-protected WAV **никогда не сериализовать внутрь preset/project без прав**. `unknown` нельзя молча повышать до `full_combo` только на основании имени файла (`Fender`, `Polytone`, `amp` и т.п.). Не переписывать existing Celestion IDs; migration to a new manifest/schema only if actually needed after local audit. Current #184 CabManifest v1 and #185 schema/presets must continue loading exactly as before.

### 2.2 Navigation UI

В существующем MR Cab добавить типы:
```text
MR Cab
  Library: [All] [Cabinet IR] [Full Combo IR] [Unclassified]
  Cabinet IR
    Celestion / G12M-50 Hempback / Open 1×12 / Mic / Mix / Room / 200–500ms
  Full Combo IR
    Polytone / Mini-Brute II / Warm Jazz / [source WAV]
    Fender / Twin Reverb / Clean / [source WAV]
  Unclassified
    Original filename / Type: Unknown → manual mapping
```

**Названия — примеры, не утверждение что у пользователя уже есть такие WAV или что эти presets существуют.** Можно отобразить внутренний общий `Collection → Device/Model → IR Variant` с `device_role=Cabinet|Combo`, не выдумывая второй catalog. В Full Combo UI не требовать mic or cab facet, если производитель их не сообщает. `Amp Gain`, `Bass/Mid/Treble` как реальные поворотные регуляторы **не добавлять**, если на IR они не влияют: можно показывать именованные статические варианты capture (Bright/Clean/Warm) при наличии.

Текущие #185 filter menus по пользовательскому решению **фильтруют WAV list, не auto-select и не автоматически переключают пресет**. Это сохраняется, пока пользователь отдельно не примет другой UX.

## 3. Signal flow / DSP

```text
[Гитара DI] → [MR Cab / Full Combo WAV @ 100% Wet] → [FX / выход]
                                         ↳ без второго IR кабинета
```

- Реальный WAV один; mono/stereo по фактическому заголовку, без «виртуальных» компонентных mic stems. Использовать существующий async loader/decoder, sample rate conversion (если нужен), convolution engine, clickless swap/Undo/preset/bypass, без отдельного графа.
- Если WAV уже содержит room и/или mic, ещё раз room **автоматически не подключать**. Если соседним **отдельным внешним plugin insert** является amp/cab, нельзя незапрошенно менять его bypass/chain/transport. Можно **предупредить** о потенциальной двойной обработке/некорректной цепи; выбор остаётся за пользователем.
- **Рекомендуемый** default Wet=100% для линейной Full Combo IR, но не перезаписывать текущие пользовательские настройки при смене режима и при Save/Open; при желании установить default только для *нового* экземпляра/пресета по согласию пользователя.
- Volume/timbre level matching проводить в A/B с учётом реальной измеренной amplitude. Не объявлять, что линейная свёртка корректно моделирует gain stage. Не применять скрытую автонормализацию/HP/LP, если пользователь не включил их.
- Rates/lengths определяются WAV, не привязывать Combo к Celestion 48k/200–500ms. Любые допустимые `44.1/48/88.2/96 kHz` (и иные поддержанные host/decoder rates) — реальные source variants; ресэмплинг/prepare в non-RT thread, не подменять длину/стерео/модель ради совпадения.
- NAM / AIDA-X full rig — потенциальный будущий `model_source=neural` с другим DSP и графовыми контрактами; НЕ добавлять в этот этап. Отдельная future issue, только после реального спроса и CPU/latency measurements.

## 4. Импорт и координация с #187

[#187](https://github.com/vladleng/MR-Studio/issues/187) остаётся общей системой импорта WAV от сторонних производителей. #196 добавляет **роль Full Combo**, пользовательский capture mapping и отображение Amp Model/Variation поверх той же системы. Код generic parser, fingerprint/IDs/relink/manual mapping **не дублировать**. Если #196 реализуется раньше P5, можно сделать минимальный `Import Selected WAV as Full Combo` на существующем загрузчике; затем переиспользовать #187 wizard. Не выдумывать полный Folder Parser специально для Polytone.

Первые источники для тестов: реальные **WAV** с измеренным Polytone/Fender, которые пользователь уже получил или разрешил скачать/подключить, либо synthetic fixtures. Наличие продукта Past to Future/другого производителя не доказывает структуру его каталога: установить её локально. Без user path / explicit permission real vendor tests = `NOT TESTED`; не паковать сторонние WAV в GitHub/CI/приложение.

## 5. Persistence, RT, UI safety

- Переиспользовать настоящую #185 систему `IRVariant`/external file identity/async prep, Versioned State, Undo/Redo, Save/Open, presets/Favorites и missing/relink. `ir_role` / source labels могут быть в library mapping prefs/manifest, но selected role must round-trip с проектом/preset и не потеряться после move/relink.
- Классификация не влияет на realtime kernel; не делать UI/locks/file I/O/new allocations/audio callback. Switching между Cabinet/Full Combo — тот же безопасный async IR swap и bounded crossfade.
- Immutable snapshot при list scan, generation token/cancellation/lifetime controls, controls reading from verified ready status; ошибки WAV/header/source path показывать явно.
- No unsafe inference, exact source identity for loaded WAV, no silent fallback to «похожий» IR другого комбо.
- Win DPI focus/keyboard/multi-instance, MR UI Kit optional future, не навязывать редизайн #185.

## 6. Acceptance

1. Испытать user-provided Polytone/Fender Full Combo WAV и Celestion Hempback Cabinet WAV, с ожидаемым **разным типом** библиотечного элемента, но **тем же Convolver**.
2. Переключаться Cabinet↔Full Combo; active source/Ready/missing correctly displays, no missing phantom variants, no double IR/room, no clicks/pop under supported scenario; successful A/B level matched; physical listening explicitly PASS/NOT TESTED.
3. Сохранение/открытие проекта и пресета сохраняет role, variant, external file, manual overrides и DSP values. Relink missing/changed, role unknown/edited/duplicate/Unicode user paths tested.
4. Старый принятый Celestion #185 работает без изменения звучания/цветов/пресетов. No regression MR Reverb/host/VST3/transport.
5. CPU, sample rate conversion/resampling and RT callback budget на тестовых files; не путать 200/500 ms IR с latency. GUI smoke 100/150/200 и Windows focus. Полный current CTest один раз на готовой stage candidate, targeted during iterations.
6. USER ACCEPTANCE отдельная, issue #185 остаётся closed. Другая подкатегория WAV не даёт оснований заявлять реальную эмуляцию усилителя.

**Delivery:** local code/configure/build/tests/packages; GitHub only docs/issues [skip ci], no Actions/source push/PR/merge. Обязательно проверять live local checkout до изменений, не трогать сторонние WAV/готовые сборки и не запускать разработку без отдельной команды пользователя.

## 7. Read-only audit: user Polytone collection, 2026-10-14

User-provided root: `D:/Samples/Past to Future/Polytone Mini Brute Jazz Guitar Amp`.
20 physical WAV: captures `Polytone 1` through `Polytone 5`, each with a separate
`B` variant, each supplied at 44,100 and 96,000 Hz. All RIFF PCM format 1,
24-bit, 2 channels. Actual durations 632.018–856.760 ms; sample counts
27,872–37,783 at 44.1 kHz and 60,674–82,249 at 96 kHz.
No 48 kHz files; use existing off-thread resampling for a 48 kHz project.
Do not trim to the Celestion 200/500 ms profiles.

Included supplier PDF describes IRs of the Polytone Mini Brute jazz guitar amp,
dark/bright settings and different gain stages, and a DI-to-IR demonstration.
This is evidence for an amp-capture collection, rather than filename-only inference.
It does not enumerate the captured preamp/power/speaker/mic/room chain or map
numbers 1–5 and suffix B to exact settings. Preserve literal variant names;
do not infer B=Bright, specific model revision, microphones, or gain values.
Suggested mapping for implementation: vendor Past to Future, amp_make Polytone,
amp_model Mini Brute, role full_combo with provenance supplier PDF + user intent;
unreported chain components/settings remain unknown.

All headers fit the current Cab scan guard (<=2 channels, <=192 kHz, <=1 second).
This is a header/code audit, not a successful application scan/load/listening test.
Current Celestion-only semantic parser leaves these filenames unclassified and
does not group the two sample rates as one capture. #196 still needs role metadata,
manual mapping, rate-pair grouping and Combo labels/persistence on the shared loader.
Paired durations differ by <0.01 ms; they fit the current same-length selector tolerance
once semantic mapping groups each capture. No new decoder/convolver is indicated
by this collection's headers.

Source files were read only; no vendor WAV copied, changed or published.
Implementation/build/CTest/physical listening not run; accepted #185 unchanged.
