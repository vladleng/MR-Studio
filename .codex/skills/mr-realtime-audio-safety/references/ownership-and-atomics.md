# Ownership and atomics

## Preferred ownership model

The audio thread should read from stable objects whose lifetime clearly exceeds the callback usage window.

Good patterns:
- processor owned by a graph whose destruction occurs after callback stop;
- immutable state block prepared off-thread and atomically published;
- double buffer with explicit active index;
- SPSC queue with one producer and one consumer.

## Raw pointers

A raw pointer is not inherently unsafe if lifetime is guaranteed.

Unsafe:
- UI/background thread may delete the pointed object while RT reads it.

Safe only when:
- ownership guarantees are explicit and verifiable;
- teardown prevents concurrent access.

## `shared_ptr`

Do not assume `shared_ptr` solves realtime ownership.

Potential RT costs:
- atomic reference-count operations;
- destruction on the thread that releases the final reference;
- hidden deallocation.

If used, ensure final destruction cannot happen on RT unless its cost is known and acceptable. Prefer publishing a stable snapshot/handle and retiring old objects off-thread.

## Atomics

Use for:
- gain;
- mute/bypass flag;
- transport boolean;
- monotonically updated counters;
- meter peaks;
- pointer/index publication where lifetime is controlled.

Avoid many independent atomics for one coherent object state.

## Memory ordering

Default to correctness and clarity.

Only use weaker memory orders when you can explain:
- what data is published;
- which write synchronizes with which read;
- why the reader sees the initialized object/state.

If uncertain, use a simpler synchronization structure.
