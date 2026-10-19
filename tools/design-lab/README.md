# MR Design Lab — Track Panel Editor (v1)

Standalone browser editor for the three existing adaptive **Arrange / Track Header** modes. This is an independent **layout prototype**, not an integrated JUCE editor, not an audio plugin host, and not a release build.

## Open

From repository root, serve the repository over HTTP, then open `/tools/design-lab/`. Example:

```bash
python -m http.server 8000
# http://localhost:8000/tools/design-lab/
```

The HTML and JS need **no npm/CMake/compilation**. Running `index.html` via `file://` is insufficient because browsers commonly block `fetch` of the adjacent SVG manifest; use localhost or GitHub Pages.

**GitHub Pages:** If the repository's Pages is configured to serve the root of `main`, after an approved merge the path will be `https://vladleng.github.io/MR-Studio/tools/design-lab/`. This is a *conditional future URL*; merely having a feature branch does not publish the page. Publishing from another source directory needs corresponding path/asset adjustments. Do not modify Pages settings or merge as part of this prototype.

## Shared MR UDS assets

The editor reads the **single existing library** at `../../assets/ui/mr-uds/`:

- `manifest.json` — registered IDs and original SVG file paths.
- `tokens/mr-plugin-colors.json` — shared visual accent.
- `svg/icons/`, `svg/controls/`, `svg/rig/`, `svg/reverb/` — referenced directly in library search/preview; nothing is copied into this tool.

The original assets and catalogs stay canonical. The browser uses SVG in `<img>` elements and validates manifest file paths, without injecting arbitrary inline markup. Asset IDs in layouts are semantic manifest references rather than external URLs. **Import does not allow arbitrary SVG or URL injection.**

## Features

- Three independently editable profile layouts: **Compact** (`Strip` height below 84 px), **Medium** (84–123), **Expanded** (124+).
- Read-only view of existing SVG library; search/categories, drag asset onto preview to add it, or click to update the currently selected SVG.
- Select, move and resize controls; inspector with coordinates/width/height, label, visibility, fader style, or referenced SVG. Snap-to-4px is optional.
- Adjust track height; preview switches between three profiles automatically according to existing thresholds. In the current app `Arrangement::trackHeight` leads to `Strip` bounds with **2 px** reduction.
- Save local draft in browser localStorage; import/export `mr-track-panel-layout-v1.json` with bounded, type-checked schema.
- No cross-service authentication, no writable GitHub credentials, no migration workflows changes.

## Important boundaries

This tool **does not modify JUCE source code**. Its sample geometry is an approximation based on current `Strip::resized()` at `apps/studio-juce/src/Desktop.cpp`, not pixel-accurate native rendering. In particular current C++ handles state, hit-testing, meters and custom-drawn controls. Existing names and layouts may differ. This stage proves the shared asset references, editing UX and JSON profile contract.

Native integration comes in a separate task **after cloud CI migration #241 is accepted**:
1. Extract layout model and the three profiles from `Strip::resized()` and dependent paint/hit-area calculations, preserving actual component identity and bindings.
2. Add a versioned JSON importer and safe UI-thread hot reload in a non-release dev profile; file watcher with last-known-good fallback; no filesystem, SVG parsing or dynamic allocations in audio callback.
3. Keep release assets compiled/packaged and validated; add targeted regression checks around profile boundaries 83/84/123/124, resize, keyboard focus, accessibility, hit targets, Undo, project persistence and hardware-independent GUI.
4. Extend the same schema and manifest-based asset registry to Mixer, MR Rig, Reverb, Strip, Live, and Plugin Lab without duplicating SVG sources or creating a second theme framework. Align implementation with `docs/MR_UNIFIED_DESIGN_SYSTEM.md` and existing MR UI Kit #189.

## Testing

Functional smoke is manual in a modern desktop browser: load via HTTP; verify asset count, thumbnails from `manifest.json`, drag SVG, switch profiles, edit width, reload browser, export/import JSON and switch on 83/84 and 123/124 px **Strip** heights. No JUCE build or CTest is warranted for this static-only change. Browser test status remains **NOT TESTED** until actually exercised.

## Safety & Git

Keep this feature branch isolated from `codex/cloud-cicd-migration`. Do not merge or change workflow triggers without approval. No build artifacts, SDK, secrets or proprietary plugin assets belong in this directory.
