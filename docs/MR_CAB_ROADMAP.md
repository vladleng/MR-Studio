# MR Cab 2.0 — roadmap, приёмка и взаимосвязи

## Уточнение пользователя, 2026-10-14 — Legacy Cab compatibility

Пользователь: «Не нужна совместимость со старым Cab». Эта инструкция заменяет
прежние требования ниже о сохранении старого Cab ID/embedded state/presets,
миграции и legacy Cab regression/parity как обязательной приёмке MR Cab2.0.
Старый DSP можно переиспользовать после аудита; обязательного legacy adapter нет.
Общая совместимость других функций проекта и MR Reverb сохраняются; неподдерживаемый
старый Cab показывать явно, без подмены IR. #23/#128 не переоткрывать.
P1/#183, P2/#184 и Simple/#185 приняты/закрыты; текущий новый scope — #196 Full Combo. [Отчёт](MR_CAB_AUDIT_183.md).


**Status:** #183/#184/#185 приняты и закрыты; #196 реализована локально в0.4b и ожидает ручной приёмки, 2026-10-14. #186/#187/#188 не начаты. [Поставка #196](MR_CAB_FULL_COMBO_196_DELIVERY.md). **Parent:** [#182](https://github.com/vladleng/MR-Studio/issues/182) → category [#111](https://github.com/vladleng/MR-Studio/issues/111).
**Главное ТЗ:** [MR_CAB_COLLECTIONS.md](MR_CAB_COLLECTIONS.md), [передача рабочему Codex](MR_CAB_CODEX_HANDOFF.md).
**Важно:** `#183–#188` ниже созданы как самостоятельные GitHub issues и пока отслеживаются **Markdown-чеклистом**. Только родительская связь, проверенная через GitHub API/UI, даёт настоящий sub-issue progress; ссылки и чекбоксы не создают native hierarchy.

## Цель первого пользовательского milestone: Celestion

На реально скачанной пользователем библиотеке Celestion (Vox V412 BN 412 C как первый пример) — выбирать один правильный заводской WAV, иметь автоматическую группу кабинет/микрофон/готовый микс/voicing/room/rate, адаптивные переключатели, внешние references, preset/SaveOpen/Undo. Дальнейшие микрофонные advanced операции только после подтверждения существующих отдельных IR; не заменять дорогой vendor cabinet pack слепой суммой импульсов.

| Очерёдность | Issue | Объём | Gate |
|---|---|---|---|
| 1 | [#183 — Аудит и реальный Celestion inventory](https://github.com/vladleng/MR-Studio/issues/183) | Реальный локальный каталог, реиспользование DSP/GUI/Reverb | инвентаризация и матрица, стратегия совместимости |
| 2 | [#184 — Коллекции и Celestion parser](https://github.com/vladleng/MR-Studio/issues/184) | manifest/IDs/facets/importer/profiles | deterministic scan, no fake variants, preview mapping |
| 3 | [#185 — Simple UI, пресеты и плавное переключение](https://github.com/vladleng/MR-Studio/issues/185) | удобный MR Reverb style на одном WAV | ручной Celestion E2E+Save/Undo |
| 4 | [#186 — Advanced multimic/room](https://github.com/vladleng/MR-Studio/issues/186) | component IR mix/phase/delay/room | reference DSP и user listening, ни одного double Room |
| 5 | [#187 — TONEX WAV и generic importer](https://github.com/vladleng/MR-Studio/issues/187) | другие WAV naming patterns + mapping UI | два разных пакета, no .txm/.txp |
| 6 | [#188 — Modular Rig contracts + Final QA](https://github.com/vladleng/MR-Studio/issues/188) | future slots, regression, measurement | final user acceptance/release gates |

Порядок **P1 → P2 → P3** без автоматического перехода. P4 проверяется на фактическом наличии **компонентных** микрофонных IR. P5 запускается **после приёмки Celestion milestone**, может идти без Advanced режима P4 при согласованной отдельной приёмке. P6 — финальная интеграция/регрессия и модель будущих расширений без нового amp DSP.

## Частоты и хвосты — стандарт MR Cab (решение 2026-10-14)

**Source of truth:** [MR_CAB_COLLECTIONS.md, §3.4](MR_CAB_COLLECTIONS.md#34-утверждённая-политика-частот-и-длительности-ir-2026-10-14).

- **48 kHz / 200 ms:** рабочий default для обычного гитарного Cab, close mic, готовых миксов, Studio/Record/предварительно Live.
- **48 kHz / 500 ms:** Room и room-included импульсы при существовании такого WAV и полезного хвоста, плюс matched A/B тесты. Не обязательное «HQ» для всех кабинетов.
- **44.1 kHz:** optional source для проектов 44.1 kHz (200/500 ms по той же функции), иначе offline ресэмплинг 48k.
- **96 kHz:** optional source для 96k проектов и quality/CPU benchmark; **88.2 kHz:** optional для 88.2k проектов. Для первого Celestion MVP не требуются.
- Проект определяет runtime sample rate: exact-rate variant при наличии, иначе worker resample **того же** IR; никогда не менять cabinet/mic/room/length ради rate. 200/500 — часть явно выбранного варианта/пресета.
- Хранить основной 48/200 + 48/500 для Room и сравнительных тестов, **полный оригинал Celestion архивировать отдельно, не удалять**; импорт WAV по metadata, без vendor media в GitHub. Длина IR **не равна latency**; фактические CPU/RAM/PDC/RT измеряются.
- **P1 #183** — инвентаризация комбинаций и хвостов, **P2 #184** — descriptor/selector exact rate and length, **P3 #185** — defaults/preset/UX, **P4 #186** — same-rate component multi-mic handling, **P5 #187** — перенос правил на generic WAV.

## MR Cab: Full Combo IR / #196 (дополнительное направление)

- **[#196 — Full Combo IR Polytone/Fender](https://github.com/vladleng/MR-Studio/issues/196)** — новое аддитивное расширение поверх **принятого и закрытого #185**, не часть уже проведённой приёмки.
- В одном MR Cab: Cabinet (Celestion) / Full Combo (WAV полного измеренного тракта) / Unknown. Одна WAV свёртка без второго cabin/room. Сохранять classification, file identity, preset/source metadata/Undo/SaveOpen, сортировка/фильтр и честное описание линейных ограничений.
- Использовать [existing generic importer #187](https://github.com/vladleng/MR-Studio/issues/187) вместо дубля; не блокировать начатые P4/P5 при отсутствии у пользователя платных Combo WAV. Для реальных испытаний пользователь должен предоставить file path, иначе synthetic fixture/NOT TESTED. Согласовывать #196 с #188 QA, но не вплетать NAM как готовую функцию.
- **[Полное техническое задание](MR_CAB_FULL_COMBO_IR.md)**. Статус: реализовано локально0.4b (source2fbe90e), full149/149 PASS; ожидает ручной приёмки. [Поставка/инструкция](MR_CAB_FULL_COMBO_196_DELIVERY.md). GitHub-native sub-issue links требуют отдельной привязки.

## Related product direction MR Rig / #197 (2026-10-15)

[#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197) — новый native guitar rig продукт, использующий этот MR Cab и Full Combo #196 как WAV IR backend. Эти два направления **не объединять в один giant issue**: #182 — Cab/manifest/convolution/Advanced/import, #197 — model cards/cover/library/RigPreset/Pre+Post FX/NAM. Полные спецификации [MR Rig architecture](MR_RIG_ARCHITECTURE.md), [GUI](MR_RIG_GUI.md), [library](MR_RIG_LIBRARY_PRESETS.md), [DSP/Pedals](MR_RIG_AUDIO_PEDALS.md), [roadmap](MR_RIG_ROADMAP.md). Issue #185 accepted closed, #196 implementation 0.4b upd2 awaiting user approval; не откатывать/переписывать. #198 audit determines migration path before code. New Rig never pretends static IR is non-linear amplifier and never forces second cab for full combo. No source changes from this note.

## Definition of Done общего продукта

- [ ] Точный и безопасный импорт первого **пользовательского Celestion** без копирования WAV в GitHub/дистрибутив.
- [ ] Обычный готовый mic-mix — **один** WAV; Room без повторного наложения; Hi/Lo только vendor facet, не самовольное изменение Gain.
- [ ] Simple выбирает существующие комбинации, без несуществующих кнопок и молчаливой подмены; внешний WAV распознаётся/сменяется быстро и без щелчков.
- [ ] Advanced не открывает фиктивные stems, контролирует фазу/задержку/room и сохраняет полный preset.
- [ ] TONEX **WAV** и generic profile, ручная mapping при неопределённости; .txm/.txp explicit unsupported.
- [ ] Global collection vs project CabPreset state, stable identity, folder move/missing/relink, old project compatibility and Undo/Redo.
- [ ] Legacy Cab IR #23 и MR Reverb #128 не регрессируют; shared DSP на едином графе, no callback I/O/locks/allocations.
- [ ] CPU/RAM/latency/ASIO и listening matrix, реальные измерения отдельно от unit tests; права на контент соблюдены.
- [ ] Пользователь принял соответствующие slices; issue #182 закрывается лишь после final acceptance.

## Вне scope

Новый нейросетевой Amp, NAM/AIDA-X inference, фирменный TONEX Tone Model, Amp/Preamp DSP, педали и производство IR/captures. Модульные слоты — future design, не работающие пустые процессоры. Любая будущая реализация — отдельная issue в #111.

## Source boundaries и workflow

- [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md), [PROCESSING_RELIABILITY.md](PROCESSING_RELIABILITY.md), [ENGINE_PERFORMANCE_PLAN.md](ENGINE_PERFORMANCE_PLAN.md).
- [CAB_IR.md](CAB_IR.md), [DSP_MODELING.md](DSP_MODELING.md), [MR_REVERB.md](MR_REVERB.md), [REVERB_LIBRARY_IMPLEMENTATION.md](REVERB_LIBRARY_IMPLEMENTATION.md), [REVERB_EDITOR_IMPLEMENTATION.md](REVERB_EDITOR_IMPLEMENTATION.md), [NATIVE_INSERTS.md](NATIVE_INSERTS.md), [PERSISTENCE.md](PERSISTENCE.md).
- В GitHub только docs/issues commits `[skip ci]`; source/build/tests/packages локальные; без Actions и PR/merge/source push без просьбы.
- **Планирование не является командой Codex начинать P1**. После начала P1 Codex прекращает работу в конце выбранной задачи и ждёт принятия пользователем.
