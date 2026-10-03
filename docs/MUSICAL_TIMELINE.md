# SHARED Stage 2 — Musical Timeline

Issue #17. API: `mrs/core.hpp`, `mrs/musical.hpp`.
One ProjectStore, ITransport and MusicalTimeline instance are shared by Arrange/Edit/Mix/Live.
No Live project copy or separate clock.

## Model and time

PPQ 960. Timeline compiles tempo/meter segments on construction; queries use binary search,
without allocations. Tempo is piecewise constant in BPM per quarter note. Meter changes
are at bar boundaries; beat counts denominator notes, including 6/8.
Samples and ticks are rounded to nearest integer; this is not a promise of exact sample
round-trips at low rates/high tempos where multiple ticks share a sample.
Ranges and conversion limits remain the Stage 0 limits.

Project schema 2 adds Chord and ArrangerSection lanes with stable globally unique IDs.
Lanes must be ordered and non-overlapping. Ranges are [start,end); gaps have no current
chord/section. Chord symbol preserves authored spelling; no harmonic parser or inference.
Section color is 0xRRGGBB. Markers reuse existing typed Marker model; they may be unordered
and multiple markers may share a tick. Editing via SetMusicalData uses the existing
atomic candidate validation and snapshot Undo/Redo.

Snapshot writer emits v2. Reader migrates exact v1 grammar to v2 with empty new lanes,
retaining all old IDs, markers, time maps and media references. Unknown versions/data,
malformed, truncated and over-limit snapshots still fail explicitly.
This is a text development snapshot, not a final DAW project package.

## Shared context

MusicalTimeline retains the exact immutable ProjectStore snapshot; context includes revision,
sample/tick/bar/beat, playback/loop, current and next chord/section and marker.
Current chord/section uses start <= tick < end; next is first start strictly after tick.
Current marker is last at/before tick (project order breaks equal-tick ties);
next marker is first strictly later tick. Current marker is an anchor, not a fired cue.
Cue/action execution, scheduling and MIDI are future stages.

Service subscribes to project changes and transport events. Signal dispatch is only on
the control thread, using existing RAII and isolated listener exceptions.
EngineTransport must still be polled by the control/UI pump; audio runs if that pump stalls.
Notifications are observed snapshots, not a stream of every crossed boundary.
A seek/loop that skips events does not replay them. Multiple workspaces subscribe to
the same service and snapshot.

Timeline rebinding is a control-thread ITransport contract. Tempo/meter edits preserve
the audio sample position and update musical interpretation for both service and transport.
EngineTransport rejects sample rates differing from the prepared device. Changing device
rate/project during playback is not implemented. Rebinding does not move sample-based WAV
clips or stretch audio; it changes musical interpretation only. It does not touch audio
callback data, graph or realtime ownership. Mailbox failures can be retried via refresh;
they must be surfaced by the future app rather than hidden.

## Navigation

seek_section/seek_marker use stable IDs; unknown IDs throw without a transport change.
next/previous choose strict later/earlier starts, with no wrap. Previous while inside a
section restarts that section, because its start is earlier than the current position.
Equal-tick markers are navigated as one position. Playback state is preserved by seeks.
loop_section maps [start,end) to exclusive sample loop; clear_loop disables it.
A range rounding to zero samples is rejected by transport.
Engine commands remain queued; context reflects callback acknowledgement, not optimistic UI.

## Coverage and boundaries

Six musical suites plus acceptance: model/conversion, gaps/ties/context,
navigation/loops, shared events/Undo/Redo/time edits, v1 migration/v2 round-trip,
real AudioEngine/EngineTransport callback/poll integration. Existing core/audio suites retained.
Fixtures are deterministic authored data, not an import or an analysis of a real song.

No GUI/editor, MIDI action execution, click/cue audio, tempo ramps, elastic playback,
hot graph replacement or device switching. Next UI work uses these same contracts.
Stage 1 basic native hardware tests at 48k/128 passed. Remaining sustained/reconnect/64
numeric and Studio Pro comparison tests were deferred by the user on 2026-10-03 and do
not block further development. They remain unverified; performance gate remains pending.
