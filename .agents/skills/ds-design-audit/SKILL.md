---
name: ds-design-audit
description: "Reviews an existing UI with scores based on evidence, then fixes the highest-impact problems. Use when a page or app screen looks generic, cluttered, or unfinished, or before you ship UI work. Run as /ds-design-audit."
---

Score the requested UI from observable evidence. An audit-only request ends with findings and recommendations; apply fixes only when implementation is requested. For MR Studio follow `mr-unified-gui` and `mr-studio-ui-design-system`: use MR UDS and actual JUCE sizes/states, not the web typography, 44px targets or browser widths below. Respect the cosmetic V1 exception.

## Steps

1. Identify the target file or URL and render it. If it does not render, report this and stop. Do not audit from the source code alone.
2. Take screenshots of the first viewport and the full page at about 1440px and 390px wide. Also capture the hover, focus, empty, loading, and error states where you can.
3. Read the target's code: tokens, fonts, colors, focus styles, media queries, and copy.
4. Give each area a score from 1 to 5. Give one line of evidence for each score: something visible or a line of code.
   - Hierarchy: the main message and action are clear within 3 seconds, and there is one focal point.
   - Typography: the fonts are paired on purpose, the scale is consistent, and the line length and leading are correct.
   - Color: there is one dominant color and one accent, the colors are tokens and not scattered hex values, and there are no decorative gradients.
   - Spacing: the spacing follows a scale, the rhythm is consistent, and the mobile alignment is correct.
   - States: hover, focus, empty, loading, error, and long-content states all exist.
   - Accessibility: the contrast is 4.5:1 or higher, the focus rings are visible, the fields have labels, the touch targets are 44px, and reduced motion is respected.
   - Copy: the copy is specific to the product, with no stock phrases and no placeholder data.
5. Record each banned default that you see: purple-blue gradients, a default font used for display, emoji used as icons, a centered hero with three identical cards, glassmorphism everywhere, gray text on gray, and generic copy.
6. Rank the fixes by their impact on users. Put broken accessibility and an unclear main action first. Put decoration last. For each fix, give the file, the change, and the score that it raises.
7. For an audit-only request, report the ranked findings and stop. If implementation is authorized, apply only the requested fixes. Fix the cause in the tokens or in the shared components, not in each instance. Keep the identity, content, and behavior of the product.
8. Take new screenshots at the same widths and in the same states as in step 2. Score each area again and show the before and after scores side by side.
9. If an area still has a score of 2 or lower, or a banned default remains, repeat steps 6 to 8 once. Do not do more than two passes.

## Stop when

- The audit-only report is delivered; or, for authorized fixes, two passes are complete or the agreed checks pass.
- The report shows the before and after scores, the evidence, the fixes that you applied, and the fixes that you did not apply, each with a reason.

## Rules

- Do not give a score without evidence. If you could not see an area, mark it "not checked" and give the reason.
- Do not change factual copy, prices, claims, or feature behavior without asking.
- If the brief requires a style that this skill would flag, it is not a fault.
- If the UI needs a full rebuild, recommend /ds-design. Do not rebuild it in this skill.
