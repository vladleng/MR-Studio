# Moon River Studio — Time Stretch / Pitch Shift

Status: **architecture and issue plan approved; implementation not started**.

Tracking: #56, child issues #57–#62.

## 1. Decision

Moon River Studio will start its native time-stretch / pitch-shift implementation with **Signalsmith Stretch**:

- upstream: `Signalsmith-Audio/signalsmith-stretch`;
- language: C++;
- license: **MIT**;
- first role in MRS: primary stretch/pitch backend;
- architectural status: replaceable adapter, **not** a permanent Core dependency.

The shared MRS contracts must remain backend-neutral so a future engine such as Rubber Band or zplane élastique can be evaluated or added without rewriting Project Model, clips, Transport or UI.

## 2. Why Signalsmith first

The current upstream library provides the capabilities required for an initial professional DAW implementation:

- polyphonic time-stretch;
- pitch shifting across a wide range;
- formant shifting/compensation;
- mono/stereo and multichannel processing;
- realtime processing;
- explicit input/output latency;
- automation-aware timing;
- reset/seek support;
- default and cheaper configurations;
- optional split/chunked computation to spread spectral work more evenly;
- MSVC/Windows support;
- permissive MIT licensing.

Upstream notes that time-stretch quality is strongest for moderate changes, approximately **0.75x–1.5x**. MRS will use this as a benchmark focus, not as a hard user limit.

## 3. Core architecture

```text
Project / Clip state
        |
Timeline / Transport / Render
        |
ITimeStretchEngine
        |
+----------------------+----------------------+
|                                             |
SignalsmithStretchEngine                 Future backend
(first implementation)                 Rubber Band / élastique / ...
```

Rules:

1. No Signalsmith type may appear in Project Model, clip serialization, Transport or workspace UI.
2. Arrange/Edit/Mix/Live use the same SHARED stretch service/path.
3. Live Mode never owns a second stretcher.
4. Persist user intent (ratio, pitch, formant policy), not vendor preset names.
5. Backend selection/version may affect cache invalidation, but not the musical project model.

## 4. Proposed backend-neutral contract

The exact C++ names are implementation details, but the Core contract needs equivalents of:

- prepare/configure;
- channel count and sample rate;
- process;
- reset;
- seek;
- flush/tail completion;
- time ratio;
- pitch in semitones/cents;
- optional formant control;
- input latency;
- output latency;
- capability flags;
- deterministic status/error reporting.

The contract must permit a mock/reference backend so unit tests do not require Signalsmith headers.

## 5. Realtime path

Signalsmith processing in the SHARED Audio Engine must follow the existing MR Realtime Audio Safety rules.

Not allowed in the audio callback:

- file I/O;
- UI/network/AI work;
- dependency construction;
- stretch reconfiguration requiring allocation;
- heap growth;
- blocking mutexes;
- logging to disk;
- unbounded queues.

Required:

- prepare/reconfigure outside the callback;
- bounded/preallocated input/output buffers;
- safe rate/pitch parameter hand-off;
- explicit latency accounting;
- seek/loop handling;
- transport start/stop/reset correctness;
- stereo/multichannel integrity;
- xrun and callback-time measurement.

Signalsmith's split-computation option must be benchmarked rather than enabled blindly. It can smooth spectral CPU work but adds an extra interval of output latency.

Initial physical target: **48 kHz / 128 frames** on the existing ASIO path. Measure 64 frames where the active hardware/driver supports it.

## 6. Offline / HQ path

Realtime playback and offline rendering share the same backend abstraction but do not have to share the same buffering policy.

Offline work includes:

- bounce/render;
- future freeze;
- future elastic-audio cache;
- deterministic full-tail processing;
- worker-thread cancellation;
- long-file read-ahead;
- duration/sample mapping;
- cache invalidation.

A cache entry must depend on at least:

- source asset identity/version;
- stretch ratio;
- pitch;
- formant state;
- sample rate/channel layout where relevant;
- backend identity and pinned backend version.

A cancelled or failed render must not leave a valid-looking partial cache artifact.

## 7. Clip state / UX

Initial non-destructive clip state should be algorithm-independent:

```text
stretch enabled
time ratio
pitch semitones/cents
formant policy/value
```

Initial UI work should support explicit ratios and pitch changes.

Not part of the first integration:

- automatic beat/transient detection;
- warp markers;
- Melodyne-style elastic editing;
- AI automatic warping;
- destructive source rewriting.

Those can be layered later on the same Core contract.

## 8. Dependency and licensing policy

Signalsmith Stretch is MIT licensed.

Before any distributed MRS build:

- pin an exact upstream revision/version;
- retain the required upstream copyright/license text;
- include it in MRS third-party attribution/NOTICE;
- document the update procedure;
- verify the exact pinned source license again at release time.

The permissive license is one reason Signalsmith is preferred for the first implementation, but licensing alone is not an audio-quality acceptance criterion.

## 9. Quality / performance gate

Signalsmith becomes the default MRS backend only after measured testing.

Test sources should include owned/authorized examples of:

- drums and hard transients;
- bass;
- guitar;
- vocal;
- keys/pads;
- dense stereo/full mix.

Minimum stretch ratios:

`0.5x, 0.75x, 0.9x, 1.0x, 1.1x, 1.25x, 1.5x, 2.0x`

Also test representative pitch shifts and formant cases.

Measure:

- audible artefacts;
- stereo/channel integrity;
- average CPU;
- maximum callback time and spikes;
- xruns/dropouts;
- input/output algorithm latency;
- seek/loop robustness;
- long-session behavior;
- default vs cheaper preset;
- split computation on/off.

Where practical, A/B the same source/ratio against a trusted DAW/reference stretch algorithm. The goal is engineering evidence, not cloning another product.

## 10. Issue breakdown

- #56 — parent decision / overall integration;
- #57 — backend-neutral contract and state model;
- #58 — dependency pin, adapter, MIT attribution;
- #59 — realtime playback, latency, RT safety;
- #60 — offline/HQ render and cache;
- #61 — clip controls, tempo mapping and persistence;
- #62 — listening/CPU/regression benchmark.

## 11. Delivery policy

Current project policy remains unchanged:

- implementation and builds are local;
- GitHub stores issues and documentation;
- no code push, PR, merge or GitHub Actions unless the user explicitly requests it.

Signalsmith integration must therefore begin locally from #57/#58 when scheduled, while this document and the issues remain the source of truth on GitHub.
