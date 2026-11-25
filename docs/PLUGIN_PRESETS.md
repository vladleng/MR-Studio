# 0.1p upd1 — plugin presets, editor shell and TH-U audible state

## Library and durable label contract

- Presets stay in per-plugin library folders. Nested folders and duplicate filenames map to exact files, never basename lookup; empty folders remain visible. Existing root presets are not moved or deleted automatically.
- New folder / Save accept relative nested names; absolute, traversal and reserved paths are rejected, enumeration does not follow symlinks. Folder/file actions are not project Undo operations.
- `NativeInsert::preset_name` is the sole durable MR preset label, including its relative folder. It survives missing library files, project reopen and shared Undo/Redo. It records provenance, not sound equality after edits or an invented plugin-internal factory preset name. Naming alone never restores a processor.
- The label was introduced in schema 15 (older snapshots read with an empty label); the `MRS_PLUGIN_PRESET 1` wrapper retains legacy payload reading. Older readers reject unsupported schema rather than guessing.
- Compact selector/menu implementation and regressions: [PluginPreset.h](../apps/studio-juce/src/PluginPreset.h), [preset library checks](../apps/studio-juce/src/Smoke.cpp). Current dimensions/fonts are as-built UI, not a prohibition on an assigned redesign.

The remainder records the original preset implementation and its historical tests; later in-place VST3 restoration is covered by the current implementation and synthetic editor-identity/rollback checks.

Local Windows JUCE J3 follow-up, 2026-10-05. User confirms the interface works and
Nuro Audio project-state restore works. Nuro is accepted; do not run special installed
Nuro tests for each build unless the user reports a new Nuro problem or asks for it.
Normal synthetic VST3 regression tests remain part of local CTest.

## TH-U diagnosis and fix

Read the user's saved Repetition project without writing it or its media.
Two fresh TH-U instances per lifecycle processed identical deterministic stereo input.
Before correction:
- restore-before-activation RMS 0.013635869;
- restore-after-activation RMS 0.016236280;
- relative sample difference 0.503286938;
- repeated runs within each lifecycle differed by 0;
- component state remained byte-identical in both cases (22502 bytes).

The component's activation resets audible DSP internals even though opaque getState
and the editor still show the saved preset. Activate after bus/setup preparation,
then restore the component/controller state, then start processing without activating
again over the restored state. No blanket parameter/program replay is introduced.
After correction both lifecycle checks produce RMS 0.016236280 and relative audio
difference 0 on the same saved state. These are software DSP comparisons, not an
audition of the user's selected preset against a manual preset click. Final listening
confirmation remains with the user.

A fixture whose activation resets DSP gain but preserves controller state verifies
actual restored sample values. Existing state, RT allocation and host regressions pass.
No project schema migration or rewriting of the user's project was performed.

## Presets and editor

Save preset... exports a full insert as a standalone .mrspreset: opaque VST3 component/
controller state and sparse host overrides, or native EQ/Gain/Cab IR settings including
embedded IR. State capture is quiescent; Pause/Stop for VST3 capture/load.
Load preset... validates format/size/type/class identity before replacing an insert,
preserves the destination slot ID/current bypass and installed module path, uses one
shared Undo command and reopens the editor. Presets are MR Studio files.

VST3 opens automatically after insertion in an MR Studio editor shell, with Save/Load
preset, Bypass/Enable, Up/Down, Remove and Parameters. Native plugin view occupies a
dedicated child HWND below the toolbar; plugin-requested size changes resize the shell.
Parameters opens the generic parameter panel. Native DSP editors also have Save/Load.
Repeated insert clicks reuse the editor. Retire child HWNDs while the plugin module is
still alive and clear handles before deferred C++ window destruction to avoid stale
HWND reuse. Failed attachments close the plugin view before destroying the host.

The channel + menu groups VST3 plugins in vendor submenus, including Unknown vendor
when metadata is empty. Existing native insert actions remain at the top.
Browser, clip canvas and track/channel panels now use #111315, approximately 70%
darker than their former graphite backgrounds. Buttons and clip/wave colors retain
their contrast and current placement.

## Verification and delivery

- Fully cached Windows Release build passed; 90/90 CTest passed.
- Final J3 component checks repeated after diagnostic label clarification.
- Native/VST preset full state roundtrip, destination identity/bypass preservation,
  wrong-plugin rejection and vendor grouping covered by J3 smoke.
- Fixture native child dimensions/DPI conversion, editor reuse/lifecycle and shared
  project/Undo/state regression checks passed.
- Installed TH-U native editor/processing/project-state roundtrip passed with the new
  shell; deterministic audible-state comparison above reproduced then eliminated
  the reported lifecycle difference.
- Reviewed darker main UI snapshot. Special installed Nuro checks were not run.

Branch: mrs/0.1p-upd1-plugin-presets-local.
Package: MR-Studio-0.1p-upd1-JUCE-J3-ASIO-Windows-local in local Builds.
EXE SHA256: 8F0048C9A0353CDC4441ADAC681E09AC3AA65C978BFBECCA5DF91B550FD4ED22.
Keep mrs_vst3_scan.exe beside Moon River Studio JUCE.exe.
Existing 0.1p and Win32 packages are preserved. User review of this update and
remaining physical J3 gates remain open. Code/builds local; GitHub issues/docs only,
no code push/PR/merge/Actions or installations.
