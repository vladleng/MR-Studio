---
name: mr-unified-gui
description: "Use for the approved MR Studio-wide GUI visual style, native JUCE plugin editors, DAW/Live screens, theme, controls, SVG/icon creation, UI Gallery previews, and UI visual regression. Do not use for DSP-only work."
---

# mr-unified-gui — MR Studio + all native plugins

## When to use
A task concerns new/redesigned MR Studio screens (Arrange, Mixer, Browser, Inspector, Live, host chrome), native plugin GUI (MR Rig, MR Reverb, MR Strip, Spatial, Saturator, Harma Waves), JUCE controls, icons/SVGs, theme, DPI, or visual acceptance.

This skill is **not execution permission**; use only the assigned issue and phase. Do not start a redesign just because a feature names a plugin.

## Minimal reading (do NOT load entire docs or all skills)
1. Assigned GitHub issue; root AGENTS.md; verify local git status, current branch and HEAD.
2. docs/MR_UNIFIED_DESIGN_SYSTEM.md — sections 1–5: **approved visual target**.
3. docs/MR_UI_KIT.md + docs/MR_UI_KIT_ROADMAP.md **only for shared components/LookAndFeel/Gallery** (#189, #190–195).
4. Read **one relevant product document**, e.g. docs/MR_RIG_GUI.md, docs/MR_REVERB.md, docs/UI_UX_CONCEPT.md, or exact DAW UI module document.
5. Search actual local code for current ui::Theme, Controls.h, editor lifetime and parameter-binding APIs; follow existing docs/UI_REFERENCE_UPD9.md and approved #145/#146 as the **as-built** baseline. Do not assume GitHub spec means code already migrated.
6. If a local mr-studio-ui-design-system skill exists, read it only when needed, reconcile overlap without creating a second Theme implementation. This skill owns **approved style**; MR UI Kit owns **implementation**.

## Design language
- Flat dark blue-graphite; neutral text; delicate cool blue-gray 1–1.6 SVG contour art; very restrained pale blue focus/selection. Green enabled, red recording/error, amber cue/warning; no color-only meaning.
- No skeuomorphic/photoreal physical controls, decorative neon glow, copied amp images/product logos, big gradients or redundant decorative meters. Restrained active-icon glow is allowed by the user's button presentation rule below.
- 4 logical-px rhythm; compact panels + real hit targets/focus and unit legibility. Larger high-priority controls in Live Mode.
- Device illustrations: original thin-line amplifiers/cabs/pedals/microphones for MR Rig; outline spaces/devices for MR Reverb, not screenshots or third-party branding.
- Target palette and proportions are in docs/MR_UNIFIED_DESIGN_SYSTEM.md; do not globally overwrite accepted current Theme colors without an approved migration slice.

**Icon and button presentation (user decision, 2026-10-16):** Prefer icons without frames. Icon-only buttons have no visible button rectangle, border or plate; activation highlights the glyph itself with a restrained glow. Text-labelled buttons retain their frames. Preserve clear hover/pressed/disabled and keyboard-focus feedback, accessible names and real hit areas without restoring a persistent icon-button box. This applies to DAW and MR-owned plugin UI in the assigned migration slice, not an automatic global repaint.

## Canonical local asset location

- **MR UDS library root:** `assets/ui/mr-uds/`. Existing SVG folders are nested: `svg/controls/`, `svg/icons/`, `svg/rig/`, `svg/reverb/`; concepts: `previews/`. **Do not flatten `svg/`: `manifest.json` and `build_kit.py` require that level.**
- Keep **`tokens/` directory, `manifest.json`, `build_kit.py` and the asset-kit `README.md` at the root `assets/ui/mr-uds/`** (not in `.codex/skills/`, not in `docs/`). This keeps the generator's relative paths together. Verify actual generator assumptions before running it.
- First inspect `assets/ui/mr-uds/manifest.json` and matching `svg/` files, then `assets/ui/icons/transport/` for existing transport assets; reuse before drawing. Do not silently move/overwrite existing transport icons.
- Treat this as the **local canonical path**, not proof files are committed on GitHub. If missing, request the kit or verify local checkout; do not invent asset filenames or pretend to have previews.
- All new art should be added to the same library and its manifest, with original/license notes and size/state variants. Source code/builds/assets remain local by AGENTS policy.

## Shared DAW and native-plugin catalog

- Develop one catalog for DAW, Live and MR-owned native plugins, with shared palette, contours, control geometry and states. Add plugin-specific controls/art to the same library; reuse existing assets before adding variants. Inspect `previews/concept_pack.html`, `catalogs/fader_control_track_utility_v1.json` and `tokens/concept_pack.json` under the library root for exact ids/paths.
- A user can select an element by name or screenshot and request replacement in a specific screen. Adapt visible size and layout while preserving actual parameter mapping, gestures, Undo, keyboard focus and accessibility. Catalog inclusion does not mean runtime adoption or acceptance of every screen.
- Keep previews, manifest and catalog entries consistent when changing assets. For the concept extension use `build_concept_pack.py`, `validate_concept_pack.py` and `render_concept_pack.cjs`; inspect their write scope first. Legacy `build_kit.py` overwrites base library files and must not rebuild the extension.
- Static meter/waveform examples are design references; runtime displays must reflect actual host data. Follow `docs/MR_SVG_RUNTIME.md` for current adoption boundaries. Catalog expansion and plugin-editor adoption are separate slices (#208 assets, #206 native editors, #205 DAW); a future direction does not start all three.
- Record dated acceptance and exact source/package scope in the relevant issue, not as a growing history in this skill. Preserve accepted visual changes in later work unless the user requests replacement.

## Fast path — cosmetic-only changes (V1)

For isolated changes to background, color, contour stroke width or icon tint **without affecting geometry, hit areas, layout, interaction, bindings, model/persistence or DSP**, use V1. Check the diff before classifying; shared theme tokens can affect several screens. User visual acceptance of V1 does **not** automatically require L4 / full CTest.

1. Read the relevant task, palette role and affected asset/source, rather than the full UI documentation.
2. Change the smallest existing Theme token or asset; avoid needlessly touching headers shared across many targets.
3. Check the diff and contrast. For a standalone SVG/resource not embedded in the compiled binary, validate and render a single relevant preview without CMake. For compiled changes, do an incremental build of the affected GUI target and a short visual smoke at a representative DPI.
4. Do not automatically run clean/reconfigure, build all targets, full CTest, every DPI/window-size/state combination, repeated editor/preset cycles, packaging or shortcut updates. Escalate only when change scope/risk, failure or explicit request justifies it.
5. Report PASS/FAIL/NOT TESTED accurately; keep comprehensive acceptance tests for substantive changes and release candidates.

## Procedure for one screen or SVG
1. **Inventory first**: show what already exists, what is accepted and the exact scope; locate existing similar components/icons.
2. **Define roles/states**: component meaning + normal, hover, pressed, focus, selected, enabled, disabled, error, loading/missing as appropriate. Selected is not a synonym for On.
3. **Reuse the shared JUCE component**, adapting through one audited semantic Theme / LookAndFeel. If #189 components are still pending, use existing approved UI controls or scoped prototype; never invent competing controls/host bindings.
4. **For SVG**: original vector with viewBox and transparent background, consistent line weight and palette, no image/font/link/script embedding. Icons 24x24 (compact 16/20 LOD when needed); equipment 320x200; stable snake_case asset id and provenance. Confirm existing kit is physically in local workspace before using it; earlier conversation ZIP is not in GitHub.
5. **Functional integrity**: real parameter metadata/units/default/range, one Undo action per gesture, keyboard/fine/reset, automation and persistence. Maintain native window owner/reopen/focus/Space behavior; do not paint in audio callback or alter DSP/graph/PDC for appearance.
6. **Compare and validate proportionally**: for cosmetic-only V1 use the fast path above. For a new/changed geometry, significant component/screen redesign or explicitly agreed milestone, check before/after screenshots, Gallery if ready, software 100/125/150/200%, min/nominal/max windows, legible text/contrast/focus, mouse/keyboard, zoom, editor close/reopen/presets and affected regressions. Actual hardware mixed-DPI and ASIO explicitly NOT TESTED unless run.
7. **Report**: exact implemented/simulated parts, test PASS/FAIL/NOT TESTED, source/asset rights, visual differences and user acceptance pending in assigned issue. Stop before next slice.

## Product-specific cautions
- **MR Rig**: PRE FX → Rig Core → POST FX; one selected module inspector; don't draw non-existent dynamic-amp controls for static WAV, or suggest NAM sounds before NAM backend exists. #197/#200 control the product implementation.
- **MR Reverb**: #128 is accepted/closed; GUI refresh is separate UI-only work and must preserve actual Pre-delay/Tone/Mix and IR browsing. No new effect/processor in a GUI task.
- **Main DAW**: no regression to Arrange, Mixer, routing, clip editing, browser selection or project save/Undo; migration incrementally.
- **Live Mode**: same style but larger clarity, cue priority and performance safety. Preserve moving Chord Track.
- **External VST3 editors**: no restyling vendor-owned UI. Only MR host chrome and MR-owned generic controls.

## Completion checklist
- [ ] follows approved dark blue-graphite / fine contour style
- [ ] shares design roles/components with MR UI Kit; no parallel Theme
- [ ] all required state/keyboard/accessibility/units visible
- [ ] no fake controls or unverified DSP meters
- [ ] 100/125/150/200 matrix for new geometry / substantial layouts (N/A for cosmetic V1); actual physical DPI recorded as NOT TESTED unless measured
- [ ] existing functional behavior and RT safety unchanged
- [ ] source/license for SVG/icon noted; original and redistributable
- [ ] checks proportional to V1/component/release gate recorded; user signoff pending until explicitly given

**GitHub: only docs/issues with [skip ci]. Local code/assets/build/test/package per AGENTS.md.**

## MR Fader visual selection (2026-10-16)

For DAW faders, read **`docs/MR_FADER_SYSTEM.md`** only when task concerns fader design / mixer / track headers. Approved direction: **MR Micro** (thin matte vertical Mixer/Bus), **MR Slide** (strict horizontal Track Header/Send), **MR Precision** (large/expanded, same family). Use existing accepted #192 `MRFader` and #194 bindings; do not rebuild three components. **Only when choosing or changing fader geometry** (track, thumb, travel, orientation, scale, hit area or new family variant), the user visual gate requires BOTH a **vector geometry reference** and a **real JUCE Gallery in-context preview** with state/DPI variations. A change limited to color, tint or contour stroke width that preserves the existing geometry, hit areas, behavior and readability is **V1 cosmetic**: verify the diff and one relevant rendered preview; do not require a new geometry reference, Gallery matrix or L4 tests. If a stroke change alters effective bounds or layout, escalate to the geometry gate. Separate fader gain from measured signal meter. Rollout #205 P2/P3, do not start from the skill alone.
