# Harma Waves Native — архитектура интеграции в MR Studio
> Решение пользователя: 2026-10-10. **Статус: PLANNED, реализация в MRS не начата.** Parent issue [#115](https://github.com/vladleng/MR-Studio/issues/115). Это продуктовый/native MIDI engine track внутри [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111), а не аудиоэффект в обычной FX chain.

> **Важное уточнение 2026-10-10 — миграция актуальной версии:** source Smart-Voicing GitHub main документирует stable 0.5, но это **не предел переноса**. В задаче [#116](https://github.com/vladleng/MR-Studio/issues/116) Codex обязан проверить все **локальные** ветки/checkpoints и конкретно наличие **Harma Waves 0.5g**. Если local 0.5g существует и проверена, переносить именно её полностью, а 0.5 использовать для регрессий. Если local 0.5g не найдена, не подменять её молча stable 0.5, а зафиксировать blocker и фактические версии. Не путать с 0.5g из отдельного проекта Smart-Improviser.

## 1. Принятое продуктовое решение

- **MR Studio — основная среда дальнейшей разработки Harma Waves** (ранее Smart Voicing): музыкальное ядро, новые режимы, native UI и интеграция с DAW/MIDI.
- Самостоятельный [Smart-Voicing](https://github.com/vladleng/Smart-Voicing) **не удаляется и не переписывается**. Его выпущенная/принятая VST3 + ARA 2 версия 0.5 — исходный reference; позже VST3 получает устойчивые Core releases. ARA 2 мост существует в исходном VST3 workflow; **он не нужен для native MRS Chord Track**.
- Разрабатываем **одно музыкальное ядро**, а не две расходящиеся реализации. Пока исходники физически размещены в двух checkout, стратегию single source of truth должен предложить первый migration slice (vendored subtree/отдельная library repository/фиксированная shared dependency). Не переписывать историю и не объявлять синхронизацию решённой до проверки локальных сборок.
- [Smart-Improviser](https://github.com/vladleng/Smart-Improviser) — **другой проект**, он тоже имеет Harmonic Engine, но импорт/унификация его логики в Harma Waves не согласованы. Избегать конфликтов терминов и скрытого слияния моделей.

## 2. Что действительно уже сделано в Smart-Voicing (не путать с MRS)

Reference: [START-HERE](https://github.com/vladleng/Smart-Voicing/blob/main/docs/START-HERE.md), [CONCEPT](https://github.com/vladleng/Smart-Voicing/blob/main/docs/CONCEPT.md), [accepted Stage 5](https://github.com/vladleng/Smart-Voicing/blob/main/docs/STAGE5-CLEAN-ACCEPTANCE.md), [guardrails](https://github.com/vladleng/Smart-Voicing/blob/main/docs/MUSICAL-ENGINE-GUARDRAILS.md).
- Stable Smart Voicing **0.5**, Stage 0–5 accepted в исходном VST3/Studio Pro; не выдавать source acceptance за MRS native acceptance.
- MIDI Direct Router: Top Down / Bottom Up / Fill 4, VoiceOutput V1..V4, независимый MIDI channel routing, CC/sustain/Voice Stack и note ownership.
- ARA + instrument bridge в Studio Pro; host-neutral HarmonicContext/ChordContext/Provider architecture, normalised chord slash/quality/extensions/alterations, key/function/real target evidence.
- Melody Harmonize: сыгранная V1 неизменна, V2..V4 подстраиваются по текущему аккорду; live chord change, несуществующий chord → V1 only, без дополнительных ложных NoteOn/Off.
- Tension Clean / Color / Rich; function/target-aware candidates, explicit chord quality/alterations/characteristic notes выше inference.
- Voicing strategies: Closed, Drop 2/3/2+4, Spread, Quartal, UST, Cluster; мелодические Unison, Octaves, Doubling.
- Невыполненный source Stage 6 — [Voice Leading](https://github.com/vladleng/Smart-Voicing/issues/10); далее profiles, performance, presets и прочее. См. [roadmap MRS](HARMA_WAVES_ROADMAP.md).

## 3. Целевая архитектура

```text
MR Studio Project Model (Chord Track, Key/Tempo/Meter, MIDI clips)
                  |           MRS Transport/timeline
         MRS Harmonic Context Adapter
                  |
         immutable context snapshots
                  v
       Harma Waves CORE (DAW-agnostic C++)
       ├ Chord/Key/Function/Target evidence
       ├ Harmonic candidate pool / TensionPolicy
       ├ VoicingStrategy + VoiceLeading
       ├ InstrumentProfile + PerformanceEngine
       └ MIDI VoiceOutput[4] + deterministic transitions
                  |
        MRS Native MIDI Processor Adapter
         |              |               |
     monitoring     MIDI clip commit    routing
         |              |               |
       MIDI instrument / tracks / Arrange-Edit
                  |
       MRS Commands, Undo/Redo, Presets,
       Project persistence, Native UI
```

**VST3 compatibility later**:
```text
Fender / other host → ARA2 context provider + VST3 instrument adapter
                   → same versioned Harma Waves Core → MIDI channels
```
ARA provider читает host harmonic context, а не определяет musical rules. Не копировать внутренности ARA в MRS.

## 4. Правильная граница компонента

Это **native MIDI processor / MIDI FX**, а не «аудиоплагин»; не создавать фальшивый AudioProcessor, если существующий MRS insert chain обрабатывает только audio. Переиспользовать **SHARED MIDI/Transport/Project Model**. Native UI может быть отдельной панелью в Arrange/Edit/Mix, но музыкальное состояние независимо от представления.

**Контекст**: актуальный явно заданный chord имеет приоритет; global Key и harmonic function его интерпретируют, а не исправляют. Current/next должны читать одно представление MRS Stage 5 #25; модификация Chord Track и пересоздание graph не должны быть сцеплены. Empty/unknown остаются empty/unknown; предполагаемая цель не подменяет real next chord.

**Модели**: Core контракты не знают JUCE UI, VST3, ARA, ASIO, MR Studio project serialization. MRS adapter отвечает за PPQ/tick↔sample boundaries, MIDI track/event model и сохранение state; Core — за музыкальное решение. Конкретные пути/имена классов уточнить после локального аудита, не выдавать псевдокод за существующий API.

**MIDI routing**: один внутренний 4-voice output контракт не равен обязательным четырём physical tracks. Пользователь может мониторить 4 канала, а затем materialize 4 редактируемые партии в MRS MIDI editor. Processed-MIDI record, clip bounce и source recording — разные явно выбираемые workflows.

## 5. Неприкосновенные музыкальные инварианты

1. Played / Melody V1 immutable в Melody Harmonize. Реальная явная гармония и characteristic tones (m7b5 b5, #5, sus, altered fifth) имеют приоритет; Clean не выкидывает явно написанные 9/11/13.
2. Stage 4 ownership (music vocabulary): Chord, Key, Function, real resolution target, Functional Tension Profile. Stage 5 ownership: **vertical shape**, Drop transforms only octave/shape from chosen Closed pitch classes. Stage 6: **continuity** и choice среди разрешённого pool. Не чинить ошибки соседнего слоя сменой MIDI-нот где попало.
3. Real next chord только как подтверждённый next в current project timeline; если next нет, не выдумывать destination. Color/Rich функциональны, Rich != «всегда altered».
4. Drop family сохраняет Closed pitch classes; Spread/Quartal/UST/Cluster имеют свои objective; Unison/Octaves/Doubling могут быть pitch-identical между tension levels.
5. V1..V4 stable identity, MIDI range 0..127, max 4 звучащих Voice, bounded search; same input+state=deterministic output. Stable sustain note ownership; seek/stop/loop/bypass/Panic всегда снимают принадлежащие notes.
6. Realtime: no allocation, file I/O, blocking locks, UI calls, network/AI in callback; project/UI changes доставлять через existing MRS command/control snapshot; обновление/уничтожение processing state вне callback.
7. Live/Studio не должны иметь два музыкальных движка. Live ограничивает только соответствующие низколатентные policies; heavy/offline musical planning может использовать другие пути при соблюдении coherent project state.

## 6. Canonical keyswitch mapping (источник Smart Voicing 0.5)

| MIDI note | Action | MIDI note | Action |
|---|---|---|---|
| 32 | UST | 33 | Cluster |
| 34 | Quartal | 35 | Spread |
| 36 | Closed | 37 | Drop 2 |
| 38 | Drop 3 | 39 | Drop 2+4 |
| 40 | Unison | 41 | Octaves |
| 42 | Doubling | 43 | Clean |
| 44 | Color | 45 | Rich |
| 46 | Direct Router | 47 | Melody Harmonize |

Номера, а не имена октав, являются контрактом. Один authoritative state для кнопок, presets и keyswitch; эти control notes не записывать как музыкальные ноты. Для отдельного Drum Patterns mode указывать scoped keymap, без global конфликтов.

## 7. Persistence, migration, versioning

- Не сохранять pointer/ARA state; только versioned semantic HarmaWavesState/Preset и привязки MRS к дорожке/проекту. Project Model владеет IDs, topology, Undo/Redo, Save/Open. Источник и target versions фиксировать для migration.
- В первом slice проверить отдельно старые Smart Voicing 0.5 UI/plugin state, новые native state, MRS project schema/legacy readers. **Не обещать прямую загрузку VST3 plugin-state blob в native** без явного конвертера.
- Релизы MRS имеют собственное версионирование; версии Smart-Voicing 0.5/0.6 не становятся номерами сборок MRS. VST3 export/rebase/sync — отдельный future gate.

## 8. Gates и источники истины

**Source accepted** (Smart-Voicing 0.5) ≠ **Code migrated** ≠ **MRS build PASS** ≠ **MRS MIDI/ASIO pass** ≠ **User musical acceptance**. Каждое состояние подтверждать отдельно в issue и `docs/PROJECT_CONTEXT.md`.

Контрольные ситуации для native parity: `Dm7-G7-Cmaj7`; `Bm7b5-E7-Am`; `A7` без next и с реальным `Dm`; `Cmaj7/E`, `Cmaj7/G`, `C13`, `G7b9`; unchanged V1; mode/keyswitch playback/sustain/stop/seek/loop. Первый gate *не* включает новый Voice Leading — сначала одинаковая музыка с stable 0.5.

## 9. Разработка/документация

MR Studio [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md) имеют приоритет. Код, сборки, тесты, пакеты и source commits **только локально**; GitHub — issues/docs с `[skip ci]`, никаких Actions/auto PR/merge. Smart-Voicing repo без изменений на этапе планирования. **Start first:** [Codex handoff](HARMA_WAVES_CODEX_HANDOFF.md), issue [#116](https://github.com/vladleng/MR-Studio/issues/116).
