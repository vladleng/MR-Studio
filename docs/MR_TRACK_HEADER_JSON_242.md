# MR Track Header JSON v1 — JUCE integration pilot (#242 / B1)

**Status:** developer opt-in candidate; awaiting Windows Actions build, focused smoke and user GUI acceptance. This is not an automatic UI migration.

## Source of truth

- Browser editor: `tools/design-lab/`.
- User-exported **v1** model committed at `assets/ui/mr-uds/layouts/mr-track-panel-layout-v1.json`. There are three independent layouts: Compact 60, Medium 110, Expanded 166 px (the Strip logical height is `Arrangement::trackHeight - 2`).
- SVG references remain semantic IDs in the single existing `assets/ui/mr-uds/manifest.json`; no copied SVG or new Theme.
- `TrackHeaderLayout.h` parses/validates the JSON **once on the JUCE message thread**, not in paint, audio callback or realtime workers. No network calls or GitHub writes.

## How to preview in a Windows build

Run the JUCE executable with the exported file path (quote paths containing spaces):

```powershell
.\MoonRiverStudioJuce.exe --track-layout "C:\path\to\mr-track-panel-layout-v1.json"
```

The reference file checked into GitHub has the same schema and the exact 28 geometries supplied by the user, but a downloaded GitHub Actions package does **not** currently auto-copy/activate this JSON; pass an absolute path to the browser-exported JSON. The normal application launch remains unchanged. Command line is session-scoped (not written into a project or preferences). Re-open with a newly exported JSON to preview another design.

Bad/missing/unsupported JSON is rejected with a message in the application log. Existing legacy Strip layout remains active; the same profile or another successful load is not partially modified.

## Runtime scope

- Only **Arrange Track Header / mini Strip** uses this JSON, and only after explicit launch flag. Mixer Strip, master and all native plugin editors retain previous behavior.
- Maps named JSON elements to the **existing** host-bound JUCE controls Mute/Solo/Record/Input Monitor, Gain, Pan, Input routing and (in Expanded) Output routing. No duplicate controls, parameter implementations or commands.
- Applies `name` bounds to the paint and name editor double-click hit area. Stereo peak-meter coordinates come from the selected profile while their **levels still come from live host meter state**.
- The concept guitar icons `track_guitar` / `track_guitar_compact` are resolved from the already built MR UDS concept asset set; no runtime SVG file parsing.
- Existing user-editable text of track names and routing, controls and levels remain host-driven (the JSON labels are illustrative only).
- Responsive lower-region geometry is fitted inside each actual Strip height; saved design coordinates are exact at 60 / 110 / 166 px. The imported format does not describe all dynamic UI (gain dB readout, instrument shortcut and track color stripe continue to use their as-built presentation).
- A failed load never mutates project, Undo history, DSP or callback. No GUI hot reload yet: reload is a separate B2 milestone.

## Scope of checks

1. Automated `juce_track_header_layout` (no audio device/window): parse the committed file, check profile boundaries 38/83, 84/123, 124/358, validate computed gain bounds, reject invalid JSON.
2. CI Windows Release compile on PR; no full CTest. Existing targeted GUI tests are run only if selected by existing workflows.
3. User pilot acceptance: test 60/110/166; manually resize through 83/84/123/124, inspect clipping (including value labels and 100–200% DPI), click M/S/R/I/gain/pan/Input/Output, verify keyboard, name double-click, project save/open, recording and automation. Check missing/invalid JSON fallback. Hardware ASIO/real DPI NOT TESTED by CI.
4. Only after this slice is accepted: consider dev hot reload with last-known-good fallback, release asset packaging, more responsive roles and full MR UDS registry support (Issue #242 B2). Do not merge automatically.

## Deployment boundary

The GitHub Pages site remains a browser-only design tool. Exporting its JSON neither commits to GitHub nor updates a running Windows executable; an explicit JSON file and the `--track-layout` flag are required for this pilot. No changes were made to the separate `gh-pages` snapshot.
