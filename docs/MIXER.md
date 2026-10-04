# 0.1f / MRS Stage 2a — track mixer and master

First slice of #22. Mix edits the same ProjectStore and AudioEngine as Arrange.
Includes portable project folders and concurrent seek protection from PR #47.
Track gain/pan/mute/solo and master gain use shared commands and Undo/Redo.
No second engine, device handle, project copy or transport is created.

## Signal path

WAV voices and the armed track's input monitor sum into audio-track channels;
track gain/pan/mute/solo -> main output sum -> existing processor graph ->
master gain -> output safety clamp. Recording taps raw input before mixing.
Without an armed track, input monitoring goes directly to the master path.
Arming/disarming requires stopped/paused playback and retains the device handle.

Center pan has unity gain on both sides to preserve 0.1e levels. Mono pan and
stereo balance attenuate the opposite side linearly. A single output ignores pan.
All soloed audio tracks are audible together; mute overrides solo, including a
muted solo track. Solo on an empty track still excludes other tracks.

The prepared graph maps stable track IDs to at most 128 fixed runtime channels.
Complete parameter snapshots cross a bounded SPSC queue at callback boundaries.
Changes ramp over 5 ms, including mute/solo and the post-processor master gain.
Queue saturation is retried by the control-thread poll; no callback allocation,
file I/O, locks, UI calls, ProjectStore access or graph rebuild occurs.
Track meters are post-fader; master meters are post-processor/post-master and
pre-clamp, exposing overloads. UI reads held stereo peaks and applies decay.

## Persistence and editing

Core snapshot v3 stores track mix and master gain; v1/v2 read with unity/center
defaults. Archive envelope version stays 1. Existing MIXR channels hydrate the
shared model when opening archives; saving mirrors shared channel values into
MIXR and includes master gain in PROJ. Unknown archive chunks remain preserved.
Older application versions reject the new core snapshot instead of discarding
mixer settings. Track deletion/Undo restores the channel state with its track.
Mixer Undo/Redo works during playback; structural Undo/Redo still requires pause.
Mixer gestures preview on the engine and commit one command on mouse release;
Escape/capture loss cancels a preview. Changes can be made during recording;
the captured WAV stays raw. Undo/Redo and saving remain disabled while recording.

## UI

Mix shows channel strips and a fixed master strip. In the original 0.1f UI, drag horizontal gain/pan
controls; double-click resets to 0 dB/center. Gain covers silence to +12 dB;
shared contracts accept gain 0..16. Mute and Solo toggle independently.
Mouse wheel scrolls channel strips. The two bars and dBFS readout show output
levels; red indicates overload. Files/ASIO/transport controls remain shared.

Buses and track/bus outputs are implemented by the subsequent 0.1g slice;
see BUSES.md for snapshot v4, routing and solo semantics. Sends/returns,
hardware output routing, device profiles and multi-input recording remain future
work. #22 remains open until its full scope is accepted.


## Current local 0.1h

Mix overlays the arrangement, with vertical gain/master faders and stereo meters.
Track mini panels expose horizontal gain/meters, a pan knob and a mono input menu.
Sends/returns, pre/post levels, snapshot v5 and recent projects are implemented;
see SENDS.md and MRS_STAGE_2C_CHECKLIST.md. User acceptance is pending.


## UI choice — 2026-10-04

User preferred the previous interface over the approximate reference skin.
Working branch and local development build returned to mrs/0.1m-upd1-fix1-local.
Use MR-Studio-0.1m-upd1-fix1-ASIO-Windows-local. The upd2 skin remains an
experimental branch/package, not the current UI direction. Full custom UI
architecture/design is a proposal only; no renderer migration or installation started.
