# Project/channel menus — 0.3c upd14 / #147

2026-10-12. File > Close project closes the active session after the existing
Save/Discard/Cancel flow, replaces the document with an empty offline project,
closes editors, clears selections and returns to Project home without Resume.
Project home remains a separate non-destructive navigation command.

M is red when active; S is yellow with dark text when active. Applies to track
headers and mixer strips. Toolbar +Track/+Bus/Del clip are hidden; menu commands
and keyboard clip delete remain available. Right-click mixer blank space or
blank insert area opens Add audio channel / Add FX channel / Add Bus channel.
Blank channel area also offers its type-specific delete; Master cannot be deleted.
Plugin buttons retain their existing context menu. Creation/deletion use existing
commands and Undo/Redo, routing cleanup and plugin lifecycle. Recording/busy
gestures disable structural channel menu operations.

FX uses bus DSP/routing with a durable `fx_channel` role, not a name heuristic.
Snapshot schema19 adds FX_ROLE after MIDIOUT. Schemas1–18 default false; existing
projects open, new schema19 files require upd14 or later. Newly created return/send
channels are marked FX. Old bus/return roles are not guessed from their names.

Affected: UI, Application commands, model role, serialization/compatibility,
Undo tests and docs. Audio callback/DSP/routing algorithms unchanged. Existing
structural edit/replace stop/quiesce path reused. New project close is not undoable,
as is normal project replacement; prompt protects unsaved changes.

Release/CTest126/126 PASS (144.80s), including GUI close/channel/FX regression. Packaged smoke and fresh
100/150 previews. Only fixtures; user projects/settings untouched. Separate
compact package and persistent desktop shortcut update after successful checks.
No push/merge/Actions; Stage5 not started. #147 stays open for user acceptance.

Final evidence: source `d46be8e4184487a8a518b444f1a561ec8d79a554`, Release/full
CTest126/126 PASS (144.80s), packaged J3 PASS (exit0). Actual Close Save/Discard/
Cancel/clean close, FX add/delete Undo/Redo, routing restore and actual saved-file
reopen PASS. Fresh100/150 mixer previews inspected in
`build/asio-local/validation/upd14/packaged-smoke`.
Compact13 files / 12 hashes verified: `Builds/MR-Studio-0.3c-upd14-Channel-Menus-JUCE-ASIO-Windows-local`.
GUI SHA256 `838B0813E1B07587CA77528B38526ABB10B630A2A0C128D9C4813B2C3431022D`.
Symbols `build/asio-local/deliveries/0.3c-upd14-channel-menus`. Persistent desktop
shortcut points to upd14 with retained MR icon; target verified. Accepted upd13
GUI hash unchanged. READY WITH MANUAL CHECK: user acceptance pending; physical
ASIO/mixed-DPI not separately measured. Earlier validation fixtures were corrected
to remove FX_ROLE before assigning legacy headers; product compatibility checks
were retained.


2026-10-12: **0.3c upd14 принят** — «Отлично, принимаю». #147 closed completed.
Existing package/manifest/shortcut unchanged for acceptance; no rebuild/push/merge/Actions.
