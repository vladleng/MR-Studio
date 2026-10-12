# MR Reverb active CPU follow-up — 0.3c upd3

Следующее дополнение по запросу пользователя — **0.3c upd4**, optional wet
Input gate и новый ход Mix: [REVERB_GATE_MIX.md](REVERB_GATE_MIX.md).
Этот отчёт сохраняет измерения и границы предыдущего кандидата upd3.

2026-10-10, #132/#128. Source `3c6903cf39985f1f504214759af59e7ad6590794`,
local branch `mrs/0.2g-fix1-editor-space-local`. User requested lower active CPU,
not merely silence savings. User Stop CSV measured guitar Reverb ~0.67–0.69 ms
per 256 frames downstream of TH-U; no input amplitude/DC evidence was exported.

## Change

Large nonuniform tiers previously serviced incremental jobs on every sample,
with small FFT/MAC batches and repeated scheduler bookkeeping. Service bounded
32-sample slices with 32× original quota, retaining the same work allowance per
32 frames, owner-before-alias ordering and P-sample slack. The immediate 128 tier
is unchanged. Arbitrary callback boundaries do not determine slice boundaries.
Direct 128-tap head uses four independent double SIMD accumulators, reducing
dependency chains while retaining double accumulation. No locks, I/O, heap work,
new workers, schema changes, shortening IR, amplitude gate or added latency.

This lowers active convolution cost including nonzero upstream idle signals.
It does not change exact-zero idle eligibility and cannot guarantee that TH-U
output becomes silent. Offline timing is not ASIO meter or whole-system CPU.
No brand/preset-dependent processing policy was introduced.

## Measurements

Three sequential paired baseline/final DSP profiles, same private quad Large Hall
189378 frames, 48 kHz, Ryzen 7 PRO 4750U. Median of mean microseconds:

| Block | upd2 signal | upd3 signal | Reduction | upd2 tiny signal | upd3 tiny signal |
|---|---:|---:|---:|---:|---:|
| 128 | 153.318 | 119.804 | 21.86% | 161.739 | 118.601 |
| 256 | 303.977 | 234.855 | 22.74% | 305.460 | 235.502 |
| 512 | 624.734 | 467.237 | 25.21% | 628.766 | 473.708 |

Tiny input around −140 dBFS is deliberately processed. Drained silence at256:
7.715 → 7.849 us, essentially unchanged. Full private impulse/tail oracle max
error 8.9407e−8; callback allocation/free0/0. OS scheduling spikes persist in
individual maxima; no hard deadline guarantee or universal Live readiness claimed.

Expanded engine benchmark with synthetic dense 8s True Stereo48k now includes
3 instances and256 frames. Correct steady output checked every measured block;
fault/timeouts0. At3/256: Workers1 mean970.663/p951658.4/max2942.7us,
Workers4 mean708.488/p951416/max2983.7us, deadline5333.33us. These are graph
measurements, not a comparison against the user's TH-U/Omnisphere project.
Longer benchmark source duration avoids running out after added256 measurements.

## Validation and delivery

Release full build PASS. Full CTest **126/126 PASS (138.39s)** on final production
DSP. Subsequently rebuilt benchmark target after expanding only optional benchmark
branches; engine benchmark PASS. Numerical mono/stereo/True Stereo and swept-sine,
44.1/48/96k, irregular blocks, 20s tails, silence/resume, tiny signals, handoff,
concurrent publication, Pre-delay/Tone, transport/PDC/export, library/state/GUI
and Cab IR regressions PASS. Private library134/134 quad+L/R pairs PASS, max
DSP error1.19209e−7, channel identity error0, alloc/free0/0. User IR read-only.

Package `Builds/MR-Studio-0.3c-upd3-MR-Reverb-Active-CPU-JUCE-ASIO-Windows-local`
in the chat workspace. 13files,12 payload hashes verified; no pictures/PDB/tests/IR.
GUI SHA256 `5C5E1C0F68647D1CDB55D28791B4CCFF34F8EF32EC7D14C5947D666784E63F8A`.
Packaged `--reverb-editor-smoke` exit0; screenshots in validation directory.
Matching GUI/crash symbols: `build/asio-local/deliveries/0.3c-upd3-reverb-active`.
Baseline/sliced profiler executables, intermediate and final logs/paired JSON:
`build/asio-local/validation/reverb-active`. Intermediate float-accumulator experiment
was discarded; final release retains double accumulation.

Actual ASIO playback/Stop CPU with three inserts and TH-U **NOT RUN for upd3**;
user tests the same project without changing IR/device/buffer/workers. Save and
close upd2 before opening upd3. Prior packages remain intact. #132/#128 open until
user acceptance; #86/#25 not started. Code/docs local, no push/merge/Actions.
