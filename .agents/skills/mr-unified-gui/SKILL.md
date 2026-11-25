---
name: mr-unified-gui
description: "Use for designing, improving, redesigning or checking MR Studio, MR Live and MR-owned native plugin GUIs: JUCE screens, layout, controls, themes, icons/SVG, usability and visual validation. Not for DSP-only tasks."
---

# MR Unified GUI — flexible design, safe implementation

## Scope and design freedom

The current user request and assigned DSH task define **what may change**. Do not treat historical approvals, screenshots, UI tokens, exact dimensions or the current `ui::Theme` as permanent design restrictions. They describe the existing implementation or a useful reference, **not a prohibition on explicitly requested changes**.

Choose the approach from the intent of the request (no special mode command required):

- **Fix** — correct the stated defect; otherwise preserve the current visual design.
- **Improve** — change the affected layout, hierarchy, sizing, controls or presentation as needed to solve the UX problem; do not limit the result to cosmetic tweaks.
- **Redesign** — allow substantial changes to the requested screen/component, including layout, geometry, visual language and relevant shared tokens. Retain only the functional contracts and the boundaries of the requested scope. Do not silently redesign unrelated screens.

If the user asks for a visible redesign, the implementation must produce a **meaningful visible difference**, not merely new color constants or a refactor of unused UI code. A previously accepted design remains the default only until the user requests a change. MR UDS provides reusable assets and optional visual continuity, not a frozen mandatory palette, icon treatment or control geometry. For broad cross-product changes, establish the intended scope before changing shared assets/tokens.

## Read only what the task requires

1. Follow the global and project `AGENTS.md`, the assigned DSH task and current local checkout. Use `dsh-task-manager-workflow` **only when interacting with the task journal**. Before editing, check current branch, HEAD and working diff.
2. Locate the **active** JUCE component, layout code, LookAndFeel/theme and affected binding; verify that the edited component is used by the actual target. Do not mistake a design document, old issue, Gallery concept or unused control for the running UI.
3. Read **only relevant sections** of `docs/MR_UNIFIED_DESIGN_SYSTEM.md` when shared visual roles or assets matter. It is a reference for design choices, not a mandatory full-document read or a fixed target during redesign.
4. Use `mr-studio-ui-design-system` only when its detailed JUCE implementation guidance is necessary (geometry, focus, DPI, window behavior). Use `mr-studio-build-test` only for applicable build/test procedures. Do not automatically chain other Skills.
5. Open product-specific docs only for the component or behavior being changed: e.g. `docs/MR_RIG_GUI.md`, `docs/MR_REVERB.md`, `docs/MR_FADER_SYSTEM.md`, `docs/MR_SVG_RUNTIME.md`, or MR UI Kit documents for actual shared catalog/LookAndFeel/Gallery work. Do not open these by default.

## Functional constraints — always preserve unless explicitly in scope

- Keep real audio/MIDI/parameter bindings, units, range/default values, automation, gestures, Undo/Redo, persistence, routing, transport and existing project compatibility intact. A visual change must not silently change DSP, graph, PDC or audio-thread behavior.
- Respect JUCE component/editor lifetime, threading, focus, keyboard operation, pointer hit areas and accessibility. Do not perform UI work in the audio callback. Verify changes to window ownership, resizing and editor reopening when affected.
- Preserve meaningful state feedback (normal, hover, pressed, selected, disabled, focus, error, recording/cue as relevant); do not rely solely on color. Provide legibility and adequate hit targets. Live-performance controls deserve particular attention to clarity and accidental activation.
- Prefer existing shared components and a consistent design source **when they fit the new design**. It is permitted to modify/refactor them within the agreed scope; do not create a competing global Theme, duplicated host binding or parallel UI framework just to realize a new look.
- UI must represent genuine functionality and live data; no fake working meters, controls or plugin capabilities. Do not restyle vendor-owned VST3 editors. Respect third-party rights for artwork/branding.

## SVG and shared assets — when relevant

- The local MR UDS asset root is `assets/ui/mr-uds/`; inspect `manifest.json` and existing `svg/`, `tokens/` and `previews/` **only if assets are involved**. Keep repository-relative paths and manifest references consistent; do not flatten the SVG tree or move the transport icon library without a separate task.
- Reuse suitable existing assets; create or revise assets where a new design needs them. Icons should remain legible, scalable and original; SVGs need a valid `viewBox`, transparent background when appropriate, no embedded scripts/remote resources, and provenance/license notes. Exact sizes, line weights and frame treatment depend on the requested design, not a universal fixed prescription.
- Inspect generator output paths and overwritten files before running `build_kit.py` or concept-pack scripts. Do not regenerate the entire kit for one icon.
- Fader families (`MR Micro`, `MR Slide`, `MR Precision`) and `docs/MR_FADER_SYSTEM.md` are prior design references, **not immutable geometry**. When changing a fader, preserve gain/automation bindings, gestures, scale and meter semantics; use vector/Gallery previews if genuinely useful and available, not as an automatic gate for every adjustment.

## Execution and proportionate verification

1. **Identify intent and scope.** For an improvement/redesign, state briefly what should look or work differently. Inspect the current UI and relevant code before implementing; use a screenshot/reference when available, without making its absence a blocker for code work.
2. **Implement in the running path.** Confirm that the changed component and theme are actually instantiated and no other LookAndFeel, resource or cached/old executable overrides the change. Keep edits restricted to the agreed screen and any necessary shared dependencies.
3. **Check according to risk:**
   - **Cosmetic only** (color, tint, stroke/background with no geometry, hit-area, layout, bindings, behavior or build-system change): inspect diff and asset/source correctness. No mandatory build, screenshots, GUI smoke or DPI matrix. Build only when an updated executable is requested or technically needed.
   - **Layout or interaction**: build/test the affected local target and changed behavior; check meaningful states, keyboard/pointer/focus, sizing and relevant DPI cases, using available tools. Avoid automatic full CTest, cross-product rebuilds or packaging.
   - **Substantial redesign**: where GUI execution or screenshots are available, compare **before and after on the same target/build/scale** and assess whether the requested change is actually visible and usable. Run only the relevant functional regressions. Do not require a full DPI matrix, vector mockup or Gallery preview unless justified by scope or explicitly requested.
4. **Report accurately.** Distinguish code changed, build/test PASS, appearance visually verified, and user acceptance. If the actual GUI cannot be run or inspected, mark visual verification **NOT TESTED** and give concise manual acceptance steps; never claim that a code diff proves the new interface looks correct. Record significant results in the DSH task when that work is authorized.

Do not initiate unrelated redesigns, new tasks, Git push/merge or releases from this Skill. Authorization and task state remain governed by project `AGENTS.md` and DSH Task Manager.
