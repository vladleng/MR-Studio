# 0.1o / JUCE J2 — Windows desktop workflow

J1 / 0.1n was accepted by the user on 2026-10-05. The parallel JUCE target now
opens the desktop workflow, sharing the existing Application, project/history,
AudioEngine, native DSP, persistence, ASIO/PortAudio and VST3 host. No second audio
engine or project format was added. The Win32 0.1m upd1 fix1 package is preserved.

## Implemented surface

- File New/Open/Save/Save As/WAV import/Open recent; compatible .mrsproject files
  and project-owned Media folders. Unsaved changes offer Save/Discard/Cancel.
- Arrange/Edit and docked Mix; Arrange/Edit/Mix/BROWS at bottom right. Mix toggles
  the embedded mixer, without creating a separate mixer window. BROWS shows/hides
  the right browser and changes arrangement width; drag its left divider to resize.
- Shared multiple track/bus/Master strips; track mini-panels with M/S/R/I, input
  selection, horizontal gain and stereo peaks, relative pan dial. Mixer vertical
  handle-only faders, Ctrl fine movement, live preview, Escape/focus cancellation
  and one Undo command. Release keeps the previewed level until model publication.
- WAV waveforms with separate source channels; clip selection, drag move/trim,
  split/delete, track add/delete/rename/reorder, snap, zoom and pixel scrolling.
- Plain wheel = vertical scroll; Shift+wheel = horizontal scroll; Ctrl+wheel =
  track-height zoom; Ctrl+Shift+wheel = horizontal zoom anchored at the pointer.
- Track-to-bus/Master/direct hardware routing, existing/new return sends, send
  gain, pre/post toggle and removal. Invalid/cyclic routes retain core validation.
- Play/Pause/Stop/record, section navigation/loop; Space toggles Play/Stop without
  repeat, returning to play start. Ctrl+Z/Y, Ctrl+S/O/N, S split, R record and Delete.
- Existing ASIO device selection/rate/buffer/physical inputs/outputs/control panel.
  Track arm/monitor/input selectors feed the existing recording and monitor graph.
- VST3 browser search, initially collapsed vendor folders, asynchronous isolated
  scanner, drag/drop to track/bus/Master; insert single-click opens the native HWND
  editor, repeated clicks reuse it. Host parent is retired before plugin unloading.
- Native Gain/filter/Channel EQ/Cab IR editors; three-band EQ plus HP/LP summed
  curve, point frequency/gain drag, wheel Q, band enable and live preview/Undo.
  Cab IR import/embedded state/mix/gain/cuts/polarity/presets. Insert bypass,
  remove/reorder and generic VST3 parameter editing use the existing commands.

First three insert names are visible in each strip. The Inserts menu exposes all
eight slots for editing. Structural edits and opaque VST3 state save use Pause/Stop;
native parameters and mixer gain/pan remain live, with existing core restrictions.

## Audio and acceptance boundaries

The app starts with a rendered demo and **Offline clock (no sound)**. For audible
playback, select the installed ASIO driver in Audio settings, a rate matching the
project, buffer and physical output numbers, then Connect. Inputs can be empty for
playback; enable physical inputs for monitor/record. Arm tracks before recording.
Opening a project switches to offline mode; reconnect ASIO explicitly afterward.
No WASAPI fallback, driver installation or automatic hardware activation is added.
Legacy recent projects and VST3 cache are read from the existing local data folder;
JUCE preferences use a separate file. Profile-management UI is not exposed yet.

User J2 acceptance is pending. Windows hardware recording/playback, the intended
TH-U preset/sound, physical multiple-monitor DPI, complete accessibility and the
remaining preference/profile polish are J3 parity gates before switching defaults.
VST3 runtime remains in process; full isolation/PDC remain Stage 3e, not this UI port.
Stage 3c/#23 and deferred #16 are not closed by the JUCE migration.

## Local build and dependencies

Pinned local JUCE 9.0.3, Noto Sans OFL font and existing cached Steinberg/PortAudio/ASIO
sources from J1. No new downloads/installations or GitHub Actions in J2. Configure
uses MRS_BUILD_JUCE_UI=ON, MRS_BUILD_VST3=ON, MRS_BUILD_ASIO=ON and
FETCHCONTENT_FULLY_DISCONNECTED=ON in build/asio-local. JUCE is opt-in by default.

```powershell
cmake -S . -B build/asio-local -DMRS_BUILD_JUCE_UI=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/asio-local --config Release --parallel 4
ctest --test-dir build/asio-local -C Release --output-on-failure -j 4
```

Executable: build/asio-local/MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE.exe.
Keep mrs_vst3_scan.exe beside it. Package MR-Studio-0.1o-JUCE-J2-ASIO-Windows-local
is in the chat workspace Builds directory. Code/commits/packages are local; GitHub
documentation/issues only. JUCE product/distribution licensing remains undecided;
this remains the local/private AGPL prototype, without commercial acceptance or
automatic public source/binary release.

## Verification — 2026-10-05

- Offline configure/build with MSVC/Windows SDK passed; own UI source uses /WX.
- 89/89 CTest passed, including retained J1 and new actual J2 component/engine tests.
  Component tests rerun after final UI/test corrections; stale-executable runs were
  superseded by builds that stop the verification command on compile failure.
- Multiple tracks/buses/sends, fader history/Undo, arm, zoom/scroll, BROWS layout,
  collapsed vendors, VST3 fixture drop/native dimensions/reuse, live EQ preview/
  commit, WAV import and L/R peak data, project media/plugin roundtrip, split/delete,
  Space repeat rejection/Stop return and minimum layout checked.
- Reviewed full UI snapshot; test asserts actual waveform pixels, not only data.
- Installed Nuro Audio Flexion and TH-U load/process/native editor/reuse/opaque state
  capture/reopen/reopened editor checked in separate timed local runs. This does not
  claim audition of the user's preset or a repeat of the full installed-plugin matrix.
- TH-U initially timed out in a synchronous test lacking a Windows message pump
  between editor cleanup and module reload. Timed test now dispatches deferred UI
  cleanup messages before reloading; successful runs supersede those timeouts.
- Existing hidden Win32 GUI smoke exited 0. Core/ASIO/VST3/persistence source unchanged.
