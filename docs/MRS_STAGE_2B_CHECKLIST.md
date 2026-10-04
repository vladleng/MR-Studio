# 0.1g / MRS Stage 2b — Windows acceptance

Run the locally built MoonRiverStudio.exe. This build awaits user acceptance.
Local Windows x64 ASIO Release configure/build and CTest 62/62 passed, together
with GUI smoke covering bus create/fader/Undo, deletion and 150% layout bounds.
Dependencies reused from the local cache; GitHub Actions not used.

1. Open a 0.1f project or import two WAVs. In Mix click + Bus; rename it in the
   project list. Use each track's Out: menu to send both to that bus.
2. Start ASIO playback. The bus meter reflects their sum. Adjust bus gain/pan,
   mute and solo; master reflects the result. Track meters remain independent.
3. Solo the bus: both members play, unrelated tracks stop. Solo one member only:
   that member reaches master through the bus, siblings stop. Mute the bus while
   solo is active: no bus audio reaches master. Clear solo/mute afterwards.
4. Pause away from zero. Add a second bus; send the first bus to it. Playback
   resumes at the retained position with the same ASIO connection. Output menus
   disable self-routes and choices that would create a cycle.
5. Drag a bus fader, release and Undo/Redo during playback. One gesture = one edit.
   Drag then Escape cancels. Changing routing/creating/deleting buses needs pause.
6. Pause, click Del on the first bus. Its inputs now go to its output destination.
   Undo restores the bus and original routes; Redo removes it again.
7. Save/reopen the project: bus names/order/controls and all destinations restore.
   Recheck 0.1f projects, portable folder relocation, Save As and unknown chunks.
8. Arm an audio track routed through a bus, monitor/record the input. Bus controls
   affect monitoring, while the captured WAV retains the raw input signal.
9. Switch Arrange/Edit/Mix during playback; device/transport remain shared.

This does not complete sends/returns, hardware multi-output or device profiles.
