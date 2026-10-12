# MR Reverb library — local implementation contract (#130)

2026-10-10, 0.3c upd1. This document describes the local implementation, not a
copy of the canonical MR_REVERB specification. Source and documentation remain
local. User acceptance of #129 does not accept #130 or complete #128.

## Owners and layers

| Layer | Scope |
|---|---|
| Shared domain/audio | AFFECTED: recursive catalogue, background decode/kernel preparation, safe publication |
| JUCE UI | AFFECTED: reusable native-editor browser component and its smoke harness |
| User preferences | AFFECTED: global root and explicit quad-map confirmation, existing ViewSettings JSON |
| Project model, commands, Undo, preset/project serialization | AFFECTED #131: external identity, three controls, native Bypass, snapshot v16 / Reverb state v1 |
| Project/media files | External references only; no library copying, bundling, normalisation or modification |
| Studio/Live | Shared backend; Live suitability is not claimed |
| Tests/docs | AFFECTED: synthetic, concurrent, Unicode/long-path, real catalogue and GUI checks |

`ReverbLibrary` is app-wide. Its worker recursively inspects WAV headers; its
immutable snapshot contains relative folder/name, duration, channels/rate, kind
and per-file errors. `set_root` changes a generation and cancels obsolete scans.
Symlinks are skipped, the WAV count is limited to 10000, files are never decoded
in full just to display a directory. A missing root is retained and reported for
relinking. Search/filter works on the snapshot, with invariant Unicode case folding
in the Windows UI. The browser uses the existing Theme and JUCE controls.

Exact supported pair suffixes: ` L`/` R`, `_L`/`_R`, `-L`/`-R`, before `.wav`
(extension comparison is case insensitive; side suffixes are uppercase).
Both halves must be in the same directory with an identical base name, stereo,
equal rate and frame count. Exactly one L and one R are required. Ambiguous,
orphan, mismatched or oversized candidates are Unsupported, never silently loaded
as two diagonal effects. Rate alternatives are distinct entries. Four-channel WAV
needs explicit user confirmation of LL/LR/RL/RR; channel count or vendor name never
confirms the map. Samplicity's supplied 48 kHz naming is covered by generic rules.

## Per-instance loading and realtime boundary

`ReverbLoader` holds a shared graph lease and one node ID. Exactly one control
producer may own each node. Multiple loaders can share a graph; each collects
retired kernels **only for its own node**. All services, snapshots, mutexes,
filesystem access, hashing and destruction are NON-RT. DSP accesses the existing
atomic kernel handoff, owns processing/history and never acquires service locks.

Selection queues a new generation and stops the previous decode/preparation.
Loading keeps the prior active identity and audible kernel. A worker verifies
metadata, bounded samples and file identity, prepares an opaque `ReverbKernel`,
then publishes only a complete successful current request. Decoding checks stop
between 8192-frame chunks, resampling every 256 frames, kernel FFT construction
between partitions. Cancellation destroys unpublished kernels on the worker.
Publication is serialized with the loader generation; with a shared catalogue it
is also serialized with root changes. UI requests do not do filesystem traversal.

The existing 80 ms DSP crossfade/acknowledgement remains. Retired owners are
collected off-thread, periodically or before publication. A completed prepared
request can wait for the previous audio handoff; that wait is cancellable and
never blocks audio. `Ready` denotes successful kernel publication, not the end of
the audible crossfade. In a quiescent graph the host must activate/warm the staged
IR in a quiescent window, or provide DSP processing to acknowledge handoffs. The
loader cannot independently warm/reset/re-prepare a graph or reopen the device.

An instance may temporarily hold active + fading + one unpublished prepared kernel
and decoded source buffers. Each kernel retains the #129 384 MiB prepared budget;
source retains the 64 MiB budget. This bounded pipeline peak must be included in
future host memory/headroom reporting, not represented as one kernel's RAM.

Identity contains root-relative path when available, absolute fallback, byte size,
deterministic raw-byte FNV-1a-64 fingerprint and explicit pair/map information.
Fingerprinting is off-thread, noncryptographic, and checks size/timestamp around
reads and preparation. A changed/missing file rejects the request; active identity,
parameters and audio remain. No similar-name substitution is performed. #131 now
serializes identity, resolves/revalidates references on reopen and preserves
missing selections through project/preset save; see REVERB_EDITOR_IMPLEMENTATION.md.

## Host integration boundary

`ReverbBrowser` supplies Browse Folder, filename search, folder filter, one-click /
Return loading, metadata, Ready/Loading/Missing/error text, and explicit quad-map
confirmation. Root/map are global user settings. Search/filter/selection highlight
are transient UI state, not project data. Old ViewSettings v1 files without the new
optional keys still decode. The owner supplies a preference-persistence callback.

The component is compiled into the application and exercised by
`--reverb-browser-smoke`; #131 connects it to normal native insert authoring and
adds the host shell, Pre-delay/Tone/Mix bindings, Undo, project/preset state and
lifecycle integration. User acceptance remains distinct from automated checks.
The owner cancels/joins loaders before prepare/graph replacement and retains
AudioEngine quiescence for processor destruction. Generic native capture now
returns only the stable processor ID; external identity remains in the model.
Application can request serialized quiescent activation through the loader with
callbacks stopped; the worker itself never stops/warms the device or graph.

## Validation entry points

- `audio_reverb_library`: mono/stereo/quad/pairs, 44.1/48 kHz alternatives,
  invalid/ambiguous/orphan pairs, silent/corrupt/huge/missing/changed files,
  cancelled and superseded requests, a thousand recursive WAVs during DSP,
  two independent loaders in one graph, callback allocation/free probes,
  Cyrillic paths and explicit Windows paths longer than 260 characters.
- `juce_reverb_browser`: folder preference round trip, local Cyrillic search,
  loading and missing-root preservation, software snapshots at 100% and 150%.
- Opt-in `mrs_reverb_library_tests --library <quad-folder> <paired-folder>`:
  actual user catalogue counts/kinds; ordinary CTest requires no private assets.
- Existing `processing_reverb` and full suite cover DSP/Cab IR regressions.

Physical DPI/editor transitions, real ASIO/VSTi and reference listening remain
NOT RUN (#132/manual). Numerical #129 evidence is distinct from catalogue validation;
catalogue counts do not imply an additional listening or per-file DSP signoff.
