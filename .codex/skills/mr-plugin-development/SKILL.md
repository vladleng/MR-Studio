---
name: mr-plugin-development
description: Develop or fix MR Studio native plugin editors, plugin presets, signal controls, IR/source selection or persistent plugin-browser state. Do not invoke for general DAW editing, generic build work or visual-only changes unrelated to plugins.
---

# MR Native Plugin Development

This skill adds **plugin-specific constraints** to `AGENTS.md`. Start with the assigned Issue and actual local source. Current paths to verify (not blindly assume): `apps/studio-juce/src/Editors.cpp`, `CabBrowser.h`, `Settings.h`, `apps/studio-desktop/src/application.cpp`, `core/processing/src`. Do not apply Cab-specific DSP rules to unrelated plugins.

## Shared visual catalog

For native-editor visuals, follow `../mr-unified-gui/SKILL.md` and the shared `assets/ui/mr-uds/` catalog. DAW and MR-owned plugins share one visual language and reusable controls; add missing plugin elements to that library. User-selected catalog replacements preserve real parameter/host behavior. Catalog preparation does not authorize migration of all editors; use the assigned plugin/slice (#206 for native rollout).

## Ownership and state

Separate: (a) sonic parameters, bypass, active source → **project/preset model** with Undo; (b) collection root/mapping → **library preferences**; (c) browsing filters/search/facets/favorites → **per-insert UI preferences**; (d) loader, kernel and editor generations → **runtime**. Don't serialize runtime as a preset or silently edit the project when changing a filter.

## Editor window and preset loading

- Configure native editor **while hidden** (layout, title, resizable flags, limits). Apply owner, HWND hooks and Space bridge **after** final peer creation, then show once. Preserve native ownership, **not** global Always On Top.
- Reopen a compatible editor by raising the same window; changing a compatible native preset updates controls/status **in place** without closing/reopening or losing geometry. An external/incompatible VST editor must instead be safely detached before processor destruction. Never retain stale processor/editor pointers.
- Guard asynchronous menu/file chooser/editor callbacks by project/insert identity and generation. A failed load must not display false Ready or leave dangling callbacks.

## Browser view, source identity and imports

- Preserve **filter choices** separately from active audio: `All`/`Any` is an explicit choice, not “unset”. Restore per insert and after delayed async scan, avoiding resets to default length/model/role on editor reopen. Bound cache/write frequency and flush on close.
- Browser filtering must not *automatically load* a WAV unless the user's current UX requirement explicitly says so. Distinguish **requested/loading** vs **verified active**. Active highlight/Ready must match exact loaded file identity/path/digest plus loader lifetime, not a revision number alone.
- On missing/unsupported/corrupt WAV, display an explicit reason, keep valid previous audio if that is the verified loader behavior and never substitute a similar source silently.
- Imported WAV IR is linear. `Cabinet` vs `Full Combo` vs `Unknown` relies on verified source/manually assigned semantics, not manufacturer names. Full Combo WAV is **not** a nonlinear guitar amp model; no fake Bass/Treble/Gain knobs. Do not add a second cabinet/room by role inference. WAV assets remain external and untouched.

## Real parameter semantics and RT

- Inspect *actual* signal order, units, default, smoothing and wet/dry routing **per plugin**. In current MR Cab baseline: `IR → wet LP/HP cut → wet Gain/Polarity → Mix`. Dry is not affected by Cab wet Gain; the accepted baseline may evolve, so verify source on each relevant task.
- UI/library import/decode/prep/relink never runs in realtime. No audio callback allocations/free, blocking locks, I/O or UI; use bounded handoff/crossfade, generation/cancellation, retire old kernels outside RT. Stop/join owners before replacing the graph.
- Prefer reusing existing native effect processors/host commands. Don't introduce another audio engine or separate amp DSP solely to classify WAV sources.

## Regression checks (only when affected)

- First show/owner, close/reopen, no duplicate editor or unexpected hide/show on `Preset A → B → A`; controls, source status and geometry remain coherent.
- UI filters after close/reopen: explicit Any length, Full Combo/model variants, delayed scan, two independent inserts, preference file round-trip.
- New loader whose revision counter repeats; failed/missing WAV; Save/Open/Undo and source identity; browser-only edit **does not** mark audio project dirty or rebuild graph.
- Changed DSP: affected RT/hosting audio checks. Screenshot tests don't prove physical focus, ASIO, listening or CPU. Use synthetic fixtures, keep vendor media untouched. Docs/skill-only edits do not require rebuilding the app.

Use `mr-realtime-audio-safety` for RT changes, `mr-studio-ui-design-system` for nontrivial visual work and `mr-studio-build-test` at the correct validation stage. A skill never independently authorizes GitHub Actions, merge, stage advance or issue closure.
