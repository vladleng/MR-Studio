---
name: mr-studio-ui-design-system
description: Detailed JUCE GUI implementation guidance for MR Studio, MR Live and native plugins: components, LookAndFeel, layout, focus, accessibility, HiDPI and window behavior. Use only when these details matter; not for DSP-only or docs-only work.
---

# MR Studio UI — JUCE implementation

## Scope and design decisions

The current user request and DSH task define the scope. This Skill provides **technical JUCE guidance**, not a second mandatory visual style. `mr-unified-gui` governs the general GUI approach; do not automatically reload it or other Skills if the applicable instructions are already known.

- **Fix:** preserve the appearance outside the defect.
- **Improve:** change affected controls, layout or hierarchy when required by the UX goal.
- **Redesign:** change requested geometry, typography, theme roles and visual composition as needed. Previous approval and existing `ui::Theme` are a baseline, not a veto on explicitly requested design changes.

Stay within the requested screen/component and necessary shared dependencies; a local redesign is not authorization for a product-wide migration. Studio and Live may share controls, but Live-specific hit targets and usage constraints apply only when relevant.

## Implement in the active JUCE path

1. Locate the **instantiated** component, actual LookAndFeel/theme, bounds/layout owner and relevant binding. Inspect the current GUI or screenshots when available. A source change alone does not prove a visible result.
2. Prefer existing JUCE controls, theme APIs and reusable components **when they suit the requested design**. It is permitted to update them within scope; do not introduce a parallel UI framework or duplicate audio/parameter bindings. If changing shared tokens, identify affected consumers first.
3. Keep painting, hit testing, gestures, keyboard focus and accessibility consistent with the **new** geometry. Inspect normal/hover/pressed/selected/disabled/focus states, clipping, long Cyrillic labels and logical scaling as relevant. Focus and critical transport/record/error states must not rely on color alone.
4. Preserve Commands/Model, Undo/Redo, project persistence, plugin-editor lifecycle, automation, parameter mapping, audio/UI threading and RT safety. Never restyle vendor-owned VST editors or change their ownership as an incidental styling side effect.
5. Open only the relevant reference: [JUCE rules](references/juce-ui-rules.md) for implementation/lifetime, [token rules](references/token-schema.md) when creating/changing shared theme roles, [reference policy](references/reference-intake-policy.md) when working from screenshots, or [project map](references/project-map.md) when ownership is unclear. **Do not preload all references.** Use the MR UDS catalog only when shared assets/components are actually involved.

## Checks and delivery

- **Cosmetic-only:** if diff does not affect layout, hit areas, behaviors, bindings, model, DSP or build configuration, follow the project AGENTS cosmetic exception: verify diff and resource correctness; no mandatory build, previews, screenshots or DPI tests. User acceptance establishes visual quality.
- **Layout/behavior:** run targeted local build/checks as described in `mr-studio-build-test` when appropriate. Examine relevant resize, pointer, keyboard, focus and DPI behavior; avoid broad test matrices without risk justification.
- **Substantial redesign:** where the GUI is accessible, compare before/after on comparable build, scale and state, confirming the requested **visible** change. If not accessible, mark visual verification **NOT TESTED**, and give specific manual acceptance steps. Do not require mockups, Gallery assets or a new design document by default.

Report code change, checks actually performed and acceptance separately. Update canonical design documentation only for durable shared contract changes, not every local visual experiment. UI templates in `assets/` are optional for substantial work.
