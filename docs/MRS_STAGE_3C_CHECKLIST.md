# MRS Stage 3c / 0.1m — acceptance checklist

Local ready 2026-10-04; last accepted 0.1l / 3b.
Source `c505128089916350950bc7bdc36ce7a36152f6d4`; local branch `mrs/0.1m-cab-ir-browser-local`.

- [x] Cab IR mono/stereo WAV import and embedded project state, v10 reading v1–v9.
- [x] Direct-head / partitioned-tail convolution, measured 0 additional algorithmic latency.
- [x] Live Mix/Gain/cuts/polarity; Neutral/Warm/Bright control presets, Undo/Redo.
- [x] Owned synthetic test WAVs supplied; Celestion external acquisition boundary documented.
- [x] Vendor-grouped right VST3 tab, scan/cache, drag/drop to track/bus/Master.
- [x] Single-click mixer/list VST3 insert native editor and repeated-click window reuse.
- [x] Invalid/cancelled drop leaves project unchanged; insertion is one Undo command.
- [x] Local ASIO configure/build, 87/87 CTest, hidden GUI and visual checks.
- [ ] User acceptance of local 0.1m on intended projects/audio hardware.
- [ ] User audition of separately downloaded Celestion WAV (email subscription required).

1. Copy an existing project before saving with v10.
2. Scan your VST3 folder in the right VST3 tab. Expand/collapse vendor folders.
3. Pause/Stop. Drag an effect onto a mixer track, bus, then Master; verify correct
   destination, appended insert, Undo/Redo. Drop outside or press Escape: no change.
4. Click a VST3 insert once: native editor opens. Click again: existing window focuses.
   The strip shows up to three slots (one at compact height); Inserts opens the whole
   chain, where each VST3 selection also opens its editor. Native effects stay native.
5. Add Cab IR and load Impulses/MRS-Test-Flat-Mono.wav, then Stereo-Echoes/Colour.
   Verify L/R, live Mix/Gain/cuts/polarity and Neutral/Warm/Bright presets during playback.
6. Save/reopen a copied project; move the external WAV and verify embedded IR still works.
   Load a Celestion WAV separately and audition your amp/DI workflow. Recording stays raw.
7. Verify no menu/fader/audio-settings flicker; check wheel navigation and Space Play/Stop.

No code push/PR/merge/Actions. GitHub docs/issues only, code/packages local.
Full #23 open; 3d Amp/Preamp and 3e reliability follow acceptance. #16 deferred.
