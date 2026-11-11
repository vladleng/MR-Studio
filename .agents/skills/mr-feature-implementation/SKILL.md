---
name: mr-feature-implementation
description: Implement end-to-end MR Studio product features affecting commands, model, UI, Undo, persistence or shared Studio/Live behavior. Not for docs-only, isolated cosmetic UI, pure realtime review, or build-only fixes.
---

# MR Feature Implementation

Use for a **behavioral feature**, not every code edit. Start from the *assigned GitHub Issue* and read only applicable spec sections and source code. Confirm scope and acceptance criteria; do not start the next stage automatically. Local code can be ahead of GitHub, so inspect the actual checkout.

## Minimal workflow

1. Identify user-visible behavior, inputs, valid/error states, affected Studio/Live scope, existing similar implementation and acceptance evidence. Use [feature specification template](assets/feature-spec-template.md) only for a nontrivial feature.
2. Classify each layer as **affected / not affected / deferred / N/A**: commands, model, engine, UI, Undo, persistence, compatibility, async work, RT, tests. See [layer map](references/layer-impact-map.md) only if unclear.
3. Name the authoritative owner of each piece of state. **Project model → command → engine → notification → UI**; UI is not the sole owner of durable state. Separate UI-only preferences, project parameters and runtime caches.
4. Prefer the smallest **vertical implementation** reusing existing patterns. Never build a decorative control with missing behavior, bypass shared commands, create a second engine or introduce a schema just for convenience.
5. Define user-action transactions and Undo/Redo; decide what persists. Check older project defaults, missing files, ID stability and explicit migrations. When this is complex read [persistence reference](references/persistence-and-compatibility.md).
6. Put scanning, import, media I/O and expensive preparations off UI/audio callbacks. RT changes require `mr-realtime-audio-safety`; native plugin editor/state changes benefit from `mr-plugin-development`; visual work from `mr-studio-ui-design-system`.
7. Add affected unit, command/Undo, persistence round-trip, engine and/or GUI regression tests; fix causes, not superficial symptoms. Use [test matrix](assets/feature-test-matrix.md) if multiple layers are affected. Use `mr-studio-build-test` when compiling/testing.
8. Compare result to the assigned Issue. Document *only* new architecture or behavior; update that Issue with actual local commit, tests, limitations and acceptance state. A user-approved stage is not an approval of unrelated parent work.

## Guardrails

- Maintain one shared Core for Studio/Live where appropriate; don't fork equivalent model/engine logic for UI differences.
- A long-running operation is cancellable/lifetime-safe; avoid synchronous global refresh and recursive listener loops. Never mutate a currently playing audio graph without the project's safe transition mechanism.
- Avoid destructive project/media moves, absolute machine-dependent references or silently substituting missing assets.
- **Do not claim READY without the required checks.** Report READY / READY WITH MANUAL CHECK / INCOMPLETE / BLOCKED / NOT VALIDATED as appropriate, with exact skipped/manual tests. [Acceptance checklist](assets/feature-acceptance-checklist.md) is optional when closing a major stage.
- Follow root `AGENTS.md`: source commits/push in task branches and configured CI are part of the cloud workflow; merge requires separate acceptance.

Details are loaded on demand: [vertical example](references/vertical-feature-example.md), [layer map](references/layer-impact-map.md), [compatibility](references/persistence-and-compatibility.md). Do **not** open all of them by default.
