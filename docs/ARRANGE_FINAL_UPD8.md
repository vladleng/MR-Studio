# 0.2f upd8 — final arrangement/editing refinements

Upd7 accepted on 2026-10-08: «По всем предыдущим пунктам тесты пройдены».
Scope is the five requested refinements, not blanket Stage 4/P4 acceptance.

## Contract and layer map

UI/actions AFFECTED: ruler numbers reuse VST browser channelFont; only the top
bar ruler permits mouse seeking, including ruler drag. Empty-track selection,
chord/section clicks do not seek. Split selects only the new right-hand clip.
Edge hover uses cached left/right bracket cursors; single edge trim works even
when a group was selected. Drag preview and commit share bounds. Audio can grow
back only within original source frames; MIDI right edge has no content-length
ceiling, only existing supported timeline limits. MIDI left extension can add an
empty source prefix toward timeline zero, shifting source note/controller offsets
to preserve their absolute tick positions. Audio left extension restores only
existing source frames. Escape cancels; editing while recording remains rejected.

Model/domain AFFECTED: shared trim_midi_source handles empty-prefix extension and
validates source/note/event overflow before mutation. Existing trim/split/take
commands are authoritative; bounds are enforced by Application/Core. One Undo per
gesture/take; existing durable clip offsets/lengths round-trip without schema
change (13); durable fields retain their meaning. Undo/persistence use the existing
mechanisms. Hover, selection and in-progress preview are transient, not saved.
File/project conventions NOT CHANGED: recordings still go to managed Media.
Live NOT APPLICABLE to these JUCE gestures; the recorder preview API is shared.

Recording feedback AFFECTED: every armed audio track has a growing red preview
and raw-input waveform, analogous to MIDI. Duration comes from captured frames,
not extrapolated wall time. A bounded multiresolution peak envelope is generated
by the existing disk worker, never the audio callback. Packed atomic extrema and
a versioned bounded snapshot transfer coherent peaks to the message thread.
Busy snapshots may be skipped; visual feedback cannot delay audio. No ring reuse
before peak aggregation. Stop/fault/project replacement clears transient previews;
the existing finalized take command creates the actual clips.

RT-CRITICAL capture/routing/processing NOT CHANGED. RT-ADJACENT recorder lifetime
and immutable publication reviewed. NON-RT disk worker and UI handle preview
aggregation/copy/render. Fixed peak capacity, adaptive bin width, bounded retries;
no plugin calls, files or UI work added to callback. Tests/docs AFFECTED.

## Validation plan

Regression tests: ruler vs body/chord/section seeking, drag/scroll/Snap;
audio/MIDI split selection; bracket hit regions and edge/body interaction;
source clamping, MIDI beyond content, preview/commit equality, Escape and Undo;
Save/Open clip bounds; audio preview mono/stereo, repeated recordings, silence,
nonfinite sanitization, adaptive reduction, and no new callback allocations.
GUI smoke and software snapshots 100%/150%; full local Release/CTest and packaged
smoke. Physical ASIO recording/edge cursor and monitor DPI need user acceptance.

## Local results — 2026-10-08

Cached configure/full Release PASS. Full 113/113 CTest PASS (63.30 s).
Packaged J3 exit 0; MIDI clips/live/recording, desktop waveform/recording/
project_folders/clip_edits and audio recording/multi_input PASS. Software Arrange
and audio recording snapshots reviewed at 100%/150%. Actual ASIO/cursor feel and
monitor DPI NOT RUN. FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK.
Upd7 accepted by user; upd8 acceptance pending. Schema 13 unchanged.
