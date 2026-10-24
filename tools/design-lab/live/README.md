# MR Live workspace — #272 / #14

Open `tools/design-lab/live/` on the existing Design Lab HTTP host. This is a static layout editor with labelled fixtures; it does not run audio or apply layouts to JUCE.

1. Add a block by clicking the palette or dragging it onto the canvas. Select it on the canvas or in Layers.
2. Move by the title, resize by the corner, or enter coordinates in Inspector. Grid snaps to 4 px and nearby block origins show alignment guides. Arrow keys move 1 px; Shift moves 4 px.
3. Set typography, MR UDS token roles, semantic SVG, visibility, padding/gap, min/max size, anchors/docking and overflow. Transport/toolbar checkboxes select actions; ↑ changes their order, row/column changes orientation. All action buttons are fixture previews.
4. Try the viewport, UI scale, fixture state and Preview controls. Free geometry edits use the source viewport; anchors/docks resolve in other sizes without changing saved bounds. Docked blocks must be undocked for free dragging. Minimum sizes overflow and scroll rather than silently shrinking.
5. Name the layout. Autosave recovers a draft on reload; Save copy keeps up to 20 named browser copies. Export JSON for durable storage or transfer. Import it again to check the roundtrip, then provide the exported JSON and reference viewport for acceptance. Reset is undoable.

`example.json` is a starting layout, not an approved MR Live screen. Browser storage can be unavailable or cleared; export files regularly. Invalid imports retain the current document. An invalid stored draft is not overwritten during startup.

## Contract

`mr-live-layout` version 1 / registry version 1 / target `mr-live-main`. The Track Header format remains unchanged. Future targets require an explicit supported version/validator update. Block order in the array is paint order; IDs remain stable through export/import. Each block stores registry type/version, bounds, min/max, anchor/dock, overflow, typography, token roles, asset ID and an ordered subset of registered semantic action bindings. Docking resolves independently against the viewport; it does not reserve space for adjacent blocks. No nested containers in v1.

Bounded JSON: 256 KiB, 80 blocks, 100-character names/titles, finite numeric ranges; unknown fields, types, versions and references are rejected. Imported code, markup and URLs are never executed. SVG paths come only from the existing MR UDS manifest. A failed manifest load disables asset references and reports the problem; it cannot silently erase a saved draft.

Registry (`registry.mjs`), validation/history/layout resolution (`model.mjs`) and DOM editor (`editor.mjs`) are independent of native services. Add a repository-owned registry entry with descriptors/defaults/renderer to extend the palette and inspector. Renderers receive fixture state and validated block data.

## Checks

From repository root:

```sh
node --test tools/design-lab/live/model.test.mjs
python -m http.server 8000 --bind 127.0.0.1
# In another terminal; Python Playwright and Chromium required:
python tools/design-lab/live/smoke.py
```

Browser smoke covers real HTTP, SVG loading, move/resize, hidden layers, history, local recovery, JSON roundtrip/rejection, viewport/scale controls, fixtures and existing Track Header startup. Physical DPI, native JUCE and audio are outside this stage. `revision.json` is deployment metadata produced alongside a Pages copy; an unpublished checkout displays `local source`.

## Visual block previews

The block interiors render realistic static fixtures: song rows, timeline/section lanes, stems waveforms, input patches, MR Micro mixer strips, transport icons, toolbar actions, chord cards, cues, lyrics and status rows. Shared MR UDS illustrations are referenced through the loaded catalog; Play/Pause/Stop reuse `assets/ui/icons/transport/` at its canonical path. These assets must also be available on the Pages host.

Waveforms/meters/readouts are illustrative, not audio measurements. Controls inside fixtures do not execute commands; select and configure the containing block in Inspector. `Playing` changes fixture styling but does not start a second transport. No schema or saved geometry changes are required: existing browser drafts and exported layouts receive the new rendering automatically.
