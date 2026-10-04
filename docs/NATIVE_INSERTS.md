# Native inserts / MRS 0.1k / Stage 3a

## Model and signal path
Core snapshot v8 adds ordered insert chains to audio tracks, buses and Master.
Each insert has a stable ID, kind, parameters and bypass. Up to eight inserts per
chain, 32 total. SetInserts uses the shared ProjectStore command/Undo history;
add/remove/reorder/bypass/parameter edits and save/reopen preserve order and state.
MIDI audio inserts, duplicate IDs, unsupported types and out-of-range values are rejected.
v8 reads v1-v7 with empty chains. Keep a project copy: old builds cannot read v8.
Archive container and legacy shared graph formats are unchanged.

Each native chain is converted into the existing shared GraphState/PreparedGraph,
not a separate audio engine. Track inserts process playback + input monitoring
before channel gain/pan/mute and pre/post-fader sends. Bus inserts process the sum
before its controls/routes. Direct hardware outputs use their channel inserts and
bypass Master inserts. Legacy master processor graph is retained; the new Master
chain follows it and precedes Master gain and physical output mapping/limiting.
Raw recorded WAVs are captured before all monitoring/mixer/insert processing.

## Effects and parameters
- Gain: linear 0–4 internally; UI -60 to +12 dB, or -inf for silence.
- High-pass and low-pass: second-order biquad, frequency 20–20000 Hz, Q 0.1–10.
- Parametric EQ: one peaking band, frequency/Q as above, gain -24 to +24 dB.

Filters use the [W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/)
coefficient equations, normalized transposed direct form II with double state.
Frequency is limited to 0.45 * sample rate at preparation/update for low rates.
Each channel has separate state; nonfinite/very small state is sanitized.
These processors report zero sample latency; frequency-dependent phase remains.
Stop/Seek resets filter state through the existing panic/reset path.

Runtime storage/coefficients are prepared off callback. Chains have fixed, bounded
scratch and event queues. The callback does no allocations, locks or file I/O.
Track/bus chains process in topological routing order per frame; Master processes
the mixed block. This simple native slice does not provide plugin latency compensation,
IR convolution, continuous parameter automation/crossfade or arbitrary plugin hosting.
The deferred physical performance matrix is not claimed measured.

## UI and changes
Mix: Inserts (N) on every audio/bus strip and Master opens its owned editor.
Choose effect kind, Add, select the slot, edit applicable parameters and Apply.
Move up/down, Bypass/Enable and Remove work through one shared command per action.
Structural and parameter edits require Pause/Stop and preserve the open device,
play position and Stop anchor. Playback/recording disables editor changes.
Typing in the editor/settings does not trigger main transport shortcuts.

Plain wheel scrolls 32 logical pixels per notch, including fractional wheel deltas,
with partial rows, synchronized headers/clips/waveforms/hit tests and bounds.
Ctrl+wheel still changes track height, Ctrl+Shift+wheel horizontal zoom,
Shift+wheel horizontal scroll (mixer channels over Mix).
Space toggles Play/Stop, returning to the start; held-key auto-repeat is ignored.
Pause remains a separate command/button.

## Validation
Local cached offline-dependency Windows x64 ASIO configure/build passed.
CTest 77/77: insert model/archive/Undo/migration/rejection, filter pass/stop response,
EQ boost/identity, stereo-state separation, bypass/low-rate stability, native capture,
track/bus/Master/direct outputs, raw recording, position retention and zero RT allocations.
Expanded hidden GUI smoke covers actual insert commands, parameter Apply, reorder,
bypass/remove/Undo, Master isolation, editor bounds, fractional wheel geometry,
actual Space messages and auto-repeat, plus earlier DPI/flicker regressions.
Main/insert editor preview images were exported and reviewed.
Local 0.1k / Stage 3a accepted by the user on 2026-10-04: MRS_STAGE_3A_CHECKLIST.md.
