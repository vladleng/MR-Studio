# Component change specification

Use only sections relevant to the requested scope; a small fix can be one paragraph.

## Context and problem
Component/screen, source owner, current build/image, requested outcome and evidence.
Distinguish observed defects from inferred behavior.

## Design decision
Information priority, reference traits adopted, layout/typography/color decisions.
Describe shared impact and deliberate differences from adjacent controls.

## Mapping
| Role | Existing owner/value | Proposed change | Status/evidence | Consumers |
|---|---|---|---|---|

## Geometry and interaction
Bounds/minimum size, layout/truncation rules, icon vs hit region, applicable states
(normal/hover/pressed/selected/disabled/focus), relevant keyboard/accessibility and
preview/commit/cancel behavior. Record preserved command/Undo/plugin contracts.

## Implementation and verification
Files/components changed; same-state before/after evidence with scale/size/build.
Checks actually run and results; unverified physical/accessibility states; remaining
issues. Update a shared design record only for a lasting system-level decision.
