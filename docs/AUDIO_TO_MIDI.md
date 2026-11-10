# MR Audio-to-MIDI — Python Medium (#368 / 0.10.2)

Python/PyTorch MuScriptor Medium is the main backend; preserved HOT-Step C++ is
experimental A/B. Only Medium is supported. Source pin:
`muscriptor/muscriptor@8a1ef88672cc23c26619adaff37ec48d82c0b906`;
weights remain the existing official hash-pinned Medium installation.
This is an implementation checkpoint, not user acceptance.

## Python runtime and controls

Convert to MIDI offers explicit installation/verification/removal of a separate
Python 3.11.9 CPU runtime. Its pinned inference and beat packages, MIT source and
BeatThis final0 checkpoint are independent of Separation. Runtime acquisition
holds a cross-process shared lease; install/remove require an exclusive lease.
Managed inventories, source archive, Python/bootstrap and beat weights are checked.
No hidden inference downloads: the child strips HF credentials and blocks network
connections. Medium weights require the existing explicit HF terms/install flow
and are never packaged with Studio. CC BY-NC 4.0 rights are not commercial clearance.

CPU Auto reserves a logical CPU and caps at 16. Manual 1..32 is affinity-capped;
selection, backend and compatibility mode persist immediately in app preferences.
Python AVX2 dispatch is selected only after CPU/OS capability checks. Compatibility
requests baseline ATen dispatch; this is not proof that every third-party wheel
works on every pre-AVX CPU. Native retains its validated generic/AVX2 selection.
Diagnostics report actual threads/ISA, load, inference, end-to-end and peak process
memory. Python processes are fresh per run: warm means warm OS cache, not a resident
model. Additional threads do not promise speedup.

Single-category and multiple-category choices use official Python `instruments`;
Auto leaves decoding unrestricted. This is not separation or GM remapping.
Reference prelude forcing, sequential five-second chunks and strict EOS are used;
a missing EOS/token-limit error fails the job rather than publishing partial MIDI.
Original Python can emit paired zero-duration retriggers before its cleanup pass;
only its protocol opts into accepting them. They are omitted from editable notes;
legacy native protocol remains strict.

Raw seconds remain unchanged by default. Optional detection uses the installed
BeatThis checkpoint; manual BPM/meter, beat scale, first downbeat, tempo-map export,
quantize subdivisions (including triplets), strength, fill gaps and leading-bar trim
use official Python musical export. BPM=0 retains detected tempo, or uses the
project tempo at the clip when detection is off; meter=0 and downbeat=-1 retain
detection. An unusable requested detection returns an error, without a hidden model
fetch or silent 120 BPM substitution. Physical channels do not identify tracks.

Preview can place original timing. When processing is enabled, placement also
keeps muted original-timing reference tracks, through the same atomic command.
An optional explicit tempo/meter import changes the clip range and restores the
previous tempo afterwards; meter import requires the clip to start on a bar line.
Source audio samples remain unchanged. Existing project MIDI follows its normal
tick-based tempo-map semantics. A project marker preserves model/source/crop
provenance without a new project format. Undo/Redo and Save/Open preserve all of it.

Windows Studio-only/ARA ON validation, full installer checks, representative real
music, the user's private bass repro, physical ASIO/DPI and comparable native/Python
benchmarks remain acceptance gates. Linux/synthetic checks do not establish them.

---

## Historical native implementation contract (0.10.1)

The following describes the preserved experimental implementation. Historical
statements below that call native the only/main backend are superseded above.

# MR Audio-to-MIDI native adapter (#342 / #346)

This is an incomplete implementation checkpoint, not a shipped 0.10.1 feature.
The only backend in scope is HOT-Step CPP `ace-midi`, source commit
`ee7286d14232b48ace3b6eed0fcf3048081ef0d1`. The current ACE-Step Studio
`engines/music-midi-source.json` still references this commit. Studio now bundles
the native CPU engine built from this pin and its license notices. Model weights
are installed separately; no Python ML runtime is required for this feature.

## Verified native contract

Pinned source: `engine/tools/ace-midi.cpp`, lines 1169–1254, 1477–1497,
1520–1590. Native invocation (separate argv elements, never a shell string):

```
ace-midi --model <directory> --transcribe <audio.wav> --out <job-private.mid> --jsonl --device cpu
```

The model directory contains `config.json` and `model.safetensors`.
The frontend averages channels to mono and resamples to 16 kHz; inference uses
5-second chunks. No trim/range CLI flags exist at this pin. `--device cpu`
is overridden by inherited `GGML_BACKEND`; the worker explicitly sets the
child environment to `CPU`. Default `auto` is not a verified CPU path.
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
NON-RT process service below feed that boundary. The ModelStore and Studio
Convert to MIDI action described below use the same controller and Commands.

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
ceil(private-input seconds / 5): a fraction of one resampled sample past a chunk
boundary does not create another chunk. The protocol clips in private-input seconds; the range offset is restored afterwards.
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
It snapshots the selected clip PCM as a standard private WAV, rejects linked/reparse paths, verifies source/model/
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
service with local Medium weights, without Python. The Studio runtime package now includes its native worker.
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
The controller requests only the selected clip's visible source range. The worker
extracts the intersecting source frames (floor start / ceil end at file rate),
then restores the extracted offset before exposing notes in full-source seconds.
Before exposing the preview, notes are intersected again with the exact clip range. Notes/roles outside that range
are omitted; a file recognition with no overlapping clip notes becomes empty.

Application exposes `queue_transcription`, `cancel_transcription`, status/preview
and `place_transcription`. Results remain a preview until explicitly placed. An
optional list selects recognized roles; empty/unknown/duplicate selections fail.
Placement uses one existing PlaceTranscription command and the same stopped,
quiesced MIDI graph/device publication path as MIDI import, preserving Undo/Redo,
source audio and earlier conversions. A device-start failure leaves the Project
unchanged and the preview available for retry. No inference runs in the audio
callback; the Studio Convert to MIDI window uses these service APIs.

`mrs/transcription_models.hpp` implements the pinned ModelStore. Its default
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
foreign folders/entries. ModelStore never stores HF credentials or trusts an
arbitrary caller-provided executable. Small/Large entries are not advertised as installed.

`mrs_transcription_controller_check` is a developer-only driver for the complete
offline path: explicit model-terms flag/local installation, queued inference,
placement, Undo/Redo and project-file Save/Open/Save As. Its work folder must be
new. Successful checks retain only self-contained test project artifacts; the
temporary installed model and native job files are removed. It is not packaged
as the product UI and requires an explicitly hash-trusted native executable.

## Studio Convert to MIDI (0.10.1-Upd3)

Right-click an audio clip in Arrange and choose **Convert this clip to MIDI...**.
The utility window offers the pinned official Medium download or installation
from an authorized local folder containing config.json, model.safetensors and
README.md. Explicit terms acknowledgement is required for either installation.
The masked HF read token is cleared from the editor when downloading starts;
it exists only in that download operation, never in project files/preferences/logs.
Accept official gated access at the linked Hugging Face page first.

Windows WinHTTP verifies TLS and handles redirects manually. HTTPS origins are
limited to huggingface.co and the exact official CDN hosts in
model_download_target; Authorization is sent only to huggingface.co. Signed CDN
URLs/headers are not logged. Each file has a pinned size and SHA256; cancellation,
network errors and hash mismatches clean private staging. A complete verified
installation is published atomically and works offline. There is no mirror fallback.
Download requires space for temporary and installed copies (about 2.5 GB).

After files are verified, **Convert to MIDI** runs the native CPU worker in the
background. Progress and Cancel remain available. Select recognized parts and
choose **Place selected parts** to create normal MIDI tracks through one undoable
command. Closing cancels/discards an unplaced result. Changed projects/sources
invalidate results. Velocity is currently 100; this does not recover dynamics.
Processed Time/Pitch/ARA clips require rendering to WAV first. Playback must stop
before placement; recording blocks conversion/placement. Only the selected trimmed clip is analyzed; no separate internal range selection
is introduced. Split/cut a smaller clip first if required.

Windows CI builds ace-midi from HOT-Step CPP pin
`ee7286d14232b48ace3b6eed0fcf3048081ef0d1` and verifies its GGML pin. Both generic SSE2 and optimized AVX2/FMA/F16C/BMI2 x64 CPU builds use static
CRT/ggml with OpenMP/GPU/host-specific options OFF. The Studio launcher checks
CPUID plus OS XMM/YMM state before choosing AVX2, otherwise uses generic.
It checks CLI exit contracts and system DLL imports. The Studio executable embeds
the resulting engine SHA256; runtime hashing rejects a replaced engine. The
package includes ace-midi.exe and its source/license notices, and excludes weights.
MR Live does not expose or package this feature. No Python ML runtime is required.

The GUI smoke uses a synthetic model and native fake process for download
acknowledgement/token clearing, progress, part selection, placement/Undo/Redo,
stale-result rejection, cancellation, cleanup and close/reopen. Software snapshots
at 100/125/150/200% exercise this window; they do not establish physical DPI or
real-model Windows inference.

## Clip range, CPU and stem input (0.10.1-fix1)

The whole **selected audio clip after trimming** is converted. There is no
inside-clip selection mode; split the desired fragment into its own clip first.
The worker copies only intersecting source PCM frames to its private WAV. It
preserves source rate/channels/PCM format, clamps the right edge at source EOF,
rejects an empty/outside range, bounds private input to 2 GiB/four hours, and
returns note seconds to full-source coordinates before tempo-aware placement.
Project sample offsets are converted to file frames with floor/ceil coverage;
final notes are clipped to the exact project range. Source file geometry/stamp
and selected PCM SHA256 are checked; there is no full long-file copy or content
hash for range jobs. Legacy developer requests without a range keep whole-WAV
semantics. Result.source_sha256 is selected-PCM hash for range jobs, whole-WAV
hash otherwise; offset, duration and rate context must accompany provenance.
The private WAV has its own full hash. Original audio remains unchanged.

**CPU threads** offers bounded Auto or manual 1/2/4/8/16/32. Auto reserves one
logical CPU and caps inference at 16; explicit choices are capped to available
process-affinity CPUs and 32. The child runs at below-normal priority. This is
background inference, never audio-callback parallelism. More threads do not
promise proportional speed; report measured timings/threads/engine per fixture.
The UI reports trimmed duration, elapsed seconds and actual thread count.

**Stem instrument** offers Auto/full mix or any canonical native category,
including drums. A ready stem enters recognition directly, without separation.
The pinned upstream CLI has no instrument argument. The published
`tools/native/ace-midi-stem.patch` adds `--instrument <canonical role>`: each
chunk teacher-forces the category's program after the tie prelude, and argmax
masks incompatible program/drum vocabulary **before** autoregressive decoding.
This changes decoding context; it is not a post-hoc MIDI rename/GM remap and
not a learned instrument-group embedding. The same weights are used. There is
no pitch/octave transform, monophony enforcement or accuracy guarantee. Auto
preserves the upstream greedy path. The worker rejects any raw role outside
the requested category. Other parts may be missed if the user gives a wrong
stem category: select Auto for a mix or uncertain material.

The exact patch and patched source SHA256 are checked by CI against a JSON pin;
engine notices link its source. `--mrs-self-test` exercises the actual pre-argmax
constraint for every canonical category without weights. Both engine variants
use this patch and independent executable hashes. AVX2 dispatch requires AVX,
OSXSAVE, XCR0 XMM/YMM, SSE4.2, FMA, F16C, AVX2 and BMI2. Missing optimized binary
falls back to generic; a present selected engine must pass its expected hash.
Models installed in Upd3 remain valid; only application/native binaries change.

## Remaining integration gates

1. Real Windows inference with an authorized local Medium installation and
   representative instrument recordings; musical/VSTi/user acceptance.
   Linux CPU native short WAV checks use official hash-verified Medium weights.
2. Durable inference provenance in project files and broader Model Manager UX.
3. Physical ASIO/DPI acceptance as applicable. Synthetic GUI/process tests and
   Linux inference do not establish Windows hardware or musical usability.

Restricted weights are not part of the installer. Experimental inference does
not establish permission for commercial weights or output use. No automatic
main merge or issue closure.
