# 0.1n / JUCE J1 — Windows local prototype

Historical J1 package, accepted by the user 2026-10-05. The current opt-in target
now opens [J2 / 0.1o](JUCE_J2.md); the accepted J1 package remains in Builds.

Parallel opt-in executable, sharing mrs_desktop/Application and the existing
model, history, transport and AudioEngine. Win32 0.1m upd1 fix1 remains the full DAW.
J1 has one demo channel, relative handle-only gain/pan, Ctrl fine movement,
Escape/focus cancellation, arrow keys, M/S, Undo/Redo, stereo peak meters and
Play/Stop (Space without repeat; Stop returns to the playback start).
Buttons use custom LookAndFeel and embedded Noto Sans (OFL).

The dedicated offline device thread renders the demo signal but discards output.
No sound is sent to hardware. ASIO selection, projects, arrangement, plugin browser
and native plugin editors are J2 work; use Win32 for those workflows.
BROWS currently toggles project information only. Full bottom-right navigation
is retained in Win32 and will migrate with the workspaces in J2.
No audio callbacks access JUCE components. JUCE replaces no audio/plugin host here.

## Dependency

Official JUCE 9.0.3, tag commit be29c81492b6151c8ea8d14c840e1311963b3a83:
https://github.com/juce-framework/JUCE/tree/9.0.3
Local source build/_deps/JUCE-9.0.3, archive build/juce-9.0.3.zip.
Archive SHA256 E12F3C39395480050A4FE50D820B248C0CFA32F65FD04D4C1B68EECFD4CC31F2.
Source dependency was explicitly downloaded before configure; no global installs.
Configure fails if the local pinned SDK is absent; it never fetches JUCE.

Local/private prototype uses the JUCE AGPLv3 option. This is no commercial
licence acceptance or product relicensing decision. Public source/binary release
requires resolving the project/distribution licence first. Do not publish this
prototype or push its source automatically. See the upstream LICENSE.md.

## Build

Use the existing MSVC/Windows SDK CMake cache:

```powershell
cmake -S . -B build/asio-local -DMRS_BUILD_JUCE_UI=ON `
  -DMRS_JUCE_SDK_ROOT="C:/Users/Vladislav/Documents/GitHub/MR-Studio/build/_deps/JUCE-9.0.3" `
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/asio-local --config Release --parallel 4
ctest --test-dir build/asio-local -C Release --output-on-failure
```

Default MRS_BUILD_JUCE_UI=OFF leaves the previous build independent of JUCE.
The J1 executable is MoonRiverStudioJuce_artefacts/Release/Moon River Studio JUCE J1.exe.
JUCE helper tools are built locally, not installed. Missing JACK/TRE/AAX diagnostics
from existing optional dependencies do not prevent this Windows build.

## Checks

juce_j1_smoke uses the actual JUCE components and a manual device to avoid competing
audio callbacks: rail clicks, relative Ctrl drag, preview without history mutation,
single release commit, Undo, Escape, engine Play/Stop return, stereo peaks and mute.
Resize checks cover 720x500 and 1350x900 logical bounds; component snapshot is exported.
Physical multi-monitor DPI, hardware audio and full accessibility acceptance remain J3.

2026-10-05: local offline configure/build passed; 88/88 CTest passed, then JUCE smoke
was rerun successfully after font/readout/snapshot export corrections. Existing hidden
Win32 GUI smoke exited 0. Reviewed 900x600 preview; 1.5x snapshot dimensions checked.
User J1 acceptance pending. No processing/persistence or existing host source changes.
