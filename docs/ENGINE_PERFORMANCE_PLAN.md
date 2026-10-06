# Shared audio engine performance plan

Approved direction: 2026-10-05. Parent: [Stage 3 / #23](https://github.com/vladleng/MR-Studio/issues/23).
Windows first. Develop a universal, efficient engine for Arrange/Mix and future
Live using the same processor graph. No project names, plugin brands or presets
may determine scheduling behavior. Own effects/instruments remain deferred.

## Current baseline and corrected report

In 0.1q fix3, host DSP executes sequentially in the ASIO callback: channels in
dependency order, then Master. Disk read-ahead, recording writes, waveforms and UI
already run separately; these do not provide parallel channel DSP. Plugins may
have their own internal threads, independent of host scheduling.

The user clarified that TH-U and Xvox Pro are on different channels; IK Multimedia
One is on Master. The two upstream channels can be independent, while Master must
wait for their outputs. They are not one serial insert chain. The previous offline
benchmark flattened plugins into one chain and measured call overhead only; its
approximately 11% mean reduction is not a measurement of this project's routing
or multicore scaling. User reports fix3 slightly improved the situation; complete
click resolution was unconfirmed at this baseline; P1 later removed regular clicks in the user's 128-frame audition, with rare clicks remaining.

## Sequence and tracking

| Order | Slice | Issue | Status |
|---|---|---|---|
| 1 | 3e-P1: parallel channel processing and dependency scheduler | [#51](https://github.com/vladleng/MR-Studio/issues/51) | 0.1r user-accepted at 128 frames on 2026-10-05; closed |
| 2 | 3e-P2: anticipative playback and separate process buffer | [#52](https://github.com/vladleng/MR-Studio/issues/52) | 0.1s initial native whole-graph slice accepted 2026-10-06; closed; VST3/live stay direct |
| 3 | 3e-P3: separate low-latency monitoring | [#53](https://github.com/vladleng/MR-Studio/issues/53) | 0.1t mixed renderer accepted 2026-10-06; closed |
| 4 | 3e-P4: detailed profiling and sustained-load acceptance | [#54](https://github.com/vladleng/MR-Studio/issues/54) | 0.1u software slice user-accepted 2026-10-06; #54 stays open for follow-up testing alongside MIDI |

Minimal measurement hooks accompany P1 so scheduling decisions have evidence;
full diagnostics/UI and broad acceptance belong to P4. Each slice receives local
validation, a separate package and user acceptance. Build version letters are
assigned when implementation starts. P1 is now implemented locally in 0.1r; see
[parallel processing and measured limits](ENGINE_PARALLEL_PROCESSING.md).
P2's initial whole-graph eligibility, history-reset transition policy and offline
measurements are documented in [anticipative processing](ENGINE_ANTICIPATIVE_PROCESSING.md).
Existing 3e1 PDC, 3e2 safe transitions/dynamic latency and 3e3 recovery/isolation
remain tracked; P1-P4 do not claim those outstanding gates completed. Transition
safety is a prerequisite within every performance slice, not deferred until later.

## P1: parallel channel processing

Compile a dependency plan outside callbacks from tracks, buses, pre/post sends,
Master and direct hardware routes. Precreate bounded audio workers, job metadata,
scratch and output contributions. The callback participates; retain a serial
reference/fallback and configurable worker limit. Choose Windows audio scheduling
and core policy from measurements, not hardcoded affinity or maximum thread count.

Only ready independent channels execute concurrently. Dependent buses/Master wait
for all inputs. A plugin instance is never called concurrently; its insert chain
remains ordered. Use isolated contributions and deterministic reduction instead
of concurrent writes into shared bus buffers. Preserve PDC, transport/automation
context, raw recording and accepted state/editor behavior. Apply FP mode policy
on every audio worker, not only the callback.

Quiesce workers before graph/device/state replacement. Avoid host RT allocations,
I/O, contended locks and unbounded waits/spins. Define diagnostics and overload
behavior; never retry a job serially while a worker may still mutate the same
plugin. Arbitrary hung/native-crashing plugin survival remains isolation scope.

Acceptance: serial/parallel sample equivalence with stated numerical tolerance,
dependency correctness, variable/short blocks and loops, no host RT allocations,
safe teardown/reconnect, and measured scaling at 1/2/4+ workers where supported.
Include small graphs where scheduling overhead may exceed any parallel benefit.
No guaranteed speedup for a single dependent chain or a heavy Master bottleneck.

## P2: anticipative playback

Process eligible prerecorded subgraphs ahead of their device deadlines with a
separate, bounded Process Buffer. Device Buffer and Process Buffer must be explicit
settings with documented memory, responsiveness and latency effects. Exclude live
input dependencies and unsupported real-time/plugin behavior from anticipation.

Keep correct timeline/plugin context, automation timestamps and PDC. Define
priming, tails, invalidation on Seek/Stop/loops/project/graph/parameter changes,
stale-buffer rejection, underrun/recovery and real-time fallback. Each stateful
plugin has one processing owner; speculative work cannot advance its state twice.

Acceptance: reference timing/output, short loops/variable callbacks, edits and
memory bounds, plus playback/mixed-routing benchmarks across process/device sizes.
A larger process buffer does not imply zero additional latency for every route.

## P3: low-latency monitoring

Keep live-input processing at device-sized blocks while eligible playback uses
anticipation. Compute the full live dependency closure through buses/sends/Master;
a shared path containing live input cannot blindly be processed ahead.

Define plugin ownership and merge points. Do not automatically clone stateful
plugins; any dual-instance approach needs explicit state/automation synchronization.
Specify PDC and mixed-route latency policy, high-latency effect handling and visible
monitoring latency. No silent disabling of effects, and plugin algorithmic latency
cannot be removed by threading. Monitor/Arm transitions must be safe and raw
recording must remain before effects/compensation.

Acceptance: mixed playback/live paths, sends/buses/Master/direct outputs, unequal
latency and monitoring toggles, followed by intended-system ASIO timing/audition.

### First P3 implementation step — prepared ownership plan (historical checkpoint)

`AudioEngine::prepare` now compiles a candidate `ProcessingDomains` report from
the validated DAG and its fixed PDC plan. It reserves all prepared input routes,
including disabled monitoring, and propagates live/unsupported processor reasons
downstream through main outputs and pre/post sends. Independent upstream native
channels retain candidate producer ownership even when a shared bus or Master has
device ownership. Direct hardware monitoring does not unnecessarily contaminate
the Master closure. This uses generic capability checks, never plugin brands.

The report lists producer-to-device main/send/Master/physical-output boundaries,
legacy unassigned playback and producer Master output, preserving the prepared
topological order. Each edge records the existing compensation delay; it must be
applied once by its owner, never added again at consumption. Shared plugin graph
aliases and routing cycles remain rejected. Capture is explicitly device-owned.
Monitoring compensation is the current fixed end-to-end PDC in samples, separate
from driver latency; no effect bypass or extra Process Buffer delay is implied.

This is a prepared candidate plan, not an active mixed renderer. The accepted P2
runtime eligibility/fallback stays unchanged. A capability change (external MIDI
or offset automation) requires revalidation before using this snapshot. Monitor
toggles cannot migrate plugin/PDC history; structural Arm/route changes require
the existing stop/quiesce/reprepare boundary.

Next implementation: preallocated per-boundary PCM packets with generation AND
monotonic render-sequence identity (loop sample position alone is insufficient),
isolated producer/device transport/mix scratch and immutable heard-context handoff.
The callback owns controls, live capture and shared DSP; the producer owns eligible
upstream DSP and its outgoing PDC histories. Reduce contributions in the original
channel/send order to preserve summation. Producer starvation must silence only
the missing playback contribution while live DSP/capture and device time continue;
never wait for/retry the same stateful instance on the other owner. Seek, toggles,
parameters, partial callbacks, stream cursors, meter publication, teardown and
recovery need reference/ownership/allocation tests before mixed mode is enabled.
P3 remains open; this step adds no new performance claim or user package.

The subsequent 0.1t delivery implements mixed execution with device-owned Master,
bounded per-channel PCM handoff and continuous live input/time on producer
starvation. The initial checkpoint above describes the foundation only; current
runtime/eligibility and validation are in [mixed processing](ENGINE_MIXED_PROCESSING.md).

Validation of this foundation on 2026-10-06: cached offline configure, full local
Windows x64 Release build and 102/102 CTest passed (48.59 s), including three new
domain suites and existing P1/P2/VST3/persistence/GUI checks. Explicit hidden JUCE
J3 fixture smoke exited 0. Tests cover disabled/legacy input, direct hardware live
isolation, shared live/unsupported closure, upstream ownership, main/pre/post send
and legacy/Master terminal boundaries, unequal latency, cycle/alias rejection,
Monitor toggles and raw stereo capture before effects/PDC. The initial raw-capture
test incorrectly used mono selectors for a stereo assertion; corrected to explicit
0/1 selectors before the final full green run. No installed TH-U/Nuro repetition,
hardware opening, mixed-render speedup measurement or new distributable package.

## P4: measurement and universal load validation

Add bounded optional channel/plugin timing, critical-path duration, worker balance
and anticipation queue statistics. Measure profiling overhead. UI consumes
snapshots; audio workers perform no display work or logging. Keep B/XR/Late/D and
separate device, PDC and monitoring latency. Under parallel processing, distinguish
callback wall time/deadline load from summed CPU work across workers.

Use reproducible synthetic fixtures and representative effects across independent
tracks, deep serial chains, bus/send fan-in/out, heavy Master, mixed monitoring,
long WAVs and automation. The user's TH-U/Xvox/One topology is one regression
scenario, never an optimization rule. Accepted TH-U/Nuro state/editor behavior
does not require repeated special installed compatibility tests on routine builds.

Measure serial/parallel/anticipative modes at 64/128/256/512 frames where supported.
Record sample rate, CPU/core topology, worker count, driver, plugin versions/states,
warmup and duration. Report throughput, p50/p95/p99/max processing time, Late/XR/D,
output/timing equivalence and sustained playback/monitor/record stability.
Success requires preserved correctness and measured improvements without hiding
regressions; no fabricated performance percentage or universal no-click guarantee.
The broader #16 hardware gate remains separate until its own matrix is executed.

### 0.1u software delivery — 2026-10-06

P3/#53 accepted: user reports stable P3 and requests the next stage. #53 is closed
for the delivered slice; no session duration or recording matrix is inferred.
P4 adds optional per-channel/processor/worker timing and UI CSV snapshots, alongside
serial/parallel/anticipative fixtures and sustained synthetic raw-capture checks.
The first estimate is dependency job cost, excluding source/reduction/waits; it is
not a complete critical-path measurement or process CPU meter. #54 remains open
for sustained ASIO/representative-effects acceptance. Full behavior and boundaries:
[profiling](ENGINE_PROFILING.md).

## Delivery policy

All implementation, configure/build/tests and packages stay local with cached
dependencies. GitHub receives issues and documentation only, with skip-ci doc
commits. No source push, PR, merge, Actions, downloads or installations.

## 0.1u accepted; MIDI next — 2026-10-06

User accepts the delivered 0.1u software/profiling slice and wants MIDI development
in the next chat while continuing P4 testing. #54 stays open for the remaining
queue-event diagnosis, recording and sustained transport/reconnect/state checks;
these checks do not block the user-authorized MIDI focus. ASIO CSV at 128 frames,
Workers 4, Process 1024 and ONE bypass reports Late/XR/D/worker timeouts 0 and
43 ahead underruns + 43 invalidations. User reports no audible issues or manual
transport/parameter changes, with a small pause in the multitrack material.
Cause/timing of those events is unproven. No full #16/3e2/3e3 acceptance inferred.
See [current handoff](PROJECT_CONTEXT.md) for the next-chat entry point.



## P4 extension: single-track instrument parallelism

Decision recorded 2026-10-06 and tracked in #54.

P1 already parallelizes independent graph nodes/channels, but one heavy stateful VST3
instrument instance remains one scheduler node. The host must not assume it can call one
arbitrary plugin instance concurrently from multiple workers.

P4 therefore includes a generic **Instrument Parallelism / multi-instance VSTi sharding**
capability for cases where one polyphonic instrument is the critical path:

```text
Logical Instrument Track
          |
     MIDI dispatcher
      /   |   |   \
    VSTi VSTi VSTi VSTi
     W1   W2   W3   W4
      \   |   |   /
   deterministic sum
          |
 common track post-FX
          |
      buses/master
```

The user sees one logical track. The engine may prepare 2/4 synchronized hidden instances
of the same instrument, assign note ownership deterministically, process the instances on
existing RT workers, sum their audio into preallocated buffers, then run the ordinary
post-instrument track chain exactly once.

Required correctness:
- stable note-on -> note-off ownership, including repeated equal pitches;
- sustain and other global controller handling that remains coherent across shards;
- coherent pitch bend/modulation/expression/aftertouch policy;
- identical initial plugin state and synchronized parameter/automation changes;
- deterministic summing before shared post-instrument FX;
- unchanged routing, PDC, buses, sends, Master and raw-capture contracts;
- safe quiesce/rebuild/state/device transitions;
- no RT allocation, file/network/UI work, blocking mutex or concurrent call into the same instance.

This is **capability-gated and opt-in first**. Mono/legato/portamento instruments, internal
sequencers/arpeggiators, round-robin/global voice stealing, shared/random state, global
envelopes/effects or other cross-voice behavior can make multi-instance output differ from
one instance. Always retain a Single Instance fallback. Auto mode is future work and requires
profiling plus equivalence evidence.

P4 measurement must include Single Instance vs Parallel x2/x4 at 48 kHz / 128 frames first,
then other supported buffers, with worker count, callback p50/p95/p99/max, Late/XR/D,
per-instance timings, controller/sustain/repeated-note stress, state save/reload and common
post-FX-once verification.

Omnisphere piano/high-polyphony at 48 kHz / 128 frames is the first representative hardware
acceptance case because the user observed intermittent clicks in MR Studio where Fender
Studio Pro remained stable. It is a regression/benchmark case, never a vendor-specific
scheduler rule.
