# Layer impact map

Classify every layer before implementation.

| Layer | Questions |
|---|---|
| UI | Does the user see/control it? New control, menu, context action, inspector, editor? |
| Model/state | What is the source of truth? New property/entity/state? |
| Domain/engine | What behavior changes independent of UI? |
| Commands/actions | How is the user intent represented? |
| Undo/redo | Should the action be reversible? |
| Persistence | Should it survive save/load/restart? |
| Project files | Does it create/reference/copy media or other files? |
| Realtime/audio | Can it affect audio callback, timing, routing, plugin processing? |
| Notifications | Who must observe the state change? |
| Studio/Live | Shared or product-specific? |
| Tests | What behavior needs automated coverage? |
| Docs | Does architecture/project format/user workflow need updating? |

Allowed classifications:
- AFFECTED
- NOT AFFECTED
- DEFERRED
- NOT APPLICABLE

Do not leave a layer unclassified.
