# MR Library Smart Audio Search (#256)

Smart analysis is local and read-only with respect to source WAVs. It does not
stretch/import audio, analyse MIDI, download a model, or send files to a service.

## Analysis and provenance

Algorithm v2 extracts Type, duration, RMS/peak, onset density, a preferred tempo
with half/double alternatives, spectral centroid/flatness and a conservative key
proposal. One streaming decode supplies the features. Bounds: 90 seconds,
16 million interleaved samples, 32 channels; exceeding a bound is an explicit
`budget` state. Pending, complete, budget, unknown and error are distinct in the
row tooltip. Silence and unreliable evidence retain unknown musical metadata.

Tempo uses onset-grid coherence across the sequence. The preferred octave is
80–160 BPM; its confidence measures grid consistency, not the composer's intended
beat unit. Half/double alternatives between 40 and 300 BPM remain visible. The
existing integer BPM filters/sort use the rounded preferred value. Filename BPM
and manual BPM (including an explicit clear) retain precedence.

Key uses a bounded set of Hann-windowed 4096-point spectra and chroma/profile
correlation. It requires a sufficiently long tonal loop with multiple pitch
classes, rejects noisy material and close relative-key ties, and reports unknown
for isolated pitches. Major/minor is a proposal, not a promise of musical intent.
Manual key/clear and filename key retain precedence. Source, confidence and
algorithm version accompany the cached audio result. The analysis spectrum uses
the first channel, downsampled by block averaging; descriptors cover 65–3500 Hz.
This is intentionally a lightweight descriptor, not a full-band perceptual model.

Type retains #263 metadata priority and manual/rejected decisions. Strong temporal
evidence may also fill an otherwise unknown loop Type. Feature scores are heuristic
and are not calibrated accuracy probabilities.

## Scheduling and controls

One dedicated non-RT worker reads/analyzes audio; only the catalogue worker writes
SQLite. Browsing, filter edits and previews remain usable while analysis runs or
is paused. A queue of at most 64 jobs refills incrementally. Reads use 4096-frame
blocks and cooperative cancellation/pause checks. The worker yields after roughly
8 ms of work, with a 1 ms sleep (subject to OS timer resolution); cancellation
polls inside DSP loops do not each incur a separate timed sleep.
Before and after decoding, file size/mtime must still match the queued identity.
Late results from cancelled/replaced generations or removed/relinked/changed assets
cannot attach to the new catalogue identity.

The browser header `...` menu provides Analyse missing, Pause/Resume, Cancel and
Reanalyse all. Progress appears in the existing status line. Cancel keeps completed
cache entries and existing metadata; it does not roll back unrelated library edits.
Folder indexing retains its existing transactional scan/cancel behavior. Startup
and source scans queue missing/stale features, reusing current results even when
sources are temporarily offline. Error results require a force reanalysis or a
changed identity to retry.

Schema v6 adds versioned features and saved searches. Migration first creates a
consistent `*.pre-v6-*.sqlite` backup; source/asset/tag IDs and manual edit history
remain. The previous Type cache remains compatible. No DAW project format changes.

## Similarity and saved searches

Row `...` → Find similar by audio features requires a complete reference analysis.
Existing folder/tag/collection filters still apply. Ranking combines log-relative
duration, attack density, brightness (centroid), spectral flatness, RMS and tempo
distance with octave equivalence. Missing/disabled/stale/unanalysed candidates are
excluded; the reference itself is excluded. The selected sort reads Feature match.
The tooltip explains the actual duration, brightness and attack-density differences
and the other criteria. These descriptors do not establish semantic or musical
similarity; key is deliberately not used as a tonal-distance metric.

Filters → Remove similarity reference or Clear leaves similarity mode. Header
`...` → Save current search stores inclusion/exclusion tags, source/folder/collection,
text, key/BPM, Likes, sort/random seed and similarity reference. Page offset/size
are not part of a saved search. Saving the same name replaces its definition;
Saved searches recalls it, Delete saved search removes it. Dangling source or
reference IDs yield no matches rather than silently redirecting to another source.

## Reproducible accuracy audit and limits

`tests/sample_analysis_tests.cpp` generates original labelled WAVs with anonymous
names. `library256-benchmark.csv` is included in Windows diagnostics. The initial
controlled set includes nine pulse-loop tempos (60–180 BPM), ten major/minor
progressions in five roots, and negatives (isolated hit, noise, silence, irregular
events, single pitch). It reports exact and octave-aware tempo counts, error,
unknown/wrong predictions and key counts, rather than hiding octave errors.

Local initial result: preferred BPM exact within 2.5% on 5/9; octave-aware 9/9,
mean nearest-alternative error 0.47 BPM (octave-aware precision/recall 100%, preferred-octave precision/recall 55.6%), zero unknown/wrong tempo estimates among
those nine. Loop Type 9/9. Key 10/10 on the constructed progressions (precision/recall 100% on this set); zero invented
BPM/key in the five negatives. This small, simple synthetic set is not a claim
of accuracy on production sample libraries. Precision/recall on a representative
real corpus, real-library acceptance and physical ASIO/xrun measurements remain
required before declaring the entire #256 DoD complete.

Contract tests additionally exercise active/paused browsing, bounded-queue refill,
cancel, manual overrides, cache invalidation/restart/offline reuse, persisted
combined searches and query integration. They do not substitute for hardware tests.

## Dependency audit

No new DSP library/model is bundled. The radix-2 FFT, extraction and ranking code
are original; the major/minor profile coefficients are the published
Krumhansl–Kessler numerical profiles (Krumhansl, *Cognitive Foundations of Musical
Pitch*, 1990). Reusing the host's realtime convolution FFT would couple catalogue
work to realtime processing internals; the small non-RT implementation stays local.

Alternatives inspected on 2026-10-22: [Essentia COPYING](https://github.com/MTG/essentia/blob/master/COPYING.txt)
is AGPLv3; [aubio COPYING](https://github.com/aubio/aubio/blob/master/COPYING) and
[libkeyfinder LICENSE](https://github.com/mixxxdj/libkeyfinder/blob/main/LICENSE)
are GPLv3. They are not added as dependencies in this implementation.
