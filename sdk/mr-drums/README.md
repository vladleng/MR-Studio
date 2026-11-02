# MR Drums SDK 0.1.0-experimental

Independent C++20 offline time-stretch library for MR Studio research (#331).
No external DSP dependencies, JUCE, device, transport or proprietary zplane code.
This is **not élastique**, is not a reverse-engineered Fender engine and has no
claim of identical sound or feature parity. It is not selected by MR Studio UI.

## API and use

`mr::drums::render(interleavedPCM, config, optionalMarkers, stopToken)` returns
owned interleaved PCM and the source-frame transients used.
`duration_ratio = output duration / source duration = source BPM / target BPM`.
All channels use the same grain positions and blend coefficients. Input is
immutable; failure returns no partial PCM. A neutral ratio copies PCM exactly.
No pitch-shift/formant or tempo-map automation API is implemented.

Build this subdirectory independently:

```sh
cmake -S sdk/mr-drums -B build/drums -DCMAKE_BUILD_TYPE=Release
cmake --build build/drums --config Release
ctest --test-dir build/drums -C Release --output-on-failure
cmake --install build/drums --config Release --prefix build/drums-install
```

Embed with `add_subdirectory(sdk/mr-drums)` and link `MRDrums::SDK`, or use
`find_package(MRDrums CONFIG REQUIRED)` against the install prefix.
See `examples/render_clip.cpp`. Rebuild the static library with the same toolchain
and runtime flags as the host; the C++ API is not a stable cross-compiler ABI.

## Algorithm

An amplitude-envelope novelty detector proposes attack boundaries (10 ms minimum
spacing). Explicit sorted, unique interior frame markers replace detection.
This is not beat detection: cymbals, noise, rolls and dense mixes can defeat it.
Each boundary maps by `floor(sourceFrame * ratio + 0.5)`; interval lengths are
differences of absolute boundaries, avoiding cumulative rounding drift.

Between boundaries, waveform-similarity overlap-add (WSOLA) searches for a common
multichannel offset. The attack guard copies original samples at unchanged local
speed and blends into the stretched body. A short end guard matches the next
attack's source context. The detector, grain/search and guard settings are part
of the rendered result/cache identity. Protected guards can shrink on short
segments. Extremely short fragments use sample hold; do not claim tonal fidelity.

## Limits and ownership

Worker/offline only: whole output and transient markers allocate. Never call from
ASIO/audio callback. No real-time, streaming seek/flush or dynamic-parameter
capability is advertised. Calls are independent and may run concurrently with
independent inputs/configs; source storage must remain immutable for each call.

Supports 8–192 kHz, 1–8 linked channels, ratio 0.25–4.0, up to 100 million input
and output frames. These are validation bounds, **not quality guarantees**.
Memory budget (default 256 MiB) covers result PCM plus reserved marker payload;
caller PCM, vector-object/allocator overhead and thread stack are excluded.
Long clips may exceed this budget and are rejected. Cancellation is checked
during input validation, detection and each synthesis grain; allocation/copy and
one grain search are not interruptible. Input must be finite.

## MR Studio integration boundary

Existing production code remains on its accepted Signalsmith backend. This SDK
is a candidate worker renderer, not an implementation of the streaming
`mrs::stretch::ITimeStretchEngine` contract. Do not make a fake realtime adapter.

A later integration should dispatch explicitly to this worker renderer from the
shared stretch-cache service, preserving source range, generation rejection,
cancellation, exact logical duration and Commands/Undo/persistence. Key the cache
by SDK version + every configuration parameter + source content/range + markers.
Reject unsupported pitch/formant automation explicitly, never silently discard
it. No second engine/device/transport is needed. Incremental streaming and an
honest ITimeStretchEngine adapter require separate implementation and tests.

## Validation and acceptance

Focused CI builds Windows/MSVC and Linux/GCC, runs synthetic contract tests,
builds the example and installs a reusable SDK package. Release tests use explicit
checks (not disabled assert). Tests cover exact length, unity PCM identity,
repeat determinism, linked/anti-phase channels, isolated transient alignment,
silence/short clips, finite inputs, marker validation, memory limits and cancellation.
The sine crossing gate is a broad pitch sanity check, not a sound-quality metric.

NOT VALIDATED: real drum/cymbal A/B, perceptual parity with Fender, complex-mix
pitch fidelity, multitrack microphone phase alignment across separate calls,
ASIO timing/xruns, CPU budgets, arbitrary seek and project/UI integration.
For microphone groups, process all channels in one call (max 8), not independently.

Use the same authorized source WAV, sample rate, ratio and no extra FX for
Fender and SDK renders. Test ratios 0.5/0.75/0.9/1/1.1/1.25/1.5/2.
Compare attacks, cymbal tails, stereo image, clicks, duration and CPU; retain
source hashes and reference settings per #62. Do not label this production-ready
or equivalent to Fender until that evidence and user listening acceptance exist.
