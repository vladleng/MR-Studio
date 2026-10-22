# Stretch foundation API — #57/#58

Implementation scope: backend-neutral state/codec and mapping in `mrs_core`,
Signalsmith adapter and bounded worker render in `mrs_stretch`, diagnostics in
`mrs_stretch_bench`. Current requirements are #57/#58/#62 and the contract revision
in [PR #269](https://github.com/vladleng/MR-Studio/pull/269). That docs PR is separate
from implementation. This slice does not activate stretched clip playback or add
clip controls; those are #59/#61. Full cache publication remains #60.

## Public boundary

`mrs/stretch.hpp` contains no backend types. `Parameters` stores duration ratio,
pitch, formant and quality intent. The version-1 standalone codec rejects future
versions/unknown enum values/trailing data without truncating them. A caller must
retain an unsupported record or reject loading it explicitly. This slice does not
change Project/Clip schema: legacy clip defaults and project persistence belong
to #61. The reference test links only `mrs_core` without the adapter.

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
