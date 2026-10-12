# MR Reverb editor/state integration — #131

Local 0.3c upd1, 2026-10-10. Canonical specification remains on GitHub; this
document records implementation and ownership. Hardware/listening QA is #132.

## State and compatibility

`NativeInsert::reverb` owns Pre-delay 0–200 ms, Tone −6…+6 dB, Mix 0…1 (default
0.25), selected name, original source rate/channels/frames, quad map, and two
external file references (relative, absolute fallback, byte size, FNV-1a-64).
No IR samples, global root, transient status, search or folder expansion are saved
per instance. The root/map preferences use the existing ViewSettings file.

Core snapshot **v16** adds enum `InsertKind::reverb` and a `REVERB 1` section only
for that kind. v1–15 migrate with empty/default Reverb state. Older applications
cannot read v16; retain a project copy when returning to an older build. Cab IR
embedded limits and archive envelope are unchanged. `.mrspreset` reuses the native
shell serialization; Reverb presets also restore their saved Bypass. Existing
non-Reverb preset Bypass behavior is retained.

## Owners and operations

Application owns one shared catalogue and a loader per Reverb node. Editors hold
views; closing an editor does not cancel a selected IR. `shutdown` cancels and
joins a loader even if a retiring view retains a shared pointer. Application joins
all loaders before graph replacement/disconnect/destruction, after callback
quiescence. Retained graph operations retain their existing services.

On worker restore, resolve the relative path under the configured root, then the
absolute fallback. Reject parent traversal/rooted relative candidates, verify both
byte count and digest before decode/preparation, and recheck identity/stamps before
publication. Never substitute a similarly named entry. Missing/changed files
preserve durable references and status; a fresh unavailable effect passes dry
audio, failed replacements retain the previous good kernel. Folder selection can
relink a missing instance to matching content at a new relative location.

Prepared kernels use the existing 80 ms overlap. A separate `active_revision`
identifies published state even if a newer request is already Loading. On Stop,
Application acknowledges each published kernel with callbacks stopped, under the
loader's control mutex shared only with worker reclamation/publication. This
prevents two rapid stopped selections from waiting forever on an unacknowledged
previous publication. It does not reopen the hardware handle. During Play, the
audio owner handles the normal atomic handoff; no new callback locks/I/O/heap work.

Successful selections enter central `SetInserts` history without rebuilding the
currently playing graph. Parameter previews use the existing mailbox; release
commits one command, cancel restores the model target. Parameter Undo/Redo works
during Play without device reopening. IR history, insert structure, project save
and preset loading follow the existing Pause/Stop structural policy. Save/preset
capture first flushes completed selections into the model. Kernel buffers are never
serialized or read by generic VST state capture. No new timeline automation lanes
are claimed: all three IDs use existing native parameter metadata/mailbox APIs.

## UI and validation

Native Inserts menu contains Add MR Reverb. The resizable editor defaults to
900×450 logical pixels (minimum 760×390), uses current graphite Theme, shared
Bypass/Save/Load/preset selector, a folder tree with subtree filtering, Unicode
filename search, a single-click IR list, source metadata/status and exactly three
flat horizontal controls with editable units. Ctrl enables fine adjustment;
double click resets each control to its default. Existing native Space bridge
excludes search/value text focus. No new raster assets or bundled user IR.

Tests cover model validation/round-trip/migration, one command per gesture,
parameter history during playback, mono route levels through the existing 0.5
foundation master graph, track/bus/Master instances, IR history and rapid stopped
selection, project/preset reopen, relocation/fallback, quad/pair hash validation,
missing/changed references, closed loader rejection and repeated editor lifetime.
GUI smoke writes software previews at 100/150/200% and minimum bounds. Physical
mixed-monitor DPI, intended ASIO device, actual listening/reference comparison
and offline-export tail acceptance are **NOT RUN** here and remain #132/manual.

Source ff6dfea4ea606399aa29dca9cf19079b7aa57d15; final Release build PASS,
CTest 125/125 PASS (136.32 s), packaged editor smoke PASS. Full package and symbol
evidence is recorded in PROJECT_CONTEXT. Software 100/150/200% and minimum-size
previews were visually compared with the canonical three-column wireframe; the
existing Theme colours and native preset toolbar are retained. User acceptance
is pending.
