# Stage 3e-P3 / 0.1t — mixed playback and live processing

Issue [#53](https://github.com/vladleng/MR-Studio/issues/53), local Windows-first
development on 2026-10-06 after acceptance of initial P2/#52. Branch remains
`mrs/0.1q-fix3-processing-local`. Source/builds/tests/packages local; GitHub only
issues/docs with [skip ci] documentation commits. No Actions/source push/PR/merge,
installation or dependency download. Universal capability/routing rules apply;
no project/plugin-brand whitelist.

## Domains and ownership

Process Buffer remains Off by default. Fully eligible playback graphs retain P2's
whole-graph producer. Other graphs can use `MixedRenderer` when Workers >= 2 and
at least one prepared playback voice belongs to an eligible channel. All potential
input routes reserve their downstream closure, even with Monitor disabled. An
unsupported processor reserves its channel and downstream outputs/sends. Native
channels upstream of that boundary can still anticipate; live/unsupported channels
and shared buses stay device-owned. In this first mixed renderer, Master/global
processors ALWAYS stay device-owned, including independent hardware-monitor cases.
Legacy unassigned playback also stays on the callback. No insert-prefix splitting.

One producer owns the eligible channels' actual processor instances, source cursors,
outgoing main/send PDC and private transport/mixer scratch. A separately prepared
audio kernel retains the original routing/latency plan and shares those existing
instances; it clears unowned insert/Master references before any processing starts.
It does not create/clone another plugin instance. The callback skips eligible DSP
and source cursor access, importing ready main/send contributions at their original
channel positions. Main/pre/post sends, bus fan-in, Master and hardware reduction
retain their original order; incoming contributions are mixed BEFORE nonlinear
shared DSP. Each PDC edge is applied once by its owner.

The producer uses one DSP participant. Callback helpers reserve the remaining
configured capacity; Workers = 1 leaves mixed graphs direct. The original P1 helper
pool is parked during mixed mode and restored after joining both active owners.
No active DSP participant exceeds the configured mixed worker count. The parked
pool is not dispatched. Both owners use FTZ/DAZ; producer/active helpers request
Windows Pro Audio MMCSS without affinity. This does not guarantee CPU throughput
for a heavy shared Master or accelerate unsupported plugins on their own channels.

## Bounded handoff, time and starvation

Prepared SPSC packets contain per-channel main/send PCM, including eligible
internal edges so the callback can preserve reduction order. Packet identity is
control/parameter generation plus a monotonic device-frame sequence; repeated
sample positions in short loops do not alias packets. The callback copies into
prepared scratch and discards stale/elapsed packets with bounded iteration.
It handles partial packets and callbacks spanning multiple producer blocks.

The producer uses the requested Device Buffer quantum and ordinary loop/tempo
chunking. Actual ASIO callback size remains visible as B. Prepared PCM, mailboxes,
wrapper and copy scratch total at most 16 MiB; large channel/output/window layouts
can reject the device start explicitly. DSP/PDC/assets have their existing separate
prepared budgets. The inactive P2 queue is released before mixed preparation.
One existing ReadAhead cursor per eligible voice is shared with control-thread
prime requests; it has exactly one processing owner. Duplicate temporary read-ahead
objects created by preparation are retired before the producer starts.

The callback owns controls, heard transport/mix ramps, live DSP, peak publication
and raw recording. A three-slot ownership mailbox publishes coherent context for
the producer. Seek/Pause/Stop/loop/mixer/native parameter generations invalidate
ahead PCM and restore producer time/mix from the heard context. There is no speculative
control journal or advancement of shared DSP. Raw recording still captures original
selected inputs before effects, PDC or any playback queue.

Applied commands/mixer updates advance the callback generation as well as enqueue
publication. End-of-callback context retains its beginning generation; it never
labels old state with a command enqueued during DSP. The producer waits for a
matching applied context before rendering. A held-Master concurrency regression
checks that Pause arriving mid-callback cannot admit stale playing PCM.

An empty/stale playback queue contributes zero for missing playback frames while
LIVE DSP, raw recording and device-frame time continue. The producer rebases its
own history to the latest heard context when behind. It never steals/calls a
callback-owned plugin. This differs from P2's playback-only silence/held head.
Missing playback can alter tails of shared nonlinear/stateful effects; recovery
does not promise equivalence to an uninterrupted session after a real dropout.
Producer rebase resets native histories/smoothing to retained targets and can cut
playback tails. No seamless rewind/crossfade or zero-click transition is claimed.

Introducing unsupported scheduling on a running producer (external MIDI or
callback-offset automation) latches a visible processing fault and pauses/silences
the session until stop/reconnect/reprepare. Stop/close joins producer and callback
helpers before restoring direct ownership, state capture or graph replacement.
Arbitrary hung/native-crashing calls still require later isolation work.

## Monitoring and latency policy

Monitor/input-monitoring mixer toggles retain prepared ownership, including muted
routes. Structural Arm/route/insert changes use the existing stop/quiesce/reprepare
boundary. Effects are not disabled to reduce latency. Fixed PDC remains on all
paths, including hardware routes bypassing Master. High-latency effects and PDC
therefore still add monitoring delay; threading cannot remove algorithmic latency.
Dynamic latency-changing presets retain their existing reject/rollback policy and
belong to 3e2, not this delivery.

In the application, Process Buffer with Workers >= 2 prepares input routes only
for Monitor-enabled or armed tracks; otherwise a shared physical default input
would accidentally reserve every playback channel. Changing Monitor on an unarmed
track changes ownership and requires Pause/Stop, captures live plugin state via
the existing edit path, then rebuilds safely (Undo/Redo likewise). Monitor toggles
on an already armed track keep device ownership and remain live mixer changes.
Global Monitor also keeps ownership. Off/Workers 1 preserve the previous behavior.
Tests verify shared-input playback eligibility, these gates, Undo/Redo and Arm.
Switching mixed/direct/P2 after stopped preparation retires the old queue/graph;
it never retains a second inactive PCM budget or stale plugin ownership.

The footer adds `mixed` to A and reports the active mixed participant capacity as W.
When a monitoring route is prepared, `Mon` is driver-reported input + output latency
plus fixed end-to-end PDC, in milliseconds. This is a model estimate, not a measured
acoustic round trip. Device I/O latency and PDC retain their separate readouts;
the tooltip explains the calculation. Process Buffer adds no intentional hold to
live input. It reserves prerecorded work and affects playback edit/recovery response.

Hosted VST3 does NOT opt into anticipation: its history reset and native-editor
generation capability remain insufficient. VST3 stays on callback-owned paths,
including shared Master, while independent eligible native playback can anticipate.
TH-U/Xvox/ONE on exclusively unsupported channels have no new producer speedup
promised. Accepted P1 parallelism remains their direct route. No special installed
TH-U/Nuro checks are repeated for this routine delivery.

## Validation and acceptance

Seven mixed suites cover exact serial PCM with unequal actual plugin/PDC delay,
nonlinear shared bus/Master, pre/post sends and direct hardware output, loops of
97 samples and partial callbacks at Device 64/128/256/512 with Workers 2/4; global
host allocation probe across device/producer/helpers; instance/reset ownership;
forced producer starvation with uninterrupted live DSP and transport; generation
edits, native parameter change and unsafe offset rejection; 16 MiB rejection;
raw stereo capture; backend stop/restart; and a 9.6 MB streamed WAV with short-loop
and prepared seek comparison to an in-memory serial reference. Existing P1/P2,
VST3 fixture, persistence and desktop/GUI checks remain required.

Build/test/matrix/package results follow below.

Final cached Windows x64 Release build and 109/109 CTest passed (58.28 seconds),
including the mid-callback command regression. The initial six-suite full run
also passed 108/108 before this additional concurrency fix/check. Host allocation
probe, DSP/reset ownership, raw capture, streamed PCM/seek, memory refusal and
backend stop/restart checks passed. Current Audio Settings preview was inspected;
guide text fits, accepted palette/controls/layout retained. Software-scaled GUI
tests do not replace physical monitor/accessibility/interface checks.
Intended-interface ASIO listening/recording at actual buffer sizes remains a user
acceptance gate. #53 stays open until that review; P4/#16 sustained measurement,
generic VST3 producer capability and 3e2/3e3 remain separate.

### Short paced matrix

AMD Ryzen 7 PRO 4750U, 8 cores / 16 logical processors, Windows x64 Release,
48 kHz, configured 4 workers, synthetic stateful processors. The precreated
Windows high-resolution timer paces 40 warmup + 120 measured callbacks per case.
Device 64/128/256/512 and Process Off/256/1024/4096 (valid combinations) cover
playback2, playback8, heavy Master, mixed live/send, streamed WAV, mixed heavy
Master and mixed streamed WAV: 105 cases, 16,800 complete blocks checked exactly
against serial PCM. Queue underruns, worker timeouts and disk misses are all zero.

Final matrix has ONE Late callback: playback-only heavy Master, Device 64 / Process
4096, max 2026.7 us, p99 840.7 us, device deadline 1333.3 us. PCM remains exact,
without a queue underrun. All 45 mixed cases have zero Late. The prior initial
105-case matrix before the additional command-generation concurrency fix had
zero Late; both raw CSVs are retained. This is short synthetic pacing, not ASIO
or a universal no-click/Fender parity claim. A recovered stream after a real
starvation would not be compared to uninterrupted stateful history as equivalent.

At Device 128, callback microseconds (Process Off -> 1024):

| Topology | p50 | p99 | Producer max | Prepared queue/copy storage |
|---|---|---|---:|---:|
| mixed live/send + 8 playback channels | 250.2 -> 129.1 | 512.6 -> 363.8 | 1281.2 | 205,848 bytes |
| mixed heavy shared Master | 658.4 -> 504.6 | 1437.7 -> 1408.4 | 1097.9 | 205,848 bytes |
| mixed streamed WAV | 225.8 -> 126.1 | 508.3 -> 312.7 | 1187.3 | 205,848 bytes |

These are callback wall times; producer maxima include warmup. No total CPU
reduction is inferred. Shared Master remains a callback bottleneck. Kernel matrix
was measured after the concurrency fix; subsequent application route/lifetime
integration does not change its steady-state fixtures or PCM processing.

Package: `MR-Studio-0.1t-P3-mixed-JUCE-ASIO-Windows-local` in local chat Builds.
Process Buffer Off default; for eligible mixed playback choose Workers >= 2 and
Process 1024/4096 >= Device Buffer. Keep scanner beside the JUCE EXE. Validation,
fixture/tests, CSVs, licenses and SHA256 manifest accompany the local package.
Status: READY WITH MANUAL CHECK; intended-interface playback/live/record audition
is required for P3 acceptance. Previous packages remain available.

Local source: `e35477c23dd145ab395662bf255543549e37f04a`. Packaged hidden JUCE
J3 fixture smoke exited 0; packaged capture/lifecycle and concurrent-command tests
passed. JUCE EXE SHA256:
`72CA8971FE99B5E3D493E6D128F6081335A2FDE7D54ABFD311E44635B41EF3B2`.
Package manifest verifies all recorded files and matches original build binaries.
