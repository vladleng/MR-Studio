# MR Timestretch Drums SDK 0.1.1-experimental

Default WSOLA search is now ±12 ms (24 ms grain, 5 ms attack guard). The former
±4 ms search could change the dominant pitch of low drum bodies during speed
changes. Low-frequency and mixed kick/snare/hat regressions cover this correction;
these are synthetic checks, not listening acceptance on every drum recording.

Independent C++20 offline time-stretch library for MR Studio research (#331).
No external DSP dependencies, JUCE, device, transport or proprietary zplane code.
This is **not élastique**, is not a reverse-engineered Fender engine and has no
claim of identical sound or feature parity. It is available as an explicit per-channel mode in MR Studio.

## API and use

`mr::timestretch::drums::render(interleavedPCM, config, optionalMarkers, stopToken)` returns
owned interleaved PCM and the source-frame transients used.
`duration_ratio = output duration / source duration = source BPM / target BPM`.
All channels use the same grain positions and blend coefficients. Input is
immutable; failure returns no partial PCM. A neutral ratio copies PCM exactly.
No pitch-shift/formant or tempo-map automation API is implemented.

Build this subdirectory independently:

```sh
cmake -S sdk/mr-timestretch-drums -B build/drums -DCMAKE_BUILD_TYPE=Release
cmake --build build/drums --config Release
ctest --test-dir build/drums -C Release --output-on-failure
cmake --install build/drums --config Release --prefix build/drums-install
```

Embed with `add_subdirectory(sdk/mr-timestretch-drums)` and link `MRTimestretchDrums::SDK`, or use
`find_package(MRTimestretchDrums CONFIG REQUIRED)` against the install prefix.
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

## MR Studio integration

Audio channel context menu → Time stretch mode → Signalsmith / MR Timestretch Drums.
Signalsmith is the default for new and legacy channels. Mode belongs to the channel,
is saved in project schema 32, and supports Undo/Redo. All stretched clips on that
channel use its mode; new clips inherit it. Older applications cannot open schema 32.

The shared worker cache dispatches explicitly to this offline renderer. Source
range, cancellation, generation rejection, exact logical duration, split sharing,
waveforms and playback remain on the existing cache/audio path. The mode and fixed
SDK configuration/version participate in cache identity. Rendering allocates on
the worker only. Input is limited to 256 MiB and SDK output to its 256 MiB budget;
large selections fail explicitly and can be split. No streaming adapter is claimed.

Drums currently supports duration ratios 0.25–4 without pitch or formants.
Incompatible clip parameters reject the edit/mode switch before changing the
project; choose Signalsmith for pitch/formant work. Algorithm quality limitations,
including early/close/weak onset detection, remain experimental.

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
