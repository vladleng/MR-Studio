# Stage5 / #25 — initial risk review

Follow-up 2026-10-09: Russian working specifications and checkpoints are now in
[STAGE_5_PLAN.md](STAGE_5_PLAN.md) and GitHub #79–87/#25. Empty issue bodies and
unresolved proposals below describe the initial audit, not the current work plan.
No feature implementation or acceptance is implied by preparing specifications.

2026-10-09, Asia/Krasnoyarsk. User authorizes transition to #25 and requests
sub-issue review/risk assessment. Review only; no feature implementation,
new version/build/package or new test-result claim. Base accepted0.2g upd10,
source15afe65; branch `mrs/0.2g-fix1-editor-space-local`, clean tree at start,
latest local handoff2b8cfa9. No push/merge/Actions; docs local.

## Verified scope and foundation

Native GitHub sub-issue listing: nine open children #79–87, each body empty,
comments0. Parent#25 scope section empty; acceptance: shared musical structure
available to Live without copying. Dependencies#15/#17 closed. Issue titles are
scope labels, not detailed acceptance contracts; proposals below are not silently
adopted requirements.

SHARED Project has authored chord symbols, ordered non-overlapping chord/section
lanes with gaps and exclusive end; sections carry0xRRGGBB colors. Markers are typed
and may share ticks. TimeMap has stepped BPM by tick and meter by bar; no ramps.
Timeline implements sample/tick/bar/beat conversion (PPQ960). MusicalTimeline
projects current/next chord/section/marker from the same snapshot and provides
section/marker seek, previous/next and section loop. SetMusicalData and snapshot
Undo/Redo/serialization exist; current schema15 persists musical fields. JUCE displays
sections/chord labels, offers previous/next/loop section and initial BPM edit,
but is not a complete lane/map editor.

## Sub-issue assessment

| Issue | Scope | Main risk / proposed acceptance |
|---|---|---|
| #79 | Chord Track editor | Medium: range creation/move/trim, gaps, adjacency and overlap rejection, UTF-8/long spelling, zoom/hit testing; one Undo per drag. Existing symbol is authored text; harmonic recognition/transposition/MIDI playback are separate scope decisions. |
| #80 | Arranger Track editor | High semantic ambiguity: current sections are metadata/navigation spans; title does not authorize moving/copying/deleting underlying audio/MIDI. Decide metadata editor versus destructive/ripple arrangement before coding. |
| #81 | Section colors/bounds | Medium; implement with #80. Stable IDs, boundaries/adjacent sections/gaps, color contrast, active loop when section moves/deletes, references to Live patches. |
| #82 | Marker editor | Medium: same-tick ties, kind/name/position, multiple visible flags, stable ID on rename/move, referenced Live action on delete. Marker/cue is metadata; opening/seeking does not execute action. |
| #83 | Tempo map editing | High: MIDI/chords/sections/markers are ticks; WAV clips are samples. BPM edits change musical interpretation of WAV but do not stretch it. Preserve sample head by current contract; MIDI recompiles, click/loop/host tempo/ahead state need coherent updates. No ramp support. |
| #84 | Meter map editing | High: meter points use bar number, other musical content uses ticks. An earlier meter edit moves later meter boundaries in tick space while existing musical items stay tick-anchored. Decide and expose that policy; denominator-note beats,6/8/3/4, first point, snap/rulers/count-in. |
| #85 | Current/next context display | Medium: shared service already exists; display gaps/exclusive ends/tied markers, seek/loop/pause/busy publication and project replacement coherently. UI-smoothed playhead and control-thread context must not show incompatible boundary states. Live uses same service; full show UI not created implicitly. |
| #86 | Navigation by section/marker | Medium: previous inside section restarts its start, no wrap; equal-tick markers are one navigation position. Playback state retained, stale IDs/full command queue surfaced; recording navigation policy must be explicit. |
| #87 | SHARED persistence | High cross-feature requirement, not final cleanup: Save/Open/Undo after every slice, stable ID/reference integrity, older projects/extensions retained. Schema bump only for genuinely new durable fields. |

## Concrete code findings

1. `core/include/mrs/core.hpp`: Chord/ArrangerSection/Marker/TimeMap contracts.
   `core/src/model.cpp` validates ordered lanes and tempo tick0/meter bar1 anchors.
   `core/src/timeline.cpp`: WAV samples versus tick-based MIDI conversion.
2. `core/src/musical.cpp::SetMusicalData` replaces four full lanes together.
   Deferred callbacks must resolve current IDs/current model rather than write
   stale captured vectors. `MusicalTimeline` subscribes on control thread;
   it is not an audio-callback project-reading API or fired-cue stream.
3. `Application::set_time_map` uses generic `edit(SetMusicalData)`; `edit` requires
   stopped transport, captures plugin state and rebuilds graph. `history` lacks
   a metadata-only classification and also rebuilds for these lane changes.
   Design a metadata path/Undo handling for names/colors/chords/markers/sections;
   tempo/meter changes keep stopped preparation initially. Do not route musical
   commands through UI-local state or direct unsafe callback mutations.
4. `core/include/mrs/persistence.hpp::ProjectDocument` holds SectionPatch and
   LiveAction outside Project. `archive.cpp::validate` requires referenced
   section/marker IDs to exist; Application::snapshot copies these records from
   document_ alongside current Project. Inferred risk: deleting/re-IDing a
   referenced entity only in Project can make Save fail; per-project Undo cannot
   restore separately altered document records. Choose reference-aware rejection
   or transactional document handling before enabling deletion. No reproduced
   new failure is claimed in this read-only review.
5. `EngineTransport::rebind_timeline` preserves sample clock; active loop is a
   sample range, not an association to section ID. Moving a looped section or
   changing BPM does not automatically retarget that loop; specify keep/clear/
   recompute policy before edits.
6. Engine chunking selects tempo at chunk start, splits for loop boundaries;
   explicit tempo-boundary splitting not found in inspected path. Master gets
   callback-start tempo. VST3 ProcessContext currently validates tempo/music
   position, not time signature/bar position. Variable tempo/meter editor exposes
   host-sync correctness risks; verify track/bus/Master tempo and MIDI timing at
   within-block changes before promising exact tempo-synced playback.
7. JUCE lanes are fixed header rows in Desktop.cpp; chords draw labels without
   duration rectangles, markers have no complete editor. New hit regions must
   preserve ruler seek, clip trim/copy/multiselect, track drag and Note Editor
   gestures; narrow window/DPI/scrolling/stale component callbacks need coverage.
   PianoRoll labels currently iterate quarter-note boundaries; compound meter
   behavior needs review alongside Arrangement/controller rulers and snap.

## Proposed decisions before implementation

- Chord authored text initially, no hidden harmony/transpose generator.
- Arranger initially edits section metadata unless user requests content moves.
- Tempo stepped, meter bar-boundary; musical items tick-anchored, WAV sample-
  anchored; no stretch implied (separate#56).
- Tempo/meter edits after Pause/Stop; metadata may support playback through a
  separate safe path. Recording edits/navigation require a declared policy.
- Explicit section-loop policy and reference-aware delete policy, consistent Undo.
- Shared context readiness for future.mrlive; no separate Live model/clock or
  implementation of full show/setlist workflow under#25.

Proposed order: time/reference contracts and#87 requirements first; #82 markers
as a small end-to-end slice, #85/#86 context/navigation, #79 chords, #80/#81
sections, #83/#84 maps. #87 validation runs with every slice. User may choose
another order. Each deliverable: shared commands, Save/Open/Undo/Redo, relevant
model/engine/UI tests, local package and acceptance; no all-at-once UI shell.

## Verification plan (not run for this review)

Model limits/IDs/overlaps/gaps/ties + one-command gestures; Unicode/long labels;
Save/Open/older snapshots/foreign extension retention/referenced entity deletes;
6/8→3/4 and tempo before/in MIDI clip; WAV sample positions unchanged; coherent
Undo with head/loop; click/count-in/MIDI/tempo-sync track/bus/Master boundaries;
busy/full-queue/project replacement; native/parallel/anticipative/mixed/PDC/raw
capture regression for timing changes. GUI100/150/minimum sizes and physical
ASIO listening only when corresponding code is implemented. Existing accepted
TH-U/Nuro state/editor matrix not repeated absent a new specific issue.
