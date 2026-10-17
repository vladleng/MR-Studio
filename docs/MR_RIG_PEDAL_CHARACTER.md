# MR Rig — Character Pedals: pedal-style algorithms and Reverb family

**Canonical specification / PLANNED, 2026-10-17.** Product owner [#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197); implementation issue parent [#202 native pedals](https://github.com/vladleng/MR-Studio/issues/202); approved first slice Compressor+Drive 0.4f stays accepted. This document covers **future** scope, not implemented DSP. Real code/build/tests are local only. No stage starts automatically.

## 1. Product purpose and sound

MR Rig pedals should behave like **musical characterful guitar pedals**, not just generic utility algorithms or graphical imitations of commercial stompboxes. Create **original** implementations with convincing musical behavior (envelope, spectral coloration, early/late diffusion, modulation, damping, resonant/drip spring textures and nonlinearities when appropriate).

Commercial products such as Strymon Flint, UA Golden Reverberator, BOSS RV-6 and Neunaber Immerse are **sonic/functional references**, not a commitment to bit-accurate recreation, licensed component models or a product with their trademarks, copied DSP, graphics, presets or factory audio. Any A/B must use authorized user-provided reference audio or legally obtained hardware/software and level-matched listening. Never claim "exact emulation" without verifiable evidence/rights.

The first character-driven project is **one native MR Pedal Reverb module**, with distinct algorithms:

| Mode | Target sound | DSP investigation |
|---|---|---|
| Vintage Plate | Smooth dense vintage-like tail, restrained tone and modulation | Efficient FDN + initial diffusion, damping and optional Early Reflections |
| Vintage Spring | Guitar spring drip, splash, resonances and dispersive decay | Separate dispersive/all-pass/waveguide-like model; plain FDN alone is insufficient |
| Small Room / Jazz Club | Near natural short space, articulate chords and believable early reflections | Short multi-tap Early Reflections + compact FDN late tail |
| Modern Ambient | Blackhole-inspired immense nonliteral space, smooth bloom, inverse/swell-like envelope and internal modulation (original sound, not clone) | Tuned FDN with structural modulation; Infinite/Freeze optional later, Shimmer outside initial scope |

No four simultaneous hidden wet engines; process **selected** algorithm only. Shared code for parameter smoothing, wet mixer, memory ownership, filtering, serialization, tails and GUI is encouraged, while Spring can use a distinct DSP topology.

## Modern Ambient primary sound benchmark — Eventide Blackhole (2026-10-17)

**User's preferred benchmark:** [Eventide Blackhole](https://www.eventideaudio.com/pedals/blackhole/). This supersedes generic ambient/Neunaber examples for [#230 05D](https://github.com/vladleng/MR-Studio/issues/230). It is an acoustic/functional reference, not authorization to copy Eventide DSP, code, presets, graphics or product identity.

Key sonic goals for an **original** Ambient mode:
- Immense unreal space, soft attack, rich harmonic bloom, very smooth tail, intelligible clean guitar attack/chords, stereo width with mono compatibility.
- Original normal-to-inverse/swell-like **perceived envelope** inspired by the expressive role of Gravity; musical interaction of decay/feedback, size and pre-delay.
- Diffusion and modulation **inside** the reverb/feedback topology, not just a chorus after the wet mix; controlled tone, gentle motion and limited metallic artifacts.
- Distinguish optional **Infinite** (new signal keeps layering) and **Freeze** (wet state held as the player performs over it). These are later opt-in slices; don't compromise stability or initial lightweight Ambient delivery.
- A true DSP-backed brand-neutral Character/Space/Swell macro can be explored; avoid unused or misleading knobs. Shimmer/pitch-shift is not required in the initial Ambient.

Reuse and benchmark the Plate FDN core where audibly viable; don't presume a generic longer Hall equals Blackhole-like Ambient. A/B with legally available reference hardware/plugin at matched levels, compare CPU p99/max callback and actual latency at 48kHz/ASIO128, and evaluate half-rate late-tail filtering only if it retains audible character. **Target is a credible original guitar pedal, never a claim of exact Blackhole replication.**

Status: planning only; no benchmark or audio implementation performed.

## 2. Host signal contract and placement

The current **NAM-first** Rig graph is `Guitar/Input → ordered PRE FX → NAM Amp → optional ONE Cabinet IR for amp_only → ordered POST FX → Output`. NAM full_rig has cabinet/mic in the capture and must not run an extra cabinet. The pedal Reverb is a **child native FX** inside existing `mrs.rig` shared graph, **not** a second host plugin or a replacement for `#128 MR Reverb` (accepted standalone convolution reverb).

- The user may deliberately position Reverb in **PRE** (before nonlinear NAM) or **POST** (after NAM/Cab, suggested default). Do not silently relocate pedals, add hidden effects or alter external host inserts.
- Presets must preserve ordered slots, pedal type/algorithm, parameters, trails state, bypass, and the NAM/Cab selections independently. One actual audio path, correct stereo/mono routing; no duplicate dry summing.
- `MR Reverb #128` remains a **separate accepted impulse-based native effect**. A short measured IR for early reflections is optional R&D only, not a default dependency or a return to convolution-only guitar reverb.
- Maintain accepted `mrs.compressor` v1 and `mrs.drive` v1 existing tone, parameter IDs and RigState snapshots exactly until explicit user-approved optional new voicings. No blanket redesign of Studio-wide GUI.

## 3. UX: Simple by default / Advanced on demand

Compact flat MR UDS / existing native Theme pedal tile, contextual inspector, **no photoreal hardware recreation**.

**Simple:** Algorithm selector `Spring | Plate | Room | Ambient`; four main controls **Decay, Tone, Mix, Character**. Character should have a *musically meaningful, algorithm-specific* mapping, documented per mode (e.g. spring drip, plate diffusion/texture, room proximity, ambient motion), not an inert knob or mandatory modulation. Defaults must be quiet-safe, guitar-useful and level-balanced.

**Advanced** (show controls only if DSP actually implements them): Pre-delay, Diffusion, Damping, Modulation, Stereo Width, Early/Late Balance, Lo/Hi Cut where useful. Do not promise all modes expose identical controls. Parameter bounds/units/defaults/automation availability established in implementation task after local audit; do not invent "amp" controls unrelated to current NAM capture.

User selects an algorithm, adjusts, saves as My Rig; there is one clear **Bypass**, optional **Trails On/Off** with defined behavior, no abrupt tail loss unless user explicitly chooses hard stop. Smooth controllable changes. Bypass must not silently create a second dry route, and wet level controls must not alter dry unity unexpectedly. PRE/POST reorder/duplicate/Undo/Redo and project restore follow established #201/#202 model/command semantics.

## 4. DSP performance and correctness

**Live reference setup:** 48 kHz, ASIO 128 frames, guitar NAM + optional Cabinet IR, Omnisphere, XVox Pro, backing playback and recording. Optimise for maximum callback/deadline headroom, not only mean CPU or host-reported percent. Studio mixing may use higher buffers and larger algorithmic quality budgets. No universal arbitrary CPU-percentage promise.

- Start with original native full-rate **FDN 8-ish delay lines** as an experimental design; compare with 12/16 only where justified by audible diffusion and reproducible CPU benchmarks. Try orthogonal low-cost feedback matrices (Householder/Hadamard where appropriate), stable damping and low-cost selective modulation. Do not preselect one as universally best.
- Spring investigates dispersive all-pass/waveguide/tank-specific resonances; prevent metallic ringing and unstable feedback. No assumption that an FDN can simply be retuned to emulate spring.
- Optional **half-rate late tail** at 48k→24k is experimental: account for resampler CPU, filtering, added group delay/PDC, changing spectral/temporal character and possible metallic artifacts. Do not promise overall 2x speedup, "0 ms algorithmic latency" or superior quality until tested; in initial Spring stage use full sample rate.
- All memory/delay buffers, model structures, coefficients and graph swaps prepared off callback. No callback alloc/free, locks, I/O, UI, waits or unsafe destruction. Handle denormals, nonfinite values, gain overflow, deterministic bypass/tail behavior, changes of sample rate/buffer, multi-instance/close/reopen.
- Report **true algorithmic latency** if introduced; pre-delay is a deliberate wet-effect control, not necessarily host-compensated plugin processing latency. A reverb can have predelay even when dry path has no additional scheduling buffer. Native PDC policy must match actual graph.
- Tests: known impulse and transients, tonal ringing, stability with silence/infinity, mono/stereo channel separation, 44.1/48/96k if supported, 64/128/256/512 frames, parameter sweeps, repeated mode changes, bypass/trails, user guitar DI A/B. Measure p95/p99/max callback, xruns on actual hardware where tested, RAM, true CPU; separate **PASS / FAIL / NOT TESTED**.

## 5. Staged implementation / Codex issues

The list is a **logical breakdown** under the existing [#202](https://github.com/vladleng/MR-Studio/issues/202), not a confirmed GitHub-native sub-issue hierarchy:

1. [#227 — 05A: references, contracts, reuse audit](https://github.com/vladleng/MR-Studio/issues/227): no implementation; adopt a shared module/state design and A/B methodology.
2. [#228 — 05B: Vintage Plate FDN MVP](https://github.com/vladleng/MR-Studio/issues/228): first playable low-overhead reverb engine; one DSP algorithm with focused testing.
3. [#229 — 05C: Vintage Spring](https://github.com/vladleng/MR-Studio/issues/229): distinct dispersive reverb model and musical drip.
4. [#230 — 05D: Small Room and Ambient](https://github.com/vladleng/MR-Studio/issues/230): efficient extension of Plate early/late FDN structure.
5. [#231 — 05E: unified pedal UI/presets/trails](https://github.com/vladleng/MR-Studio/issues/231): Simple/Advanced, named sound modes, source state, Undo/Redo/reopen, no fake controls.
6. [#232 — 05F: character expansion for Compressor/Drive and scoped Delay/Chorus contracts](https://github.com/vladleng/MR-Studio/issues/232): only optional versioned voicings after previous-sound parity; Delay/Chorus DSP becomes its **own** future issues when authorized.
7. [#233 — 05G: integrated pedal/Live QA](https://github.com/vladleng/MR-Studio/issues/233): user-performed long ASIO tests, CPU headroom, sonic references and signoff.

**Priority/dependency:** NAM [#203](https://github.com/vladleng/MR-Studio/issues/203) retains current higher integration priority per product decision 2026-10-17; pedal #05A can be researched independently. Reverb DSP can be developed with synthetic or accepted current Rig core ahead of NAM, but **Live end-to-end** 05G requires working NAM [#224](https://github.com/vladleng/MR-Studio/issues/224) / [#226](https://github.com/vladleng/MR-Studio/issues/226). After accepted 05B, 05C then 05D then 05E; 05F is optional, may be scheduled separately without blocking 05G if not part of accepted scope. [#204](https://github.com/vladleng/MR-Studio/issues/204) remains final *whole Rig* acceptance. Explicit user signoff after **each** subtask, no automatic next work.

## 6. Delivery guardrails

Follow [AGENTS.md](../AGENTS.md), issue-first source reading and targeted local checks. For every task: read only issue + needed sections of this file and existing #202 DSP contract; inspect local dirty checkout before edits. Keep all code/build/tests/packages local; GitHub Issues/docs updates only, docs commits marked `[skip ci]`, no Actions/PR/source pushes. Do not repeat broad L4 CTest/packaging on mere GUI color changes; for actual DSP candidate apply relevant realtime regression/measurement and build scope. Hardware ASIO/listening cannot be labeled PASS unless actually exercised.

**All tasks on this page are planned; no new algorithmic Reverb, Plate/Spring/Room/Ambient or character voicing has been implemented by writing this spec.**
