# Realtime hazard checklist

Use this on every RT-CRITICAL or RT-ADJACENT change.

## Blocking
- [ ] no blocking mutex
- [ ] no condition-variable wait
- [ ] no MessageManagerLock
- [ ] no synchronous cross-thread request
- [ ] no filesystem/network wait

## Allocation
- [ ] no `new/delete`
- [ ] no vector/string growth
- [ ] no hidden buffer resize
- [ ] no smart-pointer control-block creation
- [ ] no container insertion with allocation

## Work bounds
- [ ] callback work is bounded
- [ ] no unbounded retry loops
- [ ] no scan/search proportional to large project state unless explicitly bounded/preprocessed
- [ ] DSP complexity is predictable

## Ownership
- [ ] callback cannot observe destroyed objects
- [ ] shutdown ordering is safe
- [ ] device restart does not invalidate live pointers
- [ ] plugin graph replacement is synchronized safely

## State transfer
- [ ] no data race
- [ ] atomics are used only for appropriate state
- [ ] coherent multi-field state is transferred as a snapshot
- [ ] producer/consumer ownership is clear
- [ ] queue overflow behavior is defined

## Diagnostics
- [ ] no synchronous logging
- [ ] no UI calls from RT
- [ ] diagnostic overflow cannot block audio

## Audio lifecycle
- [ ] sample-rate changes handled
- [ ] buffer-size changes handled
- [ ] start/stop handled
- [ ] seek/loop handled if relevant
- [ ] silence/denormal behavior considered
