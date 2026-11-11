# Shared Persistence — Project/Show documents and recovery

**Canonical boundary (2026-11-11):** MR Studio owns a `.mrsproject` DAW document; MR Live is a **standalone app** with its own `.mrlive` show/item document. Both reuse Core serialization/storage infrastructure where applicable but **do not share a single open Studio ProjectDocument**. Source field order and schema/migrations are determined by the code, not an old milestone banner.

The original Stage 4/Stage 5 implementations and version-21 notes in the previous introduction are historical; the schema has since advanced (notably snapshot schema35 for projects with Audio-to-MIDI provenance, as described below). No current-build version or user acceptance is inferred here. For current architecture see [ARCHITECTURE.md](ARCHITECTURE.md); for show state [MR_LIVE_NATIVE_INTEGRATION.md](MR_LIVE_NATIVE_INTEGRATION.md); for CI/acceptance see [AGENTS.md](../AGENTS.md) and Issues.

## Ownership
Studio project-owned: DAW tracks/clips, timeline/chords/sections, GraphState, plugin/controller state, mixer/routing/automation, MIDI state and project-specific metadata.

MR Live show-owned: setlist/order, items, own Playback/stems/channel and patch/sub-patch state, performance/cue settings and related stable references. The historical stage-4 `LIVE` chunk in Studio archives may remain for backward compatibility; it is **not** a claim that Live operates inside the Studio document. The exact persisted schema and migrations must be checked against current Store/Archive implementation.

Stage-4 descriptions of unimplemented mixer DSP, MIDI and VST3 hosting refer only to that **historical implementation slice**, not today's feature status. Unavailable plugin/device references should be restored without attempting realtime activation; VST3 state remains opaque where required.

## Archive contract
Binary envelope: ASCII MRSARCH1 (8 bytes), little-endian u32 archive version 1,
u32 document kind (1 project, 2 show), u64 generation, u32 chunk count, chunks,
u32 IEEE CRC32 of all preceding bytes. Each chunk has a four-byte ASCII tag,
u64 payload length, payload. Checksums detect accidental corruption, not tampering.

Project requires exactly one PROJ, GRPH, MIXR, MIDI and LIVE chunk. Show requires
one SHOW. PROJ embeds the core snapshot (schema 2 at the original SHARED Stage 4
implementation). Unaffected projects retain schema32; separation records select
schema33/34 according to their existing contract. Audio-to-MIDI provenance selects
schema35 only when hidden `TranscriptionRecord` entries are present. Each entry
holds its stable ID, source clip ID, created MIDI clip IDs and bounded provenance
text; it never creates a musical timeline marker. The archive envelope is unchanged.
Older core snapshots still migrate with an empty transcription-record collection;
apps predating schema35 cannot open a project that contains those new records.
Known binary payloads
use little-endian u32 counts, length-prefixed UTF-8 strings and opaque blobs,
u32 booleans, IEEE float32 and signed int32 MIDI route settings. IDs are strings.
The implementation in archive.cpp is authoritative for field order.

Unrecognized chunks are retained byte-for-byte in their original relative order,
including repeated unknown tags. Known chunks are emitted in canonical order.
Unexpected fields inside known chunks and unknown archive versions are rejected
rather than silently dropped. Supported legacy core snapshots migrate to the
current schema with defaults for absent fields; original v1/v2 snapshots enter a
document with transparent empty graph and empty added state; stable IDs survive.

Limits: complete archive <=64 MiB, text <=1 MiB, collection count <=65,536,
each component/controller blob <=16 MiB, existing graph/core model limits also
apply. Decoders validate checksum, bounds, duplicate/missing known chunks, enums,
values and references before returning a document.

## Save and recovery
All capture/serialization and file operations run off the audio callback.
A single writer owns each path. Caller advances generation when document state
changes; SharedSession capture combines initial generation plus project/graph
revision. Metadata is immutable in SharedSession for now; a metadata editor must
advance generation explicitly. Changes to the core model that remove referenced
tracks/cues/sections must also update associated metadata before saving.

Save creates an exclusive same-directory temporary, writes and flushes it, saves
the previous *valid* primary to .bak through another temporary, then uses native
replacement. Windows uses CreateFileW/FlushFileBuffers/MoveFileExW; POSIX uses
open/write/fsync/rename plus directory fsync. Filesystem/device power-loss behavior
is outside the fault-injection guarantee. An error after POSIX rename may mean the
new primary was installed but directory sync failed; callers should re-read.
A damaged primary is refused and does not overwrite the backup: recover and Save
As to a new path. Identity changes or generation rollback on an existing valid
destination are refused. Save hooks permit deterministic pre-replace failure tests.
Windows sharing/permission failures surface to the caller.

AutosaveWorker receives immutable snapshots on the application thread, coalesces
pending work, serializes/writes .autosave on one background thread and records
errors. wait(ticket) reports the latest completed write covering that ticket;
coalesced older snapshots can be skipped. Destruction drains pending work and
joins off RT. No timer is implemented: the shell will schedule submit.
A valid primary/autosave prevents saving an autosave with a different project ID or lower generation;
a corrupt autosave can be repaired by a new valid snapshot.

Recovery validates primary, .autosave and .bak, picks the greatest generation
(ties prefer primary, then autosave, then backup), reports rejected candidates and
ignores orphan .tmp files. Candidates with conflicting project IDs are rejected
against the first valid candidate. It does not modify any files. If none validate,
it reports failure. Recovery currently covers project documents; show files have
backup and explicit load, without an automatic show recovery worker.

Within a Studio application session, SharedSession injects ProjectStore/Transport/GraphStore into Studio workspaces; this does not merge MR Live's show document or application runtime with Studio.
Capture reflects current core and graph edits and retains added metadata/unknown
chunks. Loading starts stopped at sample zero, clears Undo, and never recreates
audio-device handles, pending MIDI/parameter events, sounding notes or running
processors. PreparedGraph::capture must be called while quiescent before copying
live processor state into the document. The application must explicitly prepare
and publish the recovered graph later.

No embedded audio assets, plugin binaries, final project bundle, GUI, autosave
timer, cross-process writer locking or universal power-loss guarantee in this stage.

## Validation
Nine persistence suites plus console acceptance checker exercise full state and
opaque unavailable-plugin round-trip, legacy fixtures, unknown chunks, malformed
envelopes, ownership/references, Unicode filenames, backup/fault injection,
recovery fallback, autosave errors/coalescing/drain and shared workspace capture.
Existing core/audio/musical/processing suites run unchanged on Windows/Linux,
Debug/Release; ASIO builds run the same contracts without hardware interaction.

## Document extensions — 2026-10-03 user decision
Studio projects use `.mrsproject`; standalone MR Live shows use `.mrlive`. Original reference-based `ShowDocument` is a historical serialization foundation, **not** a statement that today's Live items open Studio songs directly;
no archive schema change or duplicate audio engine is required by the extension.
The current production Files menu opens/saves projects and WAVs. Live show file
commands and the separate show screen remain LIVE stage work.
