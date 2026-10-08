# 0.2f upd7: arrangement and audio recording

Scope: five follow-up requests after the user accepted upd6 on 2026-10-08
(«Теперь все в порядке. Тест пройден!»). This does not accept the whole Stage 4/P4.

## Contract and layer map

- JUCE arrangement cursor covers the entire timeline height, including chords,
  sections and ruler. Transparent overlay remains mouse-pass-through and uses
  the accepted 60 Hz shared visual transport; engine position remains authoritative.
- Seeking on the ruler or empty arrangement uses the nearest sixteenth with Snap
  on; Snap off preserves the rounded sample under the click. Bar lines capture
  within 6 logical pixels on either side, with the nearest bar winning. Capture
  follows the painted grid coordinate, scroll, zoom, tempo and meter changes.
  Existing clip drag/drop/trim snap semantics are unchanged.
- Record uses Application::start_project_recording, not a WAV chooser. Saved
  sessions use their own Media folder; unique Take IDs and the recorder's existing
  no-overwrite checks protect old takes. Multiple armed audio tracks get distinct
  paths. MIDI-only recording does not create a project directory or WAV.
- An unsaved audio session first becomes a unique Untitled-ID project in the
  caller-provided Projects folder (JUCE: Documents/MR Studio/Projects), including
  current imported media. Playing sessions are quiesced at their current sample
  for this initial save; retained processors and the same device configuration
  are reused. Subsequent Record uses the established project. The newly recorded
  clips remain dirty until Save; raw WAV prefixes are retained even after Undo.
- Audio and MIDI clip fills use the same 0.72 opacity; labels, border and waveform
  stay legible. No opacity preference or schema change.
- Shared JUCE waveform rendering uses antialiased filled envelope paths instead
  of disconnected pixel columns. The worker-generated pyramid starts at 16-frame
  bins, adapting to the same 65536 total-bin bound. At high zoom, smoothstep blends
  cached min/max extrema without overshoot. This is a display envelope, not a
  sample-accurate oscilloscope. Overview peak queries still retain transients;
  playback and stored PCM are untouched. No PCM reads or file I/O in paint.

No new callback code, RT locks/allocations/I/O, DSP ownership change, project
schema, or Undo command. Take commits use the existing single shared command;
Save/Open retains portable Media references and existing project identity checks.
Visual cursor, seek magnetism and waveform interpolation are transient UI state.

## Validation / manual acceptance

Automated coverage: seek both sides/zoom/scroll/Snap/tempo/meter; cursor ruler and
chord pixels and mouse passthrough; bounded smooth envelope and conservative
extrema; raw multi-track managed recording, repeated takes, one Undo/Redo,
portable Save/Open and no folder creation on offline Record rejection.

Manual ASIO checks still required on the new package: ruler/chord cursor, seek
feel with Snap on/off, repeated real audio takes in Media without dialogs,
overlapping audio/MIDI visibility and waveform zoom at actual Windows DPI.
