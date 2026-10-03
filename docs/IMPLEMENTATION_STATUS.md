# MR Studio — implementation status

Updated: 2026-10-03. Active stage: #16.

## SHARED Stage 0 — accepted

PR #36 merged into main, issue #15 closed. All 7 contract suites passed on
Windows/Linux Debug/Release. User ran the Windows checker and supplied all
four PASS lines on 2026-10-03; proceeding to the next stage confirmed acceptance.
Parent #13 updated.

API: core/include/mrs/core.hpp. Documentation: CORE_CONTRACTS.md.
No separate Live backend was introduced.

## SHARED Stage 1 — hardware prototype

Branch: shared/stage-1-audio-asio. PR #37. Parent #13. Scope issue #16.

Delivered implementation:
- MRS-owned renderer and render/routing/device contracts;
- fixed SPSC control queue, coherent state mailbox and shared ITransport adapter;
- low-latency monitor path independent of transport playback state;
- prepared multivoice/stem mixing, trim/ranges/loops;
- WAV PCM16/24/32/float32 preload with bounds and memory budgets;
- ASIO-only PortAudio adapter: driver enumeration, vendor channel names,
  native buffer rules, channel selectors, vendor panel and stream lifecycle;
- explicit error states, callback-stall watchdog and manual reopen boundary;
- callback/xrun/latency metrics and JSON hardware reports;
- seven audio suites, offline acceptance and Windows native ASIO compile CI;
- guided Windows checker, cmd launcher, hardware checklist.

Portable Linux tests passed in Debug/Release on the first build. The first
Windows build exposed intentional SPSC alignment warning /WX; the first ASIO
configure exposed a scoped imported-target check. Both are corrected in the PR.
The current PR checks, not older runs, are the validation authority.

Hardware acceptance, compatibility evidence, Studio Pro comparison and the
performance gate remain pending. No hardware result may be inferred from CI.

## Handoff

1. Read START_HERE, AUDIO_ENGINE, AUDIO_CORE and issue #16 / PR #37.
2. Run the Windows hardware checklist (SHARED_STAGE_1_CHECKLIST.md).
3. Collect interface model/driver version and JSON reports at supported
   48kHz/128 and 64 native buffers; compare the same simple load in Studio Pro.
4. Record measured observations separately from offline contract coverage.
5. Only mark #16 complete when its physical-device and performance-gate
   requirements are satisfied. Fix regressions in the one shared engine.
6. #20 application shell is still open. #17 owns richer musical timeline and
   realtime-safe rebinding coordination. #28 Live UI waits for its required
   musical interfaces; no duplicate engine.

## Remaining boundaries

No GUI, plugins, MIDI notes or Chord/Arranger editor. Initial Timeline is
immutable while the stream runs. Graph preparation is quiescent only.
Full-file preload is implemented; production disk read-ahead is not.
No seamless hot reconnect, vendor crash isolation, hot graph replacement,
final project package, device profiles, autosave or full-show readiness.
MRS 0.1 is not complete until #16 performance gate and #20 foundation.
