# SHARED Stage 4 — persistence/state/recovery

One ProjectDocument is used by Arrange, Edit, Mix and Live. There is no export to
a separate Live project. A ShowDocument stores a setlist of project ID/path
references; two entries may refer to the same song. Relative paths are interpreted
by the future shell relative to the show location. ID must be checked against the
opened project before selecting its patch; show loading does not resolve files.

## Ownership
Project-owned: core structure, timeline/chords/sections/cues, shared GraphState,
opaque component/controller plugin state, parameter/bypass settings, mixer channel
gain/pan/mute/solo state, logical-to-physical MIDI bindings, Live patch presets for
the same graph, section-to-patch assignments, authored MIDI actions and notes.
Show-owned: ordering, per-entry notes, optional patch reference, selected entry.
Selected entry is a UI hint, never an instruction to start playback.

Mixer/Live/MIDI fields are persistence contracts. This stage does not implement
mixer DSP, automatic patch switching, MIDI action execution, hardware MIDI or a
VST3 host. An unavailable device or plugin reference loads without opening it.
LivePatch is a graph preset, not a second engine. Plugin bytes are opaque.

## Archive contract
Binary envelope: ASCII MRSARCH1 (8 bytes), little-endian u32 archive version 1,
u32 document kind (1 project, 2 show), u64 generation, u32 chunk count, chunks,
u32 IEEE CRC32 of all preceding bytes. Each chunk has a four-byte ASCII tag,
u64 payload length, payload. Checksums detect accidental corruption, not tampering.

Project requires exactly one PROJ, GRPH, MIXR, MIDI and LIVE chunk. Show requires
one SHOW. PROJ embeds the existing schema 2 core snapshot. Known binary payloads
use little-endian u32 counts, length-prefixed UTF-8 strings and opaque blobs,
u32 booleans, IEEE float32 and signed int32 MIDI route settings. IDs are strings.
The implementation in archive.cpp is authoritative for field order.

Unrecognized chunks are retained byte-for-byte in their original relative order,
including repeated unknown tags. Known chunks are emitted in canonical order.
Unexpected fields inside known chunks and unknown archive versions are rejected
rather than silently dropped. Legacy core text snapshot v1/v2 migrates into a
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
A valid autosave cannot be replaced by a different project or lower generation;
a corrupt autosave can be repaired by a new valid snapshot.

Recovery validates primary, .autosave and .bak, picks the greatest generation
(ties prefer primary, then autosave, then backup), reports rejected candidates and
ignores orphan .tmp files. Candidates with conflicting project IDs are rejected
against the first valid candidate. It does not modify any files. If none validate,
it reports failure. Recovery currently covers project documents; show files have
backup and explicit load, without an automatic show recovery worker.

SharedSession injects one ProjectStore/Transport/GraphStore into all workspaces.
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
