# Realtime validation matrix

Use in addition to `mr-studio-build-test`.

| Area | Check | Result | Notes |
|---|---|---|---|
| RT boundary | audio-thread paths identified | | |
| Locks | no blocking lock on RT path | | |
| Allocation | no runtime allocation on RT path | | |
| State transfer | no race / coherent snapshots | | |
| Lifetime | shutdown/restart ownership safe | | |
| Parameters | rapid automation/change test | | |
| Transport | start/stop/seek/loop stress | | |
| Device | reopen/reconnect if relevant | | |
| Buffer | small-buffer test if hardware allows | | |
| Sample rate | alternate rate test if relevant | | |
| Long run | sustained playback | | |
| Diagnostics | no RT logging/blocking | | |
| Dropouts | no new audible glitches/xruns | | |

Allowed values:
- PASS
- FAIL
- NOT RUN
- NOT APPLICABLE
