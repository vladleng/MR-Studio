# 0.1u P4: local synthetic measurements

2026-10-06; source ef11071bd221e30f822c2fd09e4689bfbf0b98ad.
Windows 11 Pro 10.0.26200, AMD Ryzen 7 PRO 4750U, 8 physical / 16 logical cores;
Windows x64 MSVC Release, cached JUCE/VST3/ASIO build, 48 kHz. No audio driver
opened by fixtures: immediate channel calls or high-resolution paced callbacks.
Synthetic stateful Work processors, deterministic input and long float WAV;
no installed plugin/version/preset used. Scheduling has no vendor/project rule.
Full methods and limitations: [profiling contract](ENGINE_PROFILING.md).

## Complete matrix

| Fixture | Profile | Cases | Measured calls/case | Late | Worker timeouts | Ahead/disk misses | Exact PCM blocks |
|---|---|---:|---:|---:|---:|---|---:|
| channel | OFF | 112 | 400 | 14 | 0 | n/a | checksum + regression |
| channel | ON | 112 | 400 | 13 | 0 | n/a | checksum + regression |
| paced playback/mixed | OFF | 210 | 120 | 13 | 0 | 0 / 0 | 33600 |
| paced playback/mixed | ON | 210 | 120 | 13 | 0 | 0 / 0 | 33600 |

Channel: 100 warmup, 7 topologies x 4 buffers x Workers 1/2/4/8 = 112 cases/run.
Paced: 40 warmup, 7 topologies x 4 buffers x requested Workers 1/4 x valid Process
Off/256/1024/4096 = 210 cases/run. Active workers are capped by available work.
All 67,200 paced blocks, including warmup, matched the serial reference exactly.
Channel checksums also match between workers and OFF/ON; exact parallel PCM,
PDC and ownership are separately tested. Late counts use measured calls only.
No failed first fan-out setup is included: sends now target valid buses.

## Callback wall times, profile OFF

48 kHz / Device 128, values in microseconds. These are fixture observations,
not installed-project performance or process CPU percentages.

| Channel graph | Workers | p50 | p99 | max | Late |
|---|---:|---:|---:|---:|---:|
| independent8 | 1 | 807.900 | 1341.700 | 2475.300 | 0 |
| independent8 | 4 | 272.200 | 702.500 | 850.100 | 0 |
| serial8 | 1 | 817.800 | 1347.100 | 2696.500 | 1 |
| serial8 | 4 | 809.400 | 1499.100 | 2319.000 | 0 |
| fanin8 | 1 | 842.300 | 1608.100 | 2603.400 | 0 |
| fanin8 | 4 | 277.000 | 612.900 | 846.600 | 0 |
| fanout8 | 1 | 819.000 | 1335.500 | 1835.200 | 0 |
| fanout8 | 4 | 356.600 | 796.200 | 1313.700 | 0 |
| master8 | 1 | 818.800 | 1166.000 | 1430.400 | 0 |
| master8 | 4 | 818.200 | 952.500 | 1156.300 | 0 |
| tiny2 | 1 | 11.400 | 15.100 | 15.800 | 0 |
| tiny2 | 4 | 11.300 | 13.000 | 13.100 | 0 |

| Paced graph | Workers | Process | p50 | p99 | max | Late |
|---|---:|---:|---:|---:|---:|---:|
| mixed8 | 4 | 0 | 234.600 | 572.100 | 778.000 | 0 |
| mixed8 | 4 | 1024 | 146.800 | 406.900 | 658.900 | 0 |
| mixed_disk8 | 4 | 0 | 251.400 | 518.400 | 539.700 | 0 |
| mixed_disk8 | 4 | 1024 | 159.200 | 322.400 | 327.500 | 0 |
| mixed_master8 | 4 | 0 | 611.700 | 1441.300 | 1453.600 | 0 |
| mixed_master8 | 4 | 1024 | 546.700 | 1498.900 | 1501.100 | 0 |
| master8 | 4 | 0 | 450.900 | 1191.100 | 1233.700 | 0 |
| master8 | 4 | 1024 | 18.500 | 74.700 | 226.200 | 0 |

Independent tracks and fan-in/out benefit from parallel work. A serial chain and
heavy Master remain bottlenecks. In mixed_master8, p50 decreases but p99 increases
in this run; anticipation does not remove callback-owned shared Master cost.
Callbacks per second cannot be inferred as achieved throughput from paced calls;
they are paced at the device deadline. Unpaced wall time describes callback cost.

Observed median ON-minus-OFF channel p50: +1.30 us
(+0.47%); per-case relative differences span
-20.33% to +41.77%. The two sequential passes include
scheduler/thermal/background variation. Negative differences are noise, not
profiler speedup. This is an overhead observation, not a controlled isolated
instrumentation-cost estimate. Keep detailed profiling OFF for normal use.

Late events across both matrices are retained in CSV. They are not producer
underruns: every paced sample was compared; all worker/disk/producer misses are
zero. No no-click guarantee, sustained ASIO acceptance or Fender parity follows.

## Sustained fixtures

Both selected cases ran with profiling ON, Workers 4, 48 kHz / Device 128,
40 warmup + 22,500 measured callbacks. Each complete block matched serial PCM;
no comparison was skipped. Source WAV contains 62 seconds of float stereo data.

| Case | Process | paced wall s | p50 us | p95 us | p99 us | max us | Late/U/D/timeouts | exact blocks | raw frames/fault |
|---|---:|---:|---:|---:|---:|---:|---|---:|---|
| mixed_disk8 | 1024 | 60.107791 | 203.300 | 305.000 | 354.700 | 1257.300 | 0/0/0/0 | 22540 | 2885120/0 |
| disk8 | 4096 | 60.107580 | 20.400 | 36.600 | 61.800 | 298.300 | 0/0/0/0 | 22540 | 0/0 |

Mixed raw recording additionally verified every mono sample equals the original
0.2f input, exactly 2,885,120 frames, no capture fault. Sources/captures were
unique temporary fixture WAVs, removed after verification. Observed maximum
callback gaps: 3858 us (mixed record), 3696 us (playback); no burst callbacks.


Hardware ASIO monitoring/recording, representative installed effects and sustained
transport/reconnect/automation acceptance remain open on #54 / broader #16.

