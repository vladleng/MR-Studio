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
- No skeuomorphic/photoreal physical controls, faux glow, copied amp images/product logos, big gradients or redundant decorative meters.
- 4 logical-px rhythm; compact panels + real hit targets/focus and unit legibility. Larger high-priority controls in Live Mode.
- Device illustrations: original thin-line amplifiers/cabs/pedals/microphones for MR Rig; outline spaces/devices for MR Reverb, not screenshots or third-party branding.
- Target palette and proportions are in docs/MR_UNIFIED_DESIGN_SYSTEM.md; do not globally overwrite accepted current Theme colors without an approved migration slice.

## Canonical local asset location

- **MR UDS library root:** `assets/ui/mr-uds/`. Existing SVG folders are nested: `svg/controls/`, `svg/icons/`, `svg/rig/`, `svg/reverb/`; concepts: `previews/`. **Do not flatten `svg/`: `manifest.json` and `build_kit.py` require that level.**
- Keep **`tokens/` directory, `manifest.json`, `build_kit.py` and the asset-kit `README.md` at the root `assets/ui/mr-uds/`** (not in `.codex/skills/`, not in `docs/`). This keeps the generator's relative paths together. Verify actual generator assumptions before running it.
- First inspect `assets/ui/mr-uds/manifest.json` and matching `svg/` files, then `assets/ui/icons/transport/` for existing transport assets; reuse before drawing. Do not silently move/overwrite existing transport icons.
- Treat this as the **local canonical path**, not proof files are committed on GitHub. If missing, request the kit or verify local checkout; do not invent asset filenames or pretend to have previews.
- All new art should be added to the same library and its manifest, with original/license notes and size/state variants. Source code/builds/assets remain local by AGENTS policy.

## Procedure for one screen or SVG
1. **Inventory first**: show what already exists, what is accepted and the exact scope; locate existing similar components/icons.
2. **Define roles/states**: component meaning + normal, hover, pressed, focus, selected, enabled, disabled, error, loading/missing as appropriate. Selected is not a synonym for On.
3. **Reuse the shared JUCE component**, adapting through one audited semantic Theme / LookAndFeel. If #189 components are still pending, use existing approved UI controls or scoped prototype; never invent competing controls/host bindings.
4. **For SVG**: original vector with viewBox and transparent background, consistent line weight and palette, no image/font/link/script embedding. Icons 24x24 (compact 16/20 LOD when needed); equipment 320x200; stable snake_case asset id and provenance. Confirm existing kit is physically in local workspace before using it; earlier conversation ZIP is not in GitHub.
5. **Functional integrity**: real parameter metadata/units/default/range, one Undo action per gesture, keyboard/fine/reset, automation and persistence. Maintain native window owner/reopen/focus/Space behavior; do not paint in audio callback or alter DSP/graph/PDC for appearance.
6. **Compare and validate**: before/after screenshots, Gallery if ready, software 100/125/150/200%, min/nominal/max windows, legible text/contrast/focus, mouse/keyboard, zoom, editor close/reopen/presets and regressions. Actual hardware mixed-DPI and ASIO explicitly NOT TESTED unless run.
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
- [ ] responsive at 100/125/150/200 and relevant physical DPI status reported
- [ ] existing functional behavior and RT safety unchanged
- [ ] source/license for SVG/icon noted; original and redistributable
- [ ] local tests/screenshots & user signoff recorded

**GitHub: only docs/issues with [skip ci]. Local code/assets/build/test/package per AGENTS.md.**

## MR Fader visual selection (2026-10-16)

For DAW faders, read **`docs/MR_FADER_SYSTEM.md`** only when task concerns fader design / mixer / track headers. Approved direction: **MR Micro** (thin matte vertical Mixer/Bus), **MR Slide** (strict horizontal Track Header/Send), **MR Precision** (large/expanded, same family). Use existing accepted #192 `MRFader` and #194 bindings; do not rebuild three components. For the user visual gate, prepare BOTH a **vector geometry reference** and a **real JUCE Gallery in-context preview**, with state/DPI variations. Separate fader gain from measured signal meter. Rollout #205 P2/P3, do not start from the skill alone.
