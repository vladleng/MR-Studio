# MRS Stage 3 — Plugins / Native DSP sub-stages

Parent: #23, roadmap #12. Split requested by the user on 2026-10-04.
Each slice gets a separate local build, automated validation and user acceptance.
Code/builds remain local; GitHub issues/docs only. No code push/PR/merge/Actions.
Last accepted baseline: local 0.1k; #22 Mixer / Routing is completed.

| Slice | Scope | Acceptance |
| --- | --- | --- |
| 3a / 0.1k (accepted) | Native insert chains on track/bus/Master; add/remove/reorder/bypass; Gain, high/low-pass, one-band parametric EQ; parameters, Undo/persistence. Also pixel track scroll and Space Play/Stop. | Saved processing chain works on playback and monitoring; raw capture unaffected; local tests and user check. |
| 3b / 0.1l (local ready, acceptance pending) | VST3 scan/cache, load/bypass/unload, editor and parameters, plugin state, latency reporting. Define scanning/runtime crash and isolation strategy before implementation. | Actual plugin scans, opens, processes, saves and reopens; bad/unavailable plugins handled according to defined strategy. |
| 3c | Cab IR WAV import, mono/stereo convolution, mix/gain, low/high cut, phase/polarity and presets. | Correct impulse response, measured declared latency, saved/reopened IR chain and physical playback check. |
| 3d | Amp/preamp/pedal foundation and preparation for neural/model player; licensed content boundary. | First guitar-processing chain with repeatable parameters/state, agreed model integration and user audition. |
| 3e | Processing reliability: latency compensation, parallel routes, chain/patch switching and load tests; harden isolation/recovery. | Documented compensation and switching behavior; measured tests on the intended system. Deferred #16 is not implicitly passed. |

3a was accepted by the user on 2026-10-04. 3b / 0.1l is implemented locally and
awaits user acceptance; it also includes live graphical Channel EQ and handle-only
relative gain/pan gestures. 3c–3e are planned, not started. Future version letters will be assigned when each slice starts.
VST3 support is not part of 0.1k. Stage 3 stays open until all agreed slices are accepted.
The same shared processor graph serves Arrange/Mix and future Live patches.
