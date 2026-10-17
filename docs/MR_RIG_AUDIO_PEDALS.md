# MR Rig — DSP Graph, PRE/POST Pedals, NAM Modes and Safety

## CHARACTER PEDALS ADDENDUM — 2026-10-17

The current planned [#202 Character Pedals](https://github.com/vladleng/MR-Studio/issues/202) scope is authored in [MR_RIG_PEDAL_CHARACTER.md](MR_RIG_PEDAL_CHARACTER.md), split into [#227–#233](https://github.com/vladleng/MR-Studio/issues/227). This **adds** original guitar pedal-style character to PRE/POST FX; it does **not** change the NAM-first core, replace accepted #128 convolution reverb, overwrite accepted Compressor/Drive v1, or create a new audio graph.

One native algorithmic Pedal Reverb module, modes **Plate (FDN)**, **Spring (separate dispersive DSP)**, **Small Room (ER + FDN)**, **Ambient (FDN diffusion/modulation)**. Single active mode; Simple controls Decay/Tone/Mix/Character, Advanced real DSP-backed controls only. Reverb PRE/POST placement chosen by user, predictable trails and bypass, correct dry/wet, smooth parameter changes, Undo/RigPreset and versioned module state.

48 kHz/ASIO128 Live Direct reference with no new worker-buffer latency. Off-thread preparation, no allocations/waits/locks/I/O/UI in callback. Late-tail 2:1 downsampling is only a measured quality/CPU trade-off (may add filtering latency); no promise of fixed CPU gain. Full Live QA #233 depends on accepted NAM #224/#226; final whole-Rig #204 separately. This is planning only.

---


## ACTIVE DSP GRAPH — 2026-10-17: NAM-only amp core + optional ONE post-NAM Cabinet IR

**Supersedes historical WAV/Core alternatives in §1 below for all future code.** [#197](https://github.com/vladleng/MR-Studio/issues/197) is NAM-first: `Input → PRE → NAM Amp → Cabinet IR? → POST → Output`. Existing WAV convolver is not a second amplifier model. Keep accepted rack/pedal system; no new host engine, duplicated dry path or nested effects graph.

| Capture role | NAM processing | Cabinet IR DSP |
|---|---|---|
| `amp_only` | One NAM inference | Optional **one** existing convolution; user can select/bypass |
| `full_rig` | One NAM inference including speaker/mic | **Bypassed/absent from audible path**; preserve selected cab setting for later |
| `unknown` | Load supported model only with clear role state | Require explicit user classification before automatically using Cab; no inference by filename |

Full Combo WAV linear captures are NOT repackaged as Cab IR and are **not a future amp-core option**. Reuse validated #185/#196 convolver/loading implementation internally as cabinet stage. No automatic reverb/room added to cabinet IR mixes. CAB state/variant and NAM model must persist independently with PRE/POST + bypass + Undo/PDC; any new schema after local audit.

**Realtime:** #223 (NeuralAudio vs NAM Core baseline) → #224 (Direct/async/safe model handoff and one optional Cab) → #225 (CPU optimization/Live 48k ASIO128 testing); [#226](https://github.com/vladleng/MR-Studio/issues/226) owns the NAM amp browser and post-NAM cabinet picker UI. #225 and #226 can be authorized after #224 independently. Direct must add no worker-buffer latency; Live ASIO roundtrip/intrinsic algorithmic latency is distinct. Buffered/Ahead and sub-block are **optional measured research**, not hard acceptance criteria. Earlier dual WAV/NAM graph in this doc records historic as-built / former roadmap only.

---


**Parent [#197](https://github.com/vladleng/MR-Studio/issues/197)**. Staged architecture; the current implemented subset is listed below. Other pedals/NAM remain planned. Reuse accepted [#185 Cab](https://github.com/vladleng/MR-Studio/issues/185) and **accepted #196 Full Combo WAV 0.4b upd3**; former #188 Cab modular contracts/QA are folded into #198/#201/#204, with no separate Cabinet roadmap. Don't rewrite host core just to draw pedals. Base [MR_RIG_ARCHITECTURE](MR_RIG_ARCHITECTURE.md), [MR_RIG_LIBRARY_PRESETS](MR_RIG_LIBRARY_PRESETS.md).


**Implemented local P3 / #201 (2026-10-15):** `Input -> PRE -> one WAV core / Mix -> POST -> Output`, up to eight Gain/HP/LP/Bell EQ modules per lane, using existing native DSP. For this fixed zero-latency utility catalog, two banks are preallocated/prepared with the shared graph; structural edits publish a coherent numeric snapshot through the existing host queue, with a 10ms PRE/POST transition around one core. No per-edit DSP object preparation/allocation occurs in RT. Old filter histories fade out; active IIR tail bounds depend on frequency/Q/EQ gain and are separate from latency/PDC. This audited strategy replaces a per-edit general graph compilation for this subset only. Future delay/reverb/NAM/latency-bearing modules still require a safe preparation/lifetime contract. #201 accepted/closed; #202 Compressor+Drive first subset accepted, remaining pedals open; #203 not started. [Details and measured limits](MR_RIG_CHAINS_201.md).

## 1. Processing modes (exclusive active core)

```text
Input DI → Input Trim / real input meter
  → PRE FX [0..N ordered DSP modules]
  → RIG CORE:
     WAV IR / Full Combo: existing MR Cab convolution once (no extra Cab)
     WAV IR / Cabinet-only: existing MR Cab convolution (amp modeling outside Rig
                            or future *real* amp engine; clearly labeled)
     NAM / Full Rig: neural inference, cabinet baked (no extra Cab)
     NAM / Amp Only: neural inference → optional *one* CAB convolution
     NAM / Unknown: UNCLASSIFIED/needs explicit routing choice before Cab addition
  → POST FX [0..N ordered DSP modules]
  → Output Trim / Meter → host insert output
```

**Must avoid** duplicate cab, room, preamp or signal branch when Full Combo/Full Rig selected. The user can knowingly place other native/third-party FX before/after the whole MR Rig in host mixer, but Rig must **not silently bypass external plugin inserts** or change SHARED host routing.

**One host insert vs many:** preferentially one native `MR Rig` container insert owning ordered DSP modules and adapting to existing SHARED processing graph, rather than injecting a separate visible DAW-track insert per pedal. Container may reuse independent DSP processors/classes inside as child nodes **without** opening a second device, second track or nested project history owner. P0 audit must confirm feasible integration; if existing host owns every processor independently, bridge via registered internal node contracts safely rather than inventing a parallel engine. Do not assume arbitrary VST3 pedals hosted inside container; **MVP native-only**. Alternative arrangement needs explicit user approval.

## 2. Pedal position contracts

PRE FX (left): signal conditioning before current amplifier/combo source; preferred examples:
- `Gate` / `Compressor` / `EQ` / `Drive` (Drive before model only if actual DSP exists).
POST FX (right): modulation and ambience after core; examples:
- `EQ` / `Chorus` / `Delay` / `Reverb`, and if appropriate `Tremolo` future.

Position **is user-editable per rig** within compatible slots; default categories guide, don't forbid musically valid move. `Pre`/`Post` have explicit ordered list, enable/bypass/preset/drag/drop/add/remove/replace; when moved across core (if permitted), host command must update graph safely and Undo. No fake pedal module showing a functional icon unless real DSP is wired; grey-out/Coming Later for unimplemented module.

Suggested first DSP cut: **native Compressor, simple Drive, EQ** for Pre, **Chorus, Delay, Reverb** for Post, with smaller initial choice at P4/user approval to avoid scope explosion. Existing MR Reverb/EQ/Saturator processors may share **DSP implementation only after local audit** (and avoid duplicate reverb tails/summed routing). UI panels reuse Theme/UI Kit eventually; no separately instantiated VST3 GUI or standalone ASIO audio device for pedals.

## 3. DSP Module API draft (requires local audit)

```text
ModuleDescriptor { typeId, version, displayName, prePostCapabilities,
                   channelLayouts, realtimeSafe, latencyPolicy, parameters[] }
ModuleInstance { instanceId, typeId, bypass, parameterState, loadStatus }
RigGraphPlan { inputLayout, preChain[], activeCore, optionalCab, postChain[],
               outputLayout, reportedLatencySamples, generationId }
```

P0 must inspect current existing Host native processor registration, command/event flow, sample rate/preparation, ownership, project state, performance instrumentation and decide minimal adapters. New parameters use central host metadata/Undo/automation mechanisms, stable module instance IDs, lifecycle cancellation, serializable versioned descriptors. No DSP state hidden only inside an editor. Real power/bypass off avoids expensive neural processing where safe; when leaving bypass consider release tails and fade policy of time-based effects.

## 4. Realtime / performance contract

- All new IR/NAM/pedal load/alloc/decode/prepare/resample/model warm-up/graph compilation **non-RT**, with atomic/safe generation handoff at defined audio block boundaries; no mutex, allocation/free, GUI/widget access, disk/network I/O in audio callback. Crossfade or discontinuity handling bounded by CPU/RAM; do not run two heavy NAM engines concurrently without measured budget and a fallback strategy (defer/safe bypass for unsupported transitions).
- Stable plugin states during editor close, project loading, missing assets, reorder, transport Play, changing ASIO sample rate/buffer. No double-audio dry path because bypass/parallel routing; wet/dry explicit for fixed WAV/full combo. No unintended doubling of room (ready-mix IR + another auto-room).
- Latency = **measured algorithmic latency** across active modules + core, correctly reported to host PDC. IR duration (200/500ms) **not** automatic processing latency. Time-based effects Tail policy (delay/reverb) must survive a preset change appropriately or safely fade, without RT memory growth.
- CPU bench by core type + number/complexity of pedals, 1/2/4 rigs at 128/256/512/1024 frames; differentiate Studio mixing with typically higher buffer and recording/Live low latency. 96k vs 48k optional benchmarking; no claims lighter NAM until actual core/architecture measured.
- NAM backend: model format validation and supported architectures require test on actual licensed/open models and runtime; compare A2 Full/Lite only where supported; load/executable compatibility check (Windows/JUCE build) and licensing, no online model training included. Disabled when unsupported, never silently fall back from NAM to wrong WAV.

## 5. Undo/presets/audio-path invariants

`Add/Remove/Reorder/MoveAcrossCore` structural operations commit through host commands, serializable stable IDs, one transaction per gesture, Undo/Redo consistent even while playing. `parameterChange` continuously preview where supported, commit final as one undoable gesture, no callback patch recreation for each drag step. Rebuild/prepare chain asynchronously for structural changes, preserve current audio when pending and show status.

RigPreset includes chain order, module instance/bypass params, selected model/asset, core mode, optional amp-only Cab, gains/output, active image metadata in library. When unrecognized module type/version, preserve unknown state as placeholder (do not delete). Missing source leaves Rig saved state and edits intact. Legacy MR Cab preset compatibility strategy after local P0 and user approval; #185 and #196 are accepted/closed; no independent Cab stages remain.

## 6. QA before acceptance

- Same DRY test stream, output spectrum/level parity against current MR Cab when **no pedals** and selected WAV core; zero secret gain change.
- 0/1/N pedals, PRE/POST reorder including moving across Core, per-pedal bypass, remove/restore, SaveOpen/Undo/Redo, missing/relink/unsupported, multi-instance/project reopen.
- Listening A/B stable/level-matched; CPU max callback & xrun under ASIO, NO real ASIO claims unless run; 100/150/200 GUI snapshots plus physical mixed-DPI manual.
- Full current CTest once per user stage candidate plus affected GUI smoke; targeted tests during development. No commercial source contents or images committed to repo without rights.


---

## 7. Neural amp runtime policy — 2026-10-17 (уточнение #203)

Plan only: [#223 baseline/backend](https://github.com/vladleng/MR-Studio/issues/223) → [#224 native Direct engine](https://github.com/vladleng/MR-Studio/issues/224) → [#225 targeted optimization/Live QA](https://github.com/vladleng/MR-Studio/issues/225). These are **logical issues**, not yet native-attached sub-issues. Preserve accepted #201 shared graph + #202 Compressor/Drive; avoid new routing topology.

**Direct for Live:** ASIO 128 / 48 kHz as user baseline, zero **additional scheduling-buffer latency**; do not confuse this with zero hardware I/O or intrinsic NAM model latency. No worker waits, malloc/free, locks, file I/O or model warm-up in callback. Preload/prepare/validate and sample-rate **converter state** off-thread; streaming resampling still occurs safely per real-time audio block when needed. Atomic generation handoff + deferred non-RT destruction; bounded transitions, no unbudgeted dual-model inference. Mono DI processing should not duplicate heavy neural work on L/R before stereo FX. Input Trim (before nonlinear NAM) and Output Trim (after NAM) distinct, with repeatable calibration. Only one active core; Full Rig NAM includes speaker/mic and no extra cabinet, Amp Only permits at most one existing Cabinet IR; Unknown needs explicit decision.

**Optimization:** first compare pinned NeuralAudio and official NAM Core with identical legal .nam models/headless stimuli and quality; then test SIMD/AVX2 fallback, WaveNet/LSTM/multiframe/A2 controls *where supported*, recording p99/max callback and real ASIO xruns. Wrapper removal alone has no promised CPU benefit. **Buffered** (typically one additional 128-sample block = 2.67 ms at 48k) and SHARED ahead for known compatible playback are optional Studio paths, **never ahead processing of live DI**. Breaking a host block into 32/48/64 chunks does not by itself prove 0.67/1/1.33ms asynchronous latency; treat sub-block as optional feasibility research after Direct baseline. PDC aligns offline/playback but does not remove monitoring I/O delay.

**Acceptance** under [#204](https://github.com/vladleng/MR-Studio/issues/204): full live set with Omnisphere + XVox Pro + backing + live guitar + recording, 30–60 min, 48k/128, actual dropout/xrun/deadline evidence. External NeuralRack host-meter observations (~13–14% Direct, ~0% Buffered display +2.67ms) are *not* MR Studio benchmarks; CPU still executes on worker.
