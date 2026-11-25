# MR Studio Unified Design System (MR UDS)

**Scope:** MR Studio, standalone MR Live and MR-owned native plugins. The visual direction approved on **2026-10-15** is a **reference and historical baseline**, not a permanently fixed palette, layout or theme. Actual implementation and acceptance must be checked in the local code and the assigned DSH Task Manager task; this document is not a status log.

## 1. Role and authority

- **Current user request and accepted DSH task** define the GUI scope. A *fix* preserves unaffected design; an *improvement* may change the affected layout and presentation; a *redesign* may change visual language, palette, typography, geometry and relevant shared components **within the agreed scope**. No unrelated or cross-application repaint by default.
- Follow the global and project `AGENTS.md` and [mr-unified-gui](../.agents/skills/mr-unified-gui/SKILL.md). This document provides **visual references, semantic conventions and asset contracts**, not a second execution workflow or mandatory reading list.
- [MR UI Kit](MR_UI_KIT.md) owns shared JUCE controls, theme/LookAndFeel integration, Gallery and parameter-binding architecture. Extend or refactor the **actual** implementation when justified; do not create a parallel global Theme or host binding system. Existing `ui::Theme` / [Controls.h](../apps/studio-juce/src/Controls.h) describe *as-built* behavior, not immutable styling.
- Product specs define functional behavior: [MR Rig GUI](MR_RIG_GUI.md), [MR Reverb](MR_REVERB.md), [Live UX](UI_UX_CONCEPT.md), and the relevant screen/module specification. Preserve their functional contracts unless the assigned task explicitly changes them.
- Use original assets. The historical Synth Anthology 4 reference is inspiration only, **not permission to copy** artwork, trademarks or layouts.

## 2. Visual reference, not design restrictions

The October 2026 reference used flat dark blue-graphite/navy surfaces, cool light outlines, restrained accents, compact instrument-panel controls, original line-art and minimal decoration. It favored readable units, small rotary controls/thin faders, lightweight feedback animations and consistent visual semantics across apps with **different workflows**.

These are **optional starting points**, not requirements for a requested redesign. Existing screens, historical approvals and prior mockups do not prohibit changing component geometry, framed/unframed buttons, color, type, density or information hierarchy. Use a coherent design appropriate to the current request rather than preserving the appearance by default.

Historical preference (2026-10-16): icon-only buttons without a permanent frame, with feedback on the glyph; text buttons may retain frames. **This can be changed by a new design decision.** Whatever the appearance, preserve clearly discernible hover/pressed/disabled/focus feedback, accessible names and effective hit areas.

MR Live prioritizes stage-distance readability, critical cue/record/error feedback and safe operation; it does **not** have to mirror the DAW layout or density. Native plugins may use differentiated layouts. Third-party VST3-owned editors are out of scope; MR host chrome and the generic parameter editor are MR-owned surfaces.

### Reference tokens

Original target values from **MR Plugin GUI SVG Kit 01** (2026-10-15), **not verified running-theme values**:

| Token | Reference | Meaning |
| --- | --- | --- |
| canvas | `#111720` | root background |
| window | `#171E28` | window |
| surface | `#222C3C` | panels |
| raised | `#2D394E` | selected/header |
| deep | `#192333` | embedded module |
| bright | `#DAE5F4` | primary text/outline |
| stroke | `#A9BFDC` | line-art |
| muted | `#7188AB` | secondary text |
| subtle | `#4B5F7B` | separators |
| accent | `#91BBEE` | selection/focus |
| green | `#7AB69E` | ready/on |
| red | `#E68F88` | error/record |
| amber | `#E2C070` | warning/cue |
| violet | `#AB92DD` | optional accent |

Historical implemented examples: tree `#212830`, card `#2D353E`, button `#2F3841`, Arrange `#262E37`. Confirm actual values in the relevant local code before editing. When changing shared colors, prefer **semantic roles through the existing theme**, avoid per-view hardcoded copies and assess the impact on other screens. New approved tokens may replace these references within the requested scope.

Earlier **illustrative** metrics: 4 logical px spacing base (4/8/12/16 gaps), 12–15 px large-panel radii, ~1.1–1.6 SVG illustration strokes at 320×200, ~1.5–1.8 icon strokes at 24×24, ~48 px concept plugin header. These are **not mandatory measurements** for new layouts. Ensure legibility and hit targets appropriate to the context, especially Live.

## 3. UI semantics and technical boundaries

- Represent relevant states accurately: normal, hover, pressed, keyboard focus, selected, enabled/on, bypassed, disabled, armed/recording, loading, ready, missing, warning and error. **Selected ≠ enabled ≠ processing; bypass ≠ mute/solo.** Do not depend on color alone for essential status.
- Keep real parameter values, units, ranges/defaults, gesture start/end, automation, routing, transport, Undo/Redo, presets, project persistence and editor lifecycle. A gesture should produce the intended host change and undoable operation, not a disconnected visual-only control.
- JUCE UI painting and state updates belong to the UI thread. No SVG rasterization, allocations, locks, I/O or GUI calls in the audio callback. Preserve focus, keyboard navigation, pointer interactions and accessibility. Resize/DPI behavior should remain usable for the affected interface.
- UI must reflect implemented features and real data: no fake functional controls, meters, processors or invented measured IR responses. Changing visual design does not authorize DSP, graph or project-format changes.

## 4. Shared assets and local MR UDS library

**Canonical local library:** `assets/ui/mr-uds/`. The existing `assets/ui/icons/transport/` is separate; inspect/reuse it when relevant, and do not move it automatically.

```text
assets/ui/mr-uds/
├── svg/
│   ├── controls/
│   ├── icons/
│   ├── rig/
│   └── reverb/
├── previews/
├── tokens/
├── build_kit.py
├── manifest.json
└── README.md
```

- Inspect `manifest.json` and relevant assets **only when the task involves SVG/icons/kit work**. Reuse suitable items; avoid near-duplicates. Preserve the `svg/` level and manifest-relative paths. Do not blindly flatten the tree or duplicate asset directories in Skills/docs.
- New SVGs: original vector content, valid `viewBox`, appropriate transparency, consistent catalog naming (currently `snake_case`), no scripts, `foreignObject`, remote dependencies, embedded proprietary assets or unauthorized fonts. Record provenance/license when registering an asset. Choose icon sizes, stroke weight and level of detail for the new design; 16/20/24/32 and 320×200 are historical examples, **not compulsory formats**.
- `build_kit.py` resolves paths relative to the kit root and generates into `svg/<category>/`. **Check its outputs and overwrite scope before running it**; don't regenerate the whole library for one icon. Update the manifest and generation source when an affected asset requires it.
- Kit `tokens/` is distinct from the historical [MR UDS swatch JSON](design/MR_UNIFIED_TOKENS.json); do not synchronize them implicitly. Gallery and previews are tools, not automatic approval gates for every icon or cosmetic tweak.

## 5. Verification proportionate to the change

Use [mr-unified-gui](../.agents/skills/mr-unified-gui/SKILL.md) and [mr-studio-build-test](../.agents/skills/mr-studio-build-test/SKILL.md) for **procedures**; do not duplicate their test matrices here.

| Scope | Evidence to collect |
| --- | --- |
| **Cosmetic-only** (no geometry, hit area, bindings, behavior or build-system changes) | Inspect diff and source/asset correctness. No mandatory build, screenshot, GUI smoke or DPI matrix; build if an executable is requested or technically necessary. |
| **Layout / interaction** | Targeted build and relevant tests; check affected sizing, focus, pointer/keyboard interactions, states, and relevant DPI cases where possible. |
| **Substantial redesign** | Verify the **active** component/build really changed. If GUI execution is available, compare the same screen before/after at comparable scale and check visible usability. Run risk-based functional regressions, not a mandatory full release matrix. |

Distinguish code changes, build/test **PASS**, visually verified appearance, **NOT TESTED** and **user acceptance**. Do not claim visual verification from source diffs alone. Hardware/physical DPI verification must be recorded separately if not performed. No automatic Gallery, multi-DPI matrix, full CTest, packaging or unrelated product rebuild for a narrow GUI change.

## 6. References and provenance

- [MR UI Kit](MR_UI_KIT.md), [MR UI Gallery](MR_UI_GALLERY.md), [MR SVG runtime](MR_SVG_RUNTIME.md) — shared implementation and rendering contracts. [MR UI Kit roadmap](MR_UI_KIT_ROADMAP.md) records **historical GitHub stages** (#189–#195), not current DSH task status.
- Module-specific UI/DSP behavior belongs to its own specification. The design reference alone does not imply a shipped feature, an accepted implementation or permission to migrate the whole application.
- **MR Plugin GUI SVG Kit 01** historically included 65 conceptual SVGs (23 controls, 12 Rig, 11 Reverb, 19 general icons) and two mockup screens. Availability in today's checkout is **unverified until checked locally**. These are artwork concepts, not JUCE controls with working bindings.

Current task tracking, approvals, Git operations and release authorization come from the **project AGENTS.md and DSH Task Manager**, not historical GitHub instructions in old design documents.
