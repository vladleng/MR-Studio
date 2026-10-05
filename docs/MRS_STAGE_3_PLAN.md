# MRS Stage 3 — Plugins / Native DSP sub-stages

Parent: #23, roadmap #12. Split requested by the user on 2026-10-04.
Each slice gets a separate local build, automated validation and user acceptance.
Code/builds remain local; GitHub issues/docs only. No code push/PR/merge/Actions.
Last accepted UI baseline: local 0.1p fix3 including cosmetic follow-up; #22 Mixer / Routing is completed.

| Slice | Scope | Acceptance |
| --- | --- | --- |
| 3a / 0.1k (accepted) | Native insert chains on track/bus/Master; add/remove/reorder/bypass; Gain, high/low-pass, one-band parametric EQ; parameters, Undo/persistence. Also pixel track scroll and Space Play/Stop. | Saved processing chain works on playback and monitoring; raw capture unaffected; local tests and user check. |
| 3b / 0.1l (accepted) | VST3 scan/cache, load/bypass/unload, editor and parameters, plugin state, latency reporting. Define scanning/runtime crash and isolation strategy before implementation. | Actual plugin scans, opens, processes, saves and reopens; bad/unavailable plugins handled according to defined strategy. |
| 3c / 0.1m (local ready, acceptance pending) | Cab IR WAV import, embedded mono/stereo convolution, mix/gain, low/high cut, polarity and presets; vendor VST3 sidebar/drag-drop/single-click editor. | Correct impulse response, measured declared latency, saved/reopened IR chain and physical playback check. |
| 3d | Amp/preamp/pedal foundation and preparation for neural/model player; licensed content boundary. | First guitar-processing chain with repeatable parameters/state, agreed model integration and user audition. |
| 3e | Processing reliability: latency compensation, parallel routes, chain/patch switching and load tests; harden isolation/recovery. | Documented compensation and switching behavior; measured tests on the intended system. Deferred #16 is not implicitly passed. |

3a was accepted by the user on 2026-10-04. 3b / 0.1l was accepted by the user on 2026-10-04; it also includes live graphical Channel EQ and handle-only
relative gain/pan gestures. 3c / 0.1m is ready locally and awaits user acceptance. 3d is deferred by the user until the DAW foundation is ready; 3e starts locally with 3e1 / 0.1q static PDC.
3c includes owned synthetic WAVs; Celestion is obtained separately by user email subscription.
See CAB_IR.md and MRS_STAGE_3C_CHECKLIST.md. Future version letters will be assigned when each slice starts.
VST3 support is not part of 0.1k. Stage 3 stays open until all agreed slices are accepted.
The same shared processor graph serves Arrange/Mix and future Live patches.

## 3e started — 2026-10-05
User authorized Processing Reliability and deferred own effects/instruments/Amp-Preamp until the DAW foundation is ready.
Deliver 3e in reviewable local slices:
- 3e1 / 0.1q: static audio PDC, parallel paths/tracks/buses/sends/Master/direct output alignment and bounded RT storage.
- 3e2: safe chain/state transitions, latency changes and explicit switching behavior.
- 3e3: failure recovery/isolation boundaries and intended-system load validation.
See [processing reliability](PROCESSING_RELIABILITY.md). Whole 3e/#23 and deferred #16 remain open until their acceptance gates pass; 3e1 does not claim crash isolation or seamless live patch switching. Code/builds local; GitHub issues/docs only.

## Universal engine performance sequence — 2026-10-05

User authorized general shared-engine performance work; no per-project/vendor
scheduling shortcuts. TH-U and Xvox are on separate channels, One on Master.
0.1q fix3 slightly improved the user's playback; clicks are not confirmed resolved.

1. 3e-P1 / [#51](https://github.com/vladleng/MR-Studio/issues/51): parallel independent channels, dependency scheduler and bounded precreated workers.
2. 3e-P2 / [#52](https://github.com/vladleng/MR-Studio/issues/52): anticipative playback, separate process/device buffers and correct invalidation/timing.
3. 3e-P3 / [#53](https://github.com/vladleng/MR-Studio/issues/53): separate live monitoring path, explicit shared-route/PDC/latency policy.
4. 3e-P4 / [#54](https://github.com/vladleng/MR-Studio/issues/54): channel/plugin profiling and reproducible sustained-load matrix; minimal measurements begin in P1.

All four are planned; implementation/build versions not started or assigned.
Existing 3e2 transitions and 3e3 recovery/isolation remain in scope; this sequence
does not close them, whole #23 or deferred #16. Own effects/instruments remain
deferred. See [architecture and acceptance](ENGINE_PERFORMANCE_PLAN.md).