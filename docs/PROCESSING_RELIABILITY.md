# Stage 3e — Processing reliability

Started locally on 2026-10-05 after accepted 0.1p fix3. Own effects/instruments
and Amp/Preamp (3d) are deferred until the DAW foundation is ready.

## Delivery slices

- **3e1 / 0.1q — static audio PDC:** sample alignment in processor parallel
  branches, track/bus outputs, pre/post-fader sends, Master and direct hardware
  routes; bounded preallocated storage, reset/transport and deterministic tests.
- **3e2 — safe chain/state transitions:** preparation/commit/rollback, latency
  change handling and explicit transport behavior; preserve accepted editor/state
  workflow. Live seamless patch switching needs separate measured acceptance.
- **3e3 — recovery/isolation and load:** failure diagnostics, recovery and process
  boundaries, sustained-load checks on the intended system. In-process VST3 host
  currently cannot survive arbitrary native access violations; no crash isolation
  claimed by 3e1. Deferred #16 is not automatically passed.

## 3e1 behavior

PreparedGraph computes cumulative latency in topological order and delays shorter
incoming edges and summed output branches. Its output latency is the longest path;
compensation_applied reports previously unequal branches, and unresolved parallel
compensation becomes false. Live safety still requires each processor to be safe
and total path latency to fit the configured live budget.

AudioEngine computes track/bus arrival times, including all sends. Incoming
signals are aligned before bus inserts; outgoing track paths are aligned before
Master. Direct hardware routes stay outside Master gain/inserts, but receive the
additional delay needed to match the Master route. Legacy unassigned voices/input
routes are aligned with Master input as well. All prepared paths, including muted
paths, remain in the plan so Mute/Solo/send gain changes do not change latency.
Monitoring passes through the same compensation and may acquire additional delay;
raw recording taps remain before effects/compensation. No low-latency monitor mode
or device/recording timestamp offset calibration is implied.

The UI footer shows total processing-path PDC in milliseconds. ASIO buffer/device
latency is separate. This is delay-based summing alignment; playback timeline is
not pre-rolled or visually shifted. Seek/Stop clear compensation history; continuous
loops preserve delay flow. Existing playback/recording transport rules remain.

Delay storage is prepared outside the callback. Per-frame generation tags permit
constant-time history reset without clearing large rings in the audio callback.
No allocations, I/O or locks are introduced in the callback. Maximum cumulative
audio path: 262144 samples. Delay storage budget: 64 MiB per processor graph and
128 MiB for mixer routing; excessive plans reject during preparation.

## Current boundary

Latency is fixed for the prepared graph. Adding/removing/bypassing latency-bearing
VST3 or changing its latency mode requires Pause/Stop and graph re-preparation.
Latency-changing in-place presets retain their existing reject/rollback behavior.
Automatic reaction to a plugin reporting changed latency while active belongs to
3e2. MIDI scheduling and external hardware compensation are separate scope.
Plugins must correctly report their own algorithmic delay.

## Verification

Synthetic processors actually delay samples; no installed TH-U/Nuro routine tests.
Tests cover parallel output sum and internal fan-in, different/short callback sizes,
stereo impulse alignment, pre/post-fader bus sends, Master/direct output timing,
continuous signal alignment, Stop/panic history reset, host RT allocations and
excessive-latency rejection.
Raw capture with nonzero insert/PDC latency retains the input impulse at sample 0;
excessive aggregate delay memory also rejects at preparation.
Existing transport, routing, state, project/Undo and native-editor checks remain
in the full local suite.

Windows local cached dependencies/configure/build/tests/packages only; GitHub
issues/docs only. No code push/PR/merge/Actions, installations or downloads.
3e1 user hardware/project audition, later 3e slices, remaining J3 physical gates,
Stage 3c/#23 and #16 remain separately open.

## 0.1q local delivery

Cached offline configure and Windows x64 Release build passed; 91/91 CTest passed.
Synthetic latency tests cover exact timing and zero RT allocations; raw recording
with delayed inserts preserves its input at sample zero. Aggregate delay memory
and cumulative latency limits reject safely. Packaged JUCE J3 fixture smoke and
software preview are included. User audition with intended VST3/ASIO projects
remains pending; no installed TH-U/Nuro special checks were run.
Branch: mrs/0.1q-stage3e1-pdc-local.
Package: MR-Studio-0.1q-Stage3e1-PDC-JUCE-ASIO-Windows-local in chat Builds.
Keep mrs_vst3_scan.exe beside the main EXE. Previous packages are preserved.

## 0.1q fix1 — project display, audio reconnect and mixer

Local Windows update, 2026-10-05; accepted by the user: «Теперь все отлично!».

- Opening a different document rebuilds channel views even when track IDs and
  revision match the previous project. Old selections, meters and view offsets
  are reset. The saved project filename (unsaved project title otherwise), plus
  the dirty marker, is centered in the upper menu bar.
- Device/buffer reconnect stops callbacks and captures live VST3 component,
  controller and parameter state before destroying hosted instances. The captured
  snapshot is retained in the project/Undo history only when changed. A failed
  capture restarts the old device. Reconnect preserves the timeline position and
  pauses active playback; recording still prevents reconfiguration.
- Mixer strips use the track-control panel color (#393e43), against the darker
  browser/workspace surface (#25282b).
- Drag the mixer upper divider to resize upward/downward. The base height is
  persisted; at least 140 logical pixels remain for arrangement.
- Faders and stereo meters share a height independent of insert count. The
  upper insert area grows with the largest chain; at window height limits its
  list scrolls vertically. Every supported insert remains accessible (eight per
  channel); resizing the mixer changes the shared fader height.

Offline cached configure and full local Release build passed. 91/91 CTest passed
in 21.93 seconds, including JUCE J2/J3 regression checks for live native-editor
edits across buffer reconnect, same-ID document replacement, long insert chains,
divider drag and persisted height. No special installed TH-U/Nuro tests.

Branch: mrs/0.1q-fix1-local.
Package: MR-Studio-0.1q-fix1-JUCE-ASIO-Windows-local in chat Builds.
The fix1 update is user-accepted; 3e2/3e3 and whole #23 remain open.
Code/builds stay local. GitHub documentation/issues only, no source push or Actions.

## 0.1q fix2 — performance footer

CPU readout and bar use the ASIO backend's measured callback CPU load, multiplied
by 100. This is audio processing time relative to its buffer deadline, not overall
Windows CPU utilization. A full bar is 100%; overload text can exceed 100%.
The bar is green below 80%, orange at 80% and red at 100%.

The footer also shows driver-reported input/output latency in milliseconds.
PDC remains separate; these values do not claim measured acoustic round-trip
latency. Offline/disconnected/unavailable hardware readings show dashes rather
than invented measurements. UI reads backend status on its 30 Hz timer; no new
audio callback instrumentation or processing behavior changes.

Package: MR-Studio-0.1q-fix2-performance-JUCE-ASIO-Windows-local.
Branch: mrs/0.1q-fix2-performance-local. User review pending.
Cached offline configure/full local Release build and 91/91 CTest passed
(21.73 seconds), including measured-value conversion, overload bar saturation
and offline unavailable-state checks. Current software preview inspected.
No special installed TH-U/Nuro tests. Code/builds local, GitHub docs/issues only.

## 0.1q fix3 — investigate clicks at 128 frames

User reports clicks at 128, absent at 256, with TH-U, Xvox Pro and IK Multimedia
One. Clarification: TH-U and Xvox are on different channels, One on Master;
they are not one serial insert chain. Studio Pro handles the intended workload
without those clicks. This is a new
performance report, so focused installed-plugin CPU experiments are in scope.
Previously accepted preset/editor behavior is not routinely retested.

Track/bus inserts were always prepared and rendered in 64-frame slices. At a
128-frame callback each active insert was therefore invoked twice. The desktop
now prepares those inserts for the selected device buffer and passes an explicit
preferred processing chunk to the engine. Larger/unusual callbacks remain bounded
by the prepared size; loop wraps split chunks at their exact boundaries and
publish the correct track/bus plugin position. No new buffering latency is added.

An audio-thread SSE FTZ/DAZ scope prevents slow subnormal DSP math and restores
the caller's floating-point mode. Mixer scratch allocation is bounded to 128 MiB.
Tests cover one insert call for 128/256-frame callbacks, irregular blocks, short
loop/context boundaries, zero RT allocation, FP mode restoration and budget bounds.
Existing PDC/bus/send/hardware/raw-recording/state regressions remain required.

Footer diagnostics, since the last engine preparation:
- B: observed callback frame count (range if variable), not just requested buffer.
- XR: backend-reported input/output underflow and overflow flags.
- Late: measured engine callback durations exceeding their buffer deadline.
- D: disk read-ahead misses. Counters identify categories; zero does not prove
  every possible driver/plugin fault absent.

Offline experiments, 48 kHz, 128-frame callbacks, synthetic stereo sine, 400 warmup
and 3000 measured callbacks, separate processes with no ASIO device/editor:

| State | Chunk | Average us | p99 us | Max us | Over 2666.667 us |
|---|---:|---:|---:|---:|---:|
| TH-U + Xvox Pro + One initial states | 64 | 264.450 | 374.600 | 766.800 | 0/3000 |
| Same initial states | 128 | 234.307 | 361.100 | 663.700 | 0/3000 |
| Last saved project's TH-U/Xvox states | 64 | 379.715 | 1484.600 | 9131.300 | 3/3000 |
| Same saved states | 128 | 338.395 | 1190.100 | 3991.700 | 2/3000 |

The initial three-plugin graph reports 2465 latency samples with either chunk.
Average processing fell approximately 11%; rare scheduling/time spikes remain in
the saved-state experiment. The saved project contains two plugins, no One. Saved
chains are flattened for this CPU experiment; source WAVs/routing, active unsaved
presets and real ASIO scheduling are not reproduced. No project or audio settings
were written. These measurements do not establish that the clicks are resolved.

Developer tool: mrs_processing_bench CHUNK PLUGIN_PATH... or
mrs_processing_bench CHUNK --project PROJECT_PATH. CHUNK is 1..128 here; Windows
only, no device connection. Hardware comparison and user's 128-frame review pending.
Package: MR-Studio-0.1q-fix3-processing-JUCE-ASIO-Windows-local.
Branch: mrs/0.1q-fix3-processing-local. Code/builds local; GitHub docs/issues only.
Cached offline configure/full local Release build and 92/92 CTest passed
(22.41 seconds). Current software preview inspected. Real ASIO listening with
all three active presets at 128 frames remains the user's pending acceptance check.

## Universal engine performance direction — 2026-10-05

User reports fix3 slightly improved playback and authorizes systematic shared
engine development, independent of a particular project/plugin brand. Complete
click resolution is not confirmed. Follow [the performance plan](ENGINE_PERFORMANCE_PLAN.md):
3e-P1 parallel channels (#51), 3e-P2 anticipative playback (#52), 3e-P3 separate
low-latency monitoring (#53), then 3e-P4 profiling and sustained-load acceptance
(#54). Minimal profiling accompanies P1. P1 is implemented and locally validated
in 0.1r and user-accepted at 128 frames; P2 is in local 0.1s development. P3/P4 and existing
transition/isolation/hardware gates remain open.


## 0.1r / 3e-P1 — parallel channel processing

Prepared dependency levels execute independent channel insert/gain/send/PDC jobs
on persistent bounded workers plus the callback, then reduce isolated contributions
in the previous deterministic order. Buses and Master wait for dependencies; shared
mutable graph aliases reject. Workers use FP FTZ/DAZ scopes and Windows MMCSS Pro
Audio without affinity. Audio settings -> Workers -> Connect selects 1 (serial)
or a 2..8 participant limit; default 2. Cheap levels use measured serial fallback.
No process buffer or extra buffering latency is added.

A 20 ms helper-wait watchdog latches silence/paused transport on timeout without
serial retry; reconnect is required. Stop/close quiesce helpers before plugin state
access, and preparation/destruction join before storage replacement. Native plugin
calls themselves are not preemptible; crash/hang survival remains isolation scope.
W in the footer reports prepared participant capacity, not per-callback busy threads.

Cached configure/full local Release build and 94/94 CTest passed (22.25 seconds).
Tests compare exact serial/parallel samples/meters at forced 2/4/8 limits, routing,
real latency, variable/short blocks, loops/transport/mix changes, host allocations
on all processing threads, timeout ownership, raw capture and lifecycle/reconnect.
The 192-case synthetic offline matrix compares 1/2/4/8 limits at 64/128/256/512
frames under MMCSS and ordinary policy; measured Late/timeouts are zero. At 128,
8 independent synthetic channels take 788.6 us serial versus 238.3 us median with
4 participants; scheduling tails and small-graph overhead are explicitly reported.
See [the implementation/measurement report](ENGINE_PARALLEL_PROCESSING.md).
This does not establish Fender Studio Pro parity or resolution of ASIO clicks.

Branch retained: mrs/0.1q-fix3-processing-local. Package:
MR-Studio-0.1r-P1-parallel-JUCE-ASIO-Windows-local in chat Builds.
#51 was accepted and closed on 2026-10-05 after the user's 128-frame audition; broader sustained-load acceptance remains P4/#16 scope.
No special installed TH-U/Nuro routine state tests; code/builds/tools/tests local,
GitHub docs/issues only with skip-ci commits; no Actions or source push.


## P1 user acceptance — 2026-10-05

User tested 0.1r with the same plugins at ASIO buffer 128: regular clicks
disappeared, occasional rare clicks remain (also experienced in Studio Pro).
User accepts the improvement; #51 is closed as completed. No universal zero-click
claim, detailed worker/duration report, sustained mixed recording matrix or
Fender Studio Pro parity measurement is inferred. P4/#16 and isolation remain open.
P2/#52 starts locally today at the user's request.

## 0.1s / 3e-P2 — conservative native playback anticipation

The local build adds an explicit Process Buffer, Off by default, independent of
Device Buffer. A single producer owns eligible whole-graph prerecorded DSP and
reuses P1 helpers; the callback consumes bounded generation-tagged PCM and publishes
the heard head. Prepared packet/mailbox/journal storage is limited to 16 MiB.
Seek/Stop/loop/mixer/parameter changes discard stale packets, reset histories and
restore/replay heard transport/mix state. Pause/edits/restart cut previous tails;
seamless history rewind remains outside this first implementation. Empty queues
silence/hold timeline and recover without concurrent retry.

All VST3, any input-monitor/recording graph and external MIDI/offset automation
remain direct P1 processing. Native Gain/filter/EQ/Cab IR opt in through a generic
capability; no project/vendor whitelist. Unsupported scheduling introduced during
active anticipation faults until reconnect. Stop/close joins producer/helpers
before state capture/replacement. Native crashes/hangs still need later isolation.

Cached local Release build and 99/99 CTest passed (43.21 s). Exact PCM/PDC/short
loops/variable callback/transport/mixer/parameter/EQ smoothing/history tests,
allocation probe on producer/device/helpers, memory/eligibility/ownership and
9.6 MB streamed-WAV seek/loop checks passed. A 75-case short paced synthetic matrix
at Device 64/128/256/512 and Process Off/256/1024/4096 had 0 Late/U/timeouts/disk
misses with a high-resolution timer and 12,000 exact reference blocks. Initial
coarse-clock short-window underruns are retained and explained in the report.

[Eligibility, transitions and measured limits](ENGINE_ANTICIPATIVE_PROCESSING.md).
Package: MR-Studio-0.1s-P2-anticipative-JUCE-ASIO-Windows-local in chat Builds.
#52 was accepted and closed for this initial slice on 2026-10-06; VST3 anticipation capability, live/playback
dependency separation (P3), sustained load (P4/#16), 3e2/3e3 remain separate.
Code/builds/tests/packages local; GitHub docs/issues only, no Actions/source push.

## P2 accepted; P3 ownership foundation — 2026-10-06

User reports good results and authorizes closing the latest delivery and proceeding.
The initial 0.1s/#52 slice is accepted; precise new ASIO settings/duration and
unimplemented VST3/history capabilities are not inferred. P3/#53 starts with a
prepared candidate domain plan: all potential live input and unsupported processor
dependencies stay on the device through buses/sends/Master, while independent
upstream playback may be owned by a producer. Raw capture stays device-owned;
monitoring toggles preserve ownership and fixed PDC. Merge edges retain the existing
compensation, including direct hardware alignment. This preparation does not yet
activate mixed rendering or change the accepted P2 whole-graph fallback.
See [P3 implementation contract and remaining work](ENGINE_PERFORMANCE_PLAN.md).

## 0.1t / P3 mixed rendering — 2026-10-06

The next implementation separates eligible native playback channels from live
dependencies using disjoint DSP/PDC/source-cursor owners and a bounded PCM queue.
Shared buses, unsupported processors, legacy unassigned playback and Master remain
on callback-sized processing. Raw capture is before effects/PDC. Empty/stale
playback slots silence only their contribution while live DSP/capture/device time
continue. Control/parameter generation plus monotonic render sequence prevents
stale short-loop reuse; producer history rebases can cut playback tails.
Monitor toggles preserve ownership; structural Arm/route changes retain stopped
graph preparation. No effect is disabled; fixed plugin/PDC latency remains.
Workers >= 2 is required for mixed mode, within configured active DSP capacity;
Off/Workers 1 retain direct mixed processing. Unsupported VST3 remains direct;
eligible independent upstream native channels may anticipate into its shared path.
Footer `mixed` and `Mon` distinguish mixed mode and estimated driver I/O + fixed PDC
monitoring delay. No additional Process Buffer hold is introduced on live input.
Full local Release and 109/109 CTest passed (58.28 s), including seven new mixed
suites, existing P1/P2, VST3 fixture, persistence and GUI checks. Hardware acceptance,
P4/#16 and isolation remain open. [Ownership, transitions and limits](ENGINE_MIXED_PROCESSING.md).

## 0.1t fix1 — native plugin editor windows

The user reports good P3 performance and continues experimenting. This does not
close P3/#53 hardware acceptance. The editor window going behind MR Studio on
hover/focus is corrected with an owned top-level Windows window. Pin preserves
an editor when opening another plugin; per-slot close keeps other views alive.
Host bypass detaches views before stopped PDC rebuild and reconnects them to the
same JUCE windows, retaining Pin and avoiding the stale white host. Native child
HWNDs are recreated. Project/device/structural changes still retire all editors.
No engine scheduling/buffering or project-schema change; existing Undo persists.
Local Release + 109/109 CTest (57.09 s), packaged hidden J3 fixture smoke exit 0.
User accepted the plugin editor fix on 2026-10-06: plugin behavior is now correct.
Broader P3 monitoring/recording ASIO acceptance remains separate; no special
installed TH-U/Nuro test repetition. [Window lifetime and validation](ENGINE_PLUGIN_EDITORS.md).

## P3 accepted; 0.1u / P4 diagnostics — 2026-10-06

User reports stable P3 and requests proceeding. #53 is closed for the delivered
mixed slice; the earlier pending notes above are historical. No exact ASIO duration
or new monitoring/recording matrix is inferred. Sustained hardware acceptance now
remains on #54/P4 and broader #16.

0.1u adds opt-in bounded timing for actual channel jobs, individual processors,
worker participants, Master and separate device/producer dependency cost estimates.
The UI reads atomic snapshots and exports CSV on the message thread. Profiling is
OFF by default; it adds timing overhead, does not change project history/schema,
and is disabled when its panel closes. Existing callback B/XR/Late/D, PDC and Mon
semantics remain. Exact PCM, concurrent reads and all-thread allocation probes
cover profiling; existing ownership and raw capture boundaries remain unchanged.
See [profiling contract and load methodology](ENGINE_PROFILING.md). P4 remains
open pending physical ASIO sustained sessions; no isolation/3e2 gate is closed.

