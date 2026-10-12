# 0.2g upd1 fix2 — #92, preset library

User-approved contract, 2026-10-08: compact selection, folders per plugin within
the existing library, selected MR preset name retained in the project.

Selector is 300x24 logical pixels, popup items 22 px / 13 px font, one column.
Folder submenus preserve duplicate filenames in different folders; selection maps
to exact files, never basename lookup. Empty folders are shown. New folder creates
a relative nested directory; Save accepts Name or Folder/Name. Load file remains.
Existing root presets are retained unchanged; no automatic moves/deletions.
Names reject absolute/traversal/reserved paths. Enumeration does not follow symlinks.

NativeInsert.preset_name is the sole durable label owner. Schema15 reads 1–14 with
an empty label. Last MR Studio preset saved/loaded is displayed including relative
folder; it survives missing/deleted library files, reopening and shared Undo/Redo.
It is provenance, not a guarantee of sound equality after edits. Plugin-internal
factory preset naming is not exposed universally and is not invented.
Presets retain MRS_PLUGIN_PRESET 1 wrapper; old snapshot payloads remain readable.
New schema15 projects/presets require this or newer builds; older builds reject
them rather than misinterpret fields. Preferences unchanged.

Layers: UI/model/Application/commands/Undo/serialization/tests/docs AFFECTED.
Project media/routing/transport/engine DSP NOT AFFECTED. Shared model/Application,
Studio UI; Live-specific UI DEFERRED. Folder creation/save are library file actions,
not project Undo; label and loaded settings use the central project history.

RT boundary: library/menu/file/name handling NON-RT; existing VST3 capture/load
RT-ADJACENT invokes stopped/quiesced device/worker ownership. No preset disk or
plugin state calls on the callback. Metadata naming never restores a processor.
Explicit loading still requires Pause/Stop; native editor lifetime and rollback
follow existing paths. No special installed TH-U/Nuro routine checks.

Validation: Windows x64 Release build PASS; 118/118 CTest PASS (53.67 s),
including schema/Undo/defaults, native/VST preset regression, folder mapping/path
rejection and popup/display 100%/150%. Eight repeated packaged GUI/diagnostics
smokes exit 0 after the retirement correction below.

Repeated validation exposed a stale native-effect ComboBox notification after
track removal. First-chance and abort dumps traced FxPanel::sync / Desktop::chain,
not plugin DSP. Initial band selection is now silent; retirement clears pending
selection callbacks, and user sync callbacks use Application error containment.
This fixes the observed path, not a claim that all possible crashes are eliminated.
User manual acceptance remains required. Stage5/#93–100 not started.
Code/packages/docs local; only issue progress published, no Actions/source push.
