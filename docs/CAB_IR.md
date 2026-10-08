# Cab IR — local 0.1m / Stage 3c

## Workflow

In a track, bus or Master choose Inserts > Cab IR > Add, then select a WAV.
The file picker starts in the package Impulses folder. The native insert editor shows
independent mono/L/R waveforms, the source name/rate/length, Mix (0–1), Gain in dB,
low cut, high cut and polarity. Apply changes during playback; Neutral, Warm and
Bright are factory control presets. They modify controls, not the selected kernel.
Pause/Stop before adding/removing/reordering or replacing an IR. All project edits
use Undo/Redo. The decoded original kernel is embedded in project schema v10:
saving/reopening still works after the external WAV is moved or deleted.
v10 reads v1–v9; older applications cannot read v10, so keep a project backup.

## Formats and controls

RIFF WAV PCM16/24/32 or float32, mono or stereo, 8–192 kHz, nonempty and at most
one second. Silent, non-finite, excessively large or unsupported input is rejected.
Mono kernels process every output independently; stereo uses L/R diagonal convolution,
repeated for extra output pairs. This is not four-kernel true-stereo cross convolution.
No automatic peak normalization or kernel trimming; original leading delay is retained.
IR resampling uses a bounded windowed-sinc filter during preparation, with antialiasing
and sample-rate scaling. Runtime rates above 192 kHz and scratch above 64 MiB per IR
are rejected before committing a new insert. Project snapshot budget remains 16 MiB.

Mix/Gain and polarity use a 20 ms ramp. Low/high cuts are first-order filters on the
wet signal; 20 Hz low cut and 20000 Hz high cut mean disabled. Active cutoff is clamped
to 0.45 of the runtime sample rate. Dry audio is unaffected by wet filters/polarity.
Neutral: Mix 1, Gain 0 dB, cuts disabled, normal polarity.
Warm: Mix 1, Gain 0 dB, 80 Hz low cut, 5000 Hz high cut, normal polarity.
Bright: Mix 1, Gain 0 dB, 50 Hz low cut, 10000 Hz high cut, normal polarity.
Preset controls and the kernel are saved in the project.

## Processing boundary and latency

A direct 128-tap head and 128-frame FFT-partitioned tail share one streaming state.
The tail starts at kernel tap 128, when its first processed partition is available;
there is no additional block buffering latency. Tests compare complete impulse and
dense reference convolution across arbitrary callback splits, including taps 127/128,
255/256 and the last tail partition. Reported algorithmic latency is 0 samples;
an IR's own leading delay and device latency are separate. Buffers, spectra and FFT
roots are allocated off the audio thread; runtime is bounded, with no allocations,
locks or file I/O. Mix/cut/polarity changes do not rebuild the graph or reopen the device.
Raw recording precedes inserts and remains unaffected.
Full latency compensation/parallel-route and high-load validation belong to 3e/#16.

## Included and external impulses

The package Impulses directory contains MRS-Test-Flat-Mono, MRS-Test-Stereo-Echoes and
MRS-Test-Synthetic-Colour WAVs, created for this project and dedicated under CC0 1.0.
They are test signals, not measured cabinet captures or branded speaker models.
Celestion Cenzo Townshend free IR is a separate download:
https://www.celestionplus.com/free-download/
Their page requires an email subscription. Use your own subscription/download and load
an extracted compatible WAV. It is not bundled or redistributed, and its particular
WAV has not been auditioned by this automated run. Third-party content licensing is
separate from our loader and must permit any future distribution with MR Studio.

## Validation

Local cached/offline-dependency ASIO configure/build, 87/87 CTest and hidden GUI checks.
Tests cover direct/tail exactness, stereo independence, dry mix/gain/polarity, filter
responses, resampling, zero host RT allocation, live updates/Undo/Redo with unchanged
device opens, embedded save/reopen after deleting source, and raw capture invariance.
User acceptance on intended ASIO hardware is pending; #16 not implicitly passed.
