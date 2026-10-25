# MR Live — main-mode constructor / browser demo (#272)

Serve the repository root over HTTP and open `tools/design-lab/live/`. This workspace develops main MR Live; Performance Mode remains a later target. No npm, CMake, ASIO or audio is required.

## Current workspace

- **Макет Влада · 1920×1080** loads the revised user layout: Setlist at the left, a large item/track workspace, toolbar and transport. Loading is undoable; existing custom geometry is not silently reflowed on reload.
- Every entry of the active setlist is a collapsible item container with linked channel tracks. Containers run left-to-right in exactly the setlist order, including repeated references, with horizontal scrolling. Create Instrument, Player (live input), Playback (audio-file track) or Audio channels via **+ Канал**. Channel operations use **⋮** / right-click.
- Patches appear as rectangular cards with type artwork (including amplifier art for Player). Sub-patches are flatter strips inside their parent patch. Click a card/strip to select it; use right-click for properties/color or **+ Sub-patch**. Rhythm/Solo are example names, not special built-in modes.
- Patch blocks have start/length in bars on each item track (`patchOrder` + `spans`). Drag their header to move; drag either edge to resize. Sub-patches are horizontal strips with their own start/length relative to the parent; drag/resize them in the same way. Parent resizing scales nested strips proportionally. The quarter-bar grid applies to gestures; numeric properties accept finite fractional values. Overlaps are allowed. New patches are assigned to the current track; **Добавить существующий…** reuses a channel patch, and **Убрать с трека** removes an assignment without deleting the library patch. These are persisted musical-time geometries, without real audio/MIDI scheduling.
- Colors are bounded semantic choices (graphite, blue, teal, green, amber, red, violet); sub-patches can inherit the parent color. Color never replaces names/selection indicators. Graphic colors remain intact on graphite surfaces.
- **Inputs / patches** is retired. Volume/pan/M/S and Audio demo arm/monitor live in the **mixer opened by its toolbar icon**. Playback channels also expose their common overall gain in **Playback / stems**. Inputs stays hidden in legacy JSON. **Mixer / meters** is available in the palette/layers again, with movable/resizable layout bounds; its channel strips and the icon drawer share the same channel gain/pan/M/S. Strips keep a compact fixed width with horizontal scrolling.
- The **Browser icon** toggles a separate Studio-style panel with **FX / Files / MR Library**, search and folder/vendor trees, without an AI tab. This contains demo catalog metadata, not the host filesystem. FX can be dragged onto a patch card or double-clicked to assign to a selected channel's active patch; instruments require an Instrument channel. Plugin names are demo descriptions, not loaded binaries.
- Mixer and Browser work in fullscreen, toggle from their icons and have close buttons. When both are open they occupy separate areas. Legacy `view.patches` bindings normalize to `view.browser`.

## Construct and preview

Add blocks from the palette, move by the title, resize by the corner, or enter bounds in Inspector. **Стыковка** snaps adjacent edges exactly (8 screen pixel threshold); grid is an independent 4px snap. Anchors/docking/min-max/overflow resolve responsive geometry. Editing in another viewport adopts it in the same undoable gesture. A docked block must be undocked for free dragging.

Uncheck **Показывать название** to remove a heading and reclaim its space; move the block by its body. **SVG элементов блока** replaces semantic graphic slots using the single MR UDS manifest. Typography, tokens, visibility/order, padding/gap and transport/toolbar composition persist in layout JSON.

**Preview** enables UI interaction. **Полноэкранное превью** hides editor panels and provides Fit / actual size; exit with its return button or Escape. Mixer uses browser range controls for demo interaction; construction artwork uses MR UDS. No audio is recorded or played. Selected Playback files may be read for browser duration metadata without playback.

## Browser / Setlist items

Setlist's **Browser** button opens the show-item library immediately to the right of Setlist (distinct from the FX/Files/Library panel). Folders expand in place; keep several branches open and drag items onto folder headers/root. Search reveals nested matches. Right-click items/entries/folders/whitespace for CRUD, duplication, properties, movement, list operations, Undo/Redo and demo show export/import. The old action/footer panels are hidden.

Drag an item from the side browser onto a setlist entry to insert before it, or onto empty list space to append. This copies a reference; the original item remains in its browser folder. Setlist entries can still be dragged to reorder. Setlists store ordered references, including repetitions. Removing an entry preserves the Browser item; deleting an item removes its references from all lists and is undoable. Folder deletion reparents contents; cycles are rejected. Select **Открыть item** or double-click to change the working item. Items have tempo/meter/key/duration/notes; no native project/audio files are created.

## Patches and shared state

Channels have stable IDs shared by one track in every item and a mixer strip. Gain/pan/mute/solo belong to the shared channel. Track patch/sub-patch selection, geometry, sections and media belong to the item. Repeated setlist occurrences reference that same item data. Sections sit above all its tracks and support move, left/right resize, create/delete, name and color via right-click. In Preview Ctrl+Z / Ctrl+Shift+Z undo/redo a complete show gesture; layout history remains separate in construction mode. Repainting preserves timeline scroll positions.

Instrument/Player tracks use patches. Playback has **no patches**: it displays an audio filename and demo waveform. **Выбрать файл…** reads only a selected filename, not audio content; metadata duration is edited separately. Audio is the recording lane.

A base patch defines a demo instrument/chain and volume/pan/tone/mix. Sub-patches contain only selected parameter overrides; unchecked fields inherit the base, and Base restores original values. The sub-patch schema cannot contain instrument/plugin-chain fields. Patch parameters differ from overall mixer gain/pan. Choosing a sub-patch preserves the same base patch and engine description; this is not a native DSP switching guarantee.

## Persistence and compatibility

Layout JSON stays presentation-only `mr-live-layout` v1 (256 KiB / 80 blocks). Autosave, named copies, layout Undo/Redo and export/import are independent of demo show data. Unknown fields/types/references/geometry are rejected; invalid import keeps the last valid document.

Demo show uses schema v3, exports `mr-live-demo-show-v3.json`, and autosaves under the existing `mr-live-demo-show-v1` storage key for recovery. Explicit v1→v2→v3 migration preserves items/folders/setlists, creates missing channels/tracks and seeds musical geometry. Legacy Playback patches are retained as `archivedPatches` in the export, but cannot be assigned/rendered on Playback. V3 stores item `bars`/`sections`, per-track `media`/`spans` and nested sub-spans. Patch/sub-patch siblings cannot overlap. Resizing a touching edge moves both neighbours; moving the body clamps at neighbours. Nested spans scale with parent resizing. New blocks use a free gap or split an existing block when full. Untrusted v3 imports must supply valid references/extents and non-overlapping patch/sub lanes; old local drafts alone repair pre-existing overlaps while preserving IDs and nested proportions. Total document size is 1 MiB; IDs, types, references, colors, bounds and folder cycles are validated. Import error does not replace current state.

Export **both layout and demo show** to transfer a complete working mock to another device. Browser storage can be cleared/unavailable. This schema is not native `.mrlive`; layout export never changes JUCE. `revision.json` identifies the published source.

## Checks

From repository root, with Python Playwright and Chromium installed:

```sh
node --test tools/design-lab/live/recording-model.test.mjs tools/design-lab/live/model.test.mjs tools/design-lab/live/show-model.test.mjs tools/design-lab/live/channel-model.test.mjs tools/design-lab/live/snapping.test.mjs tools/design-lab/live/timeline-model.test.mjs
python -m http.server 8001 --bind 127.0.0.1
# Another terminal:
python tools/design-lab/live/timeline-smoke.py
python tools/design-lab/live/joined-smoke.py
python tools/design-lab/live/recording-smoke.py
python tools/design-lab/live/item-layout-smoke.py
python tools/design-lab/live/preview-smoke.py
python tools/design-lab/live/controls-smoke.py
python tools/design-lab/live/channels-smoke.py
python tools/design-lab/live/tree-smoke.py
python tools/design-lab/live/show-smoke.py
python tools/design-lab/live/smoke.py http://127.0.0.1:8001
```

Checks cover item grouping/folding, patch cards/nested color strips, chain invariance, icon panels and their coexistence, SVG replacement, channel/track sync, Browser CRUD/context/drag, layout history/roundtrip/recovery/snap and Track Header startup. Physical DPI, native JUCE and audio require their separate stages.


## Linked recording tracks (demo)

Every Instrument/Player item track automatically owns a nested `recordTrack` with its own ID, sharing the parent channel's independent `recordGain` (-60…+12 dB). It is not another user-created Audio channel. New/existing channels, item copies, deletion, Undo and JSON roundtrip preserve this relationship. Older v3 data gains these optional fields; `viewRecordTracks` defaults to false.

**View record tracks** in the timeline/mixer shows the child directly beneath its parent and a narrow record strip next to the parent in both mixer presentations. Hiding tracks removes both presentations without disabling armed channels. The parent **R** button arms Instrument/Player (Audio keeps its own arm). Arming and Play alone do not start capture: **Record** in the common transport starts the armed eligible channels. The mixer header repeats the same global Record/Stop controls so its overlay cannot block them. The simulated session belongs to the current item at the moment Record is pressed; other items do not display active capture. Stop/Pause end it; pressing Record again ends capture while playback stays active. A newly armed channel waits for the next Record start. Leaving Preview or reloading clears the session; arm/view/gain preferences persist. No audio/MIDI data, recording files, input selection or real-time engine are created.


## Timeline editing

Section modes are in the right-click menu; the right edge shows only the active mode glyph. Record visibility and linked-stop preferences are icon buttons with tooltips and pressed states. Item +/- changes horizontal visual scale. The right-edge grip changes the item end in bars at the current scale, preserving all content positions and source lengths. Shortening hides content beyond the boundary without deleting it. Expanding reveals saved negative take origins; The internal origin divider reveals/collapses pre-roll; no separate reveal button remains. Item modes are selected through the header context menu, with only a glyph at the right. Patch creation and library assignment live in the channel context menu.

Record normally starts playback; Shift+Record captures the demo before Play. Play aligns the capture origin to item bar 1, preserving pre-roll. A shared playhead and record regions use the same bar grid as sections/patches/Playback. The ruler can seek.

Playback is a set of source-referencing clips, initialized as one full-item region. Drag the clip body to move, use its edges to trim, and right-click to choose Pointer/Scissors, delete, edit bounds, or restore the original region via lane context menu. Scissors clicks make repeated grid-aligned cuts, with a cut guide and cursor. Hover clip edges for bracket cursors and trim; edges trim even while Scissors is selected. Clips retain source offset across splitting/movement, crop the shared demo illustration and save with Undo/Redo. This edits browser metadata, never the source audio file.


Chord Track sits directly below Sections. Double-click empty space or use its context menu to create Am; double-click/Enter edits symbol/start/end. Right-click edits/duplicates/deletes; Delete/Backspace removes. Drag body/edges moves/trims; Alt-drag copies with a fresh ID; Escape cancels. Overlap is rejected and current chord follows demo transport. Events persist and copy with the item, with no harmonic recognition or generated MIDI. The item origin divider is now inside the grid: drag right to reveal pre-roll and left to collapse it; the external left grip is removed.


Item end is stored independently as arrangement.length; +/- preserves length/lead, and the end grip preserves scale. Playback metadata supplies source duration; clips may extend beyond the item and become visible when its end is extended. File duration properties can also be entered manually. Changing/replacing a source resets its clip to the full source range. Existing media duration falls back if browser metadata cannot be read.


The live toolbar has Files (demo show open/save), Options (record visibility/linked stop and panel toggles), and Transport menus. Open/save icons were removed. The mixer has one compact row beside + Канал with record preferences, Record and Stop; no heading or close button remains. Toggle the mixer icon to hide it. Sections has no add button: double-click empty space creates Verse, or right-click to choose Intro/Verse/Chorus/Bridge/Solo/Outro. Chord Track has no add icon; double-click/context creation remains.


Toolbar Playback / stems and Status icons persist preview visibility. Hidden lower sidebar panels give their space to Setlist; constructor geometry remains untouched. Transport > Back to start optionally returns Stop to the cursor position/item where playback began (not necessarily bar 1). Sections/chords/sub-patches highlight only their active, end-exclusive interval.

Single-click a Setlist item to make it current and reveal it on the timeline. Double-click its name to rename inline (Enter/blur saves, Escape cancels). The left color swatch or row context menu chooses an item color shared with the timeline container. Colors, preferences and names survive reload/export/Undo.
