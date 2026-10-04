# 0.1i upd1 — arrangement/mixer layout and wheel navigation

Local update accepted by the user, 2026-10-04: “Все работает”. The user accepted local 0.1i
(“Все работает!”) and requested a starting UI direction based on their Studio
Pro screenshot. This update uses existing working commands and mixer controls.

## Layout

Compact two-row toolbar, wide track headers directly beside the timeline,
arrangement above a docked lower mixer, Master at the right and transport below.
Flat grey panels, blue selection/channel accents, bar/beat grid and narrower
vertical mixer strips follow the supplied reference as an initial direction.
No placeholder inserts, automation, plugin or show buttons are added. Existing
Files commands, Arrange/Edit/Mix, transport, recording, track editing, sends,
hardware outputs and Audio settings/profiles remain usable.

Track headers keep gain, horizontal meter, pan knob and mono input selection.
Their M/S buttons call the existing shared mute/solo commands and support Undo.
Click the header to select a track; use the top rename field/button to rename it.
Mix shows/hides the lower panel in the same main window. Edit retains the clip
inspector and track list. Header/clip/input hit regions follow track height and
are clipped above the mixer so obscured controls cannot receive clicks.

## Wheel bindings

| Gesture | Arrangement/header | Above mixer |
|---|---|---|
| Ctrl+Shift+wheel | Horizontal time zoom around pointer | Horizontal time zoom |
| Ctrl+wheel | Vertical track zoom | Vertical track zoom |
| Shift+wheel | Horizontal time scroll | Horizontal mixer-channel scroll |
| Wheel | Vertical track scroll | Vertical arrangement-track scroll |

Wheel up zooms in or scrolls left/up; wheel down zooms out or scrolls right/down.
Track heights range from 92 to 320 logical pixels (112 initially); the minimum
keeps all current header controls readable. Waveforms and recording blocks grow
with the row. Time zoom stays within 0.25 seconds to 24 hours per view. Small
wheel deltas accumulate independently for each gesture. Zoom/scroll do not
edit audio, change selection/history or dirty the project. Zoom buttons and Fit
remain available; Fit resets horizontal framing. View state is session-local.

## Verification

- Local Windows x64 ASIO Release build with existing offline cached dependencies passed.
- CTest 69/69 passed; audio/model/schema/profile implementation is unchanged.
- GUI smoke passed: actual WM_MOUSEWHEEL dispatch for all four modifiers,
  pointer-anchored time zoom, independent track height/time scale, partial wheel
  deltas, zoom bounds, row/header hit-test agreement, mixer modifier behavior,
  header M/S/Undo, minimum-window 150% DPI bounds.
- Existing preview/release/native-layout and Audio settings flicker regressions passed.
- Actual application rendering exported with --smoke-test --render-preview and
  reviewed visually; no user's saved preferences or physical stream are used
  for the preview. Local package includes UI-preview.png.
- User review of the new layout/wheel navigation passed (2026-10-04).

Package: MR-Studio-0.1i-upd1-ASIO-Windows-local. Branch:
mrs/0.1i-upd1-layout-local. Earlier 0.1i package is retained.
GitHub issues/docs only; no code push, new PR, merge or GitHub Actions.
The deferred physical/performance matrix #16 is not declared passed by UI tests.
