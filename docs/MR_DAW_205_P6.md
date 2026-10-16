# MR Studio #205 P6 — P1–P4 review

Authorized 2026-10-16. P1–P4 accepted individually; Live/P5 explicitly deferred.
This is a bounded GUI review, not an authorization for a full CTest/release gate.

## Reviewed scope

- P1 shell/navigation/transport, statuses, keyboard focus and shortcuts.
- P2 Arrange/compact headers, actual waveform/MIDI previews, shared clip palette,
  thin fader/pan contours and preserved white centre deviation.
- P3 Mixer routes, Master hardware labels, sends, power glyphs and owned VST3
  host toolbar. Vendor editor rendering and DSP remain outside visual adoption.
- P4 Browser search/path/list surfaces, preset menus and owned MR dialogs.
  Windows FileChooser stays native. No standalone Inspector was introduced.

No new C++ or assets are required by the review. Compiled code baseline:
`dbbadcad1ac5daaae7d1f48c4e06b32ff2010c52`.
EXE SHA256: `7a72034f9c399eff672974224786307ec5624de84f8dd67d9ff4f4c59c4552a0`.

## Evidence and checks

Run only existing `juce_uds_shell`, `juce_svg_runtime` and `juce_j3_smoke` on the
current EXE, without rebuilding unchanged code. Exact outcomes and durations are
recorded in #205 and local `build/asio-local/validation/ui205-p6/focused.log`.
The GUI smoke includes existing software size/scale captures and real gestures,
Undo, Save/Open, routing labels and Theme attachment/lifetime checks. The Shell
fixture covers transport/count-in/record and keyboard restrictions; J3 covers
synthetic VST3, sends, preset/file workflows and editor retirement.

Local `review.html` and `evidence.json` collect current PNGs with dimensions and
hashes. Representative Arrange/Mixer/Browser/dialog/host captures are reviewed;
not every state/DPI combination is claimed manually inspected.

## Remaining limits / delivery

- Browser selected-row screenshot can be empty because TreeView layout updates
  asynchronously. Do not claim selected-row pixel coverage from that capture.
- No fresh physical ASIO/listening/xrun, screen-reader or hardware mixed-DPI test.
  Previous user ASIO acceptance is retained but is not new P6 hardware evidence.
- No full CTest, clean/configure build, packaging or shortcut changes authorized.
  Main desktop shortcut still points to the earlier P2/upd1 package; use the
  current Release EXE explicitly to inspect P3/P4.
- User-owned skills/spec/AGENTS/.gitignore edits remain untouched and uncommitted
  by this review. No source push, Actions or PR.

P6 GUI review awaits user acceptance. #205 remains open with Live/P5 deferred;
this review does not claim full DAW/Live/native-plugin migration complete.
