# Feature Test Matrix

| Area | Check | Result | Notes |
|---|---|---|---|
| Model | state changes correctly | | |
| Command | action reaches correct target | | |
| Undo | undo restores previous state | | |
| Redo | redo restores new state | | |
| Persistence | save/load round-trip | | |
| Defaults | older/missing field safe | | |
| Engine | behavior matches model state | | |
| UI | control reflects true state | | |
| UI | state updates from external/model change | | |
| Studio/Live | shared behavior consistent | | |
| Realtime | RT review if applicable | | |
| Regression | adjacent behavior preserved | | |
| Build | required targets compile | | |
| Tests | automated suite passes | | |
| Runtime | smoke-check passes | | |

Allowed:
- PASS
- FAIL
- NOT RUN
- NOT APPLICABLE
