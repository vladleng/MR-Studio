# MRS Stage 0 — DAW Foundation (#20)

## Stack decision / spike
C++20 application controller plus a native Windows 10/11 x64 Win32 shell.
GDI double-buffered drawing provides the initial flat dark timeline; native
controls supply keyboard focus, text editing, file pickers and ASIO settings.
No browser/web server, new audio engine, GUI DLL distribution or additional
GUI package download is needed. The executable statically links existing libraries.
The foundation uses Segoe UI, graphite panels, blue/violet workspace selection and
amber playhead, following UI_UX_CONCEPT.md.

This stage proves UI integration, not the final drawing framework. Win32 currently
limits the graphical shell to Windows and requires explicit layout/accessibility
work. The platform-independent controller is tested on Linux too. A future UI
toolkit can replace the Win32 view without changing shared application services.
GPU rendering, rich editors and full custom accessibility are later decisions.
No claim that GDI is the final waveform/editor renderer.

Sources for the native spike:
- https://learn.microsoft.com/en-us/windows/win32/learnwin32/dpi-and-device-independent-pixels
- https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
- https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint

## Structure and ownership
apps/studio-desktop/include/mrs/desktop.hpp: application/controller contract.
apps/studio-desktop/src/application.cpp: project/device/file commands and preferences.
apps/studio-desktop/src/win32_main.cpp: Windows views/navigation/layout.
core/audio/src/offline_device.cpp: SHARED offline clock adapter.
tests/desktop_tests.cpp: application integration tests.

Application owns one ProjectStore, EngineTransport, AudioEngine, GraphStore and
MusicalTimeline per open project. Arrange/Edit/Mix/Live use those same services.
Workspace switching changes a UI enum only, never resets playback or clones state.
Project edits use the existing command/Undo contract; track rename is exposed.
Device callbacks run independently of the UI, and logging/config/file dialogs stay
on the application thread. The shared offline adapter renders into discarded
preallocated buffers on its own development clock; it never produces hardware sound
and is not a timing/performance benchmark.

## Current UI
Arrange: core clips/sections/chords, moving playhead, click to seek.
Live: moving shared chord strip, sections and common transport; no setlist editor.
Edit: read-only clip inspector. Mix: read-only processor/parameter state.
These are workspace views, not completed arrangement/mixer/MIDI/plugin editors.
Tracks/rename/Undo/Redo, project Open/Save/Save As, Demo and Open WAV are common.
Space = Play/Pause, Ctrl+S = Save, Ctrl+Z/Y = project Undo/Redo outside name editing.
Native edit controls retain their normal typing/Undo behavior.

The demo has explicit fixture harmony/sections and a quiet 220 Hz tone. Harmony
is not inferred from a WAV. Open WAV creates a new audio project with no authored
chords/sections and preloads/validates PCM/float WAV through the shared decoder.
Waveforms, clip editing, recording, resampling and MIDI editing are later stages.

Project documents use the accepted Stage 4 archive. Unknown chunks/state survive
Open/Save. Open starts stopped using the silent offline clock; it never auto-connects
ASIO or a plugin. Explicit Connect prepares the shared graph and preloads audio.
Missing media or unavailable processors produce an error and leave audio disconnected
rather than silently omitting assets or substituting processors. The project remains
open and can be saved without losing unavailable plugin state.

Absolute media paths from Open WAV are preserved. Relative sources resolve against
the opened project folder. Save As to a different folder is currently refused when
relative media would be broken; same-folder Save/Save As works. No asset copying/
consolidation yet. Dirty project replacement/close prompts to save or discard.

## Audio settings
Desktop starts in Offline clock (no sound). The ASIO artifact also enumerates the
same native vendor ASIO backend as Stage 1. User selects device, rate, buffer,
one-based physical output selectors (e.g. 1,2) and optional monitor input
(0 disabled, 1..64 physical channel). Connect validates configuration then stops
the previous callback, prepares graph/assets, opens and starts the selected backend.
Transport is reset on connect/disconnect/reconfiguration. No automatic host fallback.

Requested rate must match project/WAV rate; there is no resampler.
ASIO panel first disconnects audio; reconnect after panel/driver changes.
The dialog shows actual output latency and CPU; main status shows callback/underrun
counters. Native monitoring shares the same prepared processor graph. Monitoring
is optional and only enabled by choosing an input and pressing Connect.
Driver failures are reported; seamless reconnect/recovery remains deferred #16.

Preferences live in %LOCALAPPDATA%/MoonRiverStudio/desktop.cfg (versioned, bounded
reader), logs in studio.log. Workspace, requested device/rate/buffer/channel settings
are retained; device handles and playback state are never restored.
Preferences failures are reported and a damaged config falls back to defaults.
Config is convenience state, not crash-safe project storage. No audio callback logs.
Project autosave scheduling/recovery chooser UI is not included yet; accepted shared
autosave/recovery APIs remain available.

## DPI / verification
PerMonitorV2 manifest, GetDpiForWindow, DIP-scaled layout/fonts and WM_DPICHANGED
suggested rectangles. Main and audio windows track their own DPI. Initial main
window is clamped to monitor work area; minimum logical size 1000x620.
At short window heights only the first timeline tracks fit; scrolling/full editor
layout comes with Audio Arrangement. This is a foundation for 100/125/150% scaling.

Six desktop suites join all 41 previous contracts on Windows/Linux Debug/Release
and Windows ASIO Debug/Release. Offline Windows builds also run a GUI smoke test
that opens all workspaces/settings, renames a track and exercises 150% layout.
ASIO builds do not open a driver in CI. Hardware playback and visual DPI acceptance
are manual checks from MRS_STAGE_0_CHECKLIST.md. Existing Stage 1 performance gate
remains pending/nonblocking by the user's decision.

Build:
cmake -S . -B build -DMRS_BUILD_ASIO=ON
cmake --build build --config Release
Run build/Release/MoonRiverStudio.exe. Without MRS_BUILD_ASIO only the explicit
offline adapter is available. Linux builds the controller/contracts, not a GUI.
