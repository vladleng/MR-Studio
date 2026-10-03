# MRS Audio Arrangement — Stage 1a
First slice of #21: audio tracks, multiple WAV import and channel waveforms.
Shared RemoveTrack/ReorderTrack/ImportAudio commands operate on ProjectStore and
its existing Undo/Redo. Delete removes clips atomically. Batch import preserves
project identity and musical lanes and commits one command after all files validate.

Application owns a capped immutable asset cache reused by the same AudioEngine.
Stopped/paused track/import/Undo changes stop callbacks, prepare the updated renderer,
then restart the SAME open device. No new engine or hardware fallback.
Editing while playing is rejected until Pause or Stop. Graph rebuild preserves the
idle position, paused/stopped state and loop, independently of playhead position. Open WAV retains its existing
replace-project behavior; Import WAVs adds separate tracks starting at zero.
New creates an empty project at the current rate. There is no resampler.

Mixer channel state from loaded archives is retained in the base document for Undo;
snapshot filters deleted tracks and adds unity defaults for newly created tracks.
Unknown chunks and graph/Live data survive. Disk media paths remain external.

Waveform workers build per-channel 256-frame extrema with a binary pyramid.
Paint queries logarithmic ranges without reading files or scanning PCM.
No stereo downmix cancellation. Waveform generation does not touch realtime state.
Application-thread decoding and bounded preloading are deliberately retained in
1a; full background import/read-ahead and long-file streaming are later work.
Limits: 128 clip voices, 256 MiB/WAV, 512 MiB aggregate retained asset cache.
Retained assets for Undo count against the cache cap until project replacement.

Windows UI: New, Import WAVs, Add Track/Delete/Up/Down, Zoom +/- and Fit.
Track list selection brings its row into view; wheel scrolls tracks and Shift+wheel
scrolls time. Later substages add move/trim/split and recording/streaming.
See MRS_STAGE_1A_CHECKLIST.md. Stage 1a accepted by user on 2026-10-03; PR #42 merged. #21 remains open.
Stage 1b clip selection, move/trim/split and shared Undo/Redo is accepted below.

## 0.1b fix1
Permit track deletion/import/Undo while paused. Preserve playhead and loop when
rebuilding the same shared renderer; never resume playback automatically.
See VERSIONING.md. User accepted fix1 on 2026-10-03; PR #43 merged.

## 0.1c — Stage 1b clip editing
Shared MoveAudioClip/TrimAudioClip/SplitAudioClip/RemoveAudioClip commands preserve
source references. Move changes time/track; trim recomputes source_offset while
bounding against the original decoded asset; split preserves total duration and
source continuity with a new stable right-side ID. Undo/Redo uses the same store.
Application methods retain stopped/paused clock and hardware as in fix1.
No schema change is required: existing Clip start/length/source_offset are authoritative.

Windows selection is identified by clip ID. Drag is a UI-only preview until release,
then exactly one command is committed. Escape/capture loss cancel the preview.
Body drag moves across audio tracks, edges trim/restore hidden source. Ruler click
positions the common transport for Split. S splits; Delete removes only the selected
clip. Optional 1/16 tick-based snap follows tempo map; Shift bypasses it.
Selection is available during playback; graph edits require Pause or Stop.
No source file writes, alternate renderer or second undo stack.
See MRS_STAGE_1B_CHECKLIST.md. All six CI jobs passed; user confirmed all functions
working on 2026-10-03. Stage 1b / 0.1c accepted; PR #44 merged into main.
Next: Stage 1c / 0.1d disk read-ahead and long-file playback; development not started.
