# SHARED Stage 1 — audio core

Scope: #16. Native hardware acceptance and Studio Pro comparison remain required.

## Ownership

`mrs_audio` owns the shared realtime renderer. `mrs_asio` adapts a Windows
vendor ASIO device via pinned PortAudio. All workspaces must be injected with
the same AudioEngine/EngineTransport/device services. No Live-specific engine.
The public ITransport from #15 is retained.

## Thread contract

The audio thread owns render position, playback state and loop. Control actions
enter a 63-command SPSC queue. Commands are applied at the next callback boundary;
the caller can therefore read the previous state until that callback occurs.
A full queue fails on the control thread instead of waiting in the callback.

The callback processes prepared assets/routes with bounded storage. It does not
open files, allocate/release assets, invoke Signal/listeners/ProjectStore, lock,
load plugins, render UI or call network/AI services. Fixed atomic counters and
a 101-bin callback-load histogram retain metrics. Required atomics are asserted
lock-free at compile time on the Windows x64 target.

The audio-to-control state mailbox uses atomic fields and a sequence number.
Reading retries at most 32 times, then asks the caller to retry; audio never
waits for a reader. EngineTransport converts sample->musical position and
dispatches state subscriptions from its explicit control-thread poll.
A stalled poll does not stop audio. No-op poll does not notify listeners.

Prepare/reconfigure/reset and graph destruction are control-thread operations
only while the device callback is quiescent. Do not replace a graph on a running
engine. Assets are shared immutable handles; the callback only reads them.
Graph publication while running, crossfade/hot replacement and plugin processing
are later work. Same input and output float buffers must not alias.

## Playback + monitoring

RenderGraph contains prepared Voices and independent MonitorRoutes.
A Voice references immutable interleaved AudioData, timeline sample start,
source offset, length and source-channel/output-channel gain routes.
Monitoring sums input routes even when playback is stopped or paused.
The prototype mixes both paths in one callback with no lookahead/DSP latency.
The future heavy process-path scheduler is not implemented yet.

Output is cleared each block, then summed. Invalid/null input is silent and
diagnosed if monitoring was requested. Output overs are counted and clamped to
[-1,1]; non-finite output is silenced. This is output protection, not a mastering
limiter or gain-normalization feature.

Prototype limits: 64 I/O channels, 128 voices, 65,536 frames per callback,
960 PPQ Timeline from #15. Clip/source ranges and routing are validated
before callback start. No realtime resampling: asset/device rates must match.

## Preload foundation

load_wav runs before ASIO starts. Supports RIFF/WAVE PCM16/24/32 and IEEE float32;
standard extensible PCM/float with matching valid/container bits is accepted.
Metadata chunks are skipped with bounds/padding validation. Invalid/truncated/
non-finite files are rejected. No RF64, compressed WAV or partial valid-bit layouts.

Decoded float samples stay in memory for the entire stream lifetime.
Per-load default budget is 256 MiB; the CLI has an aggregate 512 MiB WAV budget.
This completes the preload foundation, not a production disk read-ahead worker.
Long/huge sessions need a bounded worker/streaming cache in #21, behind this same
engine. It must not move file access into the callback.

## Device abstraction

IAudioDevice offers enumerate/control_panel/open/start/stop/close/status.
Only vendor ASIO devices are selectable in the hardware build; the offline
build cannot claim hardware support. Selection uses session-local PortAudio
device indices; reconnect requires fresh enumeration. Use physical one-based
channel numbers in the CLI, converted to zero-based backend selectors.

Buffer constraints come from min/max/preferred/granularity, including powers of
two and fixed-preferred drivers. No global hard-coded buffer list. Unsupported
rates/open errors are explicit. Driver sample-rate mismatch is rejected.
Native callback size mismatch is recorded as REVIEW rather than hidden.

The device reports open/running/stopped/closed/error, driver error details,
reported latency and PortAudio CPU load. Close quiesces callbacks before releasing
the engine. Vendor panel is only opened without a stream. Driver disconnects
that expose a stopped/error stream or stop callbacks are caught by the control
watchdog; recovery is stop/close, restore device, re-enumerate, reopen. There is
no automatic hot reconnect or crash isolation for buggy vendor drivers yet.
A driver call itself may block inside the vendor implementation; process-level
driver isolation remains a reliability research task.

## Metrics / evidence

Reported:
- callback count, observed min/max native frames;
- input/output overflow/underflow status flags;
- callback body max ns and deadline misses;
- p50/p95/p99 body load buckets (1% resolution, >=100% in terminal bucket);
- invalid blocks, missing input, clipped samples;
- driver/PortAudio reported input/output latency.

Callback body timing excludes vendor/PortAudio format conversion and OS/driver
overhead. Deadline miss is a renderer timing observation, not proof of a physical
dropout. Conversely zero flags cannot prove zero audible dropouts: drivers vary
in diagnostics. Round-trip latency is NOT measured; it requires a physical loopback.
The report labels performance gate pending. Self-test/CI cannot pass that gate.

Silence/tone/WAV/monitor modes are concrete hardware probes. Repeatable comparison
uses the same computer, vendor driver/version, rate, observed native buffer,
physical channels and prepared playback/monitor load in Studio Pro. Plugin/
patch/full-show comparisons require later stages and remain unmeasured.

## Verification

CI checks #15 contracts plus 7 audio suites and offline acceptance, on Windows/
Linux Debug/Release. Windows ASIO Debug/Release separately compiles the actual
device layer against the downloaded SDK and runs the same offline tests.
Tests cover SPSC ordering/backpressure, sample routing/trim/loop/monitor, metrics,
WAV failures, no callback allocations, and audio continuing with a stalled
control thread. Hardware results must be supplied separately.

User instructions: [SHARED_STAGE_1_CHECKLIST.md](SHARED_STAGE_1_CHECKLIST.md).
Dependency decision: [ASIO_DEPENDENCIES.md](ASIO_DEPENDENCIES.md).
