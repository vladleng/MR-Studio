# Visual reference intake

A screenshot can show appearance and geometry, but it does not prove interaction,
hover/focus behavior, animation, font identity or exact design-token values. Separate
visible evidence from inferred behavior and the user's explicit request.

Classify the input: current MR build, candidate inspiration, explicitly canonical
reference, or rejected direction. A candidate does not automatically redefine the
global theme. If the user explicitly wants fidelity to a particular reference,
follow that request within scope instead of imposing a generic no-copy rule.

For the relevant region identify:
- the requested improvement and what is actually visible;
- hierarchy, grouping, proportions, density and alignment;
- typography/icon treatment and structural vs content/semantic color;
- which current MR component and theme roles would change;
- unshown states that need design or verification.

Use KEEP / ADAPT / REJECT when it helps a comparison, not as mandatory paperwork.
Explain material conflicts with readability or existing behavior. Preserve earlier
approved rules unless the user requests a replacement; an explicit replacement
request is already authorization, not a reason to seek another confirmation.

For multiple references, select useful traits by component/role. Do not combine
incompatible spacing, radii, fonts and selection conventions accidentally. Capture
only lasting decisions in project design documentation, with their source and
status; do not require a new document for every image. Inspect supplied local images
directly. Do not invent a missing reference or claim to have inspected an app from
an old screenshot. Simple JUCE controls should be designed in native code/paths;
AI raster mockups are optional concept artifacts, never proof of JUCE implementation.
