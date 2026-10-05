# Stage 3e-P2 / 0.1s — bounded playback anticipation

Issue [#52](https://github.com/vladleng/MR-Studio/issues/52), local development
2026-10-05 after user acceptance of P1/#51. Branch remains
`mrs/0.1q-fix3-processing-local`. Code, builds, tests and packages stay local;
GitHub receives documentation/issues only, with [skip ci] documentation commits.
No Actions, source push, PR, merge, installation or dependency download.

## Eligibility and one processing owner

This first P2 implementation anticipates a complete eligible playback graph.
Any prepared input-monitor route or recording tap makes the whole graph direct,
including its playback channels, buses, sends, Master and hardware routes. P3
will separate playback and live dependency closures; P2 does not clone plugins
or split a shared stateful path between two owners.

Processors opt in through a generic `anticipation_safe()` contract (default
false). Existing native gain/filter/EQ/Cab IR and controlled synthetic fixtures
opt in. Actual externally queued MIDI and nonzero callback-relative automation
offsets make a PreparedGraph ineligible; unused MIDI route declarations alone
do not. Zero-offset native UI parameters/bypass use generation invalidation.
Sample-position/playing/tempo/musical context is supplied by the ordinary engine
at the rendered timeline position; DSP chunks remain Device Buffer sized, with
exact loop/tempo boundaries and the ordinary PDC graph. Device callbacks can
consume partial packets or several packets.

**Hosted VST3 currently does not opt in.** Its existing reset does not restore
effect history, and native editor changes are not represented in an anticipative
generation. Those graphs retain the accepted P1 processing path, even when the
Process Buffer setting is nonzero. No brand/preset/project whitelist is used.
Consequently this build does not promise an additional speedup for the user's
TH-U + Xvox channels / ONE Master project. Accepted editor/state behavior is
preserved; no special installed TH-U/Nuro routine tests were performed.

A single persistent producer renders the existing engine, reusing P1 channel
helpers and their dependency plan. The device callback only copies prepared PCM,
publishes heard transport/meters and signals the producer. It never waits for
producer completion, runs speculative DSP concurrently, or retries an incomplete
plugin serially. Windows producer uses MMCSS Pro Audio; engine/helper FP scopes
still apply. This moves deadline work off the callback; it does not eliminate DSP
CPU work, plugin latency or native crash/hang risks.

## Device Buffer and Process Buffer

Audio settings -> Process Buffer -> Connect. Off is the default. Available
nonzero UI values are 256/512/1024/2048/4096/8192 frames; a value must be at least
the requested Device Buffer. Device Buffer and ASIO selectors are unchanged.
Settings persist in desktop preferences v6; v1-v5 remain readable with anticipation
off. Device profiles retain their existing format; this is a global preference.

The producer renders device-sized packets rather than one enlarged plugin call.
There are `ceil(Process / Device)` usable packets plus one SPSC sentinel slot;
capacity rounds up by less than one Device Buffer. PCM, transport/mixer contexts,
three-slot heard-head mailbox and fixed replay journals are prepared before start.
Their aggregate storage is bounded to 16 MiB (allocator/kernel bookkeeping,
existing DSP/PDC scratch and source read-ahead memory are separate). Invalid
combinations reject before starting. No packet resizing or host operator-new
allocation occurs during tested producer/device/helper processing.

The footer shows `A queued/requested`, `direct` when eligibility falls back, and
`U` for anticipation underruns. Queue size is a recent device snapshot, not a
reservation guarantee. Callback CPU/B/XR/Late and PDC/device latency remain
separate; a low callback CPU does not measure the producer's CPU work. Producer
max render duration, invalidations and prepared queue bytes are available in
engine metrics and the developer benchmark.

Process Buffer / sample rate is the speculative time horizon: e.g. 1024 at
48 kHz is 21.33 ms, 4096 is 85.33 ms. This implementation starts output at the
first rendered timeline packet rather than deliberately delaying playback by a
full Process Buffer, but startup waits on the control thread for at least one
packet (up to 1 second). Hardware/device latency and PDC still apply. Edits throw
away queued work and can introduce silence/history discontinuities; this is not
a zero-added-latency promise for mixed routes or future plugin implementations.

## Transitions, tails and overload

The published playhead and meters follow delivered PCM, not the producer's future
cursor. A bounded three-slot mailbox supplies coherent heard transport/mixer ramp
state to the producer. Control/mixer revisions and PreparedGraph parameter
revisions mark every packet. The callback drops old generations before copying
and rechecks the generation after copying; a concurrent edit silences that block.

Seek, Stop, loop, mixer and native parameter changes invalidate the queue. The
producer rebases to the last published heard head, resets PDC/native DSP histories,
restores heard gain/send/gate/master ramps and replays consumed but unheard
transport/mixer commands. Fixed journals hold 128 transport and 16 mixer commands;
acknowledged entries retire. Saturation faults rather than allocating or silently
losing a command. Zero-offset parameter targets remain in the native processor;
future timestamped external automation is excluded, not shifted to another time.

Normal uninterrupted playback/looping preserves effect state and tails. Paused
packets run ordinary zero-input processing, but generation changes (including
Pause) and device stop/restart deliberately reset effect histories and smoothing
to retained parameter targets, so old tails are cut;
seamless effect-state rewind/crossfade is not implemented. Graph/project/preset
changes retain the existing stopped-device rebuild/state workflow. Active graph
preparation is rejected while the producer owns it. Stop/close stops callbacks,
joins the producer, quiesces P1 helpers and restores the heard head before state
capture or memory replacement. Arbitrary native hangs can still block that join.

An empty queue supplies silence, increments U and holds the unheard timeline.
Recovery consumes the next packet once; it does not skip source time or concurrently
reprocess a plugin. Unsupported external scheduling introduced while active
latches processing silence and requests reconnect, rather than switching ownership
inside a callback. On reconnect the graph uses direct P1 processing. A P1 helper
timeout also quarantines the graph: no rebase touches still-owned DSP/PDC memory
until control-thread quiescence.

## Local validation

Cached offline configure and local Windows x64 Release build passed; CTest
99/99, 43.21 seconds, including JUCE J1/J2/J3 smoke. New checks cover:

- Exact PCM versus serial with real DSP latency/PDC, bus/send/Master/direct routes,
  97-frame loops, tempo boundaries and partial 17+111-frame callbacks; 1/2/4
  participants, Process Buffer 256/1024/4096. Heard playhead and restart history.
- Global host allocation probe on producer/device/helpers, and active-prepare
  rejection. Fixed memory limits and Off/live/unsupported fallback.
- Seek/pause/play, Stop anchor, stale rejection, partial-head mixer ramps,
  180 acknowledged transport/mixer edit rounds, native parameter invalidation
  and EQ speculative smoothing reset versus a fresh owner at retained targets.
- Controlled producer starvation, silence/held timeline/recovery, one DSP owner
  and join; unsafe offset automation cannot silently remain anticipative.
- 9.6 MB float32 stereo WAV via actual disk read-ahead, short loops at a distant
  source position, prepared seek, exact comparison with memory reference and no
  observed disk errors/misses. Desktop preference migration/binding/backend wiring.

Existing raw-recording, VST3 fixture/state/editor, native DSP, Undo, reconnect,
PDC and P1 ownership regressions remain in the full suite. These tests do not
establish intended-system ASIO audition, long sustained-load acceptance or Fender
Studio Pro parity. P2/#52 remains open for user review; P3/P4 and 3e2/3e3/#16 remain
separate. Previous local packages are preserved.

## Reproducible measurements

`mrs_ahead_bench`: local synthetic arithmetic DSP, AMD Ryzen 7 PRO 4750U,
Windows x64 Release, 48 kHz, requested 4 participants. Prerecorded 2/8 channels,
heavy Master, mixed monitored/send graph (direct fallback) and eight streamed
voices from a 9.6 MB WAV. Device 64/128/256/512, Process Off/256/1024/4096 where
valid: 75 cases. Each case has 40 warmup and 120 measured callbacks, paced with
a precreated Windows high-resolution waitable timer, outside the timed callback.
Recorded callback intervals expose clock jitter/bursts. This is a short
offline matrix with long source files, not sustained ASIO or acoustic latency.
Each complete delivered block compares exact PCM to a serial reference (disk
sources use a memory reference). CSV records callback p50/p95/p99/max, Late,
U/invalidations, producer max, worker timeouts, disk misses, queue bytes and checked
blocks. Hardware XR is unavailable without a connected driver/device.

The complete high-resolution-clock matrix has 0 Late, 0 U, 0 worker timeouts and
0 disk misses; all 12,000 delivered blocks compare exactly with serial reference.
Maximum reserved queue/mailbox/journal storage in this stereo matrix is 2,473,024
bytes. Prepared storage remains reserved for direct fallback too; Off has 164,744
bytes of fixed wrapper/mailbox/journal storage.

At 128 frames / 48 kHz (deadline 2666.667 us):

| Topology | Process frames | Active | Callback p50 us | p99 us | Max us | Producer max us |
|---|---:|---|---:|---:|---:|---:|
| playback2 | Off | no | 179.4 | 274.0 | 323.1 | — |
| playback2 | 1024 | yes | 14.8 | 34.7 | 38.2 | 251.9 |
| playback8 | Off | no | 183.5 | 435.4 | 447.1 | — |
| playback8 | 1024 | yes | 9.1 | 33.7 | 37.3 | 590.4 |
| heavy Master / 8 tracks | Off | no | 446.2 | 1256.9 | 1288.9 | — |
| heavy Master / 8 tracks | 1024 | yes | 11.7 | 31.7 | 43.4 | 1388.7 |
| mixed monitor + send | Off | no | 224.5 | 485.8 | 522.3 | — |
| mixed monitor + send | 1024 | no | 268.6 | 498.8 | 510.9 | — |
| streamed WAV / 8 voices | Off | no | 287.3 | 613.0 | 706.8 | — |
| streamed WAV / 8 voices | 1024 | yes | 17.7 | 95.2 | 160.5 | 676.3 |

These are callback wall times, not total-CPU speedup or plugin throughput. Producer
maxima include warmup; callback percentiles use 120 measured blocks. Fallback case
variation demonstrates OS/thermal/dispatch noise, not a Process Buffer benefit.
At Device 128 / Process 1024, prepared wrapper/queue storage is 488,960 bytes.

The first coarse `std::this_thread::sleep_until` matrix is retained as
`P2-ahead-bench-coarse-initial.csv`: 1250 U across 75 cases, all with Process 256;
1024/4096 had none. Bursty development-clock delivery can drain a short window
even when callback work meets its deadline. Its very low starved-callback medians
include silence and must not be reported as successful optimization. The corrected
timer matrix is `P2-ahead-bench.csv`; `--coarse-clock` reproduces coarse-clock mode.
The offline device itself is a development clock, not an ASIO timing instrument.

Package: `MR-Studio-0.1s-P2-anticipative-JUCE-ASIO-Windows-local` in chat Builds.
Launch `Moon River Studio JUCE.exe`; keep `mrs_vst3_scan.exe` beside it. README,
validation log, CSVs, synthetic fixture/tests/benchmark, licenses and SHA256 hashes
accompany the local package. Process Buffer remains Off by default. User ASIO
review of 0.1s and broader sustained-load acceptance remain pending.
