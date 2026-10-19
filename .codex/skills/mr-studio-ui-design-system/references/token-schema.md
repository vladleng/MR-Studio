# Theme tokens: roles, evidence and implementation

Use for establishing or changing shared styling, not for every local layout fix.
Reuse the actual theme API; names below describe roles rather than mandated C++ IDs.

## Record only useful tokens

| Field | Meaning |
|---|---|
| Role/name | What the value means, not just its hex code |
| Value and unit | ARGB/color, logical JUCE dimension, font height convention, ms |
| Status | LOCKED = explicit user-approved decision; PROVISIONAL = observed/inferred; EXPERIMENTAL = proposed trial |
| Evidence | User direction or source/build/screenshot; record date/commit when useful |
| Implementation owner | Existing constant, colour ID, Theme method or component |
| Consumers | Affected control types/screens; shared Studio/Live variants if relevant |
| Rationale | Why this role is distinct and any contrast/layout constraint |

Do not infer LOCKED from a successful build or broad functional acceptance. A user
can replace an earlier decision directly; do not ask for the same approval twice.

## Role families

- **Chrome:** canvas, panel, raised/input surfaces, subtle/default border.
- **Text:** primary, secondary, disabled and on-accent; check actual foreground and
  background combinations, including alpha compositing and hover/disabled variants.
- **Interaction:** accent, hover, pressed, selection and keyboard focus. Focus must
  remain visible over selection; selected is not synonymous with active playback.
- **Semantic:** recording/armed, connected/disconnected, warning/error, clipping.
  Keep distinct meanings stable and reinforce critical signals with labels/shapes.
- **Content:** user track/clip/section/chord colors; keep independent of chrome.
  Check label legibility across bright and dark content colors.
- **Geometry:** recurring gaps, control heights, hit regions, border width, radius,
  icon inset/stroke and minimum usable component dimensions.
- **Typography:** existing font asset/fallback, family, weight, size and explicit
  point-height vs logical-height convention; role, baseline and truncation behavior.
- **Motion:** only where used; duration/update source and reduced-motion behavior
  where supported. Avoid decorative animation competing with meters/playhead.

For a genuinely new area, a 4-unit spacing rhythm and a small typography/radius
family can be useful candidates. Derive the actual values from existing component
geometry and tested density. Do not automatically import a web/mobile scale into
the DAW; a thin border or local alignment offset need not become a global token.

## Adoption and migration

Map a proposed role to an existing owner first. Add a token when a distinct semantic
role or recurring value justifies it; equal numeric values can have different roles.
List shared consumers before changing a value. Implement the requested scope,
verify adjacent components, and keep temporary experiments from silently becoming
approved global defaults. Update the existing design record rather than maintaining
a second unconnected palette in the skill. Numeric contrast claims require an
actual measurement of the rendered color pair; screenshots alone are not proof.
