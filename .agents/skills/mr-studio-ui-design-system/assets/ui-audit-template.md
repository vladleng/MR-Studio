# MR UI audit

Use for a requested audit/global redesign; omit irrelevant sections for local work.

## Evidence and scope
Date, repository/build/commit, screens/source owners, image paths, window/panel size,
render scale and actual physical DPI checks. Separate observed and inferred findings.

## Findings
| Area/source | Evidence and problem | User impact | Priority | Proposed action |
|---|---|---|---|---|

Review relevant color roles/contrast, typography/Cyrillic/truncation, layout/density,
icons/hit regions, interaction/focus/disabled states, resize/DPI, accessibility and
meter/transport meaning. Include MR Live only when in scope.

## Shared decisions
| Role/component | Current owner | Proposed change | Status/evidence | Affected screens |
|---|---|---|---|---|

## Implementation order
Rank by user impact and shared dependencies. For broad redesign choose a representative
pilot; for a narrow audit give the focused next action. Preserve existing interaction
contracts and mark behavior changes separately when requested.

## Verification
What was actually inspected/tested, evidence paths, unresolved questions and physical
checks still unavailable. Do not equate a screenshot or successful build with complete
accessibility, mixed-DPI or real-device acceptance.
