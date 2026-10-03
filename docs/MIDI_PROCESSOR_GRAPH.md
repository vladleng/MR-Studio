# SHARED Stage 3 — MIDI / Processor Graph
Issue #18. Public API: mrs/processing.hpp. One GraphStore and PreparedGraph per shared
AudioEngine; Arrange/Mix and Live inject the same services.

## State and ownership
GraphState is an authored DAG with stable IDs, root-input edges, gains and output nodes.
Each node has native/VST3 identity, bypass, parameter values and opaque component/controller
state. VST3 class IDs use 32 hexadecimal characters; blobs are limited to 16 MiB each.
GraphStore publishes immutable snapshots, atomic validated replacement, bounded Undo/Redo
and application-thread RAII subscriptions. Patch is the same graph state with a name.
PreparedGraph retains a snapshot/revision and never follows edits automatically.
After editing, prepare/restore/warm a replacement off the callback and publish it only
while the device callback is quiescent. Old graph destruction/capture are also quiescent.
Do not share one mutable PreparedGraph across multiple audio callbacks/engines.

State capture returns current processor blobs and parameter values for later restoration.
It is an in-memory contract; project snapshot v2 does not serialize this graph yet.
Project/package persistence and graph/project association belong to #19.
No VST3 SDK dependency, scan/load/editor or executable third-party plugins here:
MRS #23 implements a host adapter behind IProcessor. VST3 restore/capture are tested with
a mock adapter preserving distinct component and controller blobs, not an actual plugin.

## MIDI
MidiEvent contains block-relative sample offset, kind, 0-based channel and 7-bit data.
Notes, CC, program, pressure, pitch bend (two 7-bit bytes) and poly pressure are represented.
Note-on velocity zero normalizes to note-off at ingress.
IMidiDevice abstracts ports, open/close, connection, receive and send. MockMidiDevice
is implemented. Native Windows MIDI enumeration/driver I/O, timestamps/SysEx/MIDI2,
recording, musical note scheduling and external reconnect remain future adapter work.

MidiRoute matches source-port ID/channel and routes to a graph node, remapping channel
and transposing note/poly-pressure numbers; out-of-range notes are filtered.
Calls enqueue_midi and enqueue_parameter have ONE application-thread producer.
Driver callbacks must marshal through that producer, not directly write to this queue.
Offsets apply to the next processed block, not absolute timeline ticks. Offset 0 is
appropriate for basic live ingress; latency/timestamp calibration is not implemented.
An offset outside the actual variable-sized callback is diagnosed and discarded.
Do not use this as an offline note scheduler or a sample-accurate MIDI recording promise.

Each node gets bounded, stably ordered MIDI and parameter buffers (256 each);
equal-offset events retain enqueue order. Audio DAG edges do not forward MIDI:
explicit source-to-node MIDI routes provide it. Processor MIDI output uses a bounded
audio-to-control queue with node index; application maps it to an IMidiDevice if needed.
There are no system/driver calls in the audio callback.

MIDI queue/buffer overflow requests panic. Next processing block resets every processor
and provides CC120/CC123 on all 16 channels. Stop/seek also request panic in AudioEngine.
Parameter overflow is explicit backpressure/metrics. Output queue overflow is counted;
it cannot guarantee delivery of note-offs to disconnected external hardware.
No seamless recovery or automatically scheduled loop note chasing. Queue drains are
bounded even if the producer continues writing.

## Runtime processing
IProcessor owns preparation, restore/capture, parameter metadata/access, warm/reset,
latency/live-safety and bounded noexcept process contracts.
Native mrs.gain is implemented with sample-offset automation, bypass and MIDI pass-through.
Other processors require an explicit factory; missing processors fail preparation.
Warm hook executes off-thread before publishing graph. Third-party RT safety must be
verified by its adapter; interface noexcept alone is not isolation.

AudioEngine renders playback and independent input monitoring, then invokes this same
graph and performs final finite/clipping protection. Graph preparation checks rate,
channels and block budget match the shared engine.
DAG order and scratch buffers are compiled off-thread (32 nodes, 128 MiB scratch budget).
At runtime edges sum predecessor buffers, processors work in-place, designated outputs
sum into the shared output. No per-block allocation, lock, I/O, shared ownership changes
or listeners. Nonfinite node output is silenced before downstream processing.
No stereo layout conversion, feedback cycles, hot graph swaps or separate Live chain.

## Latency
Each node reports own and cumulative longest-path latency. Graph reports output maximum,
processor live-safety and a caller-specified live latency budget (default 128 samples).
Bypassed nodes report zero effective latency. Unequal parallel-path latencies are detected
and classify the graph as not live-safe. This is reporting, not automatic PDC:
delay alignment and latency-changing plugins are later work. Mock latency tests are
metadata tests, not hardware measurements. Latency metadata is prepared/frozen;
reprepare if plugin state/parameters change its latency.

## Validation
Eight suites plus acceptance: MIDI routes/device mock; graph validation/cycles/limits;
serial/parallel mixing/bypass; sample-offset automation; shared patch state/Undo/capture;
latency and VST3 state mock; real AudioEngine playback+monitor integration; callback
allocation probe, queue/buffer overflow, panic and stalled output consumption.
Existing core/audio/musical suites retained. Physical ASIO/MIDI/plugin tests are not
inferred from CI. Stage 1 deferred hardware/performance checks remain in #16.
