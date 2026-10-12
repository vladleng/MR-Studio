# #98 — instrument track from browser drop

Started locally 2026-10-09, from accepted 0.2g upd8, branch
`mrs/0.2g-fix1-editor-space-local`. Candidate version: **0.2g upd9**.
Issue: https://github.com/vladleng/MR-Studio/issues/98 (parent #55).

## Contract

Drag a VST3 browser entry onto the empty Arrange body below all existing track
rows (including a project with no tracks). Create and select one instrument track,
named after the plugin, with one VST3 insert; open its existing editor workflow.
The ruler and occupied timeline are not creation targets. Existing mixer strips
retain insert drop behavior; compact Arrange controls retain their rejection of
VST3 drops.
No new menu, shortcut, asset copy, instrument-specific special case or Stage 5 work.

Creation and assignment form one project command: one Undo removes the complete
track; Redo restores track ID and insert. Save/Open uses existing schema 15 fields.
No cache/preferences/project-format migration. Invalid payload/index is ignored;
missing module, invalid class, effect without MIDI event input and loading errors
report through the current UI error area without changing model/revision/history.
Pause/Stop is required, recording blocks creation. Native plugin crashes retain
the existing in-process host boundary; no new crash isolation is claimed.

## Layers and ownership

| Layer | Classification | Implementation |
|---|---|---|
| UI | AFFECTED | Arrangement hit testing, Desktop addInstrument, existing editor/error/selection |
| Model/state | AFFECTED | Existing TrackKind::instrument and NativeInsert; ProjectStore owns durable state |
| Application/commands | AFFECTED | add_instrument_track(name, insert), one Create instrument track command |
| Engine/domain | AFFECTED | Prepare candidate routing with existing renderer; reuse validated new instance |
| Undo/Redo | AFFECTED | Existing ProjectStore snapshots, no separate history |
| Serialization | AFFECTED | Existing fields round-trip; serializer code unchanged |
| Files/schema/compatibility | NOT AFFECTED | Existing plugin path/class references; no media/schema migration |
| Audio/RT | AFFECTED, RT-ADJACENT | Device stopped around prepare/rollback; callback code unchanged |
| Notifications | AFFECTED | Publish once only after candidate engine preparation succeeds |
| Studio/Live | AFFECTED | Shared Application/model/engine; JUCE Arrange entry point |
| Tests | AFFECTED | MIDI live Application regression and MIDI4a GUI regression |
| Docs | AFFECTED | This contract and PROJECT_CONTEXT; local only |

## Transaction and RT review

Message-thread path: validate name/insert/track capacity, make unpublished candidate,
stop device callbacks/workers, capture existing plugin state, prepare VST3 with
instrument ProcessConfig. Existing host checks MIDI event input and audio output;
browser cache has no type field, so it is not trusted for classification. The
creation-only VST3 factory additionally requires the selected class to declare
the `Instrument` subcategory. Ordinary insert loading is unchanged; no extra
module probe or cache migration is introduced.
Prepare mixer/MIDI/routing against a scoped candidate pointer while retaining
existing plugin instances. The new instance is inserted into retained runtime;
it is not loaded again after commit. Prepare engine and restart the device, then
publish one project command. Candidate pointer is cleared before observers run.
An uncommitted failure removes the new runtime and restores old routing/engine;
existing Redo and project revision are preserved. If engine recovery itself fails,
disconnect audio while keeping the unchanged project. Without a connected device,
validate the plugin before publishing and use normal preparation on connection.

No new callback allocation, locks, I/O, UI calls, or plugin state capture on RT.
Instances retain one processing owner, old plugins are not destroyed during create.
Undo/Redo and project reopen use existing stopped graph lifecycle.
Editor opening is a separate post-create UI action: editor failure does not erase
an otherwise valid assigned instrument; its existing error display remains.

## Validation

Cached offline configure and Windows x64 Release build PASS. Final full CTest
**118/118 PASS (51.75s)**, including MIDI live/recording/clips/external,
VST3 probe/processing/state/editor/scan/desktop/realtime, engine allocation/PDC/
parallel/mixed/ahead, Undo/persistence, J2/J3 and diagnostics regressions.
Application create tests cover disconnected/connected operation, one revision,
Undo/Redo and exact project invariant on missing path/bad class/effect/MIDI effect/
instrument without MIDI input, preservation of Redo, backend start failure recovery,
actual synth output through the existing shared graph, playback guard and Save/Open.
GUI tests cover empty/below-existing hit targets, ruler/invalid payload rejection,
selection, one-command create, effect error and existing mixer insert behavior.
Software100/150 snapshots inspected: two instrument tracks and assigned mixer inserts.
Initial full run 115/118 PASS: new fixture assertions expected unsupported compact
Arrange-strip insertion and unity shared-graph gain; corrected to existing mixer
drop policy and .5 synth x .5 foundation graph = .25. Final full run above is green.
Physical ASIO, real pointer drag, installed instrument audition and physical
mixed-DPI were not run in the automated validation; user feature acceptance is
recorded below, without claiming a separate hardware/DPI matrix.
#95 and Stage 5 not started. No source push/merge/Actions or GitHub docs publication.

## Delivery

Local source **73fd5f9**, same branch. Package:
`Builds/MR-Studio-0.2g-upd9-Instrument-Drop-JUCE-ASIO-Windows-local` in the chat folder.
Packaged J3+diagnostics **exit0**, session log confirms **0.2g upd9**.
Matching Release EXE/PDB, helpers, synthetic fixture, assets/licenses and SHA256
manifest included. Previous packages preserved. EXE SHA256:
**A68B36837DC644D23B89A5E5B231F1EA66536398788D0E0C3432C708A1DAB0C9**.
Delivery status was **FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK**.

## Acceptance — 2026-10-09

User: «Отлично, закрывай, все работает.» **FEATURE ACCEPTED**.
#98 closed completed on explicit request; corresponding parent55 items checked.
Current accepted build: **0.2g upd9**. Acceptance is scoped to #98, not all of #55
or hardware/P4. Package contents, EXE/PDB and SHA256 remain unchanged. No new
build/tests needed for acceptance recording. Docs local only; #95/Stage5 not started.
