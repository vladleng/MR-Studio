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

For a single background/color/border or similarly isolated paint change, invoke `mr-unified-gui` **V1 fast path**. Confirm the diff is visual-only; use existing configured build tree and a single incremental affected GUI target for changed C++/embedded assets, with diff/source correctness and user visual acceptance; no automatic smoke/preview/DPI. Runtime-loaded uncompiled SVG/JSON needs asset validation/preview, not compilation. **Do not require** a full DPI/state/window-size matrix, full CTest, clean rebuild, all Studio/Live targets or packaging for this task. Escalate only when affected behavior/ownership or an actual regression justifies it.

## Verification and delivery

- Proportional affected local build/GUI smoke (`mr-studio-build-test` when relevant); for V1 use the cosmetic shortcut above. For behavioral/layout changes compare same theme/size/source build, keyboard/pointer/focus/resize and relevant software 100% plus non-100% DPI. Physical mixed-DPI transitions and ASIO listening are **NOT TESTED** unless performed.
- Use [UI audit template](assets/ui-audit-template.md) or [component template](assets/component-redesign-template.md) only for substantial work; no mandatory giant report or new design documents for tiny fixes.
- Describe visual changes, affected components, verification and known limits. Shared token decisions update current canonical docs, not a second competing design-system specification. For docs-only skill edits, no application rebuild.
