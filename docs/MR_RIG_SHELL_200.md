# MR Rig P2 / #200 - Shell and minimal RigPreset

**READY WITH MANUAL CHECK**, 2026-10-15. Version **0.4d MR Rig Shell**.
Local source: `6ce0a07d94accba154f978bc2c36f1da4c747b0c`.
User authorized #200 after accepting #199 / 0.4c upd1. #201 onward are not started.

## Implemented contract

A new native insert **MR Rig**, processor identity `mrs.rig`, uses the shared host
and the accepted WAV core. Project schema 24 adds InsertKind::rig. There is no
Cab-to-Rig conversion, compatibility wrapper or requirement to migrate old test
projects. Existing files and accepted packages are preserved. Add a new MR Rig
insert for this stage's UI.

The shell has a compact model library, PRE FX / central Core / POST FX, selected
IR and real controls. One card still represents one explicitly identified model,
with captures selected inside it and a separate Save IR default. Expand library
exposes the accepted model curation tools; Manage files opens the real secondary
source browser. NAM explicitly remains unavailable. PRE/POST are empty, with no
fake active pedals or enabled Add controls before their implementation.

The central image uses the user's permitted model cover or an original outline
combo from the user-provided MR SVG kit. Only that unchanged runtime SVG is copied
into the checkout asset subset; the complete kit remains at the supplied project
assets/ui/mr-uds location (confirmed 2026-10-15; the combo SVG matches the embedded
copy byte-for-byte). No generator run, vendor pictures, logos, WAV or
NAM files are bundled. Rig cover overrides have a rights note and use the same
bounded worker thumbnail validation as model covers.

## Durable state and preset library

- RigState v1 owns input/output trims, Rig name, optional cover/rights, empty PRE/POST
  arrays. Nonempty arrays and unsupported versions are rejected until real modules.
- The new insert reuses common NativeInsert external WAV selection, exact
  fingerprint/source/model metadata and wet gain/mix/cuts/polarity fields. These
  fields and RigState form the complete durable Rig snapshot; runtime kernels and
  library/UI caches are excluded. No new duplicate signal engine.
- Typed **MRS_RIG_PRESET 1** wraps an exact native Rig snapshot; the existing project
  serializer provides precision and validation. Media remains external; embedded
  IR samples/plugin binary blobs are rejected for the Rig insert.
- My Rigs stores a bounded **MRS_RIG_LIBRARY 1** in global preferences (128 presets,
  16MiB). New/save, explicit overwrite confirmation, load, rename, duplicate and
  confirmed deletion work on this same RigPreset. Names are separate from stable
  library IDs. Library operations do not delete/copy source WAVs.
- Active snapshot and selected preset ID belong to the project. Library rename,
  duplicate/delete do not silently edit sounding instances. Edited state is marked
  with an asterisk; replacing unsaved settings prompts in the UI. Double-clicking
  the current Rig name edits project state; saving stores that name in My Rigs.
- Preset/source loads are verified asynchronously. The model commits only after
  source success, as one Undo step, preserving the insert ID. Missing/corrupt WAV
  cannot partly apply a preset's bypass or levels. Requested status remains pending
  until the verified source and project snapshot agree.
- Project SaveOpen, Undo/Redo and editor close/reopen preserve the complete snapshot.
  Source selection and preset changes retain the compatible owned editor HWND and
  geometry. Existing Space and retirement hooks are reused. Gesture previews are
  canceled on preset/Undo/retirement and do not overwrite source identity.
- Bad saved-library metadata is reported without overwriting its original bytes
  or changing project audio. Source/media failures are independent from covers.

## Actual signal semantics and realtime boundary

Input trim -> one WAV core -> wet low/high cuts -> wet gain/polarity -> Mix ->
Output trim. The dry side also sees input/output trims, and bypasses wet gain/cuts.
Global Rig bypass bypasses the whole insert, including the trims. Full Combo WAV
is linear; there are no simulated amp dynamics or fake Gain/Bass/Treble controls.

Input/output targets use the existing 20ms smoothing. Neutral values reproduce the
accepted Cab core exactly. Latency stays0; native Live/anticipation capability
remains conservative rather than claiming an unmeasured Live gate. Actual block
peaks are published through two lock-free atomic scalars; bypass is labelled.
No new callback allocations/free, locks, I/O, worker joins or UI access. WAV/cover
preparation and retired-kernel collection remain outside the callback. The existing
bounded IR crossfade is reused; preset parameters use the shared parameter queue.

## Validation and delivery

Release all-target build PASS. **Full CTest155/155 PASS,165.77s**, run once on the
candidate. A final UI-only correction reused the accepted invariant Unicode search
key for My Rigs; the app was rebuilt and **six affected GUI tests6/6 PASS,12.25s**
after that correction. Core/DSP source did not change after the full run. This
separates the final GUI evidence from the earlier full candidate evidence.

New suites core_rig, processing_rig and juce_rig_shell cover typed/project/library
roundtrips, identity association, validation bounds, one Undo transaction, empty
PRE/POST enforcement, missing-file rollback of all controls, native HWND/owner and
geometry, gesture cancellation, duplicate/rename/delete, My Rigs search (including
uppercase Cyrillic), bad-library non-destructive handling, preferences roundtrip,
editor close/reopen and explicit NAM gating. Existing native editor/Cab/Reverb
regressions pass. DSP tests prove exact neutral output parity with the accepted
Cab core at44.1/48/96k mono/stereo, actual input/output and peak semantics,
completed IR transition, and no callback heap allocate/free at64/113/128-frame
blocks. They do not establish physical ASIO deadlines or listening quality.

Gallery snapshots100/125/150/200 exported; nominal100 and compact940x640 inspected.
Read-only real Polytone verification PASS: one model / ten captures / twenty WAV
sources. All twenty SHA256 match the #196 baseline; no downloads or WAV changes.
Packaged --rig-shell-smoke exit0. **13files/12SHA256 entries verified**. GUI SHA256:
`25444335FC06FBC8644A32370E37CBA20AD813C3AC0A1111AF07C4276DF00A3F`.

Package: `MR-Studio-0.4d-MR-Rig-Shell-JUCE-ASIO-Windows-local` under the MR Studio
project Builds directory. Desktop MR Studio shortcut updated; icon retained.
The previous0.4c upd1 package remains intact and all its hashes were verified.
Symbols: `build/asio-local/deliveries/0.4d-rig-shell`.
Logs/screenshots: `build/asio-local/validation/rig200`.

Manual workflow: restart using the shortcut, add a **new MR Rig** insert, select
Polytone Mini Brute, select an IR variant, adjust levels, then Save Rig and name it.
My Rigs -> select + Load Rig (or double-click) restores the full saved snapshot.
Save IR instead remembers only the model's default capture. Expand library opens
metadata/cover curation; Manage files opens the secondary raw source browser.
#200 remains open for visual/listening acceptance. #201 onward not started.

Hardware ASIO/listening and physical mixed-DPI remain NOT TESTED. Software
snapshots do not prove those gates. Full current CTest is run once on the ready
candidate; only affected tests are used while iterating.
