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

Local Windows update, 2026-10-05; user review pending.

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
User ASIO/project review remains pending; 3e2/3e3 and whole #23 remain open.
Code/builds stay local. GitHub documentation/issues only, no source push or Actions.

