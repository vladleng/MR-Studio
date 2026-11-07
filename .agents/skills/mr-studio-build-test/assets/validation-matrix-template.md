# MR Studio validation matrix template

Adapt this matrix to the actual change.

| Area | Check | Required? | Result | Notes |
|---|---|---:|---|---|
| Configure | Canonical configure succeeds | Only when configure is required | | |
| Build | Shared/core target | If affected | | |
| Build | MR Studio app | If affected | | |
| Build | MR Live app | If affected | | |
| Tests | Unit tests | If present/affected | | |
| Tests | Integration tests | If present/affected | | |
| Runtime | App launches | Usually | | |
| Runtime | Changed feature smoke test | Yes for runtime work | | |
| Persistence | Save/reload | If state changed | | |
| Audio | Device opens | If audio/device changed | | |
| Audio | Playback | If engine changed | | |
| Audio | Recording | If recording changed | | |
| Hardware | ASIO device test | If required | | |
| Regression | Adjacent scenarios | As needed | | |

Allowed result values:
- PASS
- FAIL
- NOT RUN
- NOT APPLICABLE
