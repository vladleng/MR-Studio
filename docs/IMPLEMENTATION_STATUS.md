# MR Studio — implementation status

Updated: 2026-10-03. Source of truth for scope: issue #15.

## SHARED Stage 0

Implementation in branch `shared/stage-0-core-contracts`, PR #36.
Status: implemented, automated validation available, awaiting user acceptance.
Issue #15 remains open; main is not merged by this stage delivery.

Delivered:
- C++20 standard-library-only `mrs_core` static library;
- shared Project Model v1 and stable identity/reference validation;
- tempo/meter coordinate contracts and conversion service;
- ITransport and deterministic MockTransport;
- Command API, candidate validation, bounded snapshot Undo/Redo;
- immutable snapshots and RAII state subscriptions;
- versioned text snapshot serializer/parser;
- common demo services/fixtures;
- six contract suites and console acceptance checker;
- Windows/Linux Debug/Release GitHub Actions and Windows Release artifact.

Public API: `core/include/mrs/core.hpp`.
Detailed behavior: [CORE_CONTRACTS.md](CORE_CONTRACTS.md).
User check: [SHARED_STAGE_0_CHECKLIST.md](SHARED_STAGE_0_CHECKLIST.md).

CI must be green for the current PR head. The initial implementation passed all
four configurations; the follow-up adds a long-position 1/64 meter regression
test and static MSVC runtime for the Windows checker. Consult PR #36's checks
for validation of the current head.

## Handoff

1. Read START_HERE, CORE_CONTRACTS and issue #15.
2. Inspect PR #36 and its current CI/acceptance state before continuing.
3. After user acceptance, merge the stage and close #15; update parent #13 and
   this file. Do not mark MRS 0.1 complete: it also needs #16 and #20.
4. Next backend work is #16 Audio Engine / ASIO. Read AUDIO_ENGINE first and
   respect the performance gate. The UI/SDK/engine technology spike is still open.
5. #20 DAW Foundation can use these contracts for the application shell.
6. #17 adds full musical timeline and shared transport rebinding on map edits.
   Stage 0 fixture transport uses the initial immutable timeline.
7. #28 Live UI still needs the required #17 interfaces.

## Explicitly unfinished

No GUI, audio devices/engine, ASIO integration, plugin host, MIDI note backend,
Chord/Arranger editor, asset loading, final project package, migrations,
atomic disk saves or recovery. The checker uses an in-memory project and mock
transport; it cannot be used for music production or concerts.
