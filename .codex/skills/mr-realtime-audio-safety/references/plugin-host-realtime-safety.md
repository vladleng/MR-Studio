# Plugin-host realtime safety

When MR Studio hosts third-party plugins:

## Keep these off the audio thread
- scanning
- instantiation if avoidable during playback
- state serialization/deserialization
- preset disk I/O
- GUI creation/destruction
- plugin metadata queries that may block
- graph rebuild preparation

## Processing path
- validate bus/channel layout before processing
- preallocate host-side buffers
- respect plugin latency reporting
- avoid host allocations around each `processBlock`
- define bypass/failure behavior
- handle MIDI buffer ownership clearly

## Graph mutation
For insert/remove/reorder:
1. prepare replacement graph/state off RT;
2. publish/swap with bounded RT work;
3. retire old graph off RT.

Avoid mutating containers in place while the audio thread traverses them.

## Failure containment
A plugin may misbehave. Host-side realtime code should not add additional blocking/unsafe behavior around it.
