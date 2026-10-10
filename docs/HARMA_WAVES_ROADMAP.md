# Harma Waves — MRS Native roadmap / mapping из Smart-Voicing

Дата решения: 2026-10-10. Parent [MRS #115](https://github.com/vladleng/MR-Studio/issues/115). Статус всех **MRS** задач: *planned / not implemented*, пока пользователь не принял конкретную локальную сборку. Stable **Smart-Voicing 0.5** относится к другому репозиторию и другим host tests.

> **Важное уточнение 2026-10-10 — миграция актуальной версии:** source Smart-Voicing GitHub main документирует stable 0.5, но это **не предел переноса**. В задаче [#116](https://github.com/vladleng/MR-Studio/issues/116) Codex обязан проверить все **локальные** ветки/checkpoints и конкретно наличие **Harma Waves 0.5g**. Если local 0.5g существует и проверена, переносить именно её полностью, а 0.5 использовать для регрессий. Если local 0.5g не найдена, не подменять её молча stable 0.5, а зафиксировать blocker и фактические версии. Не путать с 0.5g из отдельного проекта Smart-Improviser.

## 1. Какие исходные задачи сохраняем и куда переносим

| MRS issue | Новый объем, фактический статус | Source Smart-Voicing |
|---|---|---|
| [#116](https://github.com/vladleng/MR-Studio/issues/116) | P0 / migration of latest local 0.5g if verified; stable 0.5 parity regression, not started | [#1](https://github.com/vladleng/Smart-Voicing/issues/1), [#2](https://github.com/vladleng/Smart-Voicing/issues/2), [#17](https://github.com/vladleng/Smart-Voicing/issues/17), [#3](https://github.com/vladleng/Smart-Voicing/issues/3), [#8](https://github.com/vladleng/Smart-Voicing/issues/8), [#9](https://github.com/vladleng/Smart-Voicing/issues/9) **accepted in old VST3** |
| [#117](https://github.com/vladleng/MR-Studio/issues/117) | Native project harmonic context, processor/MIDI routing, not started | #3, #17; MRS Stage 5 [#25](https://github.com/vladleng/MR-Studio/issues/25) |
| [#118](https://github.com/vladleng/MR-Studio/issues/118) | Native UI/state/keyswitch, not started | #9, [#21](https://github.com/vladleng/Smart-Voicing/issues/21) |
| [#119](https://github.com/vladleng/MR-Studio/issues/119) | First NEW musical development: Voice Leading + Color/Rich, not started | [#10](https://github.com/vladleng/Smart-Voicing/issues/10) **open** |
| [#120](https://github.com/vladleng/MR-Studio/issues/120) | Instrument/ensemble profiles, not started | [#11](https://github.com/vladleng/Smart-Voicing/issues/11) |
| [#121](https://github.com/vladleng/MR-Studio/issues/121) | Deterministic performance/CC expression, not started | [#31](https://github.com/vladleng/Smart-Voicing/issues/31) |
| [#122](https://github.com/vladleng/MR-Studio/issues/122) | Presets/performance configurations, not started | [#12](https://github.com/vladleng/Smart-Voicing/issues/12) |
| [#123](https://github.com/vladleng/MR-Studio/issues/123) | Native 4-voice MIDI capture/commit/drag, not started | [#13](https://github.com/vladleng/Smart-Voicing/issues/13) |
| [#124](https://github.com/vladleng/MR-Studio/issues/124) | Melodic Context / Soli / independent lead, future | [#23](https://github.com/vladleng/Smart-Voicing/issues/23) |
| [#125](https://github.com/vladleng/MR-Studio/issues/125) | Brass/Strings/Bass, Chord Voicing, Riff, future | [#20](https://github.com/vladleng/Smart-Voicing/issues/20), [#21](https://github.com/vladleng/Smart-Voicing/issues/21) |
| [#126](https://github.com/vladleng/MR-Studio/issues/126) | Drum Pattern Engine, distinct non-voicing mode, future | [#40](https://github.com/vladleng/Smart-Voicing/issues/40) |
| [#127](https://github.com/vladleng/MR-Studio/issues/127) | Cross-cutting QA / Panic / future VST3 reuse, not started | [#14](https://github.com/vladleng/Smart-Voicing/issues/14), [#4](https://github.com/vladleng/Smart-Voicing/issues/4) |

Source references [#24](https://github.com/vladleng/Smart-Voicing/issues/24), [#25](https://github.com/vladleng/Smart-Voicing/issues/25), [#28](https://github.com/vladleng/Smart-Voicing/issues/28), [#29](https://github.com/vladleng/Smart-Voicing/issues/29) — музыкальные guardrails/accepted decisions, **не создавать дублирующие MRS stages**. Архивные/ошибочные Smart-Voicing issues #5–7 и #27, #32–36 не импортировать.

## 2. Приоритеты и реальные зависимости

```text
MRS [#25 Chord Track]/[SHARED MIDI infrastructure]
            |
        #116 Core 0.5 migration/parity
            |
        #117 Native Context/MIDI runtime
            |
        #118 Native UI/keyswitch/state
            |
        Native MVP user acceptance
            |
        #119 Voice Leading + Color/Rich
            |
        #120 Instrument Profiles
            |
        #121 Performance Engine
            |
        #122 Presets / Config
```

**Параллельно после #117/#118:** #123 MIDI capture (с расширенной CC/expression совместимостью после #121). **Отдельные будущие branches после #119:** #124 Melodic Context; после #120: #125 Ensemble workflows; #126 Drum Patterns может проектироваться независимо после готовности native MIDI API, но не получает 4-harmony Voice как dependency. #127 проверяется **по всем** slices; отдельный финальный gate и future VST3 rebase — позднее.

Порядок реализации определяется последним прямым запросом пользователя; checklist и роадмап не разрешают автоматический запуск всей цепочки. MRS Stage 5/#25 остаётся самостоятельным приоритетом разработки DAW; когда он не реализован, #116 можно делать как **standalone core parity**, но #117 нельзя объявлять готовой native chord integration.

## 3. G0–G4 checkpoints для первого native MVP

**G0 / Audit** (#116):
- проверить latest local checkpoint (включая 0.5g); source stable 0.5 не заменяет актуальные локальные функции;
- подтвердить фактические локальные ветки/изменения обоих репозиториев, лицензии/интерфейсы и CMake targets;
- сохранить source Smart-Voicing 0.5 musical reference/golden tests, не пытаться «улучшить» voicing при переносе.

**G1 / Portable Core** (#116):
- перенести **все** функции и тесты подтверждённой последней local версии, сохраняя 0.5 как сравнительный baseline;
- build и tests source-neutral Harmony/Core внутри MRS;
- 0.5 parity: 4 voice, key/chord normalization, 11 voicing/texture modes, Clean/Color/Rich, keyswitch semantics;
- ни одного ARA/Fender/VST3 type в harmonic engine API.

**G2 / Native MIDI/MRS context** (#117):
- MRS source Chord Track, Key, transport/tempo/meter read by adapter;
- bounded/immutable snapshots, PPQ/tick↔samples, chord transitions within block;
- 4-voice MIDI routing, no stale/off notes; failure/no data fallback.

**G3 / Native UI/project state** (#118):
- window/panel, mode/voicing/tension controls, key/real-next/chord status;
- same state via UI/MIDI 32..47/project commands, Undo/Redo and Save/Open;
- MIDI FX semantics, not audio insert.

**G4 / Musical native acceptance** (#116–118/#127):
- cross-repo source/MRS golden parity on accepted 0.5 test progressions;
- Windows local configure/build/CTest + GUI smoke; ASIO and actual instrument sound user gate separated from automated tests;
- only then start #119 new Voice Leading music, except if user explicitly changes order.

## 4. Long-term gates

- **Voice Leading (#119):** same voice continuity; correct target-aware functional Color/Rich; regression on major/minor progressions, chromatic/transposed chords and chord identity.
- **Profiles (#120):** playable vs practical vs comfortable range; same Voicing with alternative ensemble adapted to register without rewriting harmony.
- **Performance (#121):** delay-only humanize, CC11 management, Voice expression trim, deterministic seed and full event ownership.
- **Presets (#122):** versioned host-neutral state, save/open/migration/automation/keyswitch.
- **Capture (#123):** produced MIDI becomes editable tracks/clips, with correct note and control events, Undo/Redo.
- **Melodic/Ensembles (#124/#125):** dedicated policies above stable harmony; mode-specific tests.
- **Drum patterns (#126):** independent MIDI pattern engine, slots and quantized launch with drag capture, not 4-voice harmonic logic.
- **QA/Portable VST3 (#127):** local CPU xrun/lock/alloc and persistence tests, separate VST3/ARA2 adapter and future host portability gates.

## 5. Перенос исходных документов

**MRS master copy**: [architecture](HARMA_WAVES_NATIVE.md), этот roadmap, [Codex first slice](HARMA_WAVES_CODEX_HANDOFF.md). **Detailed source reference** остаётся в Smart-Voicing и должна читаться Codex перед соответствующей задачей:

- [CONCEPT](https://github.com/vladleng/Smart-Voicing/blob/main/docs/CONCEPT.md) + [Stage 5 addendum](https://github.com/vladleng/Smart-Voicing/blob/main/docs/CONCEPT-STAGE5-ADDENDUM.md) — продукты и layer ownership;
- [musical guardrails](https://github.com/vladleng/Smart-Voicing/blob/main/docs/MUSICAL-ENGINE-GUARDRAILS.md), [Functional Tension](https://github.com/vladleng/Smart-Voicing/blob/main/docs/FUNCTIONAL-TENSION-PROFILES.md);
- [Clean rules](https://github.com/vladleng/Smart-Voicing/blob/main/docs/STAGE5-CLEAN-HARMONIZATION-RULES.md), [acceptance](https://github.com/vladleng/Smart-Voicing/blob/main/docs/STAGE5-CLEAN-ACCEPTANCE.md);
- [Voice Leading](https://github.com/vladleng/Smart-Voicing/blob/main/docs/VOICE-LEADING-DIRECTION.md);
- [Performance Engine](https://github.com/vladleng/Smart-Voicing/blob/main/docs/PERFORMANCE-ENGINE-ROADMAP.md);
- [Sound Variations / MIDI 32..47](https://github.com/vladleng/Smart-Voicing/blob/main/docs/STUDIO-PRO-SOUND-VARIATIONS-WORKFLOW.md);
- [handoff/history](https://github.com/vladleng/Smart-Voicing/blob/main/docs/CHAT-HANDOFF-RULE.md).

**Почему не копируем исходники 1:1 на GitHub:** они описывают VST3/ARA2/Studio Pro и прошлую CI acceptance. В MRS написан отдельный native architecture/roadmap с ссылками на подробные оригиналы, чтобы не выдавать несуществующую MRS реализацию и не поддерживать два расходящихся текста музыкальной спецификации. При переносе Core Codex может добавить MRS-local tests/fixtures и конкретные source revision pins.

## 6. Когда VST3 снова развивается

После каждого стабильно принятого versioned musical Core milestone: делается отдельная будущая задача в Smart-Voicing по совместимости с новым Core, проверке existing ARA2 bridge и VST3 host regression. Не требовать полной feature parity любых MRS-specific операций (например, direct project MIDI commit) если VST3 API host не позволяет; provide bounded export/fallback. Исходный VST3 0.5 не меняется автоматически из-за закрытия MRS issues.

## 7. Как читать прогресс

Parent #115 содержит checkbox-index. Эти ссылки и Markdown checklist **не равны нативным GitHub parent/sub-issue relation**. Если connector не поддерживает GitHub sub-issues API, привязать native relations отдельно через GitHub UI/API, не заявлять что progress bar связан пока не проверена реальная native иерархия.

Открытые пункты в Source остаются открытыми там до отдельного решения о переносе ответственности. Созданный план MRS не закрывает Smart-Voicing issues.
