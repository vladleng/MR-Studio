# MR Rig — единый Roadmap (после объединения MR Cab #182)

## FIRST CHORUS DECISION — Boss CE-1 (2026-10-19)

**User's approved sound direction:** first Chorus for MR Rig is **MR Vintage Chorus**, drawing on BOSS CE-1 Chorus Ensemble (classic colored BBD chorus + authentic-character Vibrato). The implementation has its **own [#238 05H issue](https://github.com/vladleng/MR-Studio/issues/238)** under [#202 Character Pedals](https://github.com/vladleng/MR-Studio/issues/202). This is a **low-overhead original perceptual model**, not an exact BOSS circuit/code clone or an entire Roland JC-120 two-speaker spatial simulator. [#232](https://github.com/vladleng/MR-Studio/issues/232) retains Compressor/Drive optional voicing roadmap + Delay planning; do **not** implement Chorus there.

[#238](https://github.com/vladleng/MR-Studio/issues/238) slices: reference/local API audit (read-only) → playable BBD-inspired Chorus/Vibrato DSP → native PRE/POST compact UI/presets and test/level-matched listening → per-stage user acceptance. Requires **separate authorisation to start**. NAM #203 official Core and performance remain prioritized in the full-Rig path; #233 full pedal Live QA and #204 final integrated release remain unchanged. Source/build/tests local; this documentation update does not run any DSP.

---


## ACTIVE NAM BACKEND — 2026-10-18 (overrides earlier candidate-comparison roadmap)

**Chosen baseline:** [official upstream NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) (MIT core, subject to pinned commit and transitive license review) is the sole **default** native NAM DSP for MR Rig. [TONE3000](https://github.com/tone-3000/tone3000-plugin) is a verified JUCE/C++ example using Core **in-tree** and an optional controlled audio/performance comparison reference; do not vendor/copy TONE3000 or its ecosystem. [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) is a **conditional backup research option** after a reproducible upstream blocker **and** separate user approval, not a parallel-engine deliverable. The original candidate comparison in older sections of this roadmap is **superseded**.

**P5 progression (no new issues needed):** [#223 06A](https://github.com/vladleng/MR-Studio/issues/223) **official-core audit/licensing/pinned version + built-in benchmodel baseline** → [#224 06B](https://github.com/vladleng/MR-Studio/issues/224) **native Direct integration** → [#225 06C](https://github.com/vladleng/MR-Studio/issues/225) **Core-specific optimizations/Live xRun/headroom QA**; [#226 06D](https://github.com/vladleng/MR-Studio/issues/226) NAM-first UX/Cab may follow #224 in parallel with #225. Final acceptance [#204](https://github.com/vladleng/MR-Studio/issues/204). NAM Amp is still the ONLY main amp core; Full Rig no extra Cab; Amp Only optional single Cabinet IR; Live ASIO128/48k Direct without added worker buffer.

**External test evidence is preliminary:** user image TONE3000 shows ~9.3% in its own plugin meter; prior NeuralRack ~13–14% was another host/test. Do **not** infer a percentage speedup without matching model/host/CPU/meters. All new performance/ASIO evidence remains NOT TESTED until performed under #223/#225. Do not begin any stage or rebuild merely from this documentation update.

---


## PEDAL CHARACTER ROADMAP — 2026-10-17 (supplements NAM-first)

[#202](https://github.com/vladleng/MR-Studio/issues/202) now covers **original characterful guitar pedals**, starting with one algorithmic Pedal Reverb: Vintage Plate / Vintage Spring / Small Room / Modern Ambient. Full specification [MR_RIG_PEDAL_CHARACTER.md](MR_RIG_PEDAL_CHARACTER.md). Accepted Compressor+Drive 0.4f remain immutable v1 sound and accepted status; independent convolution [#128 MR Reverb](https://github.com/vladleng/MR-Studio/issues/128) remains unchanged. This is future PEDAL work and does not reopen Cab or supplant NAM Amp as the one core. NAM [#203](https://github.com/vladleng/MR-Studio/issues/203) retains higher integration priority.

**Logical #202 subtask order (native GitHub attachment NOT confirmed):**
[#227 — 05A contract/audit](https://github.com/vladleng/MR-Studio/issues/227) → [#228 — 05B FDN Plate](https://github.com/vladleng/MR-Studio/issues/228) → [#229 — 05C Spring](https://github.com/vladleng/MR-Studio/issues/229) → [#230 — 05D Room/Ambient](https://github.com/vladleng/MR-Studio/issues/230) → [#231 — 05E UI/trails/state](https://github.com/vladleng/MR-Studio/issues/231) → [#233 — 05G Live QA](https://github.com/vladleng/MR-Studio/issues/233). [#232 — 05F existing Compressor/Drive opt-in voicings and Delay/Chorus plans](https://github.com/vladleng/MR-Studio/issues/232) is a **separately authorized optional branch** (not a forced 05G blocker if outside accepted scope).

Do not start the next subtask automatically. Full Live pedal 05G requires current NAM engine/interface #224/#226 accepted; [#204](https://github.com/vladleng/MR-Studio/issues/204) remains final whole-Rig QA. Each subtask has own acceptance. No new DSP is implemented by documentation.

---


## ACTIVE ROADMAP — 2026-10-17: NAM-first supersedes Full Combo WAV as Amp Core

**[Parent #197](https://github.com/vladleng/MR-Studio/issues/197): MR Rig = NAM Amp + optional ONE post-NAM Cabinet IR + existing PRE/POST pedals / My Rigs.** Stop developing a first-class IR-combo gallery or dual `WAV Amp Core / NAM Amp Core`. Keep accepted code, assets and prior acceptance #183–185/#196/#198–201; reuse existing IR convolution **only for Cabinet IR** in future sound paths, no independent Cab line #182. Old stage table and WAV-first product claims below = historical development/accepted initial versions, **not target architecture**.

**Current progress:** #198–201 accepted/closed; #202 Compressor+Drive first subset accepted and remaining pedals optional/open, now **lower priority than #203 NAM**; #203 NOT STARTED, #204 final NAM-first QA. Native GitHub #198–204 parent attachments as previously recorded; new #223–226 below **logical only** until manually attached.

| NAM-first P5 work | Issue | Result | Gate |
|---|---|---|---|
| 06A | [#223](https://github.com/vladleng/MR-Studio/issues/223) | **Official NeuralAmpModelerCore** licence/version/compatibility + benchmodel baseline | Separate user authorization, accept before 06B |
| 06B | [#224](https://github.com/vladleng/MR-Studio/issues/224) | Native NAM Direct DSP (48k/ASIO128 baseline), safe load/swap, semantic Full Rig / Amp Only | 06A accepted |
| 06C | [#225](https://github.com/vladleng/MR-Studio/issues/225) | Profile guided optimization and true Live load/xrun stress; optional Buffered/Ahead | 06B accepted, separate authorization |
| 06D | [#226](https://github.com/vladleng/MR-Studio/issues/226) | NAM-first model browser, one post-NAM Cabinet IR slot and RigPreset UI | 06B accepted, separate authorization; can parallel 06C |
| Final P6 | [#204](https://github.com/vladleng/MR-Studio/issues/204) | NAM-first end-to-end Studio/Live/persistence acceptance | NAM P5 accepted and scoped pedals |

**Routing:** `PRE → NAM Amp Only → [optional Cab IR] → POST` or `PRE → NAM Full Rig → [no external Cab] → POST`; role Unknown → manual decision. Imported WAV Full Combo is **not Cabinet IR** and never double-stacked silently. Existing WAV convolver/source identity is internal reusable code, not an amp product. Old test-only IR projects need no full migration; do not delete user's files or accepted package. Direct no added buffer, measurement before CPU optimization, ASIO128/48k Live baseline including Omnisphere/XVox/playback/recording. **Issues/docs update only, implementation NOT STARTED**.

---


**Решение 2026-10-15. Канонический активный parent — [#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197).** Отдельное направление [#182 MR Cab 2.0](https://github.com/vladleng/MR-Studio/issues/182) прекращено / закрыто как superseded; **его выполненные результаты сохраняются**, не удаляются и не переписываются. [#111 Native Plugins](https://github.com/vladleng/MR-Studio/issues/111) остаётся категорией. Это перенос roadmap, **не реализация кода**.

## 1. Реализованный фундамент (historical accepted)

| Issue | Статус | Что переиспользует MR Rig |
|---|---|---|
| [#183](https://github.com/vladleng/MR-Studio/issues/183) | **Accepted, closed** | Аудит IR-кода и коллекции Celestion |
| [#184](https://github.com/vladleng/MR-Studio/issues/184) | **Accepted, closed** | Manifest, импорт/идентификация WAV, Celestion data |
| [#185](https://github.com/vladleng/MR-Studio/issues/185) | **Accepted, closed** | Существующий IR convolver, async switching, native editor, presets |
| [#196](https://github.com/vladleng/MR-Studio/issues/196) | **Accepted, closed — 0.4b upd3** | Full Combo WAV, роли cabinet/full_combo/unknown, изменения native editors |

Принятая локальная поставка #196 — 0.4b upd3 Native Editors; **не** «pending acceptance». Существующий DSP является внутренним переиспользуемым компонентом MR Rig. Кабинетный Celestion IR остаётся как **дополнительный ассет**, а не отдельный пользовательский плагин с новым roadmap; никакие исходные WAV или пользовательские проекты не удалять.

## 2. Старые незавершённые задачи — решение

- **[#186 Advanced mic/phase/room/composite](https://github.com/vladleng/MR-Studio/issues/186)** — **отменить**, не переносить в обязательные функции MR Rig.
- **[#187 Generic TONEX WAV / IR Collections importer](https://github.com/vladleng/MR-Studio/issues/187)** — **отменить как отдельный продукт**. Только практичный `Import My IR` и manual role/artwork/tags, external asset identity, safe missing/relink и нужные WAV formats идут в [#199](https://github.com/vladleng/MR-Studio/issues/199). **Не переносить** универсальный каталог микрофонов, многофирменные вендорские профили и полноценный import wizard.
- **[#188 Cab-only modular/QA](https://github.com/vladleng/MR-Studio/issues/188)** — **отменить как отдельный этап**. Общие ограничения RT, shared host, module extension, PDC и QA распределить в [#198](https://github.com/vladleng/MR-Studio/issues/198), [#201](https://github.com/vladleng/MR-Studio/issues/201), [#204](https://github.com/vladleng/MR-Studio/issues/204). Не реализовывать второй аудиодвижок.

Архивные документы [MR_CAB_COLLECTIONS.md](MR_CAB_COLLECTIONS.md), [MR_CAB_ROADMAP.md](MR_CAB_ROADMAP.md), [MR_CAB_CODEX_HANDOFF.md](MR_CAB_CODEX_HANDOFF.md) — **справка о принятых Cab этапах, не активное ТЗ**. Спецификация дальнейшей разработки только в `MR_RIG_*.md` + активных #197–204.

## 3. Единственная активная последовательность

| Этап | Issue | Реальный результат | Dependency / gate |
|---|---|---|---|
| **P0** | [#198](https://github.com/vladleng/MR-Studio/issues/198) | Read-only аудит существующего 0.4b upd3 backend, plugin ID/state, пути перехода к Rig | **Начать только по запросу**, согласовать migration strategy |
| **P1** | [#199](https://github.com/vladleng/MR-Studio/issues/199) | Curated IR Models / NAM Models / My Rigs с фото, тегами и **опциональным Import My IR** (поглотил нужное из #187) | P0 accepted, реальный файловый asset |
| **P2** | [#200](https://github.com/vladleng/MR-Studio/issues/200) | Компактный MR Rig Shell: PRE FX слева / модель в центре / POST FX справа, первым реальным backend WAV | P1 accepted, UI acceptance |
| **P3** | [#201](https://github.com/vladleng/MR-Studio/issues/201) | Reusable ordered Pre/Post module slots, bypass/reorder, one SHARED DSP graph, Undo/PDC | P2 accepted; часть требований бывшего #188 |
| **P4** | [#202](https://github.com/vladleng/MR-Studio/issues/202) | Нативные педали (компактный сначала утверждённый subset) | P3 accepted |
| **P5** | [#203](https://github.com/vladleng/MR-Studio/issues/203) | NAM engine, Full Rig/Amp Only/Unknown, optional one Cab only for Amp Only, CPU safety | P0–P3 accepted; необязательно ждать все педали |
| **P6** | [#204](https://github.com/vladleng/MR-Studio/issues/204) | Сквозной RigPreset/GUI/Studio+Live QA, accepted release | Все согласованные готовые capabilities, user signoff |

**Не создавать отдельную линию #182/#186–188 в дополнение к таблице**. P1 reusable asset loader не должен становиться бесконечным универсальным IR browser. Не обязателен multi-mic mixing или самостоятельный cabinet editor.

## 4. Product/technical invariants

- Главный UX **модель звука с обложкой**, а не WAV file picker; full RigPreset = selected ToneModel/source + Pre/Post FX modules/params/output. Artwork user-owned/licensed, fallback safe.
- WAV Full Combo = статический **линейный** захват целого пути, один existing convolver без второго cabinet; WAV Cabinet Only честно так обозначен и требует реальной amp processing где нужно.
- NAM — **другой** engine; Full Rig contains cab/mic → no extra Cab; Amp Only → optional single Cabinet IR, Unknown → manual classification. До реализации NAM tab помечен Planned/Unsupported.
- Не менять вслепую `mrs.cab-ir`, native plugin ID или schema; P0 #198 проверить migration и user preset compatibility. Код/тесты/пакеты локально, GitHub docs/issues [skip ci], сторонние коммерческие WAV/NAM/обложки не публиковать.
- Realtime no alloc/free/locks/I/O/UI in callback, async prep, no double signal path, command model/Undo/SaveOpen/PDC, качественное измерение studio vs record/live.
- Начинать новый этап только после команды пользователя, отдельная приёмка каждой ступени. **Ни один MR Rig этап не реализован этим обновлением документации.**

## 5. Кодекс

Рабочий чат читает **текущий [AGENTS.md](../AGENTS.md) по принципу Issue-first**, [MR_RIG_CODEX_HANDOFF.md](MR_RIG_CODEX_HANDOFF.md) и необходимые разделы спецификаций для **назначенного** issue. Не сканировать `MR_CAB_*` как обязательный roadmap. Начинать с #198, read-only. Старый #182 и его native sub-issues — история; #198–#204 **нативно прикреплены к #197 (проверено API, 7/7)**, Markdown-ссылки остаются навигацией. GitHub-progress зависит от реальных состояний задач.


---

## 6. Текущий статус / NAM runtime scope — 2026-10-17

**Принято и закрыто:** #198–#201. **#202 открыта:** пользователь принял Compressor+Drive (0.4f), остальные педали — отдельные согласуемые порции. **#203 не начата; #204 — будущий final QA.** Прежний P0 handoff в старых разделах — исторический, не указание запускать аудит заново.

P5 #203 детализирован следующими **логическими** задачами; они **пока не native-attached** в GitHub и не заменяют семь уже прикреплённых к #197 этапов:
- [#223](https://github.com/vladleng/MR-Studio/issues/223) — 06A **official NAM Core**: supported model architectures, pinned upstream/license, built-in tools and controlled baseline; NeuralAudio fallback only for proven blocker with approval.
- [#224](https://github.com/vladleng/MR-Studio/issues/224) — 06B **Native MR Neural Amp**: Direct без добавочной buffered latency, async model warm-up/handoff, Full Rig / Amp Only + existing Cabinet IR, preset/Undo/PDC.
- [#225](https://github.com/vladleng/MR-Studio/issues/225) — 06C **Core-specific optimizations и Live QA**: compiler/Eigen SIMD/A2 only where official Core supports them, 48k ASIO128/Omnisphere/XVox/playback/recording 30–60 min, optional Studio buffered/ahead; 32–64 sub-block только гипотеза, не обещание.

**Live policy:** сохраняем рабочий ASIO 128 сэмплов при 48 кГц. Внешний NeuralRack Direct дал ~13–14% host audio performance на Polytone NAM, Buffered показал ~0% host meter +2.67ms — не системный CPU и не результат MR Studio. Сначала benchmark и direct engine; worker buffering/субблок — только исследование после измерений. Отдельная пользовательская приёмка каждого шага, не запускать их автоматически. Детали — [#203](https://github.com/vladleng/MR-Studio/issues/203) и [#204](https://github.com/vladleng/MR-Studio/issues/204).
