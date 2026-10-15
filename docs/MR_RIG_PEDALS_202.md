# MR Rig #202 вЂ” Compressor + Drive, first approved slice

User approved this subset on 2026-10-15 after accepting #201. #203 NAM is not
part of this work. Further pedals remain unavailable until separately implemented.

## Signal and state contract

The existing one-insert shared graph remains Input -> PRE -> one WAV Core/Mix ->
POST -> Output. Both pedals support either lane, reorder, duplicate, per-module
bypass, Undo/Redo, project/RigPreset/My Rigs save/load and the existing editor
lifetime. Nothing is inserted automatically. There is no second core or dry sum.

Stable type IDs are `mrs.compressor` v1 and `mrs.drive` v1. Project schema25,
RigState v2 and the outer preset/library formats remain unchanged. Parameters
are physical values in existing typed module arrays, with stable instance IDs.
Old Gain/HP/LP/Bell EQ arrays remain readable; unknown modules remain recoverable.
The private host rack transport now has seven fields per slot (type, bypass,
five parameter values): 119 parameters per Rig, at most 960 changes for eight
complete racks including host bypass. It is not a public automation identity.
Existing command/gesture preview and native sample-offset parameter events are
used; this slice does not add dedicated module automation lanes to the DAW.

## Compressor

| ID | Control | Range | Default |
|---|---|---|---|
| 0 | Threshold | -60..0 dBFS | -18 |
| 1 | Ratio | 1..20 :1 | 4 |
| 2 | Attack | 0.1..100 ms | 10 |
| 3 | Release | 10..1000 ms | 100 |
| 4 | Output | -24..24 dB | 0 |

Feed-forward stereo-linked peak detector, hard knee, no lookahead or automatic
makeup. Desired reduction in dB is max(0, inputPeakDB - threshold) * (1 - 1/ratio).
Attack/release are one-pole time constants on this reduction: exp(-1/(time*rate)).
The same reduction applies to both channels. Ratio1 with Output0 is unity.
There is no audio tail or algorithmic latency; gain-reduction control state
continues to release while processing silence.

## Drive

| ID | Control | Range | Default |
|---|---|---|---|
| 0 | Gain | 0..36 dB | 6 |
| 1 | Tone | 200..12000 Hz | 6000 |
| 2 | Level | -24..12 dB | 0 |

Symmetric soft clipping tanh(gain * input) / tanh(gain), then a one-pole low-pass
at Tone (clamped to 0.45 * sample rate), followed by Level. It is fully wet;
module bypass gives the original signal. Gain0 is still an enabled nonlinear
pedal, not a bypass. There is no oversampling: high-frequency nonlinear aliasing
is a known MVP limitation, so no band-limited or analog-model claim is made.
No lookahead/buffer delay is added; the tone filter has frequency-dependent phase
and a bounded decaying history, accounted for in the rack tail estimate.

## Preparation, smoothing and UI

Local audit found no existing Compressor/Saturator/Drive backend to reuse. New
native child DSP uses the established IProcessor contract. Both banks and all six
catalog processor types per slot are allocated/prepared off RT with the owning
graph. No new allocation/free/locks/I/O/UI or destruction is used in processing.

Pedal-only parameter edits preserve envelope/filter history and use 20ms parameter
smoothing; expensive coefficient updates are bounded to every16 samples. Structural
edits and bypass retain the 10ms PRE/POST bank transition around one WAV core.
Rapid structural edits coalesce. A dirty flag avoids comparing full rack snapshots
on every unchanged sample. Input/output nonfinite values and denormals are sanitized.
All currently available modules report zero algorithmic latency; no PDC change.

The existing compact editor and Theme remain. Compressor has five controls in two
rows, Drive three in one row; values/units/defaults come from descriptor metadata.
One gesture commits once; typed values/reset use existing JUCE bindings. Chorus,
Delay and Reverb remain disabled, and NAM remains unavailable.

## Validation and acceptance

**READY WITH MANUAL CHECK — 0.4f MR Rig Compressor + Drive.**

- All-target local Release build PASS; focused CTest7/7 PASS8.75s.
- Full current CTest **155/155 PASS124.57s**, including ai_authorization.
- Descriptor/native ranges/defaults, project/preset/library compatibility, full
  eight-Rig transaction, unsupported module preservation and Undo: PASS.
- Independent compressor transfer/attack/release/output and Drive clip/tone/level
  numerical oracles at44.1/48/96k mono/stereo: PASS. Stereo link/channel isolation,
  native parameter-event offsets, nonfinite/silence, PRE/Core/POST parity, per-pedal
  bypass and64 repeated edit/bypass transitions: PASS, callback alloc/free0/0.
- 100 concurrent pedal snapshot edits on PreparedGraph: PASS, max callback272us
  at128frames/48k in the full run, alloc/free0/0. Offline synthetic sparse200ms IR,
  16 mixed pedals per Rig,1/2/4Rigs at128/256/512/1024frames: PASS, heap0/0.
  Example4Rigs/128: mean773.404us, max1696.3us vs2666.67us buffer duration.
  This is an offline measurement, not a hardware deadline/xrun guarantee.
- GUI: five-control Compressor, three-control Drive, gesture Undo/Redo, move across
  Core, exact preset/project reload and editor lifetime: PASS. Software screenshots
  at100/125/150/200%, min940x640/max1800x1200. Minimum,150% and maximum plus Drive
  screenshots inspected. Existing Theme and artwork retained.
- Package `Builds/MR-Studio-0.4f-MR-Rig-Pedals-JUCE-ASIO-Windows-local`:
  packaged Rig smoke and diagnostics/J3 smoke both PASS exit0. Matching GUI symbols
  retained in `build/asio-local/deliveries/0.4f-rig-pedals/`.
- Logs, test detail and screenshots: `build/asio-local/validation/rig202/`;
  build log: `build/asio-local/rig202-final-build.log`.

Hardware ASIO, guitar-DI listening A/B, real xruns, physical mixed-DPI and a
screen-reader session: **NOT TESTED**. Drive aliasing limitation above remains.
User acceptance of this subset is pending; #202 remains open for later separately
approved subsets. Source/assets/builds/tests/commits stay local; GitHub only
receives this contract and issue status, no Actions/source push/PR/merge.

Manual acceptance: restart the shortcut into0.4f, open MR Rig and choose a real
source. Add Compressor and Drive, compare enabled/bypassed at matched output
level, change attack/release/tone/level, reorder/move PRE/POST, Undo/Redo, Save Rig,
reload from My Rigs and close/reopen the editor. No further pedal slice or NAM
stage begins automatically.

