# MR Library browser

Scope: #261 and #263, building on accepted #252–255. Splice is a reference for search
organization; MR keeps its existing UI theme, local catalogue and audio engine.

## Navigation and results

Folders opens cover cards for enabled sources, then direct child folders.
Folder scope includes WAVs below that path. Tags opens category cards, then tag
cards; choosing a tag filters the WAV list. Collections opens virtual collection
cards. Back returns to a parent/root; navigation shares the results toolbar. The
redundant All WAV control and separate navigation row are removed.
Cards come from the full index, independently of the bounded WAV page.

List rows and grid cards show filename, tags (One-shot/Loop first), square pack
artwork and a short bar waveform beside the filename while a title reservation of
up to 150 px (the full longest page name if shorter) and 64 px of waveform fit.
The waveform shrinks before hiding; there is no fixed
560 px cutoff. The instrument and waveform occupy common columns calculated once from the
longest filename on the current result page. They share a horizontal centre line
and remain bounded by row width; narrow cells hide the waveform. Loop/One-shot glyphs precede the
larger filename; hover and keyboard selection expose Play, while playback shows
Stop. Clicking Stop stops preview and disables arrow audition. Folder paths and technical
metadata for the selected WAV are in the bottom browser panel. Full paths/tags are
available as tooltips. Instrument glyphs reuse the canonical MR UDS SVG resources.
List preserves WAV multiselection; navigation cards cannot become WAV drag data.
Grid drag selects the card under the pointer. Frozen IDs/paths retain the #255
stale/missing/relink guards and normal Arrange import/SRC.

Search accepts up to 128 UI characters and evaluates up to 16 whitespace-separated
words. Every word must occur in the path, a non-rejected tag or a collection name;
words may match different fields. Case folding includes Cyrillic. There is no
semantic/cloud search. Type signal analysis is described below. Relevant sorting scores exact tag
matches and path matches; filename sorting uses the leaf name.

Instrument/Genre/Pack/Type menus combine inclusion (OR within a group, AND between
groups) and explicit tag exclusions. Counts reflect the full matching query.
Other catalogue choices remain available with zero current matches. The Filters
menu removes individual conditions; visible chips expose selected tags or quick
suggestions when no tags are selected. Clear retains the current folder/collection
scope. Key uses catalogue major/minor spellings, matches flat/sharp equivalents and supports Unknown; BPM allows
an exact value or range. Sorting offers name, relevance, BPM, duration, modification,
recently indexed and a seeded random order stable across pages.

Row `...` contains Likes, collection membership, tag creation and Explorer actions.
The header menu contains collection creation/deletion, rescan and current cover.
All browser popup menus and submenus use the owning MR Theme, matching the rest of
Studio. Shared popup backgrounds use 85% opacity (15% transparency), including
blurred backdrops and native preset menus; menu text remains opaque.
Context menus anchor to the screen click; keyboard menus retain their focused
control anchor. The compact footer provides previous/next sample, Play/Pause and Stop,
with separately identified pagination controls.
The existing Sources and Edit Tags panels remain available. DAW transport keyboard
bindings outside the browser and preview routing remain unchanged. Browser focus
blocks both the parent key-press and key-state transport paths. In the
focused results list, Space toggles keyboard audition: Up/Down select and play
while enabled, and select silently after stopping. Modified arrows retain JUCE
multiselection; text entry retains its own keys. Type contains only One-shot and
Loop; FX, Fill, SFX and Transition belong to Instruments. Existing Type tags are
normalized transactionally at startup, preserving decisions and edit history.
Type aliases include plural/camel/separated One Shots/Oneshots and Loops, including
source-root names. Explicit custom type rules win, then filename, then nearest
labelled ancestor. Conflicting explicit types remain unknown and show `?`.
When no explicit Type exists, recognised single-hit categories
(percussion, impacts, stabs, shots) supply One-shot; tempo-bearing rhythmic phrase
names supply Loop. Numeric take suffixes are supported. Generic instrument/FX names,
tempo alone and conflicting explicit types do not suffice; these rules do not analyse
audio. Before using weak word-derived guesses, the worker analyses the WAV signal
independently of filenames: isolated attacks/swells with decay, repeated attacks
on a time grid, or a repeated changing energy envelope. RMS envelopes are smoothed
to reject carrier beating and monotonic trends; duration/tempo alone do not prove
Loop. Audio proposals carry `audio` provenance and a heuristic confidence score
(not a calibrated accuracy probability), visible in tooltips and Edit Tags.
Ambiguous sustained sounds, silence and files exceeding the bounded analysis
budget (90 seconds, 16 million interleaved samples, 32 channels) remain unknown
unless metadata supplies a type. This is local deterministic DSP, not a trained
model or a guarantee of the creator's intended Type. Files without enough evidence
remain `?`. Confirmed manual types suppress automatic alternatives; setting a
type replaces the active opposite type in the same undoable batch. Startup/rescan
reclassifies the existing index without changing source media.
Signal results (including uncertain results) are cached by asset identity, source
root, size, modification stamp and algorithm version. Rescan invalidates changed
audio; unchanged files and offline sources reuse the cache. Analysis streams bounded
blocks on a dedicated analysis worker, supports pause/cancellation, and does not run
in GUI/audio callbacks. Browsing continues independently. Batch cancel keeps finished
features; folder-scan cancellation still restores the previous complete index.
See [Smart Audio Search](MR_LIBRARY_SMART_SEARCH.md) for BPM/key, feature comparison,
progress controls, saved searches, accuracy audit and limits.

Pack follows Genres in the filter row. It is indexed from the named folder before
the first conventional content category (Drums, Loops, FX, etc.), otherwise the
first child folder beneath the source; a source pointed at a pack retains its own
name. This is folder-derived metadata, not a vendor identity lookup. Pack supports
the same inclusion/exclusion, full-query counts and pagination as other tag groups.
Confirmed manual Pack metadata suppresses automatic alternatives.

## Covers and persistence

Folder, tag category, individual tag and collection cards have a cover menu at the
image corner. Choose copies the selected PNG/JPEG into `artwork/` beside the library
SQLite database on its worker. Original images and source folders are not modified.
Sample artwork uses the highest ancestor with a cover as its pack cover, falling
back to the asset artwork. Managed covers take priority over automatic/manual
source artwork at the same folder. Reset restores the inherited/automatic
cover or the folder fallback. Managed filenames are unique, so cache keys cannot
reuse stale decoded thumbnails after replacement.

Library schema v4 added managed covers; v5 added audio-Type cache; v6 adds musical
features and saved searches. v1–v5 migrate in place
without changing existing source/asset/tag/collection IDs. Before upgrading an
existing database, SQLite backup writes a consistent `*.pre-v6-*.sqlite` copy
(including WAL contents); a failed backup aborts the upgrade. This does not change the
DAW project format. Images are limited to 20 MiB, 8192 pixels per dimension and
16 megapixels; bounded header validation precedes worker decode. Thumbnails and
waveforms retain bounded queues/caches and stay outside the audio callback.
Waveform cache holds 384 small peak envelopes (more than the maximum 300-file page),
and evicts the least recently used entry rather than the lexically first key.
Painted/selected waveform reads refresh recency; background completions cannot
continually evict and reload a visible low-key waveform.
Deleted sources/collections lose their cover associations. Unreferenced managed
files after removal/crash may remain as cache files; automatic cache garbage
collection is not part of this stage.

WAV drags in Arrange show translucent clip rectangles at the snapped import
position, with project-rate duration calculated by the same ceiling conversion
as SRC. Multiple files are sequential on an existing audio track, or create
separate tracks at the same start in empty space. Header inspection runs on a
bounded latest-request worker. Exit/drop/project rebuild clears the preview;
non-audio targets and unreadable WAV batches have no insertion ghost. No vertical
insertion line is painted; the transport cursor is hidden during a WAV drag and
restored on exit/drop. A compact drag label sits 18 px right and 12 px below
the pointer, leaving the actual insertion preview ahead of the label without
changing its snapped drop position.

## Validation

Focused catalogue tests cover multiword queries, inclusion/exclusion, dependent
counts and available choices, stable shuffle pagination, managed image import,
reset/restart/rescan and source integrity, plus prior catalogue/preview regressions.
`juce_library_browser` exercises real catalogue snapshots, root/folder/tag/collection
navigation, narrow/wide geometry, WAV selection and the existing preview callback.
It exports 100%, 125%, 150% and 200% software images to Windows test artifacts. User visual
acceptance and physical mixed-DPI/ASIO measurements remain separate.
