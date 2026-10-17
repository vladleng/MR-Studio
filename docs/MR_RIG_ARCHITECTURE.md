# MR Rig — Product/Technical Architecture (canonical)

**Status:** staged implementation; later DSP stages remain backlog, 2026-10-15. **Parent:** [#197](https://github.com/vladleng/MR-Studio/issues/197), under [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111). **Read with:** [GUI](MR_RIG_GUI.md), [library and presets](MR_RIG_LIBRARY_PRESETS.md), [audio/pedals](MR_RIG_AUDIO_PEDALS.md), [roadmap](MR_RIG_ROADMAP.md), [handoff](MR_RIG_CODEX_HANDOFF.md).
**Baseline 2026-10-15:** #183/#184/#185 and **#196 accepted/closed**, last #196 package **0.4b upd3 Native Editors**. **#182 superseded by #197**, no further standalone Cab stages: #186 multi-mic cancelled; only a minimal user WAV import part of #187 flows into #199; #188 graph/QA safeguards flow into #198/#201/#204. Audit local checkout before new code.

**Accepted P2 / #200, 2026-10-15:** 0.4d MR Rig Shell is accepted/closed; [actual state, signal semantics and validation](MR_RIG_SHELL_200.md). #198/#199 are accepted/closed. New native `mrs.rig`, project schema24, RigState v1 and typed RigPreset are implemented on the shared WAV core. At that accepted baseline PRE/POST arrays were empty and NAM unavailable; current #201 implementation is described below. Source/build/package remain local. User acceptance of #200 is recorded in the closed issue.


**Current P3 / #201 (2026-10-15):** 0.4e MR Rig Pre-Post accepted and closed. Schema25 / RigState v2 owns ordered native modules (eight per lane), stable instance identity, bypass/parameters and recoverable unsupported state. One shared host insert keeps one WAV core; Gain/HP/LP/Bell EQ reuse existing native DSP. The utility rack is preprepared with the graph and edited through coherent bounded snapshots. #200 is accepted/closed; #202 has an accepted first subset (Compressor+Drive); #203 not started. [Actual contract and validation](MR_RIG_CHAINS_201.md).

## 1. Product charter

**MR Rig = user-facing native Guitar Tone Environment**, not a generic IR file picker. Primary gesture is **select a named/categorized sound (visual Model card), adjust compact signal chain, save as Rig**. Advanced users can import and map raw WAV IR/NAM files as secondary workflow. Natural default for user: jazz clean sounds (Polytone/Fender examples), but product must not be genre/vendor-hardcoded.

Primary workspace:
```
 MODEL / RIG BROWSER (IR Models | NAM Models | My Rigs) [Search][Filters]
 ┌──────────────────────────────────────────────────────────────────┐
 │ PRE FX [+]  →  RIG CORE [image / model, variants] → POST FX [+] │
 │  compact chain      active: IR OR NAM             compact chain  │
 └──────────────────────────────────────────────────────────────────┘
 [Selected block inspector: values | bypass | preset] [input/output meters]
```
One native insert processor in the existing MR Studio SHARED host graph, visual/editor lifetime independent of audio DSP. The central Core is not a parallel DAW/plugin host with a separate device; no auto bypass of external inserts when choosing a full combo.

## 2. Types and ownership contracts

**Three different persistable entities; do not conflate them:**
1. `SourceAsset`: typed actual on-disk media: `wav_ir` (one validated WAV or approved channel mapping), later `nam_model`. Tracks external identity/root-relative path/fingerprint, rate/length/channels or NAM architecture metadata. No bundled commercial media.
2. `ToneModel`: **curated presentation + source semantics**: immutable stable model ID (not an absolute path), type IR/NAM, semantic capture role (`cabinet_only`, `full_combo` for WAV; `nam_full_rig`, `nam_amp_only`, `unknown` for NAM), vendor/model/variant/capture provenance and confidence, picture/thumbnail URI as a validated local reference, tags/preset defaults/favorite. One model can map multiple validated source variants. An image is presentation metadata, not DSP.
3. `RigPreset`: ordered Pre FX `moduleInstance` snapshots + selected ToneModel/SourceAsset variant + Rig Core controls + ordered Post FX snapshots + input/output gains, bypass, version and compatible capability flags. One ToneModel can serve many Rigs, no WAV duplication. RigPreset is **not** a collection-manifest row and not a NAM binary.

Possible later `FactoryCuratedSet`: collections of metadata/bookmarks, only redistributable if rights allow; user must install/source vendor files. Factory app must remain useful with legal placeholders/examples and user's own free/licensed media. Default card image missing → neutral placeholder, not invisible model.

## 3. Core backend selection (actual DSP, not UI theme)

`RigCoreType` = `WAV_IR` or `NAM` (future) with capability negotiation and explicit type. At most one *audible* active core per rig except bounded transition for clickless switching.

- `WAV_IR` with `full_combo` capture role → use existing MR Cab convolution; **no extra cabinet/room automatically**. The WAV is a fixed **linear** captured response, not dynamic Fender/Polytone emulation, no fake interactive preamp knobs. `cabinet_only` is available but **not** a standalone amplified sound: UI labels it accurately; permit users to place separate amp plugin elsewhere or use future onboard amp core only if real DSP exists.
- `NAM` full-rig → NAM runtime with captured speaker/mic as documented; no additional Cab. `NAM` amp-only → optional post-neural **one** Cabinet IR loader (reuse current Cab convolution, not a second audio engine), before Post FX. Scope unknown → mark `unknown`; ask user to classify, never infer full-rig solely from filename. Don't promise extraction/removal of embedded cabinet from a full-rig model.
- User-selected Own WAV IR → remains optional `Import My IR` and may be classified as full_combo/cabinet/unknown manually; default unknown until evidence.
- Generic other rig types future only after explicit product decision. NAM A2 architecture/version compatibility must be audited before promises; non-neural `*.nam` is **not WAV** and requires its own decoder/runtime/inference engine.

**One actual DSP graph** contract: Input utility (meter, optional tuner/gate) → ordered Pre FX → Core (selected model; optional cabinet stage only for explicit NAM amp-only) → ordered Post FX → output meter. Detailed contracts [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md).

## 4. Transform existing MR Cab: updated compatibility scope

**User decision, 2026-10-15:** «Старые проекты сохранять не нужно, они были только для тестов».
Backward compatibility with old test Cab projects is **not required**. This replaces
earlier requirements to retain old Cab processor IDs/schema/presets for project migration.
Do not implement Cab→Rig Convert, compatibility wrappers or a fallback Cab editor solely
to keep those test projects working. This does not authorize deletion/modification of
existing user files, vendor WAVs or accepted build packages.

Reuse the verified WAV convolver, async preparation/switching, source validation,
shared host commands and safe editor lifetime. P0 #198 now recommends one new
`mrs.rig` container and a clean versioned RigState, without legacy conversion;
the revised audit was accepted by the user on 2026-10-15; #199 was then authorized. Internal DSP names need not be renamed.
No processor ID or project format is changed by this documentation.

New MR Rig projects and RigPresets must round-trip and support Undo/Redo, stable
SourceAsset identity, explicit missing/relink and proper GUI persistence. Existing
MR Reverb remains a separate native insert; shared DSP reuse does not change its
contract. Minimal Import My IR/manual role mapping belongs to #199; #189 UI Kit is
optional for the initial shell. #186 Advanced multi-mic remains cancelled.

## 5. User workflow

1. Insert MR Rig → default browser IR Models, curated cards with photo/placeholder, labels `Full Combo WAV` or `Cabinet only`, Ready/Missing/Unsupported.
2. Choose model card → load validated source asynchronously, display photo and model/variation, play without clicks, level match manually/record.
3. Add (or reorder) one or more left PRE FX and right POST FX. Select a tile → bottom inspector shows **only relevant, real controls**.
4. Save current chain as `My Rigs / Warm Jazz`, custom image optional; reopen exact state and current render, even if UI was closed.
5. Advanced option: `Import My IR` / later `Import NAM` → user selects local file, maps role/capture, assigns artwork/tags, then saves Tone Model. Don't overwrite source media.
6. When model missing/unsupported: show full non-destructive status and original source identity, leave rest of RigPreset intact. **Never auto replace missing Polytone with another Fender file**.

## 6. Acceptance and boundaries

Scope is staged. Design and docs are not proof of a running plugin. Each issue has acceptance gates and no work before user commands. GUI smoke at 100/125/150/200% logical zoom; physical mixed-DPI and actual ASIO require separate manual evidence. CPU/RT deadlines measured for buffers applicable to recording/Live vs larger studio mix buffers; full current CTest at candidate acceptance, targeted during iteration.

No proprietary WAV, NAM or amplifier pictures in repo/packages/fixtures without license; use synthetic WAV and permitted original assets. Direct web downloads, image scraping, new VST3 host or neural training not scope. Host command/Undo/multi-instance/lifetime and realtime audio safety governed by [AGENTS.md](../AGENTS.md) + [LOCAL_BUILD_POLICY](LOCAL_BUILD_POLICY.md).


---

## Текущая стадия / 2026-10-17

**#198–#201 приняты и закрыты.** #202 открыта после принятой первой порции Compressor/Drive (0.4f); другие педали не приняты автоматически. #203 NAM пока не реализован и разбит на логические [#223 — runtime comparison](https://github.com/vladleng/MR-Studio/issues/223), [#224 — Direct native](https://github.com/vladleng/MR-Studio/issues/224), [#225 — performance/Live](https://github.com/vladleng/MR-Studio/issues/225). GitHub-native attachment этих новых issues **не выполнен**. Их рабочие контракты — [#203](https://github.com/vladleng/MR-Studio/issues/203) и [MR_RIG_AUDIO_PEDALS.md](MR_RIG_AUDIO_PEDALS.md) §7. Full Rig / Amp Only routing и shared host graph остаются неизменными. #204 — финальная пользовательская приёмка. Это docs-only изменение, не новое состояние исходников и не разрешение стартовать код.
