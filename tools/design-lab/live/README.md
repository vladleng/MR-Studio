# MR Live workspace — #272 / #14

Open `tools/design-lab/live/` on the existing Design Lab HTTP host. This is a static layout editor with labelled fixtures; it does not run audio or apply layouts to JUCE.

1. Add a block by clicking the palette or dragging it onto the canvas. Select it on the canvas or in Layers.
2. Move by the title, resize by the corner, or enter coordinates in Inspector. Grid snaps to 4 px and nearby block origins show alignment guides. Arrow keys move 1 px; Shift moves 4 px.
3. Set typography, MR UDS token roles, semantic SVG, visibility, padding/gap, min/max size, anchors/docking and overflow. Transport/toolbar checkboxes select actions; ↑ changes their order, row/column changes orientation. All action buttons are fixture previews.
4. Try the viewport, UI scale, fixture state and Preview controls. Switching viewport previews anchors/docks without changing saved bounds. The first move/resize in another viewport adopts that viewport as the layout reference in the same undoable gesture; blocks are resolved and fitted inside its bounds. Cancel leaves the original layout intact. If a block minimum exceeds the viewport, lower its minW/minH before editing. Docked blocks must be undocked for free dragging. Minimum sizes overflow and scroll rather than silently shrinking.
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

To remove the editor heading and reclaim its space, select a block and uncheck **Показывать название** in Inspector. The setting persists in drafts and JSON (`showTitle`, optional in v1, defaults to `true` for older layouts). With the heading hidden, drag the static block body; the resize corner remains available. The block name is still shown in Layers and exposed as its accessible label. Undo restores the heading.

## Main MR Live constructor and interactive preview

This workspace develops **main MR Live**. Performance Mode remains a later target.

- Select a block; the Inspector's **SVG элементов блока** section lists the graphic slots actually rendered in that block. Choose any registered MR UDS asset to replace a slot, or **По умолчанию** to restore it. Overrides are per block and persist through Undo, local drafts and JSON export/import. Existing v1 files default to an empty `assetOverrides` map. URLs and unregistered IDs are rejected.
- **Полноэкранное превью** opens the selected viewport without editor panels or handles. **Вписать** fits it to the display; **Реальный размер 100%** uses logical dimensions with the chosen UI scale and allows scrolling. Exit using the visible return button or Escape. Browsers that deny native fullscreen still receive a full-window preview.
- In Preview, move mixer faders, toggle M/S, select songs/patches, use transport state buttons and open toolbar menus. These are local UI simulations; menus do not access real show files. Mixer interaction uses browser range controls for this prototype. SVG fader artwork is used in the construction view. Demo interaction state is temporary and never exported as layout, audio or project state.

Additional browser check: `python tools/design-lab/live/preview-smoke.py` (server on port 8001). It covers fullscreen exit, fader/mute, transport/menu actions, SVG override/reload and no layout writes from simulated actions.

Main controls follow the Studio strip structure (`Desktop.cpp::Strip::resized`): inserts/sends at the top, horizontal pan, fixed 145px fader/stereo meter, dB readout, M/S + record/monitor, output routing. Enlarging a mixer block adds space to inserts/sends; a short block scrolls rather than shrinking the fader. Browser controls/menus remain demo counterparts of native JUCE, not a native renderer. Transport readout and buttons stay on one horizontal line (a too-narrow block scrolls). Input/patch rows include separate volume and pan controls, interactive in Preview.

`python tools/design-lab/live/controls-smoke.py` checks transport alignment, strip hierarchy, patch volume/pan and send-menu interaction over localhost:8001.

## Browser and Setlist demo workspace

**Макет Влада · 1920×1080** loads the user-supplied layout without changing its geometry (layout Undo can restore the previous layout). In Preview/fullscreen, the Setlist block switches to **Browser**. Browser owns all items and nested folders; setlists hold ordered references, including repeated occurrences. Removing an occurrence keeps the Browser item. Deleting a Browser item explicitly removes its references from all setlists and can be undone. Deleting a folder moves its contents to its parent. Folder cycles are rejected.

Create/edit item name, tempo, meter, key, duration and notes; duplicate items, move them to folders, search, and add them to the active setlist. Setlist entries move via ↑/↓ or drag onto another entry; Browser items can be dropped onto visible folder rows. Multiple named setlists can be created, renamed and deleted (the last setlist is protected). **Открыть item** or a double-click selects the working item and updates the header/timeline/cue preview. No actual project/audio files are created.

Demo show state autosaves separately under `mr-live-demo-show-v1`. **Undo show/Redo show** affect the library/show, not layout geometry. Export/import **demo show** uses bounded, versioned JSON (1 MiB, 500 items/folders/setlists, 1000 entries per setlist), retaining last valid state on errors. Layout JSON remains presentation-only. A layout export does not contain the Browser library; export demo show as well to transfer both to another device. This browser schema is not the native `.mrlive` format. Other fixture panels remain prototype functionality; this stage does not imply a complete native show runtime.

Checks: `node --test tools/design-lab/live/show-model.test.mjs`; `python tools/design-lab/live/show-smoke.py` against localhost:8001.

**Стыковка** is enabled by default independently of the 4px grid. Move/resize snaps nearby block edges and viewport boundaries to identical logical coordinates (within 8 screen pixels), with guide lines, eliminating small gaps during placement. Uncheck it for free positioning. It preserves intentional overlaps and does not rewrite an imported layout automatically. Targeted Node snapping tests and HTTP drag/resize assertions cover exact flush edges.

Browser folders now expand **in place** as a nested tree; expanding one does not hide its siblings. Drop a draggable item onto any visible folder header (including a collapsed folder), or onto **Корень / Все папки** to move it to the root. Multiple branches can remain open simultaneously; search temporarily expands branches to reveal matches. The folder selector selects the destination for newly created items/folders, not a separate folder page.

Right-click an item, setlist entry or folder for its context menu; right-click list whitespace for creation/list actions. Menus use the same validated operations as the visible buttons, dismiss outside/on Escape, support arrow-key selection and appear above the canvas in fullscreen. Item selection no longer rebuilds the tree, preserving scroll and double-click activation. `python tools/design-lab/live/tree-smoke.py` covers expanded nested/sibling folders, cross-folder drag persistence, item/folder/setlist context actions, Undo and fullscreen menus.

Browser no longer shows folder selectors, folder-management buttons, selected-item action panels or the demo-show footer. Right-click a folder to create within it, rename/move/delete it; right-click an item/entry for its operations. Right-click empty list space to create at the root. Undo/Redo and demo-show export/import are also context-menu commands; import opens the file picker. The same selected-item/footer command panels are hidden in Setlist. Setlist selection/list-management controls remain at the top. Foreground/SVG/waveform colors are preserved; only surfaces use the graphite palette.
