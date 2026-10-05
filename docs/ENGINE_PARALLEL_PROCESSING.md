# Stage 3e-P1 / 0.1r — parallel channels

Local implementation: 2026-10-05. Issue: [#51](https://github.com/vladleng/MR-Studio/issues/51).
Branch retained: `mrs/0.1q-fix3-processing-local`. Windows x64 Release / JUCE J3 / ASIO.
Target: an efficient universal shared engine comparable in intent to Fender Studio Pro;
parity and resolution of the reported clicks are not established by this delivery.

## Processing and ownership

Preparation compiles dependency levels from channel outputs and every pre/post send,
including muted paths. Within a level independent channels may run concurrently;
buses wait for their upstream levels, and Master runs after all channel reductions.
Each job executes the ordered insert chain, gain/pan/gates/ramps, sends and route PDC.
It writes only that channel's scratch, contribution buffers, delay history and peaks.
The callback then reduces contributions in the original topological order, preserving
floating-point summation order. Direct hardware outputs retain their Master bypass.
Raw input capture and playback/monitor source gathering remain on the callback.

The callback reserves the first job and participates in claiming further jobs through
a lock-free atomic counter. Persistent helpers use precreated Windows events; no
host RT allocation, file I/O, mutex or spin loop is introduced. FP FTZ/DAZ scopes
apply to the callback and every worker batch. Shared mutable PreparedGraph aliases
between channel/Master positions reject before processing. Single-instance native
plugin crashes or hangs are still outside the in-process host's survival guarantee.

All channel input/output/send scratch is prepared and bounded to 128 MiB in aggregate;
existing PDC limits remain. Job context lives in the engine rather than on a callback
stack, so a timed-out worker retains valid storage. ASIO/offline stop and close
quiesce helpers before state capture/editor changes; preparation and destruction
join them before replacing plugin/scratch storage. Existing state/Undo/reconnect
behavior remains covered by ordinary synthetic fixture regressions.

This first scheduler uses dependency-level barriers. A downstream bus can wait for
an unrelated slow job in the same preceding level. Source gathering, deterministic
reduction and Master still consume callback time. It does not anticipate playback,
add a process buffer, or promise speedup for a serial chain or heavy Master.

## Controls and adaptive scheduling

Audio settings -> Workers -> Connect: 1 is the serial reference; 2..8 are upper
limits including the callback, with at most seven helpers. Actual capacity is also
capped by prepared graph width; one dependent chain has no helpers. Default 2 is a
conservative starting point for the measured two-channel case, not an all-core rule.
The setting persists in desktop preferences v5, which reads older v1-v4 files;
project schema is unchanged. Hardware device profiles keep their previous mapping
format; the worker limit is a separate global audio preference. The footer's W
reports prepared participant capacity, not the number busy on every callback.

Preparation measures median empty-batch dispatch cost over 32 batches. The first
chunk uses serial processing to measure channel cost; subsequent levels compare
estimated savings with twice calibrated dispatch cost. Costs use per-frame timing
and a bounded moving average. Small/cheap levels remain serial; no plugin brand,
project name, preset, hardcoded affinity or maximum hardware-thread policy is used.
There is one start/end clock pair per channel when a worker pool is prepared; serial
reference mode has no channel timing calls. P4's full per-plugin profiling remains
separate. Developer config can disable adaptation to exercise forced parallelism.

Windows helpers register MMCSS Pro Audio (without affinity or an extra priority
boost); successful registrations are counted. Both MMCSS and ordinary policy were
measured below. These offline runs do not establish a universally superior policy;
the conservative default retains Pro Audio and the user can compare Workers limits.

Late still counts callback wall time exceeding the device deadline. It is distinct
from a scheduler watchdog: helper completion waits at most 20 ms per submitted batch
by default (developer-configurable 1..1000 ms), with no retry. A watchdog timeout
silences the current output, pauses transport at the processed position and latches
a processing fault. Later callbacks output silence without touching worker-owned
plugin/buffers; recording marks a dropout. The footer requests audio reconnect.
Stop/close may wait on the control thread for the outstanding native process call;
only after quiescence may state be captured or the graph re-prepared. Arbitrary
hung plugins, including a plugin on the callback, still require later isolation.
The watchdog bounds host scheduling wait, not the duration of a native plugin call.

## Local verification

Offline cached configure and full Release build passed. CTest: 94/94, 22.25 seconds.
No special installed TH-U/Nuro/Xvox editor/state compatibility checks were run.

- Forced 2/4/8 participant modes match serial output and channel meters exactly
  (tolerance zero) across 1..128-frame blocks, 97-frame loops, playback, monitoring,
  unequal real DSP latency, bus/send fan-in, pre/post sends, Master/direct outputs,
  mute/solo/gain/send ramps, Seek/Stop and repeated reprepare/quiesce/destruction.
- A global allocation probe covers callback and helper threads; no operator-new
  allocations occurred during tested processing. This is a host/fixture observation,
  not a guarantee about allocations inside arbitrary third-party plugins.
- Worker FP modes, exactly-once job claims, aliased graph/cycle rejection, bounded
  timeout quarantine without retry, post-timeout quiescence/reprepare and raw input
  capture before parallel processing/PDC are checked.
- Existing transport/processing/streaming/recording, project/Undo/state/native-editor
  and JUCE J2/J3 reconnect regressions passed. Audio Workers binding and software
  preview were checked.

## Reproducible offline measurements

CPU: AMD Ryzen 7 PRO 4750U, 8 cores / 16 logical processors. Windows local MSVC
Release build; 48 kHz; no ASIO driver/device connected, no disk sources or installed
plugins. `mrs_channel_bench` uses deterministic stateful arithmetic DSP fixtures:
2/8 independent tracks, 8-deep serial bus chain, 8-way fan-in bus, heavy Master,
and cheap 2-track graph. Blocks: 64/128/256/512; requested participants: 1/2/4/8.
Each case has 100 warmup and 400 measured callbacks (tight offline loop, not paced
ASIO playback). There are 96 cases per policy, 192 total; both policies have zero
measured Late and zero worker timeouts. No XR/D hardware/disk observation is claimed.
Every case compares its complete output checksum with serial; detailed exact-sample
comparison is provided by the separate deterministic tests above.

The CSV files in the local package contain p50/p95/p99/max, Late, requested/actual
capacity, MMCSS registrations, checksum, parallel batch count and calibrated
scheduler overhead. Percentiles below are microseconds at 128 frames; this block's
48 kHz deadline is 2666.667 us. Parallel-batch totals include warmup. Do not infer
sustained ASIO acceptance or a guaranteed performance percentage from this matrix.

### MMCSS Pro Audio, 128 frames

| Topology | Requested / capacity | p50 us | p95 us | p99 us | Max us | Parallel batches |
|---|---:|---:|---:|---:|---:|---:|
| independent2 | 1 / 1 | 199.000 | 211.400 | 215.100 | 258.500 | 0 |
| independent2 | 2 / 2 | 115.400 | 140.500 | 142.000 | 248.800 | 499 |
| independent2 | 4 / 2 | 126.800 | 236.300 | 254.500 | 583.500 | 499 |
| independent2 | 8 / 2 | 115.500 | 140.900 | 244.000 | 295.000 | 499 |
| independent8 | 1 / 1 | 788.600 | 795.000 | 802.800 | 810.700 | 0 |
| independent8 | 2 / 2 | 420.200 | 726.400 | 1136.900 | 1351.500 | 499 |
| independent8 | 4 / 4 | 238.300 | 376.900 | 527.000 | 538.100 | 499 |
| independent8 | 8 / 8 | 158.000 | 422.200 | 649.100 | 880.800 | 499 |
| serial8 | 1 / 1 | 789.900 | 819.500 | 842.000 | 867.100 | 0 |
| serial8 | 2 / 1 | 788.600 | 809.100 | 831.200 | 852.600 | 0 |
| serial8 | 4 / 1 | 786.100 | 804.200 | 828.800 | 874.100 | 0 |
| serial8 | 8 / 1 | 786.100 | 802.600 | 836.200 | 873.900 | 0 |
| fanin8 | 1 / 1 | 834.000 | 875.100 | 893.900 | 975.600 | 0 |
| fanin8 | 2 / 2 | 458.400 | 878.200 | 1123.800 | 1391.200 | 499 |
| fanin8 | 4 / 4 | 283.200 | 470.900 | 755.400 | 812.400 | 499 |
| fanin8 | 8 / 8 | 253.400 | 436.500 | 482.900 | 703.400 | 499 |
| master8 | 1 / 1 | 796.600 | 821.300 | 851.400 | 876.500 | 0 |
| master8 | 2 / 2 | 796.700 | 819.400 | 846.600 | 871.100 | 0 |
| master8 | 4 / 4 | 795.200 | 815.400 | 845.000 | 876.600 | 0 |
| master8 | 8 / 8 | 796.000 | 841.100 | 913.400 | 1586.800 | 0 |
| tiny2 | 1 / 1 | 10.800 | 10.800 | 11.100 | 12.500 | 0 |
| tiny2 | 2 / 2 | 11.000 | 13.200 | 15.800 | 41.300 | 0 |
| tiny2 | 4 / 2 | 11.100 | 11.400 | 19.300 | 22.200 | 0 |
| tiny2 | 8 / 2 | 11.000 | 11.200 | 17.100 | 65.000 | 0 |

### Ordinary worker scheduling, 128 frames

| Topology | Requested / capacity | p50 us | p95 us | p99 us | Max us | Parallel batches |
|---|---:|---:|---:|---:|---:|---:|
| independent2 | 1 / 1 | 199.100 | 202.800 | 208.100 | 233.400 | 0 |
| independent2 | 2 / 2 | 112.600 | 140.700 | 179.900 | 255.600 | 499 |
| independent2 | 4 / 2 | 114.200 | 151.900 | 244.700 | 355.700 | 499 |
| independent2 | 8 / 2 | 116.300 | 145.900 | 220.300 | 256.700 | 499 |
| independent8 | 1 / 1 | 791.500 | 806.000 | 813.600 | 840.000 | 0 |
| independent8 | 2 / 2 | 428.100 | 696.200 | 1247.700 | 1337.400 | 499 |
| independent8 | 4 / 4 | 258.300 | 406.000 | 706.400 | 760.800 | 499 |
| independent8 | 8 / 8 | 274.100 | 426.100 | 555.400 | 772.000 | 499 |
| serial8 | 1 / 1 | 798.700 | 837.700 | 855.100 | 972.100 | 0 |
| serial8 | 2 / 1 | 803.300 | 889.400 | 944.700 | 1539.200 | 0 |
| serial8 | 4 / 1 | 786.800 | 810.400 | 837.100 | 867.500 | 0 |
| serial8 | 8 / 1 | 786.700 | 802.100 | 830.900 | 851.700 | 0 |
| fanin8 | 1 / 1 | 822.400 | 843.700 | 859.700 | 902.900 | 0 |
| fanin8 | 2 / 2 | 451.000 | 883.100 | 1186.700 | 1382.300 | 499 |
| fanin8 | 4 / 4 | 268.400 | 414.300 | 592.900 | 768.000 | 499 |
| fanin8 | 8 / 8 | 229.800 | 494.200 | 699.400 | 949.500 | 499 |
| master8 | 1 / 1 | 796.300 | 827.300 | 851.100 | 876.800 | 0 |
| master8 | 2 / 2 | 824.200 | 879.000 | 922.000 | 1038.400 | 0 |
| master8 | 4 / 4 | 794.900 | 822.200 | 845.500 | 863.800 | 0 |
| master8 | 8 / 8 | 794.800 | 820.600 | 849.300 | 879.900 | 0 |
| tiny2 | 1 / 1 | 10.900 | 13.900 | 15.500 | 53.100 | 0 |
| tiny2 | 2 / 2 | 11.000 | 11.300 | 15.400 | 17.500 | 0 |
| tiny2 | 4 / 2 | 11.100 | 11.700 | 15.400 | 16.100 | 0 |
| tiny2 | 8 / 2 | 11.200 | 11.300 | 12.400 | 12.900 | 0 |

Independent-channel medians improve, with remaining scheduling tails: e.g. at
128 frames, 8 independent channels with MMCSS take 788.6 us serial versus 238.3 us
with 4 participants (about 3.31x by median). Their p99 improves from 802.8 to 527.0 us;
2 participants instead have a worse p99 of 1136.9 us despite a better median.
For two independent channels the median is 199.0 -> 115.4 us with 2 participants.
The adaptive cheap graph issues zero parallel batches: median 10.8 -> 11.0 us,
though timing/OS noise increases its p99. The heavy Master stays effectively serial.
More workers are not automatically better, and p99 varies between the two policies.

## Delivery and acceptance

Package: `MR-Studio-0.1r-P1-parallel-JUCE-ASIO-Windows-local` under chat Builds.
Launch `Moon River Studio JUCE.exe`; keep `mrs_vst3_scan.exe` beside it. Previous
packages are preserved. Compare Workers 1 and 2 (4 if useful) on the same ASIO
buffer/project; TH-U and Xvox on separate channels, ONE on Master is a regression
case, not a scheduling rule. Listen at 128 and inspect CPU/B/XR/Late/D/W, then compare
256 without changing the plugin states. User listening acceptance on 2026-10-05:
at the same plugins and 128 frames, regular clicks disappeared; occasional rare
clicks remain, also experienced by the user in Studio Pro. P1/#51 is accepted and
closed. Worker setting and audition duration were not reported. Long sustained
mixed playback/monitor/recording and Fender Studio Pro parity remain unverified.
P2/P3/P4, dynamic-latency transitions,
process isolation and the wider #16 hardware matrix remain separately open.

Code, tools, tests, logs and packages remain local. GitHub receives only documentation
and issue updates; documentation commits use [skip ci]. No source push, PR, merge,
GitHub Actions, installation or dependency download was performed.
