# Time / pitch API and manual clip integration — #57–#61

Implementation scope: backend-neutral state/codec and mapping in `mrs_core`,
Signalsmith adapter and bounded worker render in `mrs_stretch`, session WAV cache
in `mrs_audio`, stopped manual clip controls and cached playback in the shared
Desktop application / JUCE Arrange. Diagnostics use `mrs_stretch_bench`.
Requirements remain in #57–#62 and the revision in
[PR #269](https://github.com/vladleng/MR-Studio/pull/269).
Dynamic realtime parameter scheduling, persistent/HQ cache policy, bounce/export
integration and final quality/default approval are not completed by this slice.

## Public boundary

`mrs/stretch.hpp` contains no backend types. `Parameters` stores duration ratio,
pitch, formant and quality intent. The version-1 standalone codec rejects future
versions/unknown enum values/trailing data without truncating them. A caller must
retain an unsupported record or reject loading it explicitly. Project schema 31 added optional `ClipStretch`; schema 32 adds optional per-clip
`file_tempo` (authored source BPM). Schemas 1–30 load without stretch,
with original duration, pitch and source references. Older applications reject
new schema-32 projects explicitly. Schemas 31 load without source BPM. The archive envelope is unchanged. The
reference contract test links only `mrs_core` without the adapter.

Duration ratio is output/source; speed is its inverse. `output_frames(N,r)` rounds
the absolute endpoint once, half up. Split lengths are differences of rounded
absolute endpoints. `FrameDemand` carries fractional source demand between output
blocks, including rate changes; it does not re-round each block independently.
Dynamic event scheduling/smoothing and integration with PDC remain #59.

First-adapter bounds: mono/stereo; 44.1/48/96 kHz; ratio 0.125–8; combined pitch
±24 semitones; cents ±100; formant shift ±12 semitones; optional base hint
20–2000 Hz (0 selects 200 Hz). These are finite resource/API bounds, not a promise
of uniform quality. Configure input/output block caps up to 32768/4096 frames.
An output request exceeding the input cap at its current ratio rejects before DSP.

## Ownership and lifecycle

One stream has one `ITimeStretchEngine` owner. `prepare`, `reset`, `seek`, `flush`
and destruction are worker/control operations with exclusive ownership and may
allocate or free. A caller quiesces processing before them. `reset` rebuilds with
seed 0 because the pinned upstream reset does not reset its random engine;
repeated renders start from identical backend state. Failed prepare validation
leaves the previous prepared engine intact.

`seek` resets/reseeds and accepts zero-padded context ending at the next source
read position. It changes generation and discards fractional demand. Maximum
context is the reported analysis block plus interval. No arbitrary realtime seek
or state publication/lifetime scheme is claimed here.

`input_required(output_frames)` previews demand without advancing it. `process`
accepts planar channel arrays with at least that many input frames; input/output
regions and output channels cannot overlap. The returned counts describe actual
consumption/production. Insufficient input/output capacity or invalid buffers do
not advance history. Zero-output requests are no-ops with correctly sized channel
arrays. Requests and parameters are checked before DSP, including finite input.

Prepared `process` and same-quality `set_parameters` perform bounded work without
allocation/free/locks/I/O; synthetic allocation probes include the first call,
irregular blocks, transposition/formants and dynamic ratios. The adapter preserves
the caller's floating-point mode using `ScopedNoDenormals`. Non-finite DSP output
is zeroed and latches `backend_error` until reset/reprepare. This does not certify
physical ASIO or the future full playback graph.

Quality/enable changes require reprepare. Backend-native split computation is a
prepared runtime option, not serialized musical intent. `flush` requires a buffer
of output-latency frames, drains once and enters `drained`; it is not a repeatable
streaming read. Subsequent process requires reset/seek. No callback reclamation,
background scheduler or new audio device is introduced.

## Timing and worker render

```text
source read head = source processing position + inputLatency (source frames)
processing time = time at which pitch/ratio parameters apply
audible head = processing position - outputLatency (output frames)
```

An input latency is not simply added to an output latency in PDC, particularly
when ratio varies. The adapter reports both separately. #59 must map automation
and residual route delay to the existing Process Buffer/PDC system; no automatic
PDC compensation is added by this library.

Worker `render` primes with the first input-latency source frames, padded with
silence at boundaries. It then reads ahead, streams blocks, discards exactly the
reported output pre-roll and writes the exact logical length. Reads beyond the
source end are zero-padded. One-shot flush supplies the last logical output frames;
short clips discard the remaining pre-roll across process and flush. No extra
spectral windows are advanced beyond the logical duration to replace that flush.
Tail-inclusive export needs a separate policy.
Empty/short sources and non-integer ratios are supported without reading beyond
the source. Frame arithmetic rejects signed overflow before stream processing.

Read/write callbacks receive interleaved PCM at the processing rate and must fill
the requested read span or return false. They run synchronously on the calling
worker. False returns `io_error`; callback exceptions propagate to the caller.
Cancellation is checked at bounded block boundaries. Results include the number
of frames already written: a caller must stage and atomically publish a cache/file
only after successful completion. This helper itself creates no files/cache.

The worker's neutral-state path copies PCM without spectral processing. Playback
may enter/leave that path only through latency-aligned, tested transitions (#59).
Backend process itself retains spectral latency even at neutral parameters.

Formants: preserve compensates pitch; shift applies semitones plus compensation;
follow-pitch disables formant compensation. The pinned implementation converts
`setFormantBase` with `freqToBand`, so the hint is Hz/sampleRate (cycles/sample),
despite ambiguous upstream README prose about Nyquist. A positive default avoids
an implicit pitch detector. Polyphonic formants are not a monophonic vocal editor.

## Manual clip state, cache and playback

Audio clip context menu **Time / Pitch...** exposes enabled/bypassed, duration
ratio, semitones, cents, formant mode/shift/base, quality, Apply and Reset. Source
and current output durations are displayed. One Apply is one undoable command;
editing requires stopped/paused transport. The feature is opt-in and experimental:
the #62 non-unity transient-peak criterion is still failing, not waived by the GUI.
ARA and native stretch on the same source are rejected in both directions to
avoid ambiguous/double processing, including bypassed bindings. Reset native
state before attaching ARA; detach ARA before native processing.

`Clip.source_offset` anchors a raw source region; `source_frames` is its original
length. Double-precision source-domain `window_begin/end` select a view into that
region. Clip length is `round(end*r)-round(begin*r)`, not an independently rounded
scaled length. Split shares one boundary. Ratio/bypass changes retain both
endpoints; Trim changes endpoints using the central mapping helper. Neutral or
bypassed playback reads the original PCM at `anchor+round(begin)`; processed
playback reads the cache at `round(begin*r)`. Extending a trimmed processed view
is bounded by its initial region; Reset first to extend into other original audio.
Selections shorter than one raw sample cannot be reset/bypassed to a zero-length
clip and are rejected. Fades keep their output-frame durations, clamped to clip
length. Undo restores the complete previous state. MIDI stretch is rejected.

The optional trailing `STRETCH_CLIPS` section stores clip identity, quoted version-1
Parameters codec, raw region length and source-domain endpoints with max_digits10.
Missing, duplicate/unknown identities, invalid windows and unsupported codec state
are validated explicitly. Original media references remain unchanged; projects
never serialize backend types, PCM caches, temporary paths or waveform peaks.

`prepare_stretch_cache` runs on a worker and writes bounded float32 RIFF blocks
into its own newly created temporary directory. Cancellation, write errors or
source changes discard that directory; only a complete render and prepared
waveform are published. The WavFile lifetime retains its own directory until the
last graph/data owner retires outside callbacks. Shutdown/replacement cancels and
joins workers before releasing source assets. One active batch, bounded progress
atomics, stale ProjectStore/revision rejection and draining cancelled futures
prevent old results from entering a new edit/project. Esc cancels; Apply retries.
Playback/record start refuses missing/invalid results; there is no raw PCM fallback.

The session key includes retained immutable source identity, rate/channels/length,
region anchor/length, all Parameters, pinned backend revision and cache render
version. Split/trim views share one result. Total required output is capped at
512 MiB and existing disk playback remains limited to 32 streamed voices / 256 MiB
read-ahead buffers. Entries unused by the current project are evicted on control;
Undo regenerates them. Reopen/relink creates new source identity and regenerates.
File-backed sources are checked by size/mtime across render and before playback;
external mutation requires reopening. This is not a durable content-hashed cache
or a guarantee against adversarial same-size/same-mtime media replacement.

The graph consumes prepared PCM through its existing ReadAhead/seek/loop/routing
and fade path. The cache has exactly the logical duration, with backend pre-roll
already removed, so it introduces zero additional route PDC. DSP/reset, WAV I/O,
waveform construction and cache reclamation never execute in the audio callback.
No live spectral parameter automation or additional engine/device is introduced.

## Direct clip timing controls — #61

The audio clip context menu begins with File Tempo, Speedup, Transpose and Tune
(cents). Hover a numeric field and turn the wheel; Ctrl+wheel gives finer steps.
Keyboard entry uses Enter to commit and Escape to revert the field text. Wheel
bursts are buffered for 350 ms and committed as one Undo command; closing the menu
flushes a pending burst even after the popup component has been destroyed. The
existing Time / Pitch dialog retains bypass, reset, formants and quality.

Speedup is the inverse of the stored duration ratio (0.125–8). Source BPM is
unknown by default: 0 in the menu clears it; authored values are 1–999 BPM.
Changing source BPM alone changes metadata. **Fit to project tempo** explicitly
sets ratio = source BPM / project BPM at the clip start, anchors the start, and
preserves pitch/formants. It rejects unknown BPM and unsupported ratios. This is
one constant ratio: it does not follow later tempo-map changes or warp a clip
across tempo boundaries. No detection or metadata rewrite of source WAVs occurs.
This explicit fit command extends the original manual-only MVP at user request;
automatic follow, warp and library preview/drop synchronization remain deferred.

Alt+left-drag either audio edge stretches the grabbed clip; the opposite edge,
raw source anchor and source-domain window stay fixed. Pitch/formants and BPM are
retained. Timeline zero and supported ratio bounds constrain the gesture. Visible
length uses the nearest representable difference of rounded window endpoints,
which may be one sample from the requested moving edge on a trimmed source.
Neighbors do not ripple; multi-selection does not stretch other clips. Plain
edges still trim; Alt+drag inside a clip still copies. Shift bypasses grid snap.
Only the waveform geometry changes during preview; no DSP/model edit happens on
mouse move. Release commits one command; Escape/focus loss cancels. Edits require
stopped transport and reject stale source clips/projects and native+ARA sharing.

Source BPM persists in an optional `CLIP_TEMPOS` section after `STRETCH_CLIPS`
and before `END`: quoted clip ID and BPM, max-digits precision. Duplicate, unknown,
MIDI, nonfinite and out-of-range records are rejected. Split/copy inherit BPM;
resetting processing retains BPM. It is clip metadata and not a DSP/cache key.

## Diagnostics and acceptance

`mrs_stretch_bench --seconds 300 --warmup 30 --repeats 3 --voices 8 --ratio 1.25`
prints p50/p95/p99/p99.9/max/mean block time and counts above the nominal device
deadline. Options: `--rate`, `--block`, `--pitch`, `--cheap`, `--split` and
`--voices` (1–32). This is a serial, aligned-start, non-device synthetic load;
there are no hardware xrun measurements and no whole-graph CPU claim.
Record the CPU/OS/compiler/revisions alongside CSV and repeat on Windows/ASIO.

`--output NEW.wav` also writes at most 30 seconds of the original procedural
harmonic/transient fixture through the worker render. A new staging directory and
no-overwrite publication protect existing files. Render outputs are float32 RIFF;
the input WAV importer/SRC is unchanged. `--impulse-report` is a diagnostic CSV:
it reports observed peak displacement and the one-frame criterion without
pretending that producing the report means passing that criterion.

The unity-ratio, near-neutral-pitch impulse regression isolates latency accounting
from spectral transient behavior. The non-unity impulse report is separate #62
quality evidence. Do not weaken or declare that gate passed based on exact output
length or an ordinary unit-test PASS. Current measurements, remaining failures,
listening/hardware status and commits belong in issues #57/#58/#62.

Windows artifacts use **MR Studio 0.7.1**, with application/PE version 0.7.1.
The separate Stretch Evidence artifact contains the diagnostic executable,
synthetic source/ratio/pitch WAVs and CSV/hash records. Runtime packages contain
the product and required licenses; evidence files are not product media.
