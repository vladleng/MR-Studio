# MRS Audio Arrangement — Stage 1a
First slice of #21: audio tracks, multiple WAV import and channel waveforms.
Shared RemoveTrack/ReorderTrack/ImportAudio commands operate on ProjectStore and
its existing Undo/Redo. Delete removes clips atomically. Batch import preserves
project identity and musical lanes and commits one command after all files validate.

Application owns a capped immutable asset cache reused by the same AudioEngine.
Stopped track/import/Undo changes stop callbacks, prepare the updated renderer,
then restart the SAME open device. No new engine or hardware fallback.
Editing while playing/paused is rejected until Stop. Open WAV retains its existing
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
See MRS_STAGE_1A_CHECKLIST.md. Acceptance pending; #21 remains open.
