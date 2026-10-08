# MRS Stage 2e / 0.1j — inputs and simultaneous capture

Accepted by the user, 2026-10-04: «Все проверил, все работает!».
Latest accepted local baseline: 0.1j. #22 Mixer / Routing completed;
deferred #16 physical/performance matrix is not claimed passed.

- [x] Per-track mono/stereo inputs; physical-to-stream mapping and validation.
- [x] Independent Arm/Monitor; monitoring through shared track/bus/send mixer.
- [x] Simultaneous raw mono/stereo float32 WAV takes with shared sample start.
- [x] Batch take attachment/Undo/Redo, retained media and dropout prefixes.
- [x] R/I next to M/S on each track; global Arm/Monitor hidden.
- [x] Separate horizontal L/R indication for stereo input or playback.
- [x] Stop returns to Play/Record start; Pause stays; rebuild retains anchor.
- [x] Core v7 reads v1-v6; input/Monitor commands, Undo, archive save/reopen.
- [x] Local cached offline-dependency Windows x64 ASIO configure/build, CTest 73/73.
- [x] Hidden GUI smoke: independent R/I/Undo, distinct L/R meter pixels, previous
  zoom/DPI/fader/menu/settings flicker regressions. Exported UI preview reviewed.
- [x] User acceptance of local 0.1j workflow (2026-10-04).

## User check

1. Copy a project. Open local 0.1j and connect your ASIO device at the project rate.
2. Add two audio tracks: mono input on one, stereo pair on another. Click R on
   both; arming the second must retain the first.
3. Toggle I independently. Feed different L/R levels; verify meters, monitoring,
   M/S, fader, pan and bus/send routes.
4. Start away from zero, record, toggle I off/on, Stop. Verify return to the exact
   starting position and aligned mono/stereo takes with raw source channels.
5. Undo/Redo changes both clips together and retains files. Save/reopen preserves
   input/Monitor; Arm resets. Newly saved project uses snapshot v7.
6. Play/Pause retains position; Play then Stop returns to the latest Play start.
   Verify looping and repeated Play do not shift that anchor.
7. Unavailable stereo pair/armed Off must give a clear error without creating
   files or damaging the current project/device connection.

Next roadmap stage after acceptance: #23 Plugins / Native DSP.
Code/builds local; GitHub issues/docs only; no code push/PR/merge/Actions.
