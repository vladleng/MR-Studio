# SHARED Stage 0 — Core contracts

Issue: #15. Implementation: C++20, standard library only, CMake 3.20+.
Public API: `core/include/mrs/core.hpp`.

This is the first backend contract implementation. It makes no choice of UI
framework, ASIO SDK, plugin SDK or final project container. These choices belong
to their own stages. No audio is rendered in Stage 0.

## Ownership and boundaries

Every workspace receives a copy of the same `mrs::Services` handles:
- `IProjectStore` owns the project and command history;
- `ITransport` owns playback state;
- immutable project snapshots can be held safely without changing under a reader.

There is no workspace-specific backend. `demo_services()` creates the services
once. Calling that factory separately per workspace would be incorrect.
`tools/core_check.cpp` demonstrates Arrange/Live with identical handles.
The integration suite also checks Edit and Mix.

All Stage 0 services and subscriptions execute on one application/control
thread. They are neither thread-safe nor realtime-safe. No allocation, locking,
snapshot copying, random ID generation, serialization or synchronous observer
dispatch may be used in an audio callback. #16 must implement its audio boundary
with a dedicated realtime-safe bridge; these objects do not constitute an engine.

## Project Model v1

A Project represents one song/production session, with identity (ID/title/artist),
sample rate, tempo/meter maps, nested folders, audio/MIDI track descriptors, clip
descriptors and markers. A clip/event descriptor points at a track and opaque
asset reference; Stage 0 does not open assets, render audio or store MIDI notes.
MIDI events, processors, chords and sections are later extensions (#17/#18/#24).

IDs are opaque, nonempty, at most 128 bytes, globally unique within a project.
Generated IDs have an off-thread random prefix and counter; they are not UUIDs.
Importer IDs and fixture IDs are accepted. Serialization and Undo/Redo preserve
every ID. Readers must not derive identity from track names or array indices.
New entity IDs should be generated before command execution, so redo reuses them.
One Project ID will serve as a stable setlist reference. No separate Song copy
or Live project format is introduced.

Validation rejects unsupported schemas, duplicate IDs, broken references,
hierarchy cycles, invalid enum values, invalid tempo/meter and negative or
overflowing clip ranges. Clips use non-destructive source offsets.

The model is intentionally a mutable value while constructing a private command
candidate. Published snapshots are `shared_ptr<const Project>`.

## Time coordinates

- Samples: signed 64-bit, zero-based, valid range 0..2^52.
- Musical time: signed 64-bit quarter-note ticks, PPQ = 960, 0..10^12.
- Display positions: bar/beat are one-based; tick is zero-based within a beat.
- A beat follows the meter denominator: 6/8 has six eighth-note beats.
- Tempo starts at tick 0; strictly increasing step changes, BPM 1..1000.
- Meter starts at bar 1; strictly increasing bar-boundary changes. Numerator
  1..64, denominator a power of two 1..64.
- Samples and ticks convert with nearest-integer rounding.
- Double accumulation may introduce rounding at very long durations; this is a
  Stage 0 conversion contract, not sample-accurate engine scheduling.
- Tempo ramps, preroll/negative time, drop-frame display, chords and sections
  remain #17 work.

`Timeline` is an immutable value service for a particular map/sample rate.
The fixture transport uses the initial project map. There is no tempo/sample-rate
editing command or live timeline rebinding in this stage. A generic future
command that changes those fields requires the shared application service to
refresh/rebind transport context; #17 must implement that coordination before
adding timeline editing UI. It must never become a workspace-owned timeline.

## Transport

`ITransport` exposes state, play/pause/stop/seek/loop and subscriptions.
State includes sample and musical position, playback state and optional loop.

- Play is idempotent and resumes at the current position.
- Pause preserves position and only changes playing to paused.
- Stop resets to sample 0 and preserves loop settings.
- Seek changes position without starting/stopping playback.
- Loop uses [start, end), enabled by an optional range; null disables it.
- Seeking before a loop allows an intro; advancing through its end wraps.
- Seeking beyond its end is allowed; the next positive playing advance normalizes
  into the loop and preserves overshoot.
- Changes publish one coherent state; no-op actions do not notify.

`MockTransport::advance(frames)` is deterministic and only moves while playing.
It has no background clock, ASIO device, UI timer or audio engine.

## Command / Undo

`ICommand::apply(Project&)` runs on a private project copy. The complete candidate
is validated before committing. Invalid/throwing commands preserve the project,
revision, history and notifications. Reentrant mutation from inside apply is
rejected. No-op commands preserve redo history and revision.

`ProjectStore` uses bounded immutable before/after snapshots (default 128 edits)
for the initial Undo architecture. This is deliberately simple, off-thread and
not suitable for copying large sample or plugin buffers. #19 should retain the
API while introducing compact deltas/transactions as needed.

An effective edit, undo or redo increments revision and publishes state containing
the immutable snapshot, revision, history availability and command description.
A new effective command after undo clears the redo branch. Runtime history is
not serialized. AddTrack and RenameTrack are initial concrete command examples.

Observer-triggered commands are permitted after commit. Events are queued FIFO
so observers see coherent revisions in order even during reentrant publication.

## Subscription lifetime

Keep the returned move-only `Connection` alive. Destroying/disconnecting it removes
the callback; disconnecting after source destruction is safe. Subscribe does not
emit an initial event; read state, then subscribe on the same control thread.
Callbacks run synchronously on the publishing thread. Exceptions in individual
listeners are isolated; the Signal diagnostic counter records them. Reentrant
events are queued FIFO, newly subscribed listeners start with the next event,
and a listener disconnected during dispatch will not receive remaining events.

## Serialization/versioning foundation

`serialize`/`deserialize` implement an explicitly tagged, quoted UTF-8 byte text
snapshot with header `MRS_CORE_SNAPSHOT 1`. This is a contract fixture archive,
not the final MRS document extension/container. UTF-8 bytes are preserved; the
parser does not normalize or validate Unicode. Numbers use the classic locale
and full round-trip double precision.

The reader validates sections, counts, enums, references and schema. It rejects
truncation, unknown schema versions and trailing/unknown data. It never silently
drops future fields. Limits: 16 MiB input/output, 100,000 entries per collection.
There is no migration from any older format yet; unsupported versions fail
explicitly. File I/O, atomic save, assets, migrations, autosave, crash recovery and
unknown-field preservation in the eventual package remain #19.

## Building and verification

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\mrs_core_check.exe
```

Linux uses the same commands with `-DCMAKE_BUILD_TYPE=Release` at configure
and executable `./build/mrs_core_check`.

CI builds/tests MSVC and GCC in Debug and Release with warnings as errors.
CTest runs six suites (model/timeline/transport/commands/serialization/integration)
and the acceptance tool. The Windows Release artifact contains the console
checker and test executable, linked with the static MSVC runtime. This is a backend check, not a DAW installer.

## Next stages

- #16: actual Audio Engine / vendor ASIO and performance gate.
- #17: full Musical Timeline, shared editing/rebinding and chords/sections.
- #20: application shell and technology spike; inject these same services.
- #28: Live UI on shared fixtures after required #17 interfaces are fixed.
