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
These are source observations, not runtime validation.

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

`mrs/audio_to_midi.hpp` accepts validated, paired note events. It does not yet
parse JSONL, launch a process, install a model or expose a UI action.

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

## Remaining integration gates

1. Reproducible native build, executable SHA and transitive license audit;
   real short WAV inference with officially authorized, pinned/hash-verified
   weights. Upstream code is MIT, MuScriptor weights CC BY-NC 4.0. Do not use
   ACE-Step Studio's open mirror as a workaround for official gated access.
2. Bounded JSONL parser and isolated Windows worker/Job Object with environment,
   path/hash checks, atomic outputs, progress, cancel, timeout, crash and cleanup.
3. Explicit optional model installation/verification/removal/offline behavior,
   rights metadata and Studio-only UI placement through the existing application
   transaction path. Provenance persistence is not implemented by this checkpoint.
4. Windows Studio CI, Save/Open/Save As file integration, musical A/B and
   user acceptance. Core snapshot round-trip tests do not establish these gates.

Restricted weights are not part of the installer. Experimental inference does
not establish permission for commercial weights or output use. No automatic
main merge or issue closure.
