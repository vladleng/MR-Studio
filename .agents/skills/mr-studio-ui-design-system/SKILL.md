---
name: mr-studio-ui-design-system
description: Design or modify MR Studio JUCE visual components, themes, controls, layouts, window behavior, keyboard focus, accessibility or HiDPI. Skip for engine-only, audio DSP, docs-only or unrelated build work.
---

# MR Studio UI Design System

Work within the **actual accepted UI** and the user's latest visual request. A design audit alone does not authorize implementation or a global redesign. Same MR Studio/Live family; apply Live-specific larger targets only if Live/shared UI is in scope.

## Shared catalog

For visual language and catalog choices, follow `../mr-unified-gui/SKILL.md`. The canonical `assets/ui/mr-uds/` catalog serves DAW and MR-owned native plugins; extend it consistently and keep previews current. A named catalog replacement applies to the requested screen and preserves functional bindings. Keep asset availability, runtime adoption and issue-recorded user acceptance distinct.

## Scope and visual decisions

1. Inspect changed components and their current theme/LookAndFeel; inspect actual screenshot or fresh preview if available. If only source exists, mark appearance *unverified* rather than asserting it looks right. For unfamiliar layout/ownership use [source map](references/project-map.md) selectively.
2. Preserve existing command/Undo/project/editor behavior. Reuse native JUCE controls, theme and geometry; no new parallel UI framework or global restyling for a local fix.
3. Neutral DAW chrome vs track/clip/record/warn/selection/focus/meter states must remain semantically distinct. Keep compact/readable and restrained in depth; approved visual decisions take precedence over generic defaults.
4. For changed controls inspect normal/hover/pressed/selected/disabled/**keyboard focus**, hit area, truncation, icon accessibility, tooltip, Cyrillic and logical scaling. Hit area can be larger than visible icon. Focus must not be conveyed only by selection color.
5. For a visual reference use [reference policy](references/reference-intake-policy.md); for shared tokens use [token rules](references/token-schema.md); for JUCE code changes use [JUCE rules](references/juce-ui-rules.md). **Do not load all three on an ordinary local color adjustment.**
6. Preserve audio/UI threading; graphical components never own RT processing or directly mutate audio graph outside accepted Commands. Do not restyle external VST editors or change native editor window ownership unless explicitly in scope.

## Cosmetic-only shortcut

For a single background/color/border, SVG tint or similarly isolated visual change, invoke `mr-unified-gui` **V1 fast path**. First confirm the diff has **no** changes to geometry, hit areas, layout, interactions, bindings, model/Undo/persistence, DSP, dependencies or build configuration. Check only diff and source/resource correctness (including SVG syntax/manifest where affected); the **user** performs visual acceptance. **No automatic build, tests, GUI smoke, screenshots, preview or DPI checks**, even when the cosmetic source is C++ or an embedded asset. Build only the affected GUI target **if an updated executable is requested**; otherwise explain that the visual change appears in the next build. Do not require a full matrix, full CTest, clean rebuild, other products or packaging. Escalate if the diff exceeds V1 or an actual regression requires it.

## Verification and delivery

- Use the affected scoped GitHub Actions Windows profile and proportional GUI/state checks (`mr-studio-build-test`) for behavioral/layout changes. For V1 use only the cosmetic shortcut above, with no automatic preview or build unless requested. For layout/control behavior, compare the same theme/size/build, keyboard/pointer/focus/resize and relevant software DPI; physical mixed-DPI transitions and ASIO listening are **NOT TESTED** unless performed by the user.
- Use [UI audit template](assets/ui-audit-template.md) or [component template](assets/component-redesign-template.md) only for substantial work; no mandatory giant report or new design documents for tiny fixes.
- Describe visual changes, affected components, actual PASS/FAIL/NOT TESTED checks and user acceptance status. Shared token decisions update current canonical docs, not a second competing design-system specification. For docs-only skill edits, verify diff/links only; no application rebuild.
