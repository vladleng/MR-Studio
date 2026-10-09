# Shared Audio P4 — profiling acceptance and remaining hardware checks

**Status:** 2026-10-09; P4 issue #54 remains open **only for verification and profiling follow-ups**, not for implementing new engine features. Historical delivery: 0.1u, accepted software portion on 2026-10-06.

## 1. What was already done and accepted

- An optional engine profiling panel: Transport → Engine profiling…, off by default, 2 Hz snapshot refresh, Export CSV; temporary session state only.
- Bounded per-channel/native-VST3-processor/worker timings and anticipation queue counters, without host audio-thread allocation or I/O.
- Callback/deadline utilization separated from summed job durations, process CPU and reported ASIO input/output latency. Dependency cost is only an estimate, **not** complete critical-path wall duration.
- Deterministic synthetic graphs (independent / serial / fan-in / fan-out / heavy Master), replay/mixed Process Buffer conditions, raw-recording checks and offline load matrix at buffers 64/128/256/512.
- Local 0.1u Windows Release build; **110/110 CTest PASS**, focused checks 2/2; packaged GUI smoke exit 0. **No sustained real-ASIO driver session was run in those synthetic tests.**
- The 2026-10-06 user accepted **this initial software/profiler slice**, requested to switch next development attention to MIDI, and continued P4 testing in parallel. This is not full P4 acceptance.

Detailed reproducible artifacts and limitations:
- [Profiler UI / contracts / test methods](ENGINE_PROFILING.md)
- [0.1u synthetic benchmarks](ENGINE_PROFILING_BENCHMARK.md)
- [Universal engine plan](ENGINE_PERFORMANCE_PLAN.md)

Software tests covered 112 channel cases with profiling OFF and 112 ON, and 210 paced cases with OFF and 210 ON; 67,200 exact paced PCM blocks compared. Two 60.107-second paced synthetic tests reported no producer/disk/worker misses. Those facts are **not** a no-click guarantee on ASIO, nor Fender Studio Pro parity.

Package: `MR-Studio-0.1u-P4-profiling-JUCE-ASIO-Windows-local` (local Builds). Source `ef11071bd221e30f822c2fd09e4689bfbf0b98ad`. EXE SHA256 `3B75C8CF1886CC983544254171D6B9499D515E7CA0500F193BE50CF874DBCBA9`. Historical package and provenance are unchanged.

## 2. User's physical ASIO CSV — observation, not diagnosis

Reported in #54 after initial acceptance:
- Komplete Audio ASIO Driver; 48 kHz; device buffer 128 frames; Workers 4; Process Buffer 1024; ONE bypass; fixed PDC 0.
- Callback p50/p95/p99 (percentage of callback deadline): **16% / 27% / 32%**; max callback 1.4593 ms vs 2.6667 ms deadline.
- Late 0, XR 0, disk misses 0, worker timeouts 0; ahead queue **43 underruns + 43 invalidations**.
- Snapshot covers 80,135 device calls / 213.6933 seconds of processed audio frames; this is not necessarily complete wall-clock session duration.
- User reported **no audible clicks or dropout**, and a small pause within multitrack material. CSV alone cannot correlate that silence to queue events.
- It is **not established** whether the queue events are expected priming/invalidation, transitions, producer starvation or a defect. No automatic corrective patch is authorized by this report.

## 3. What remains

### A. Hardware sustained-load acceptance (separate issue linked from #54)

- Collect sustained **physical ASIO** playback, input monitoring, raw record, automation/transport/loop/seek, stop/reconnect/state transitions at supported 64/128/256/512 buffers. Do not assume every driver supports every setting.
- Record sample rate, driver/interface/version, CPU/OS, actual worker count, Process Buffer, project topology, plugin versions/presets, profiling OFF/ON, warmup and test duration.
- Use both representative VST3 effects and synthetic reference cases. Example topology: TH-U + Xvox on **separate** tracks, ONE on Master. No vendor-specific scheduling.
- Log p50/p95/p99/max, Late/XR/D, worker timeouts, producer underrun/invalidations, PDC, actual monitoring latency limitations and audible results.
- Compare normal Studio Mix buffer sizes (e.g. 512/1024 when supported) and separate Studio Record low-buffer scenarios; **do not demand 128 frames for all studio mixing**.
- Only claim what was physically tested. No requirement to repeat previously accepted TH-U/Nuro editor/preset tests unless new issues arise.

### B. Ahead queue anomaly triage (separate issue linked from #54)

- Reproduce/locate the previously observed 43 underruns and 43 invalidations **without assuming they are an audible bug**.
- Correlate counters with priming, transport, pauses, editing, graph/device preparation, producer starvation and Process Buffer state; distinguish expected invalidations from unexpected underruns.
- If event categories are ambiguous, propose low-overhead diagnostic reason counters (bounded/non-RT UI export) and tests for false positives.
- Only implement a bug fix after an identified reproducible cause; preserve old history/RT safeguards.
- Attach resolved interpretation or a documented non-reproducible boundary to the acceptance report.

## 4. Explicitly NOT part of #54

- **Parallelism inside one logical VSTi**: separate SHARED capability [#112](https://github.com/vladleng/MR-Studio/issues/112), design [ENGINE_INSTRUMENT_PARALLELISM.md](ENGINE_INSTRUMENT_PARALLELISM.md). Host-level sharded instances are a new feature, not P4 profiling.
- Generic full runtime crash isolation, safe live patch migration / 3e2 / 3e3, feature-specific VST3 compatibility beyond the P4 test matrix are separate reliability scope under #23.
- #16 is **closed** in GitHub; older narrative referring to its pending broader matrix is historical and does not redefine the state of #16. Any new stress coverage is precisely defined in active P4 follow-ups.
- No auto-start of new software stages, changes to current MIDI/DAW development order, or requirement for GitHub Actions/source push.

## 5. Completion conditions for #54

#54 may be closed **only after** the remaining agreed ASIO/stress validation is accepted and the ahead queue observation is triaged/documented. Do not require #112 implementation for P4 closure. Preserve accepted 0.1u state; never retroactively downgrade it because broader testing remains open.

All code/configure/build/tests/packages stay local; GitHub used for docs/issues with [skip ci]. New implementation requires an explicit user request.
