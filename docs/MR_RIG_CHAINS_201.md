# MR Rig P3 / #201 — ordered native PRE/POST chains

**READY WITH MANUAL CHECK**, **0.4e MR Rig Pre-Post**, 2026-10-15.
Local code commit: `2deef5a3f003e706f8755f843981af95d51367e1`.
#200 was accepted by the user; #201
was explicitly authorized. #202 and later stages have not started.

## User-visible behavior and state

One MR Rig host insert owns two ordered lanes, up to eight modules each. Add Pedal
offers the existing native **Gain, High pass, Low pass and Bell EQ**. Compressor,
Drive, Chorus, Delay and Reverb remain disabled / coming later. NAM stays unavailable.
Selecting a module opens its real parameter inspector. Up/Down reorder it; Move to
PRE/POST preserves identity and settings; the context menu also duplicates/removes.
Core controls returns to the accepted source selector and Rig input/wet/output controls.

Add, remove, reorder, move across Core, duplicate and bypass use the existing
SetInserts project command. Each action is one Undo transaction; a slider previews
through the host and commits once per gesture. Preset load, Undo and editor retirement
cancel the preview. Module editing/preset loading retains the same editor HWND/owner.
Inspector selection no longer discards the pending preset completion status.

Project schema25 and **RigState v2** persist ordered RigModule arrays. Each instance
owns a stable ID, type/version, bypass, typed parameter values and bounded optional
opaque state. ModuleDescriptor declares version, PRE/POST capabilities, mono/stereo,
RT suitability, zero latency and actual ranges/defaults. Tests compare its metadata
with the reused native DSP. Unknown type/version/state is retained as an explicit
unsupported placeholder with disabled DSP controls; it is never replaced silently.
RigPreset and My Rigs include these arrays through the existing typed snapshot.
The outer RigPreset/library formats remain version1; empty RigState v1 is readable.
There is no legacy Cab conversion. User media and previous packages are preserved.

## Processing and bounded transitions

`Input -> PRE -> one existing WAV core / wet controls / Mix -> POST -> Output`.
Whole-Rig bypass bypasses this complete path. Module bypass affects only that module.
PRE also affects the Core dry side; POST acts after the wet/dry mix. No second WAV,
device, audible host insert, nested DAW or project history owner is created.

Local audit selected a **fixed, preprepared native utility rack** instead of allocating
and asynchronously compiling a general-purpose graph on every reorder. Both banks,
sixteen slots per bank and four existing utility processor types per slot are created
and prepared with the owning graph, outside RT. No object construction, prepare,
allocation, ownership transfer or deletion occurs on a module edit in the callback.
This is the bounded preparation strategy for these four zero-latency utility types;
future delay/reverb/NAM/latency-bearing modules need their own preparation contract.

The existing host SPSC parameter transaction publishes the entire numeric rack state
at the audio boundary. Eight complete Rig snapshots fit the bounded 1024-change
transaction. Unchanged private rack fields are omitted from subsequent packets;
queue saturation keeps the producer's latest pending desired snapshot for retry.
Hidden slot fields are internal state transport, not exposed automation identities.
User-facing module identity stays stable in the project/preset model.

An edit configures an inactive, already prepared bank using the existing bounded
native set_parameter/reset methods. A 10ms fade blends PRE before the single core
and POST after it; rapid edits coalesce until the current fade completes. Old utility
filter histories fade out rather than running indefinitely. Active filter tail bounds
include frequency/Q/EQ gain and the finite IR support; they are separate from PDC.
All available utility processors and the accepted core report zero algorithmic latency.
Nonfinite child output is sanitized before feeding the next child/core. Added callback
paths contain no locks, I/O, UI calls or owning-object destruction (source inspection);
allocation/free probes and concurrent publication are tested separately below.

## Local files and visual scope

- Model/validation: core/include/mrs/core.hpp, core/src/model.cpp.
- Project/preset persistence: core/src/serialization.cpp, existing core/src/rig.cpp.
- Shared processing: core/processing/src/model.cpp, graph.cpp, reverb.cpp.
- Application/Undo/preview: apps/studio-desktop/src/application.cpp.
- Native editor/inspector and GUI fixture: apps/studio-juce/src/RigPanel.cpp.
- Core and DSP regressions: tests/rig_tests.cpp and tests/reverb_tests.cpp.

The accepted Theme/controls and original combo outline remain in use. This is not
the global MR UDS theme migration. The user's canonical local kit now has the original
`assets/ui/mr-uds/svg/{controls,icons,rig,reverb}` layout, with previews, tokens,
manifest, README and build_kit.py next to svg. All65 manifest paths and preview links
were checked; path metadata was reconciled after the user's move. SVG bytes are
unchanged, the generator was not executed. The embedded checkout combo SVG matches
the canonical kit SHA256: 33831522C2217B95CA6BBA3B90B2637B42F1835652112770E023B416094988AC.

The updated AGENTS.md, MR_UNIFIED_DESIGN_SYSTEM.md and mr-unified-gui skill in the
ChatGPT Projects/MR Studio folder agree on this layout and migration/RT restrictions.
The separate GitHub checkout still has its older AGENTS.md and lacks those two newer
reference files; it was not mass-synchronized. The user's current project instructions
and explicit scope governed this work.

## Validation and delivery

- All-target Release build: PASS (`build/asio-local/rig201-final-all-build.log`).
- Full current candidate CTest: **155/155 PASS, 207.14s**. No functional source
  changes followed this run. Full log: `rig201-full-ctest.log`; detailed output:
  `build/asio-local/validation/rig201/CTest-LastTest.log`.
- Core/persistence: descriptor/native metadata parity, ordered arrays, strict bounds,
  unsupported type/version/opaque recovery, project/RigPreset/library roundtrips,
  Undo/Redo, and eight Rig snapshots in one host transaction: PASS.
- DSP: neutral exact Cab parity at44.1/48/96k, mono/stereo, PRE/Core/Mix/POST routing,
  bypass, independent existing-filter oracle, repeated transitions, callback heap
  probes and100 concurrent snapshot edits through PreparedGraph: PASS.
- Concurrent edits measured max callback327.8us /128frames /48k; alloc/free0/0.
  Offline1/2/4-Rig matrix at128/256/512/1024frames,16 modules per Rig, synthetic
  sparse200ms IR: PASS, alloc/free0/0. Example4Rigs/128: mean512.231us,
  max861.6us vs2666.67us buffer duration. This is an offline synthetic measurement,
  not a vendor-IR worst case, hardware deadline guarantee or xrun measurement.
- GUI: real lane add/reorder/move/duplicate/remove/bypass, one Undo per slider
  gesture, exact preset/close/reopen state, unsupported controls/status, missing
  source preservation and unchanged HWND/owner/geometry: PASS. Captured logical
  100/125/150/200%, min940x640 and max1800x1200; minimum and maximum inspected.
  Earlier accepted screenshots are preserved under `validation/rig200`.
- Real Polytone metadata: **1model /10captures /20sources PASS**. All20 original
  WAV SHA256 still match the pre-#196 baseline; originals were not edited.
- Package: `MR-Studio-0.4e-MR-Rig-Pre-Post-JUCE-ASIO-Windows-local` in the project's
  Builds folder. **13files /12 manifest hashes verified**, packaged Rig smoke
  **PASS exit0**, existing desktop shortcut updated with its icon preserved.
  Prior accepted0.4d package hashes verified; running user process was not closed.
- Packaged GUI SHA256:
  `BED75E405F5BD5CA8EFD03E39E625286FF888F73923EEE39E701DC45B28F7FCD`.
  Matching GUI PDB stays in `build/asio-local/deliveries/0.4e-rig-chains`.
  Source, assets, builds, symbols, screenshots and tests stay local; only docs/issues
  are published. No source push, PR, merge or GitHub Actions.

Hardware ASIO, listening, real xrun counts and physical mixed-DPI: **NOT TESTED**.
Software screenshots do not establish hardware acceptance. #201 remains open for
the user's functional/visual/listening acceptance; #202 has not started.

Manual acceptance: restart via MR Studio shortcut, add/open MR Rig, select a real
model/IR, add a Gain or filter in either lane, change its value and bypass, move it
across Core, reorder, Undo/Redo, then Save Rig and reload it from My Rigs. Close/reopen
the editor and verify the whole chain persists. Other pedal families are unavailable.
