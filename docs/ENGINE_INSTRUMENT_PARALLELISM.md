# MR Studio — Parallel VSTi / multi-instance note sharding

**Status:** design / backlog, **not implemented**. 2026-10-09.
**Track:** SHARED Audio Engine #13; independent from #54 P4 profiling and hardware acceptance.
**Original request:** Omnisphere polyphonic piano in MR Studio occasionally clicked at a 128-frame device buffer, while Fender Studio Pro handled a comparable playing scenario. This is a motivation and an acceptance example, **not** a plugin/vendor-specific optimization.

## Problem / boundary

Parallel scheduling of independent **tracks** (P1 / #51) is not parallelism within **one plugin instance**. A host cannot safely invoke an arbitrary VST3 processor instance concurrently from several workers, and even two instances may not produce an identical signal to one instance. An instrument's own internal multithreading belongs to the vendor.

Proposed feature: keep **one visible logical instrument track**, but optionally schedule **2 or 4 hidden synchronized instances** of the same VSTi with separated MIDI notes and deterministic audio summing before shared track effects. Optional 8 only after measurements. The existing SHARED graph, PDC, routing, automation, raw recording and Live/Studio engine stay common.

## Preliminary processing plan

```text
                 Logical instrument track (one UI entity)
MIDI input -> normalized event stream -> ownership-aware dispatcher
                                       |-> Shard 0: VSTi instance -> buffer 0
                                       |-> Shard 1: VSTi instance -> buffer 1
                                       |-> Shard 2: VSTi instance -> buffer 2
                                       |-> Shard 3: VSTi instance -> buffer 3
                                           deterministic sum
                                                  |
                                    common track post-VSTi FX (ONCE)
                                                  |
                                   existing sends/buses/Master/PDC
```

All shards are prepared and scheduled through existing bounded RT workers and preallocated audio/MIDI buffers; **a particular VST3 instance receives exactly one processing call at a time**. Keep an explicit single-instance path/fallback and a user opt-in UI flag. No silent sharding for arbitrary instruments.

## MIDI correctness and sound equivalence

- Deterministic ownership of each note-on through matching note-off, including overlaps and repeated notes of identical channel/pitch; choose a stable voice-ID scheme within the host.
- Sustain/sostenuto and note-offs during pedal states, all-notes-off, panic and seek/transport reset across all shards.
- Global CC, pitch bend, mod wheel, expression, channel/poly aftertouch, per-note expression/MPE and program changes require an explicit policy. Broadcast blindly only when semantics are safe; map per-note events to the owner instance.
- Tempo/beat context, automation, preset state, latency reports and sample-rate changes must remain coherent across instances. State snapshots must represent **one logical track**; distinguish editable canonical state from per-instance live DSP state.
- Some plugins (mono/legato, arpeggiator, internal sequencer, shared FX/reverb, unison, round-robin, randomization, global voice stealing or licensing/session limits) cannot be decomposed without changing their sound. Such plugins require single-instance fallback; **do not claim bit-identical sharding as a universal property**.
- Multi-instance loading increases RAM and CPU overhead, and can create licensing or concurrent-instance limits; benchmark net headroom, not just the number of workers.

## Realtime safety, routing and monitoring

- New prepared instance/shard buffers live outside the callback; avoid audio-thread allocations, blocking locks, file/network I/O, plugin/editor calls outside contract.
- Deterministic reduction occurs before track's common FX **once**, maintaining channel/bus sends/Master order and PDC.
- Define how dynamic per-shard plugin latency is synchronized, reported and compensated; fail closed to single instance if shards disagree or preparation fails.
- Graph/device/State replacement: stop and join relevant workers, quiesce plugin processing owners, preserve Undo/Redo/persistence, existing recorded MIDI, raw recorded audio and transport.
- Studio low-latency monitoring at 128 frames is an important test, but sharding is *not required* for all recording or Live workflows.

## Candidate implementation slices

1. **Feasibility and plugin-capability policy:** derive how compatible multi-instancing is detected/manual opt-in, identify disallowed state/processing categories, benchmark single-instance baseline.
2. **Prepared shard graph and MIDI dispatcher:** x2 (later x4), note ownership + global/per-note controls, stable automation & state fan-out, proper cleanup and fallback.
3. **Audio/PDC integration and local validation:** parallel worker scheduling with deterministic summing, common post-VSTi FX once, latency safety, project persistence and UI.
4. **Target hardware tests and user acceptance:** evaluate whether x2/x4 improves actual deadline headroom without unacceptable sonic differences or regressions.

## Acceptance matrix

- Sample rate 48 kHz / device buffer 128 first, then 64/256/512 as supported, with actual driver identification and CPU/core topology; Workers 1/2/4+.
- Single vs x2 vs x4 on a heavy polyphonic patch with matched MIDI/state and loudness; log callback p50/p95/p99/max, Late/XR, plugin timings, memory, core-worker utilization and instrument latency.
- Repeated-note-on/off, sustain, panic, controllers, MIDI channels, transport seek/loop, automation, project save/reopen and stopping during active notes.
- Document exact equivalence by **instrument class** (not blindly PCM identity for stateful/reverberant instruments); failover tested on unsupported instrument and mismatched state.
- Confirm no concurrent processing of the *same* VST3 instance, no host callback allocations, no RT locks and no shared track FX repeated per shard.
- First user comparison: Omnisphere piano, 48 kHz/128 frames with matching conditions; compare Studio Pro only as a measured reference, **not a promise** of equal performance.
- Acceptance requires user test of the local package. Do not label a design/benchmark as implementation.

## Dependencies and out of scope

Depends on existing shared plugin state model #18, scheduler #51, PDC/reliability #23 and current instrument host capabilities; do not reopen previously accepted tasks merely to record this design.

Out of scope: forking the audio engine for Live, modifying external VST3 binaries, vendor-specific scheduling hacks, assuming host can thread one opaque plugin instance, mandatory auto mode or arbitrary plugin equivalence.

## Implementation workflow

Read [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [ENGINE_PARALLEL_PROCESSING.md](ENGINE_PARALLEL_PROCESSING.md), [ENGINE_PERFORMANCE_PLAN.md](ENGINE_PERFORMANCE_PLAN.md) and local code before implementation. Code/build/tests/packages local; GitHub issues/docs only with [skip ci]. Start on a specific user request; no automatic PR, Actions or source push.
