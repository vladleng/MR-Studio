# 0.1c — MRS Stage 1b: clip editing
Download MR-Studio-0.1c-ASIO-Windows, extract and run MoonRiverStudio.exe.
0.1b fix1 is accepted/merged. This is the next substage of #21.

1. New, then Import WAVs at the project sample rate. Pause or Stop.
   Click a clip: amber outline and edge handles identify the selected clip.
2. Drag the BODY right/left: clip moves in time. Drag vertically to another audio
   track: clip changes track. Add an empty track if needed.
   One completed drag = one Undo. Escape during drag cancels without any change.
3. Drag LEFT edge inward: beginning is hidden; drag it back to restore source.
   Drag RIGHT edge inward/outward likewise. No trim can exceed source WAV bounds.
   Play: audio starts/ends at the visible boundaries and uses the correct source offset.
4. Select the clip, then click the RULER at a position INSIDE it. Press Split (S)
   or S on keyboard. Two clips result with continuous audio and no overlap/gap.
   Cursor at/outside a clip edge produces a clear error.
5. Select one split part. Del clip / Delete removes just that clip, leaving the track.
   Undo restores it; repeated Undo/Redo restores each move/trim/split/delete exactly.
6. Snap off is default. Snap 1/16 rounds move/trim/ruler seek to a musical grid.
   Hold Shift to bypass snap during a drag. Zoom +/- and Fit remain available.
   Wheel over timeline scrolls tracks; Shift+wheel scrolls time.
7. Play then Pause away from zero. Edit/delete/Undo: position and paused state
   remain, ASIO stays connected. Play resumes at the retained position.
   During active playback clip selection is available; edits require Pause/Stop.
8. Save .mrsproject and reopen: move/trim/split bounds, source offsets, IDs and
   track assignment persist. Original WAV remains unchanged.
9. Resize / 100–150% Windows scale: clip controls stay inside the window.

Selection/drag preview belong to the UI; edits go through shared ProjectStore.
WAV PCM/float only; no resampling, disk streaming or recording yet.
Overlapping clips sum in the existing renderer; no crossfade/overlap editor yet.
Preload limits still apply; at most 128 clip voices (split consumes one).
#21 remains open after acceptance of 1b. Next: disk read-ahead (Stage 1c).
