# MRS Stage 0 — Windows acceptance
Download/extract MR-Studio-MRS-Stage-0-ASIO-Windows from ASIO audio prototype.
Launch MoonRiverStudio.exe. This is now a desktop window, not a console checker.

1. Startup: demo project is visible, transport stopped, Offline clock (no sound).
   Arrange shows chords/sections and one demo clip. On first launch no ASIO device opens.
   Once connected, subsequent launches restore that driver with transport stopped.
2. Play: playhead/bar/beat advance. Switch Arrange/Edit/Mix/Live while playing;
   position continues. Pause/Stop and section navigation/loop work.
3. Rename Demo tone in the left rail; see the same name in Arrange/Edit/Live.
   Undo/Redo work and title marks changes with an asterisk.
4. Save a .mrsproject (choose any test folder). Open it again: renamed track,
   musical/processor state preserved, transport stopped. Close/replacement asks
   to save dirty changes.
5. Audio settings: choose Komplete Audio ASIO Driver, 48000 Hz, 128 frames,
   outputs 1,2, monitor input 0. Connect, then Play: quiet demo tone is audible.
   Pause/Stop control it. Switching workspaces must not interrupt it.
6. Stop. Open WAV and select a test WAV. This creates a new project and restores the connected
   ASIO driver, buffer and physical channels at the WAV/project rate. Press Play
   without another Connect. Repeat with a second WAV; audio should still work.
   The current shared graph is native gain at 0.5; demo tone also has an extra
   attenuation. Open WAV does not analyze harmony or generate chord data.
7. Optional monitoring: enable physical input 2 and Connect. Disable by setting
   input 0 and reconnecting. Avoid enabling a mic feedback path.
8. Resize and check Windows scale 100/125/150%: title, buttons, project tracks,
   transport and Audio settings remain readable. Mix/Edit are initial read-only
   views; detailed editors arrive in later stages.
9. Close/relaunch: workspace/requested audio settings retained; the saved ASIO
   driver reconnects with transport stopped. Explicit Disconnect persists disabled
   restoration; relaunch then stays offline until Connect. Config/log are under
   %LOCALAPPDATA%/MoonRiverStudio.

If no ASIO device is available, steps 1–4 and resize still work offline.
The MR-Studio-MRS-Stage-0-Offline-Windows artifact has no ASIO support.
Send confirmation or describe the failing step/screenshot.

MRS Stage 0 #20 remains open until acceptance. Stage 1 #16 sustained load,
Studio Pro comparison and reconnect/performance tests remain deferred.

Acceptance fix: open Audio settings from a fresh launch. It must open directly
without "Select an available audio device". Editing rate/output fields before
choosing a device must not show a popup; validation happens when pressing Connect.
Long-WAV ruler labels are spaced rather than printing every bar number.
