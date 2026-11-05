# MR Audio-to-MIDI native adapter (#342 / #346)

This is an incomplete implementation checkpoint, not a shipped 0.10.1 feature.
The only backend in scope is HOT-Step CPP `ace-midi`, source commit
`ee7286d14232b48ace3b6eed0fcf3048081ef0d1`. The current ACE-Step Studio
`engines/music-midi-source.json` still references this commit. No upstream
runtime, model weights or new dependencies are bundled with MR Studio.

## Verified native contract

Pinned source: `engine/tools/ace-midi.cpp`, lines 1169–1254, 1477–1497,
1520–1590. Native invocation (separate argv elements, never a shell string):

```
ace-midi --model <directory> --transcribe <audio.wav> --out <job-private.mid> --jsonl --device cpu
```

The model directory contains `config.json` and `model.safetensors`.
The frontend averages channels to mono and resamples to 16 kHz; inference uses
5-second chunks. No trim/range CLI flags exist at this pin. `--device cpu`
is overridden by inherited `GGML_BACKEND`; a future launcher must explicitly
set the child environment to `CPU`. Default `auto` is not a verified CPU path.
Exit 0 indicates completion, 1 a model/audio/output error, 2 invalid invocation.
Linux CPU invocation/exit/progress have been exercised with official Medium
weights. Windows native inference and musical quality remain separate gates.

JSONL stdout contains:

- `progress`: `completed`, `total` (chunks).
- `note_start`: `index`, `pitch`, `time` (seconds), `instrument`.
- `note_end`: matching `index`, `time`.
- `done`: `notes`, `midi_bytes`, emitted after writing the MIDI file.

Diagnostics are written to stderr. Success must require exit 0, a valid `done`,
paired/bounded note events and validated output. Upstream does not check the
return values of `fwrite`/`fclose`; the `done` line alone cannot prove a valid
MIDI file. No cooperative cancel flag exists: cancellation/timeout must terminate
the isolated process tree and discard partial results.

## Implemented domain boundary

`mrs/audio_to_midi.hpp` accepts validated, paired note events. The protocol and
NON-RT process service below feed that boundary; model installation and the UI
action are not implemented yet.

`PlaceTranscription` is prepared off the audio callback from the immutable
project snapshot captured when inference starts. Its input times refer to the
**full source file**. An extracted-range worker must add the extraction offset
before calling it. Clip sample positions and `source_offset` use project-rate
samples; worker resampling must not change that coordinate system.
Stretched clips and sources with ARA bindings are rejected until rendered audio
and its time coordinates are explicitly supported.

Notes intersect the visible source range, then map to project samples and
through the existing piecewise `Timeline::to_ticks`. Both endpoints are mapped
independently; no fixed BPM, grid quantization or minimum musical duration is
invented. Notes shorter than one representable tick are omitted. Velocity is
the explicit default 100, not an inferred property.

The pinned vocabulary contains 34 melodic categories **plus drums**. Each
nonempty category becomes an existing instrument track/MIDI clip with stable
IDs. Melodic tracks use zero-based channel 0 and their representative GM program
event; drums use channel 9. No external MIDI port or VSTi is silently assigned.
Independent tracks can reuse channels; external port allocation is a later UI
decision. Source clips and earlier conversions remain intact.

One `ICommand` makes placement reversible through existing ProjectStore history
and existing serialization. A changed project snapshot rejects late results
atomically; the future controller must also check captured project ID/revision
before placement. No project schema migration is introduced. The current
boundary creates all nonempty parts; selective placement remains unimplemented.

## Bounded protocol and isolated process service

`mrs/ace_midi_protocol.hpp` incrementally accepts the pinned flat ASCII JSONL
schema, including CRLF and arbitrarily fragmented reads. Unknown/duplicate fields,
coerced numbers, unpaired/repeated indices, incomplete EOF, missing/out-of-order
progress, missing/duplicate done or trailing traffic fail the whole job. Escaped
strings and JSON extensions are deliberately outside this native schema.
Expected chunks use the pinned resampler's truncated 16 kHz frame count, not
ceil(full-source seconds / 5): a fraction of one resampled sample past a chunk
boundary does not create another chunk. Clipping still uses full-source seconds.
Limits: 4 KiB per line, 16 MiB stdout, 100000 messages, 32768 raw notes,
4 hours of source audio. Done counts must match paired notes; note times must
fit the padded 5-second chunk envelope. A final note_end after final progress
is valid. Output notes are then intersected with the actual source duration;
this handles hallucinations in a padded final chunk. Zero raw notes or zero
surviving notes is a successful **empty** result, with no placement Command.

`mrs/audio_to_midi_worker.hpp` provides a **blocking NON-RT service call** used by
the controller's owner job thread. It never captures the ProjectStore, audio engine or GUI.
The caller supplies an explicit executable, model directory, WAV, temporary root,
expected SHA256 digests and model rights/revision metadata. It performs no download
or trust-on-first-use; publisher trust and license approval belong to Model Manager.
It snapshots the full WAV, rejects linked/reparse paths, verifies source/model/
executable integrity before and after inference and checks the snapshot again.
The result echoes project/clip/revision, digests and model metadata in memory;
durable project provenance is still outstanding. The controller checks caller
identity/revision again before preparing placement.

Windows launches suspended, assigns a kill-on-close Job Object with one-process
and memory limits, then resumes. Only explicit stdin/stdout/stderr handles are
inherited. The CRT argument vector is quoted without a shell. The child receives
an environment allowlist with GGML_BACKEND=CPU and bounded CPU threads; no inherited
HF token, proxy credentials, probe or GPU override. Linux development uses a
process group, address-space limit and nonblocking pipes. This contains lifecycle
and resource use; it is not a security sandbox for an untrusted executable/DLL.

Cancellation/deadline cover snapshot/hash/inference/verification. A child must
exit 0; done alone is insufficient. Bounded stdout/stderr are drained separately,
stderr is capped at 4 MiB traffic/64 KiB retained. The SMF is structurally parsed,
its declared bytes checked, and notes compared to raw JSONL by program, channel,
pitch, velocity and time, accounting for pinned writer minimum durations and
overlap trimming. SMF's fixed 120 BPM is never used for project placement.
The process tree is killed/reaped before cleanup on every path. Results are
published as typed notes only after validation; private input/output files are
removed on success, empty, failure, cancel and timeout. A cleanup failure is an
explicit failed result, not silently successful placement.

The developer-only `mrs_audio_to_midi_check` accepts executable/model/source/temp
paths followed by executable/weights/config SHA256 values. It exercises the full
service with local Medium weights, without Python or bundling the native worker.
`mrs_audio_to_midi_worker_tests` uses a C++ fake executable to test failure paths;
that does not establish Windows MuScriptor inference or musical quality.

## Controller and local model connection

`mrs/transcription_controller.hpp` owns one background job and at most one pending
result. The job captures request values, a model lease and immutable media lifetime
guards; it does not capture Application, ProjectStore, GUI or engine pointers.
Progress is coalesced through one atomic packed counter; future completion transfers
the full Result. Control-thread poll/cancel never join an unfinished job. Shutdown
requests stop and joins during NON-RT teardown. A cancelling job must finish/reap
before another can start; a ready result must be placed or explicitly discarded.

The controller captures the ProjectStore identity, project ID/revision/content,
clip ID, document path and source file stamp. Changes invalidate a pending result
or request cancellation of active work. Undo back to equal Project content still
changes revision and cannot revive an old result. Replacing/opening/new-project
requests cancellation; media guards keep staged WAVs alive until cleanup. The
placement guard runs again immediately before the Command is prepared.
Before exposing the preview, notes are intersected with the selected clip's
visible source range, retaining full-file seconds. Notes/roles outside that range
are omitted; a file recognition with no overlapping clip notes becomes empty.

Application exposes `queue_transcription`, `cancel_transcription`, status/preview
and `place_transcription`. Results remain a preview until explicitly placed. An
optional list selects recognized roles; empty/unknown/duplicate selections fail.
Placement uses one existing PlaceTranscription command and the same stopped,
quiesced MIDI graph/device publication path as MIDI import, preserving Undo/Redo,
source audio and earlier conversions. A device-start failure leaves the Project
unchanged and the preview available for retry. No inference runs in the audio
callback; these are service APIs, not a Convert to MIDI GUI action yet.

`mrs/transcription_models.hpp` implements an offline local ModelStore. Its default
catalog is official `MuScriptor/muscriptor-medium` at
`f32236969308476e01fd3aae67357de5feb05a2d`, with exact config/weights/model-card
sizes and SHA256. Installation requires explicit acknowledgement of the CC BY-NC
and HF-specific conditions and a supplied authorized local folder. It verifies
source files, copies into an exclusive private staging folder with a disk cap and
cancellation, verifies the copy and source again, then atomically renames the
complete installation. An acknowledgement receipt is metadata, not commercial
permission. Existing installations are not overwritten implicitly.

Verification is blocking NON-RT and works offline. Acquiring a lease checks only
receipt/file geometry; the worker independently checks executable/config/weights
hashes. A lease blocks install/remove through that ModelStore instance and its
copies while inference is using it. External filesystem mutations remain subject
to runtime hash checks. Removal accepts corrupted weights but refuses linked or
foreign folders/entries. ModelStore does not download, store HF credentials, install
a native executable, or claim an arbitrary caller-provided executable is trusted.
Official downloads, native binary/dependency distribution and installation UI
remain separate gates. Small/Large entries are not advertised as installed.

`mrs_transcription_controller_check` is a developer-only driver for the complete
offline path: explicit model-terms flag/local installation, queued inference,
placement, Undo/Redo and project-file Save/Open/Save As. Its work folder must be
new. Successful checks retain only self-contained test project artifacts; the
temporary installed model and native job files are removed. It is not packaged
as the product UI and requires an explicitly hash-trusted native executable.

## Remaining integration gates

1. Windows native executable/build validation and distribution/transitive license
   audit; real instrument recordings and musical acceptance. Linux CPU native
   short WAV inference uses official hash-verified Medium weights. Upstream code
   is MIT, MuScriptor weights CC BY-NC 4.0. Do not use
   ACE-Step Studio's open mirror as a workaround for official gated access.
2. Official model download/native engine registration and Studio Convert to MIDI
   UI, progress/cancel and part selection through the implemented controller APIs.
3. Durable inference provenance, native executable/dependency distribution and
   complete Model Manager UX; local install/verify/remove APIs already exist.
4. Windows native inference and project-file/musical/VSTi acceptance. Developer
   fake-process file tests and Linux real Medium checks do not establish Windows
   hardware, GUI or musical usability.

Restricted weights are not part of the installer. Experimental inference does
not establish permission for commercial weights or output use. No automatic
main merge or issue closure.
