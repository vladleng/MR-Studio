# Codex Handoff — Harma Waves Native / начало работы в MR Studio
> 2026-10-10. **План подготовлен, код не переносился, сборки не выполнялись.** Главная задача [#115](https://github.com/vladleng/MR-Studio/issues/115). Первое локальное задание [#116](https://github.com/vladleng/MR-Studio/issues/116). Перенос VST3 host layer не является первым действием.

> **Важное уточнение 2026-10-10 — миграция актуальной версии:** source Smart-Voicing GitHub main документирует stable 0.5, но это **не предел переноса**. В задаче [#116](https://github.com/vladleng/MR-Studio/issues/116) Codex обязан проверить все **локальные** ветки/checkpoints и конкретно наличие **Harma Waves 0.5g**. Если local 0.5g существует и проверена, переносить именно её полностью, а 0.5 использовать для регрессий. Если local 0.5g не найдена, не подменять её молча stable 0.5, а зафиксировать blocker и фактические версии. Не путать с 0.5g из отдельного проекта Smart-Improviser.

## Задача нового рабочего чата

Основная разработка Harma Waves (бывший Smart Voicing) теперь в **локальном MR Studio**. Использовать **последнюю проверенную локальную версию Smart-Voicing/Harma Waves (0.5g, если доступна)** как источник миграции, а stable 0.5 GitHub — как baseline regression. **Не переписывать с нуля** Core/voicing. Сделать сначала безопасный inventory + перенос portable Core и parity tests. После отдельной приёмки перейти к native context/MIDI и UI, затем Voice Leading/Color-Rich. Не приступать к остальным задачам просто потому, что они указаны в roadmap.

## Прежде чем что-либо менять

1. Прочесть [MR Studio AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md), [ARCHITECTURE](ARCHITECTURE.md), [MRS Stage 5 #25](https://github.com/vladleng/MR-Studio/issues/25), [Harma Native Architecture](HARMA_WAVES_NATIVE.md), [Harma roadmap](HARMA_WAVES_ROADMAP.md), issue #116.
2. Найти текущие **локальные** checkout MRS и Smart-Voicing. Документация MRS указывает MRS `C:/Users/Vladislav/Documents/GitHub/MR-Studio`, но путь актуален только если реально существует. Источник Smart-Voicing найти, не придумывать путь. Для обоих: `git status --short`, branch, HEAD, recent commits, untracked work. Не сбрасывать, не checkout main поверх текущего MRS, не делать автоматический pull/merge.
3. Прочесть source [Smart-Voicing START-HERE](https://github.com/vladleng/Smart-Voicing/blob/main/docs/START-HERE.md), [CONCEPT](https://github.com/vladleng/Smart-Voicing/blob/main/docs/CONCEPT.md), [GUARDRAILS](https://github.com/vladleng/Smart-Voicing/blob/main/docs/MUSICAL-ENGINE-GUARDRAILS.md), [Stage5 acceptance](https://github.com/vladleng/Smart-Voicing/blob/main/docs/STAGE5-CLEAN-ACCEPTANCE.md), [TEST-0.5](https://github.com/vladleng/Smart-Voicing/blob/main/docs/TEST-0.5.md) и actual code/CMake/tests. Не копировать исторические VST3-only instructions в MRS project rules.
4. Сверить MRS MIDI runtime (tracks/clips, VST3 hosting, MIDI capture, transport/PPQ, project schema) **по локальному коду**, а не считать pending Chord Track #25 готовым. Зафиксировать risk flags: gaps/current next, tempo/meter edits, sample boundaries, note ownership, native processor graph capability.
5. Прочесть релевантные skills из локальной `.codex/skills`: `mr-feature-implementation`, `mr-realtime-audio-safety`, `mr-studio-build-test`, `mr-studio-ui-design-system` (только когда дойдёт до UI).

## Выполни при запросе начать #116: Phase A — audit

Представь краткую таблицу:
| Layer | Smart-Voicing location/API | MRS target API/ownership | Reuse unchanged? | Risks/tests |
|---|---|---|---|---|
| Core Chord/Key/Function/Tension | find actual | host-neutral library | prefer yes | explicit symbols/target |
| Voicing/11 modes | find actual | host-neutral library | prefer yes | V1, Drop-family |
| VoiceOutput/Router/Sustain | find actual | MRS MIDI adapter | decide | note ownership |
| ARA Bridge/VST3 UI | find actual | **not imported to native** | no | future reuse |
| Timeline/Transport/Project state | host adapter | MRS SHARED | adapt | seek/loop/save |

Отдельно сравни license/third-party dependencies/assets и формат state 0.5. Сформируй план точных файлов, CMake target и тестов без копии исходников движка на два независимых места. Сначала запусти существующие тесты на исходном tree если локальный toolchain позволяет; фиксируй что реально проверено.

## Phase B — минимальный перенос Core

1. В локальном MRS tree добавить isolated library/target и необходимый Core/fixtures. Имена файлов/каталогов определяются текущей структурой; **не создавай заранее выдуманный API**. CMake link отдельно от UI, VST3 и audio engine. Временное vendor-копирование возможно только с version pin/provenance и явным планом одинарного source of truth.
2. Сохранить public boundary `HarmonicContextSnapshot → Harmonic Decision/VoicingStrategy → VoiceOutput[4]`. Реальный C++ API брать из исходников, а не из условных названий этого документа.
3. Добавить parity golden tests 0.5: key/chord, explicit slash, altered dominant, sample chord change, stage5 voicings, tension policies, keyswitch state/control notes, sustain/panic. Для логики tests не требуется ASIO или native editor.
4. Сначала получить **одинаковый вывод с выбранной последней локальной версией**; не изобретать новые Voice Leading или Color/Rich heuristics сверх того, что уже реализовано в этом source checkpoint. Документировать target CPU/RT constraints и куда не разрешено помещать allocation/locks.
5. Пройти локальную сборку и test suite MRS; если зависимостей/локального исходника нет — зафиксировать blocker, не подменять remote documentation доказательством кода. Preserve existing MRS tests.
6. Отметить issue #116 выполненным только после требуемых локальных gates + пользовательской приёмки; проектный контекст обновить с branch/head/tests/package/ограничениями.

## Phase C — не делать до отдельного запроса

- #117 native Chord/Key context / MIDI FX stage: wait for availability of common MRS Chord Track #25 and actual MIDI pipeline.
- #118 native UI, keyswitch/Undo/project state.
- #119 Voice Leading + Color/Rich (источник Smart-Voicing [#10](https://github.com/vladleng/Smart-Voicing/issues/10)).
- #120–#127 future profile/performance/preset/capture/ensemble/drum/QA/VST3 reuse.
- Не редактировать Smart-Voicing VST3 repo и не изменять/закрывать его issues без указания.
- Не модифицировать Smart-Improviser, даже если его термин «Harmonic Engine» похож.

## MVP acceptance snapshot

- **Source accepted:** GitHub main документирует 0.5; **local 0.5g** требует аудита кода, локальных тестов и подтверждения. Если есть, миграция берёт 0.5g, не 0.5.
- **MRS code migrated:** NO (на момент создания).
- **Native MIDI engine integration:** NO.
- **Native Harma UI:** NO.
- **MRS ASIO musical acceptance:** NO.
- **Core future VST3 sync:** planned.
- **Current MRS Stage 5:** #25 planned as documented, не считать завершённым.
- **Первое допустимое действие после команды пользователя:** #116 — local source version audit (0.5g verification) → latest Core migration → baseline+latest parity regression.

## Workflow и handoff в конце каждого slice

- Code/source commits, builds, tests, packages остаются **локально**. GitHub — только docs/issues commits с `[skip ci]`. Не создавать Actions, PR/merge или source push без отдельного запроса.
- При наличии user acceptance обнови issue и `docs/PROJECT_CONTEXT.md`, но не закрывай parent #115 пока остальные задачи остаются не приняты.
- Каждый тест помечай отдельно: compile PASS, automated regressions PASS, ASIO/instrument physical PASS, user musical acceptance.
- Если локальный checkout отстал от опубликованных docs/issues, local code не затирай; зафиксируй divergence и предложи последовательную безопасную адаптацию.
- При handoff: версия/branch/source commit, изменённые файлы, смысл, tests, regression/ASIO gate, что не выполнено, NEXT ACTION. Сохраняй актуальный `docs/HARMA_WAVES_CODEX_HANDOFF.md` или создавай компактный отдельный slice report в локальной документации.

**NEXT ACTION для нового Codex:** только после запроса «Приступай к #116» открыть локальные checkout MRS и Smart-Voicing, составить migration inventory, выполнить первую host-neutral parity сборку и отдать пакет/тесты на проверку.
