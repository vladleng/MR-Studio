# Input routing / MRS 0.1j

Local Stage 2e implementation for #22; physical ASIO acceptance pending.
Code/builds stay local; GitHub receives issues/docs only.

Each audio track has M/S, R (Arm) and I (input Monitor) in its arrangement header,
above the input selector. Buses retain M/S. Global Arm/Monitor are hidden; global
Record captures all armed audio tracks. Arm is independent and session-only.
Monitor is independent, saved with the project, and uses shared commands/Undo.

Inputs: Default mono, Off, physical mono, adjacent physical stereo pair. The engine
opens the deduplicated union needed by armed/monitored tracks and remaps physical
indices to selected stream order. Default uses the first Audio settings/profile input.
Mono feeds both canonical sides; stereo feeds L/R (averaged for a mono output).
Track mixer, bus/send and hardware routes apply to monitoring; capture stays raw.

Input selection/Arm and stream reconfiguration require Pause/Stop. Monitor on an
already open input is live; enabling a missing input requires Pause/Stop. Disabling
Monitor is live and retains the stream, including while recording. Missing channels
are rejected before opening files or closing the device. Silent offline clock can
open/edit saved routes without validating unavailable inputs; it cannot capture.

Stereo input/playback selects separate horizontal L/R track meters. Mono tracks
show one horizontal meter; bus/mixer/Master use shared stereo peaks.

Core snapshot v7 adds stereo/monitor flags and reads v1-v6 (mono, Monitor off by
default). Archive/device profile formats are unchanged. Keep a project copy:
older builds cannot read newly saved v7 snapshots. Limits: 64 physical channels,
32 armed/captured tracks and shared source/clip/disk budgets. No nonadjacent stereo
pairs, resampling, punch/loop recording or automatic latency compensation.

See [recording](RECORDING.md) and [acceptance](MRS_STAGE_2E_CHECKLIST.md).

