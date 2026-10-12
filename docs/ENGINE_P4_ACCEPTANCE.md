# P4 acceptance — #54 / #113 / #114

Updated2026-10-09, Asia/Krasnoyarsk. P4 acceptance recorded at local build0.2g upd10,
source15afe65; branch `mrs/0.2g-fix1-editor-space-local`.

## Historical accepted software evidence

Initial0.1u profiling slice accepted2026-10-06: channel/processor/worker profiler,
GUI snapshots/CSV, callback/deadline/ahead metrics;110/110 local CTest PASS.
Synthetic64/128/256/512 and two60-second paced sessions do not constitute
physical ASIO measurements. See ENGINE_PROFILING.md and
ENGINE_PROFILING_BENCHMARK.md for implementation/methodology.

Historical user CSV: Komplete Audio ASIO,48k/Device128,Workers4/Process1024,
ONE bypass,PDC0; Late/XR/D/worker timeouts0, callbackp99=32%,43 Ahead underruns
and43 invalidations. No audible clicks reported in that session; a pause in
material did not establish counter causation. These are historical settings,
not assumed settings for the latest user report.

## User outcome and triage —2026-10-09

User: «#113 сбоев не обнаружено. #114 щелчки появляются только при высокой
нагрузке, я считаю в пределах нормы».

- #113 closed completed: accepted no-failure result from the user's use.
- #114 closed not_planned after user triage: clicks at high load are within the
  user's tolerance; further diagnosis not performed/planned under accepted scope.
- #54 closed completed: accepted #113 outcome and #114 triage satisfy closure.

No new settings/buffer matrix, CPU/driver versions, duration, CSV or high-load
threshold supplied. Do not mark the detailed measurement checklist as measured.
The cause of historical43+43 counters and their relationship to high-load clicks
remain unestablished. No counters changed, reset or hidden; no diagnosis/bugfix
or universal no-click/throughput/Fender parity/round-trip latency claim.
Revisit on a new problem, reproducible evidence or explicit user request.

## Local handoff

No code/build/test or package change for this acceptance record. Keep immutable
upd10 package/SHA256 and all earlier packages. #16 and #53 already closed;
parent Stage3/#23, #112 and3e2/3e3 remain separate. Stage5 had not started at that acceptance; current status: [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md).
Acceptance originally recorded locally; now synchronized as documentation only. No source push/merge/Actions.

## Historical profiling evidence and proposed measurement scope

The following pre-acceptance plan is retained for methodology. Its unchecked hardware
items were not measured by the acceptance; #54/#113 are closed, #114 triaged not_planned.
#112 is an independent future capability, not a requirement to reopen P4.

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

## 3. Historical proposed checks (not a current open gate)

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

## 5. Historical proposed completion checklist

#54 may be closed **only after** the remaining agreed ASIO/stress validation is accepted and the ahead queue observation is triaged/documented. Do not require #112 implementation for P4 closure. Preserve accepted 0.1u state; never retroactively downgrade it because broader testing remains open.

All code/configure/build/tests/packages stay local; GitHub used for docs/issues with [skip ci]. New implementation requires an explicit user request.
