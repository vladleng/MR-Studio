# Moon River Studio — Time Stretch / Pitch Shift

Status: **planned; implementation and audio/performance acceptance not completed**.
Contract revision: 2026-10-22. Tracking: #56, native child issues #57–#62.
This document specifies future behavior; it does not authorize starting implementation.

## 1. Decision and scope

Use **Signalsmith Stretch** (MIT) as the first candidate backend behind a backend-neutral SHARED contract. Selection as the default shipping backend requires #62 evidence. No reason to replace the candidate has been established by the documentation audit.

Arrange/Edit/Mix/Live and future Library tempo preview use one engine/service architecture. Each processing stream has its own prepared state and exactly one processing owner; shared architecture does not mean sharing mutable DSP history between clips.

MVP: explicit fixed duration ratio, pitch, optional formants, realtime clip playback, offline render, clip Commands/Undo/save/load. Dynamic ratio/pitch hand-off belongs to the engine contract; a full automation editor, automatic tempo-follow, transient detection, warp markers, Native Pitch Editor and destructive rewriting are outside this first integration. #72 is an idea record, not evidence of implementation.

## 2. Architecture and backend boundary

```text
Clip / Timeline / Transport / Render / future Library preview
                         |
                 ITimeStretchEngine
                         |
             Signalsmith adapter / future backends
```

No vendor types, preset names or headers in Project Model, Transport, serialization or UI. Persist user intent; resolve quality intent to adapter settings. Cache records backend/version/configuration separately. A mock backend must exercise the contract without Signalsmith headers.

Use existing Commands, media ownership, read-ahead, Process Buffer, routing/PDC and raw recording invariants. See [ENGINE_PERFORMANCE_PLAN.md](ENGINE_PERFORMANCE_PLAN.md), [PROCESSING_RELIABILITY.md](PROCESSING_RELIABILITY.md) and [AUDIO_ENGINE.md](AUDIO_ENGINE.md). No second audio device, workspace engine or independent stretch scheduler.

## 3. Ratio, positions and process contract — #57

- Define duration ratio `r = outputDuration / inputDuration`: r=2 doubles duration; r=0.5 halves it. Playback speed is 1/r. Pitch is independent: semitones plus cents, frequency factor 2^(semitones/12 + cents/1200).
- Source and output sample positions use explicit domains and sample rates. The stretch path operates at the prepared project rate; source-rate conversion uses the existing SRC path, not pitch/time stretch.
- For a fixed-ratio source span of N frames at the processing rate, logical output length is `floor(N*r + 0.5)` frames (nonnegative nearest rounding, half up). Preserve a fractional source-position accumulator across blocks; never independently round each block's input demand. Final duration error versus the unrounded target is at most 0.5 frame; no accumulated block drift.
- Split/trim derive positions from one absolute source/output mapping and shared rounded boundaries, not independently rounded piece lengths. A variable-ratio path integrates its timestamped mapping continuously; final length is derived from the accumulated mapping. Test different block partitions of the same automation schedule.
- Configure sample rate, channels/layout, quality intent, finite parameter ranges, maximum input/output frames, queue/memory limits and supported capabilities off RT. The benchmark focus 0.75–1.5 is not a hard user limit. #58 must declare tested finite supported bounds; unsupported values, zero/negative ratio, NaN/Inf and overflow reject deterministically, without silent clamping.
- Process uses separate, preallocated non-aliasing input/output buffers. Return consumed/produced frame counts and explicit statuses: progress, need-input, output-full, drained, invalid-parameter, unsupported, not-prepared/error (names are illustrative). No hidden growth, unbounded retry or exception across callback.
- Specify prepare/configure → priming → processing → draining → drained; reset/seek invalidates the previous generation. Zero-frame/short-block calls, input starvation and end-of-input must have bounded, testable behavior. RT methods operate only on prepared state; expensive seek preparation is off RT.
- Capabilities distinguish realtime/offline, channel layouts, formants and dynamic parameters. Unavailable backend/capability preserves serialized intent and reports unavailability; never silently rewrites the project or substitutes different timing.

## 4. Timing, latency, automation and transitions — #57/#59

Keep source-read time, processing time and audible output time distinct. Signalsmith reports input look-ahead and output latency separately. For automation, parameters correspond to processing time; the adapter supplies input ahead by inputLatency and receives output behind by outputLatency in their respective sample domains. Map input look-ahead through the source/output mapping; do not blindly add input and output sample counts or assume one constant ratio under automation.

#57 must document a sample-domain timing diagram and #59 must verify it with impulses, timestamped parameter steps and two routes of unequal latency. Distinguish:
- source prefetch/priming, which prerecorded playback can supply ahead;
- residual route delay actually requiring PDC;
- device/driver latency, reported separately.

Do not compensate latency twice through pre-roll and PDC. Automation timestamps survive Process Buffer anticipation; define the bounded smoothing/ramp policy and test its audible timing, not only the parameter value.

Seek/start/loop invalidate stale work by generation, reset history and prime from available source context. Signalsmith recommends seek context of at least block + interval; missing context at source boundaries is zero-padded. Repeated seeks, loops shorter than analysis windows, stop/restart and device/rate changes must not leak old audio or corrupt state. #58 records the exact pinned API priming/flush procedure.

Prepare replacement state off RT, publish at a safe boundary and retire old state off RT (including deallocation). No callback waits for workers. Until a safe active latency-transition mechanism is proven, changes to quality/split mode/other latency-bearing configuration require Pause/Stop and graph re-preparation under the existing PDC policy. Same-latency parameter changes use the prepared hand-off path.

## 5. Realtime integration and optimization — #59

No allocation/free, blocking locks, disk/network/UI work, configuration, logging or unbounded loops in callback or its RT workers. Bound buffer capacities, work per callback, active voices and memory during preparation. Reject unsupported resource demands before activation.

Integrate eligible prerecorded playback with existing anticipative Process Buffer/read-ahead. A live-input dependency cannot be moved into anticipation blindly. Preserve raw recording before effects/compensation. One owner advances each DSP state; no concurrent retry on another worker.

Empty/stale queue output follows the existing bounded underrun policy, increments diagnostics and cannot play a previous generation or silently switch to unstretched timing. Define recovery/priming and maximum transition work before acceptance.

Optimization candidates, not established speedups:
1. Neutral-state fast path: r=1, pitch=0, no formant modification; preserve source PCM with no unnecessary spectral processing. Enter/leave only with latency alignment and a tested click-free transition; stopped reprepare is acceptable for MVP latency changes.
2. Reuse anticipative playback and cache instead of adding another worker pool.
3. Compare split computation off/on: upstream adds one output interval of latency. Choose by callback peaks AND latency, not average CPU alone.
4. Profile Release before introducing FFT/SIMD dependencies. Benchmark CPU, memory and clip-count scaling on identical fixtures.

Initial physical realtime target: 48 kHz / 128 frames; measure 64 where supported. 256/512-frame Studio mixing and Process Buffer configurations are separate cases, not failures of the 128-frame Live/recording policy.

## 6. Offline render, tails and cache — #60

Realtime and offline share the adapter contract, with different buffering policies. Offline rendering streams long files with bounded memory, deterministic mapping and worker cancellation; no file I/O on UI/audio threads.

Separate three quantities: logical clip duration, startup/pre-roll latency, and DSP drain/tail. Feed the input-latency silence required by the pinned backend at end-of-input, then drain/flush. Discard documented pre-roll and return exactly the logical frame count for the clip render. Drain fully even if samples outside the logical range are discarded. An explicit tail-inclusive export may return a separate tail extent; it must not silently lengthen the timeline clip. Silence padding and end rounding must be deterministic.

Cache identity includes:
- source content identity/revision, selected source range and boundary/loop context;
- ratio/pitch/formant intent including base-frequency policy and, when supported, automation schedule plus relevant tempo-map revision;
- processing rate, channel layout, quality intent and actual resolved configuration;
- backend and transitive DSP revisions, adapter/render-contract version and output format.

Use bounded memory/disk budgets, eviction without deleting in-use entries, job deduplication and generation checks after relink/parameter/project changes. Write temporary results and atomically publish only complete validated entries; failure/cancel leaves no valid-looking partial result. Backend/configuration/source changes invalidate incompatible results.

Tests: first/last impulses, empty/very short/long clips, arbitrary block sizes, source offsets, loops, repeated render, cancel/retry, concurrent identical requests, relink and project close. Report determinism tolerance for a fixed build/backend; cross-platform bit identity is not assumed. #62 defines the reproducibility gate.

## 7. Clip state and editing — #61

Persist enabled/mode, duration ratio, pitch, formant policy/value, optional base-frequency hint and backend-neutral quality intent. Older projects load disabled with neutral values; unsupported future state must be retained or rejected explicitly, not silently discarded. Version the schema only when implementing the state; preserve source media and Undo/Redo.

MVP editing rules:
- Ratio change anchors the clip start; end follows the selected source span and ratio. No ripple or movement of neighboring clips. Existing overlap rules apply.
- Trim changes the selected source span through the mapping. Split preserves pitch/ratio and the shared source mapping/boundary rounding so adjacent parts do not gain a gap or overlap through rounding; prepare context around the split without rewriting the source.
- Repeated loop iterations use the same defined logical loop extent and mapping; short-loop priming and boundaries require regression/listening checks.
- Bypass keeps clip start/source selection, restores original selected-source duration and unprocessed pitch/formants, and retains settings for re-enable. Reset explicitly clears settings. Neither action silently ripples neighboring clips.
- Unavailable backend: preserve requested geometry and state, visibly report unavailable processing, silence the affected playback and reject processed export with an actionable error. An explicit user bypass is available; no automatic wrong-speed fallback.

Verify Commands/Undo/Redo, Save/Open/Save As, trim/split/loop/bypass/reset and missing backend. GUI uses existing MR UDS controls; no vendor-specific UI.

## 8. Future tempo-sync and Library boundary

Manual ratio is the MVP; automatic tempo-sync is a separate follow-up after #57–#62 acceptance, not a promised feature of #61. Existing tempo/meter editing keeps its current no-audio-stretch behavior.

For a future constant-tempo loop, r = sourceBPM / targetBPM. Unknown/unreliable BPM disables automatic sync and offers explicit user entry; never silently invent BPM. Half/double-time interpretation is an explicit choice stored separately from original source metadata. Tempo changes inside a clip require a defined piecewise mapping/automation contract, not one average BPM.

Library preview and Arrange must share ratio/pitch intent and mapping. A future “sync on drop” action copies the chosen intent through the normal import Command; plain drop keeps existing behavior. Do not change source WAV/BPM metadata, introduce an independent stretcher, or alter global transport for audition.

#255/#261 delivered Library preview/import/browser, not tempo-stretch. #255 SRC preserves pitch/duration and stays separate. #235 must keep its explicit unsupported-feature report until stretch import mapping is implemented and accepted; it must not invent or bake unknown warp semantics. Native stretch plus per-clip ARA processing requires an explicit source/processing-order contract before combining them, with no double processing.

## 9. Dependency, formants and licensing — #58

Pin the exact Signalsmith Stretch revision and Signalsmith Linear/transitive dependencies; record FFT provider/build flags and reproducible offline/cache acquisition. Retain all required licenses/NOTICE and verify the exact pinned source at release. Optional FFT providers need separate license/build review and measured benefit.

Evaluate presetDefault, presetCheaper and split computation in optimized Release builds; upstream reports Debug can be much slower. Map MRS quality intent to these settings inside the adapter.

Formant compensation is optional and is not a promise of monophonic vocal-editor quality. Preserve/shift modes must define interaction with pitch and a documented base-frequency policy: optional user hint in Hz, otherwise a recorded adapter default with known limitations. No automatic pitch detector is implied. Confirm the pinned API's normalization against its implementation/tests (upstream prose and example units must not be guessed); cover vocals and non-vocal/polyphonic material. Unsupported capability is explicit.

References: [upstream README](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/main/README.md), [Signalsmith Linear](https://github.com/Signalsmith-Audio/linear). README/main is contextual; the implemented dependency lock and its source are authoritative.

## 10. Quality, performance and acceptance — #62

Two gates within #62:
- **Early feasibility**, after #58: adapter quality, duration/latency, formants, memory and Release CPU. Publish evidence before committing to full #59/#60 integration and #61 UI. This checkpoint is not closure of #62.
- **Final integration**, after #59/#60/#61: full regression, hardware and listening acceptance. Do not create a circular native dependency by making #59 depend on completion of #62.

Owned/authorized sources: drums/transients, bass, electric/acoustic guitar, vocal, keys/pads, dense stereo mix; synthetic impulses/sines/silence for objective checks. Record fixture hashes/provenance.

Minimum ratios: 0.5, 0.75, 0.9, 1, 1.1, 1.25, 1.5, 2. Pitch: 0, ±1, ±7, ±12 semitones, cents and formant combinations. Moderate 0.75–1.5 is the priority quality range; extremes need explicit limitations, not an undocumented hard clamp.

Benchmark matrix:
- 1/8/32 concurrent mono/stereo clips; include aligned starts (spectral peaks), staggered starts, short loops, seek bursts and concurrent raw recording;
- 44.1/48/96 kHz functional/offline coverage; physical 48 kHz at 128 frames, 64 if supported, 256/512 Studio, representative Process Buffer off/on;
- default/cheaper quality, split off/on, neutral fast path, cold/warm cache and memory/cache pressure;
- record CPU model, RAM, OS, driver/device, compiler/flags, exact revisions, rates/buffers, fixture hashes and other active DSP;
- performance cases: 30 s warmup + 5 min measured, three repeats; one 60 min sustained representative mixed-session test. Publish supported load envelope; 32 clips is a scaling probe, not a guaranteed hardware capacity.

Record average CPU, callback p50/p95/p99/p99.9/max, deadline overruns, xruns/underruns, input/output/residual-route latency, peak memory and cache hit/render time. At 48 kHz/128 the whole callback deadline is ~2.667 ms, not the stretcher's private budget.

Objective acceptance:
- zero observed callback allocation/free/blocking violations and invalid samples;
- exact logical frame count, no long-duration rounding drift; impulse alignment within one output frame of documented mapping after latency compensation;
- neutral bypass PCM identity; fixed-build repeated renders have normalized PCM max absolute difference ≤1e-6, or a justified tighter/alternative tolerance recorded before acceptance;
- supported physical load envelope has zero observed deadline misses/xruns during measured steady-state runs; transitions separately report underruns/recovery and must not emit stale audio, corrupt state or create unbounded work;
- no monotonic memory growth after warmup; configured queue/cache limits hold.

Listening: level-aligned A/B on the same sources/ratio/rate; record smearing/transient loss, phasiness, pitch/formant instability, stereo image changes and boundary clicks with severity and timestamps. Compare a trusted DAW/reference where available. Moderate-range unacceptable artifacts block default-backend approval; extreme-range limitations must be documented. User listening acceptance remains necessary.

If early quality fails, keep the adapter boundary and investigate configuration/alternative backends before default selection. If RT budget fails at the required representative load, profile first and offer explicit offline/freeze or a tested lower-cost mode; no silent quality downgrade. CI does not prove physical ASIO or listening quality. Report PASS/FAIL/NOT TESTED separately.

## 11. Delivery order and issue ownership

| Issue | Deliverable | Native blocked by |
|---|---|---|
| #57 | Contract, mapping, lifecycle and serializable state design | — |
| #58 | Dependency lock, adapter, formants and licenses | #57 |
| #59 | RT/anticipative integration, transitions, latency/PDC | #58 |
| #60 | Offline render, exact length, tails and cache | #58 |
| #61 | Manual clip controls, editing and persistence | #59, #60 |
| #62 | Early feasibility and final benchmark/acceptance | #58 |

#62 can start after #58; final closure additionally requires accepted #59/#60/#61 and the final gate above. #59/#60 completion requires the recorded early checkpoint, not closure of #62. Contract/state design for #61 may be prepared with #57, while functional acceptance waits for playback/render.

Implementation order: #57 → #58 → #62 early checkpoint → #59/#60 → #61 → #62 final gate. Close #56 only after child acceptance; status percentages describe native issue completion, not measured implementation progress.

## 12. Development policy

Follow current [AGENTS.md](../AGENTS.md) and [CLOUD_DEVELOPMENT.md](CLOUD_DEVELOPMENT.md): GitHub holds source/docs/issues, Codex Cloud works in a thematic branch, Windows builds and focused checks run in Actions under the supported profile. Local Windows is for user acceptance and physical ASIO/MIDI/vendor checks. Historical local-only restrictions are superseded.

Docs-only updates need diff/link/consistency review, no build. Implementation needs checks appropriate to actual risk; full CTest is not automatic. Merge to main and issue closure require the applicable user acceptance. This documentation update does not claim implementation, benchmark results or permission to merge.
