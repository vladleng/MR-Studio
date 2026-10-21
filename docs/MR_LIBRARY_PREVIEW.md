# MR Library WAV audition

Scope: [#255](https://github.com/vladleng/MR-Studio/issues/255), within
[MR Library #251](https://github.com/vladleng/MR-Studio/issues/251). User acceptance
is tracked in the issue; this document defines behavior rather than acceptance.

## Playback and output

Each visible WAV row/card has an asynchronous waveform, Play/Pause/Resume and
click-to-seek. The footer Stop clears the active audition. Pause retains the
position; EOF returns the Play glyph. Waveform metadata shows duration, sample
rate, bit depth and channels; unavailable media and decode failures have text
feedback. The context menu reveals the WAV or its source folder in Explorer.

Audition uses the existing device callback and selected Master hardware outputs,
at 0.65 gain multiplied by Master volume. It mixes after project effects, so
Master inserts do not recolor the browser reference. Mono is centered; a mono
Master output receives the stereo average. Output remains clipped to [-1, 1].
The same rule applies to direct, mixed and whole-graph anticipative processing;
audition is never stored in speculative Process Buffer packets.

Project playback may continue alongside audition. Audition does not start, seek
or stop project transport. Recording cancels/mutes audition, and the UI rejects
new requests while recording or without an active hardware audio device.
Offline clock is not an audible preview device. Closing/hiding the browser,
rescan, relink, disabling/removing a source and preparing another audio device
cancel audition. Tempo sync and time stretching are outside this stage.

## Work and memory boundaries

The existing WAV reader supports PCM16/24/32 and float32. Audition supports mono
and stereo and performs linear sample-rate conversion on its decoder worker.
Missing, corrupt and unsupported files fail without altering project transport.

Only the decoder worker reads audio. It uses a 4096-frame decode cache and a
32768-frame fixed SPSC ring with generation-tagged stereo samples. The device
consumer discards obsolete generations in logarithmic work, then consumes at
most the requested callback frames. Stop/seek never waits for decoding; there
are no preview allocations, destruction, locks, filesystem or UI calls in the
callback. Stream ownership is retained until device shutdown and engine teardown.

Waveforms run on a separate cancellable worker, with 256 peak bins, a 96-entry
cache and at most 20 queued tasks. Cache identity includes source root and asset
metadata; explicit rescan/relink invalidates pending work and cached waves.
Painting requests cached data without reading WAVs or SQLite.

## Arrange import

JUCE ListBox's row drag mechanism starts a WAV drag, including multiple selected
rows in list mode. Arrange resolves valid indexed source-relative WAV paths and
passes them to its existing file drop/import command. Missing files, directories
named `.wav`, traversal and reparse paths are rejected. Original WAVs are never
modified. Clip creation, Undo/Redo and project Save/Open remain owned by the
existing importer and project model.

Software regression tests cannot establish physical ASIO/xrun performance,
listening quality, mixed-DPI behavior or user acceptance of the browser.
