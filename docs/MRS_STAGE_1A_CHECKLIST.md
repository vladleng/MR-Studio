# MRS Stage 1a — tracks / batch import / waveform
Issue #21; first substage only. Launch MoonRiverStudio.exe from
MR-Studio-0.1b-fix1-ASIO-Windows.

1. Connect your saved ASIO driver. Press New and accept saving/discarding the old project.
   New creates an empty project at the current project rate. The connected ASIO restores stopped.
2. Import WAVs: select two or more WAVs at the project sample rate (Ctrl/Shift in picker).
   Files appear as separate tracks starting at zero in THIS project.
   Import does not replace project identity, authored harmony or existing tracks.
   Open WAV still replaces the entire project; use Import WAVs for adding files.
3. Wait for channel waveforms. Stereo shows two separate channels; no phase-cancelling mono sum.
   Fit shows the project; Zoom +/- changes scale. Wheel over timeline scrolls tracks;
   Shift+wheel scrolls time. Select a track from the list to bring it into view.
4. Play: all imported tracks sound through the same ASIO connection. Use sensible
   source levels: summed tracks can clip; a mixer arrives in #22.
5. Stop. Add Track creates an empty audio track. Select a track; Rename, Up/Down and
   Delete work. Delete includes its clips, with confirmation and Undo.
6. Undo/Redo removes/restores a complete import batch and restores deleted clips.
   Play after each operation: renderer agrees with the visible model.
   No additional Connect is required. Editing/import/Undo requires stopped or paused transport;
   a clear error is expected if attempted during playback.
7. Save .mrsproject, close/relaunch, reopen. Track order, clips and names persist;
   waveforms regenerate. Playback stays stopped, saved ASIO restores.
8. A batch with a missing/invalid WAV or different sample rate must not partly
   modify the project. WAVs must match the project rate; no resampling yet.
9. Resize / 100–150% Windows scale: new controls remain usable.

Boundaries: PCM/float WAV, 128 clip voices, 256 MiB decoded per WAV and
512 MiB aggregate cached decoded data (including assets retained for Undo).
Waveforms have 256-frame base resolution and logarithmic range lookup; peak
generation runs on workers. Decoding occurs on the application thread in this
preload substage and never in audio callback or paint. Disk streaming, recording,
move/trim/split and advanced selection remain later #21 substages.
User acceptance of 1a does not close the whole #21.

## Acceptance — 2026-10-03
User confirmed all working. Stage 1a accepted; PR #42 merged into main.
The whole #21 remains open; next substage is 1b clip editing.

## 0.1b fix1 check
1. Play, then Pause away from the beginning. Delete a track: no stop warning.
2. Undo/Redo: track/clips return, playhead stays in place and playback remains paused.
3. Import WAVs / Add Track at that paused position: no warning and no reset to zero.
4. Stop, click timeline away from zero; delete/import/Undo also work there.
5. Resume Play: starts from retained position. ASIO needs no manual Connect.
6. While actually playing, editing remains protected: Pause or Stop is requested.
