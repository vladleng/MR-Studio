# AI analysis hooks — #166 / 0.3m

## Contract and workflow

Browser > AI > Analysis computes a local, read-only report without an API key or
provider call. It preserves the unsent composer. Ordinary AI messages receive a
fresh project snapshot plus the local summary; no audio samples, stems, offline
render, plugin state, paths or credentials are uploaded. The provider is instructed
to cite source/range/status and distinguish authored settings, measured source PCM
and musical inference. Analysis does not add editing tools or bypass permissions.

Core ReadView::analysis operates on frozen project values. Source handles are
captured by Application on its owner thread, max16 audio clips; the existing AI
worker runs analysis and network serially. Cancellation/deadline/access guards,
stale result delivery and session/revision guards are reused. Cached report is
memory-only and belongs to that worker, not the audio thread or project archive.

## Metrics, units and limits

analysis_version1, context_key(project/session/project_revision/graph_revision),
source/confidence/status fields. Whole authored arrangement, not current Live
activity. At most16 sections/128 clips/131072 note visits; use project range when
there are no sections. Notes are clipped to visible source trim and section range.
Report counts visible note intersections, notes per quarter (960ticks), union of
active note ticks, polyphonic ticks and MIDI pitch min/max. Partial work is marked;
polyphony is an intersection metric, not a musical mistake or same-pitch classifier.

At most16 mixer tracks/4 sends each: authored linear gain/pan/mute/solo, destination
track and send gain/pre-fader/bypass. These are settings, not measured loudness.
Audio statistics cover up to65536 source frames per captured clip, decoded in2048
frame blocks, aggregate interleaved linear RMS and peak, sample rate/channels/source
frame start/measured length/partial flags. Missing/invalid/read-failed sources are
unavailable. Audio is dry source PCM before processing, not VSTi or processed mix.
Spectrum/processed levels are explicitly unavailable: no FFT/ML/stem separation,
plugin rendering or implicit audio upload. Serialized report is bounded32KiB.

Cache invalidates by exact context key and immutable source identity, file size and
mtime for streamed sources; changed media during the read produces stale output,
not a cache entry. Preloaded PCM identity reflects its immutable loaded version.
File size/mtime is a lightweight signature, not a content hash. Cancellation is
checked between blocks/loops; an individual OS file read cannot be forcibly aborted.
Synchronous QueryKind::analysis remains unsupported; use the background analysis
hook. Context advertises the new analysis capability. No project/schema migration.

Default provider snapshot now24KiB, global pages16items/2KiB, focused track/clip
metadata explicitly included, selected MIDI pages still64items/8KiB. Full mixer/
plugin pages are omitted by default in favor of summaries; old queries remain.
No measured claim of token savings; actual usage and cost depend on the request.

## MIDI deletion latency and ownership

Old remove_clip/remove_clips used general edit: capture plugin state and full audio
rebuild. Only-MIDI deletions now use existing staged Store/TimingPlan transaction:
prepare on control thread, stop device, quiesce/join, symmetric MIDI data swap,
commit/restart/finish. Rollback restores data/model/history on restart failure.
No plugin capture/reload, editor teardown, routing change or MIDI generation reset.
Human Delete clip/Remove selected clips history uses that same path; mixed audio
removal and other human MIDI operations retain their existing general path.

AudioEngine::swap_timing has a guarded MIDI-only branch: stopped/joined, unchanged
time map/rate/routing, no count-in. It swaps only MIDI note/event/playback vectors,
retains metronome/tempo/meter/head/loop and permits existing loop while stopped.
AI Apply retains its old loop-off guard. Offline clock graph preparation now copies
the authored time map/click settings as hardware render already did; the strict
unchanged-time check exposed the previous mismatch. Callback/DSP code unchanged.

Normal marker badge fill alpha0.25 (75% transparent). Text/flag/selection outline
remain unchanged; other lanes and Alt-copy ghost keep their existing opacity.

## Layer and RT classification

UI/provider/read DTO/domain/Core/App/history integration/tests/docs AFFECTED.
Durable model/project schema/archive/config NOT AFFECTED. Analysis/cache/JSON/PCM
file reads NON-RT, immutable source ownership; no device or plugin calls in worker.
MIDI publication RT-ADJACENT via existing stopped/joined ownership protocol;
no new callback allocation, lock, filesystem, network, FFT or UI call. Old graph
and MIDI vectors are retired on control thread after stop/join, never during callback.
Undo/Redo share the central Store; analysis is not a project edit/history entry.
Shared Core, one DAW including Live Mode; current Live metrics remain unavailable.

## Provider billing and rejected commands

User supplied a rejected quantization proposal with usage12549in/1256out. Provider
completed successfully before local validation; this is distinct from HTTP errors.
Local rejection now displays a safe reason category, without raw tool arguments,
credentials or paid retry. The actual quantization rejection cause was not provided
as raw JSON and has not been claimed fixed. ProxyAPI bills successful responses by
actual usage; provider-side rejected/error requests have no model charge per
https://proxyapi.ru/docs/cost-of-request (verified2026-10-14). Exact tariff/balance
was not read or estimated. Local Analysis has no provider call.

## Validation and acceptance

Current exact build/test/package evidence is in PROJECT_CONTEXT. Synthetic fixtures
cover note trim/range/density/privacy/cancellation/revision invalidation, known0.5
RMS/peak PCM, no-network local analysis/cache and summaries-only wire, MIDI deletion
rollback/generation retention/loop/Undo/Redo/SaveOpen. GUI local analysis requires
no key and preserves project/draft/paid call count. Existing render/history tests
cover broader transitions; hardware latency/ASIO+network/recording/DPI and actual
provider explanation quality remain manual checks. #166 stays open until acceptance.
Source/build/tests/packages local; docs/issues GitHub, no source push/PR/merge/Actions.