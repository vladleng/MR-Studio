# MR Control Binding #194 — host contract

#194 supplies one message-thread binding for real native insert parameters and
DAW track gain/pan/master gain. Gallery remains a project-free sandbox. Accepted
product editors are not migrated; the opt-in editor pilot belongs to #195.

## Public API and ownership

`MRControlBinding.h/.cpp` expose `MRControlBinding`, `MRParameterHost` (alias of
`MRNativeParameterHost`), and `MRHostParameterKey` (alias of `NativeParameterKey`).
The key has a domain, optional track UUID, insert UUID, optional Rig module UUID,
and descriptor parameter ID. It never persists a private numeric rack slot index.
Domains: nativeInsert, trackGain, trackPan, masterGain. Mixer keys contain no
insert/module identity. MIDI-only tracks and third-party VST3 targets are excluded.

One shared host coordinator is required per Application; duplicate coordinators
are rejected. Own it at the host/desktop level, not per editor. Supply a resolver
that checks the lifetime of the Application owner on the message thread (e.g. a
weak application owner or Component::SafePointer of the Desktop). It must never
return an editor/processor pointer or a dangling Application. The owner must stay
alive for a synchronous Application operation; all construction, calls, refresh,
and destruction are UI-thread-only. The coordinator holds no audio node pointer.
Bindings hold weak coordinator/store identity and a Component::SafePointer.

Create a control from `host.describe(key).control`, then attach one
`MRControlBinding(host, key, control)`. The control cannot already have an event
sink or an active gesture. A binding exclusively owns that sink until detached.
Do not replace onEvent while attached. `sync()` is available for explicit host
refresh; the binding also polls the model on a 15Hz message-thread timer.
The wrapper cancels, stops its timer and removes callbacks on destruction;
control-first destruction and host-first invalidation are supported.

## Parameter authority and units

Native ranges/defaults/capabilities come from Application::plugin_parameters on
the prepared native instance. Hidden/private fields and unprepared instances are
rejected, without opening a device. Rig module metadata comes from the existing
versioned ModuleDescriptor and its stable module UUID; public UI identity is not
a new automation lane ID. Module parameters remain non-automatable because the
current rack transport is private, hidden, packed and non-automatable.

Native gain/filter/EQ/channel EQ, Cab/WAV core, Reverb, Rig core and supported Rig
module parameters share the adapter. Units are the verified native/editor units:
Hz, ms, dB, ratio; wet mix uses percent display with the existing 100:1 conversion.
Raw linear gain stays linear (unity=1), not an invented dB parameter. Frequency
uses true log mapping; ranges spanning zero use bipolar mapping. Display steps
and precision are UI choices; physical metadata/defaults are not replaced by them.

DAW scalars use existing Track::Mix and Project defaults/ranges, now declared as
model constants also used by model validation. Values, serialized fields, layout,
and schema version are unchanged: track/master gain0..16 default1, pan-1..1
with default0. Native, mix and master updates use their existing Application APIs.

`ParameterSpec::exactHostValues` is opt-in for host controls. It permits native
float endpoints and defaults not on a decimal grid. A custom JUCE normalisable
range preserves those exact reset/endpoints; setAuthoritativeValue synchronizes
external values without snapping them or creating an event. It never writes DSP.
Unchanged formatted text + Enter preserves the original precise host float;
real edited input still validates and snaps to the declared UI step. Unchanged
model polling preserves pending text and visible validation errors. The old
sandbox defaults retain their prior snapping behavior.

## Transactions, conflict and queue policy

- begin captures current ProjectStore/session, revision, insert generation and
  target kind/module type+version. A stale displayed value cannot start a write.
- preview validates physical value and cross-parameter invariants, then calls
  preview_inserts or preview_mix. It does not execute a history command.
- commit uses set_inserts / set_track_mix / set_master_gain exactly once if the
  actual model value changed. No-op/reset-to-current gestures create no history.
- cancel restores the latest authoritative model through the existing cancellation
  path. Escape, focus loss, disable, owner close, target removal, project/history/
  preset/graph change or another editor superseding the lease cannot commit a
  stale chain. Cancelled pointer motion is latched off until mouse-up.
- The existing preview cancellation APIs are global. One lease serializes all
  coordinated editor gestures; a new editor cancels the old uncommitted preview.
  Other editors show the authoritative model, then follow the committed value.
- Global revision conflicts conservatively cancel rather than merge stale data.
  Model/store identity prevents old editors from following a reopened project even
  when restored UUIDs are identical. Parameter-only Undo/Redo retains the existing
  live graph/device path. Structural/bypass/preset operations keep existing host
  Pause/Stop rules and are not exposed as arbitrary parameter writes.
- Native pending snapshots use the existing coalescing/retry mechanism; the normal
  host poll must continue. A full mixer preview queue cancels with a retry status,
  without a false commit. Mixer edits work without a device; no device is opened
  merely to edit the project. The fixture host polls explicitly like the desktop.
- Exceptions in preview/commit roll back owned preview and remove the gesture;
  missing/invalid targets are disabled with a description. Host recovery restores only
  a disable applied by the binding; consumer disable is preserved.

## Automation and RT boundary

The adapter preserves `automatable` metadata but introduces no lanes, read/write
mode, automation recording or VST3 GUI writes. UI writes use existing block-boundary
host batches; `sampleAccurateUIWrites` is false. Existing PreparedGraph sample-offset
scheduling remains separate and is regression-tested. Mixer automation capabilities
are not fabricated. A UI commit is an existing model command, not a new automation
command or structural state restore.

All descriptor queries, string conversion, immutable project reads, chain copies,
validation and UI access are non-RT. No new callback, processing code, allocation,
lock, I/O, editor pointer, queue consumer or meter path is added to audio processing.
The existing host publishes bounded queues/snapshots to DSP; the binding never
reads or retains a current processor/editor object. Meter publication is untouched.
GUI-thread checks reject public sync/host access from another thread.

## Explicit fixture and evidence

Standalone MRBindingCheck / MR Host Binding Check0.4 always opens the fixture.
Studio developer flags: --control-binding-preview opens the same fixture;
--control-binding-smoke runs the fixture checks. These return before normal
Desktop/settings/diagnostics setup. The fixture owns a private temporary project
and a manual device, not a physical ASIO stream. Two Gain controls target the same
real native insert; Frequency targets another; a DAW fader targets track gain.
Undo/Redo and guarded Space operate only this fixture. No accepted runtime shortcut
or editor is replaced. Fixture saves use unique project IDs inside the QA folder.

CTest juce_control_binding and juce_control_binding_standalone cover exact command count, real DSP preview/rollback,
queue saturation/final value, physical units, fine/reset, typed validation, text/error
preservation, off-grid defaults, multiple editors, consumer disable, late pointer
events after Undo, store/graph/preset changes, host/control lifetime and duplicate
coordinator rejection. It also covers DAW gain/pan/master, stopped/no-device edits,
existing sample-offset scheduling, guarded Space and44100/48000Hz with64/128/512
manual blocks. Serial fixture callback timing is diagnostic, not hardware evidence.
QA remains local in build/asio-local/validation/ui194/.

Physical ASIO, audible result/xruns, hardware mixed-DPI and screen reader are NOT
TESTED by this stage. Prior user ASIO acceptance is not reclassified. #194 remains
open for user acceptance; #195 is not started automatically.
