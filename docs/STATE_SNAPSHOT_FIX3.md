# 0.2h fix3 — busy audio-state reads

## Evidence and contract (2026-10-08)

User supplied fix2 session `mr-session-1791441386160-12772-1`.
Matching Release symbols identify raw stack candidates: std::runtime_error,
AudioEngine::state (engine.cpp:483), Desktop::paint (Desktop.cpp:220).
Exception 0xe06d7363, main thread 6912. Source confirms a throwing bounded
snapshot read used directly inside paint. KERNELBASE is the exception-raising
location, not evidence of a Windows/plugin fault. This incident does not prove
every previously reported crash had the same cause.

UI drawing/refresh must survive an audio publication overlapping all 32 read
attempts: hold the last coherent display head, retry on the next normal UI tick.
No waiting, extra retries, locks or changes to the audio publisher. Commands
requiring fresh state continue to fail closed and report on the UI thread.
MIDI timers cancel a gesture if a fresh state is unavailable; note-edit permission
never uses the display cache. Space handlers contain strict-read exceptions.

## State and layer impact

Authoritative state remains the audio engine; a message-thread-only Desktop cache
is a display projection, cleared on resetDevice/new document clock reset.
try_state commits a local candidate only after coherence is verified. Failure
leaves the caller's output unchanged. Successful reads clear a previous loop
when the newly published state has no loop. EngineTransport's control/UI
projection can return its last published transport state during contention.

- AFFECTED: shared engine snapshot consumer API, transport projection, JUCE
  paint/refresh/Space, piano/controller timers and editing guard, tests/docs.
- NOT AFFECTED: RT publisher/callback/worker scheduling, plugin lifetime/DSP,
  routing/PDC/audio or MIDI capture, durable model, editing command semantics,
  Undo/Redo, notifications ordering, project files, schema 13, Preferences v7.
- Shared core: snapshot semantics and EngineTransport; Studio UI: display cache.
  Live remains the future built-in mode, not another product/implementation.
- Cache is neither persisted nor undoable. No visual restyling or new controls.

## Realtime review

Boundary: audio publisher → atomic coherent mailbox → non-RT UI/control readers.
Read loop retains its fixed 32-attempt bound; no sleeping, unbounded polling,
allocation, I/O, plugin calls or mutexes are added to either side. A failed UI
read may briefly hold a displayed cursor; it does not stall audio or authorize
editing against stale state. Device/project transitions retain stopped prepare
and ownership rules. Strict AudioEngine::state remains available for guarded
commands/quiescent reads, not paint or unguarded timer/event callbacks.

## Regression matrix

- Deterministic odd-sequence fixture on a stopped/manually driven engine:
  original strict reader throws, new paint/refresh/playhead survives.
- Last coherent head held; next publication resumes; document reset clears cache.
- Piano/controller busy timers and Space handlers do not escape exceptions.
- Busy state rejects MIDI editing; project revision/Undo unchanged.
- Reused snapshot clears removed loop; failed read preserves all output fields.
- Shared transport projection survives busy reads with zero host allocations.
- 50,000 single-owner producer callbacks versus concurrent snapshot reads:
  successful fields coherent, failed outputs unchanged, final sample correct.
- Full CTest, packaged J3 + diagnostics smoke and software preview.

Test access is a narrowly friended stopped-engine fixture, not a production
injection API. Hardware Komplete ASIO / 48 kHz / 128 / Workers 8 / Process Off
retest of the user's session remains manual. Keep crash logging enabled.
Validation results/provenance: PROJECT_CONTEXT.md and package VALIDATION.md.
