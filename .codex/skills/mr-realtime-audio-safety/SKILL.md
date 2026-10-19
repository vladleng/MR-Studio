---
name: mr-realtime-audio-safety
description: Audit or modify MR Studio real-time audio callbacks, device/audio graph, plugin DSP, recording, transport timing, routing, realtime state transfer or audio-thread lifetime. Not required for docs-only or UI-only edits with no audio-thread interaction.
---

# MR Realtime Audio Safety

**Hard rule:** the audio callback must **never wait on non-realtime work**. If uncertain whether a path runs in RT, inspect call sites/ownership and initially classify it RT-adjacent.

## Hard bans in RT callback

- No blocking locks, waits, sleeps/yields for synchronization; no file/network I/O, plugin scans, logging/formatting or GUI/message-thread calls.
- No heap allocation/**free**, growth of dynamic containers, unexpected object destruction or exceptions crossing callback; beware hidden allocation in `std::function`, strings, ref-counted pointers and third-party APIs.
- Never run unbounded loops/heavy prep or read mutable multi-field state without coherent publication and lifetime guarantees.

## Minimal workflow

1. Map **RT-critical**, **RT-adjacent** and **non-RT** code and exact UI/model → audio thread → meter/UI handoffs. Identify memory ownership, devices, sample rate, buffer and transport consumers.
2. Review [hazard checklist](references/rt-hazard-checklist.md) for affected paths; check blocking, dynamic memory, data races, denormals, bounded work, processor lifetime and shutdown.
3. Choose the *smallest* safe transfer: atomic scalar for a scalar; immutable snapshot and atomic publication for coherent parameters; bounded SPSC/FIFO for streams. Preallocate workspaces and complete decoding/IR/NAM preparation/graph compile on non-RT. Avoid lock-free complexity unless necessary.
4. Prove lifetime: publish only fully prepared state, atomically switch at safe boundaries, retire memory outside callback, quiesce and join workers before destroying a graph, plugin or device. Atomics need appropriate memory ordering; an atomic pointer alone is not a safe ownership design. Read [ownership notes](references/ownership-and-atomics.md) when applicable.
5. Check changed state transitions: device reopen, sample rate/buffer, transport start/stop/seek/loop, plugin replace, project close, channel mapping, UI teardown. For third-party hosting read [plugin RT notes](references/plugin-host-realtime-safety.md) if affected.
6. Run affected RT regressions locally: no alloc/lock probes where available, callback max time/xrun, sample-rate/buffer changes, rapid switching and shutdown. 128-frame Live/recording checks are distinct from higher-buffer Studio mixing; avoid imposing one limit on all effects. [RT matrix](assets/rt-validation-matrix.md) optional for stage acceptance.

## Reporting and scope

- Distinguish **RT SAFE**, **RT SAFE WITH MANUAL CHECK**, **RT RISK FOUND**, **NOT RT-VALIDATED**. Compilation/ordinary CPU percentage alone do not prove safety; do not claim physical ASIO/listening without measurement.
- Use local `mr-studio-build-test` for actual build/CTest; don't duplicate build instructions here. Optional [review template](assets/rt-code-review-template.md).
- Follow `AGENTS.md`: no new stage, source push, GitHub Actions or production media changes merely because this skill was invoked.
