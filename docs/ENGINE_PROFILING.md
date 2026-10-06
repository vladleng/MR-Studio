# 0.1u / P4: engine profiling and load validation

Issue: #54; P3/#53 accepted by the user on 2026-10-06. This is the first
software delivery of P4. Hardware sustained-load acceptance is still open.
Code, builds, tests and packages are local; GitHub gets docs/issues only.

## Controls and interpretation

Transport -> Engine profiling... opens a read-only snapshot panel. Enable timing
diagnostics explicitly; it is OFF by default and adds measurement overhead.
Closing the panel disables collection and its refresh timer. Graph/device/project
changes retire the panel through the existing editor teardown. This is temporary
session UI state: no project schema, save or Undo/Redo entry is added.

The panel refreshes at 2 Hz; Export CSV saves a captured UI snapshot. It shows:
- actual callback buffer bounds, Workers, Late, XR and disk misses;
- Process Buffer, buffered anticipation frames, underruns, invalidations,
  producer maximum and worker timeouts;
- device/producer channel job mean/max/calls and worker job totals;
- actual processor process() mean/max/calls, including native and VST3 inserts;
- Master/shared inserts and a dependency cost estimate for each execution domain;
- callback deadline-load histogram percentiles, callback maximum and fixed PDC.

CPU remains callback wall time / device deadline. The existing histogram uses
1 percentage-point bins capped at 100%; it is not a precise microsecond percentile
or a process-wide CPU meter. Bench CSVs compute exact sample quantiles in us.
Summed job wall durations overlap across workers and include preemption; they are
not summed thread CPU consumption. Worker 0 is the submitting audio thread;
1..7 identify helpers. Worker rows count channel jobs, not entire callbacks.

The dependency estimate sums job costs along prepared track/output/send edges,
takes the longest path for each chunk, and adds Master/shared insert duration.
It excludes voice/source decoding, mixer reduction, scheduler/queue waits and
other callback work. It identifies expensive dependency chains but does not
claim to measure the complete critical-path wall time. Processor rows time DSP
process() only, excluding surrounding host automation, copying and sanitization.
Bypassed processors have no new DSP samples. Separate channel and insert totals
must not be added: insert time is already inside its channel/Master job.

Counters are cumulative while enabled and freeze when disabled. Engine counters
reset during stopped prepare; processor counters belong to each PreparedGraph
and reset when that object is recreated. A reused graph retains its old counters.
Each field is an independent atomic read: concurrent reports are approximate
snapshots, not coherent transactions. Disabling during a callback may leave one
in-flight sample; comparisons should use a stopped, freshly prepared fixture.

## Realtime boundary

Fixed storage: 128 channels x two domains, 8 worker rows per domain, Master/path
rows, and 32 timing cells per prepared processor graph. Storage is allocated only
on control-side preparation. Each DSP owner writes its own cell; relaxed 64-bit
atomics publish cumulative totals. Mixed producer/device share the storage but
write different domain cells. No profiler log, file write, UI call, new host
allocation or mutex is performed by an audio worker. UI report formatting and
CSV I/O occur on the message thread. steady_clock sampling and atomic updates
have a cost; detailed diagnostics are optional, not an optimization of DSP.

Exact PCM/all-thread allocation and concurrent snapshot regressions cover
profiling on/off. Existing P1/P2/P3 ownership, PDC, transport/automation, streaming,
raw capture, teardown and VST3 fixture suites remain required.

## Reproducible fixtures

mrs_channel_bench: 100 warmup + 400 measured callbacks per case; independent
2/8 tracks, serial chain of 8, fan-in bus, fan-out sends, heavy Master and tiny
2-track workload. Buffers 64/128/256/512; Workers 1/2/4/8; 48 kHz. Runs with and
without --profile expose instrumentation overhead on the same implementation.
Checksums must match across workers; exact equivalence is covered by regressions.

mrs_ahead_bench --all-workers: 40 warmup + 120 measured paced callbacks; seven
playback/mixed/disk/Master topologies, four device buffers, Process Off/256/1024/
4096 where >= device, Workers 1/4. Synthetic Work processors preserve state and
use deterministic fixtures. The P4 memory and streaming fixtures use the same
long float WAV; they differ from historical P3 sine fixtures. Historical timing
percentages cannot be compared as identical inputs. No installed plugin is used
by these synthetic tools. Helpers retain the existing MMCSS policy.

--seconds N --record selects one sustained case and verifies every raw captured
sample and frame count, independent of processed playback. A high-resolution
Windows waitable timer paces callbacks; this is not an ASIO driver. Report the
observed maximum callback gap and burst count as scheduler evidence. PCM is
compared to the serial reference only until the first producer underrun, since
a history rebase can change later stateful tails; pcm_checked exposes that limit.
Underruns and deadline misses are retained in reports rather than filtered out.

Automation/seek/loop, mix updates, reconnect/state and capture transition checks
remain deterministic regression coverage; they are not a claim of sustained
hardware automation/reconnect testing. Installed TH-U/Nuro state/editor checks
are already accepted and are not repeated routinely.

## Hardware acceptance still required

For ASIO 64/128/256/512 where supported, record actual CPU/core/thread topology,
driver/device/version, sample rate, Workers/Process, plugin versions and presets,
project topology, warmup and wall duration. Compare profiling OFF/ON and serial/
parallel/eligible anticipation. Keep TH-U and Xvox on different channels, ONE on
Master as one representative topology; no vendor or project scheduler rules.

Run sustained playback, live monitoring and raw recording, including long WAVs,
automation, transport/loop/seek, stop/reconnect and stopped state changes. Record
p50/p95/p99/max, Late/XR/D, ahead underruns, worker balance, PCM/timing checks and
any audible behavior. Keep driver I/O, fixed PDC and estimated Mon separate.
Physical ASIO, acoustic latency, installed-effect throughput and mixed-DPI/native
window transitions are not established by synthetic PASS. #54 remains open for
that acceptance; broader #16 and 3e2/3e3 recovery/isolation remain separate.

[Local measurements](ENGINE_PROFILING_BENCHMARK.md) and package provenance are in the delivery VALIDATION.md and
CSV files; no universal speedup, Fender-equivalent efficiency or no-click guarantee
is inferred from these fixtures.
