# MR Library browser

Scope: #261 and #263, building on accepted #252–255. Splice is a reference for search
organization; MR keeps its existing UI theme, local catalogue and audio engine.

## Navigation and results

Folders opens cover cards for enabled sources, then direct child folders.
Folder scope includes WAVs below that path. Tags opens category cards, then tag
cards; choosing a tag filters the WAV list. Collections opens virtual collection
cards. All WAV shows samples in the current scope; Back returns to a parent/root.
Cards come from the full index, independently of the bounded WAV page.

List rows and grid cards show filename, tags (One-shot/Loop first), square pack
artwork and a short bar waveform beside the filename when the cell is at least
560 px wide. Narrow cells hide the waveform. Loop/One-shot glyphs precede the
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
hidden audio analysis or semantic/cloud search. Relevant sorting scores exact tag
matches and path matches; filename sorting uses the leaf name.

Instrument/Genre/Type menus combine inclusion (OR within a group, AND between
groups) and explicit tag exclusions. Counts reflect the full matching query.
Other catalogue choices remain available with zero current matches. The Filters
menu removes individual conditions; visible chips expose selected tags or quick
suggestions when no tags are selected. Clear retains the current folder/collection
scope. Key uses catalogue major/minor spellings, matches flat/sharp equivalents and supports Unknown; BPM allows
an exact value or range. Sorting offers name, relevance, BPM, duration, modification,
recently indexed and a seeded random order stable across pages.

Row `...` contains Likes, collection membership, tag creation and Explorer actions.
The header menu contains collection creation/deletion, rescan and current cover.
The footer provides previous/next sample within the page, Play/Pause and Stop,
with separately identified pagination controls.
The existing Sources and Edit Tags panels remain available. DAW transport keyboard
bindings outside the browser and preview routing remain unchanged. Browser focus
blocks both the parent key-press and key-state transport paths. In the
focused results list, Space toggles keyboard audition: Up/Down select and play
while enabled, and select silently after stopping. Modified arrows retain JUCE
multiselection; text entry retains its own keys. Type contains only One-shot and
Loop; FX, Fill, SFX and Transition belong to Instruments. Existing Type tags are
normalized transactionally at startup, preserving decisions and edit history.

## Covers and persistence

Folder, tag category, individual tag and collection cards have a cover menu at the
image corner. Choose copies the selected PNG/JPEG into `artwork/` beside the library
SQLite database on its worker. Original images and source folders are not modified.
Sample artwork uses the highest ancestor with a cover as its pack cover, falling
back to the asset artwork. Managed covers take priority over automatic/manual
source artwork at the same folder. Reset restores the inherited/automatic
cover or the folder fallback. Managed filenames are unique, so cache keys cannot
reuse stale decoded thumbnails after replacement.

Library schema v4 adds a managed cover association table; v1–v3 migrate in place
without changing existing source/asset/tag/collection IDs. Before upgrading an
existing database, SQLite backup writes a consistent `*.pre-v4-*.sqlite` copy
(including WAL contents); a failed backup aborts the upgrade. This does not change the
DAW project format. Images are limited to 20 MiB, 8192 pixels per dimension and
16 megapixels; bounded header validation precedes worker decode. Thumbnails and
waveforms retain bounded queues/caches and stay outside the audio callback.
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
