# Native inserts — 0.1l

Stage 3a / 0.1k was accepted. Stage 3b / 0.1l adds a combined graphical Channel EQ
and live native parameter editing; acceptance of 0.1l is pending.

## Channel EQ
One slot contains HP (low cut), three parametric bell bands and LP (high cut).
Each bell: 20–20000 Hz, gain −24..+24 dB, Q 0.1..10. HP/LP are second-order filters
with adjustable cutoff and Q. Filters start disabled; the three bells start flat.
Gray HP/LP points mean disabled: select one and click Band disabled to enable it.

The logarithmic curve shows the combined response at the current project rate.
Drag a bell point horizontally for frequency, vertically for gain. Drag HP/LP
horizontally for cutoff. Wheel over a point changes its Q; wheel in the plot changes
the selected band's Q. High-resolution wheel deltas are retained. Ctrl gives finer
drag/wheel control. Select a band to type exact values or enable/disable it.
Escape/capture loss cancels the drag. A drag commits one Undo entry on release.

Parameters, band enables and native bypass work during playback (also during raw
capture), without closing/stopping the device or replacing the graph. UI changes
enter the shared prepared graph via bounded complete parameter transactions;
a full mailbox retains the newest target for the next UI poll. EQ frequency/Q/gain
and wet transitions use 20 ms smoothing, per-channel filter state, finite sample
sanitization and denormal cleanup. The drawn response is the settled target curve.
Filters clamp their effective cutoff to 0.45 × sample rate. Native EQ/gain/filter
processors report zero algorithmic sample latency.

## Chains and compatibility
Track/bus inserts are pre-fader and precede pre/post sends; Master inserts follow
the shared legacy master graph and precede Master gain/physical routing. Explicit
direct hardware outputs bypass Master inserts. Monitoring is processed; WAV
recording stays raw. Up to eight slots/channel, 32 slots/project.
Add/remove/reorder and VST3 bypass require Pause/Stop. Native parameter Undo/Redo
works during playback. Core snapshot v9 preserves all five band states and VST3
state, reads v1–v8, and retains legacy separate HP/LP/one-band EQ effects unchanged.
Keep a project copy: older 0.1k cannot read a newly saved v9 project.

Mixer/track/Master gain and pan gestures begin on their visible handle/knob.
Rail clicks do not jump the value. Dragging is relative to the press position;
Ctrl reduces sensitivity. Cancel restores the audio preview; release saves one
command. Clicking a handle without moving adds no Undo command.

## Validation
Local Windows x64 ASIO configure/build; 85/85 CTest; hidden GUI smoke and exported
main/editor previews. EQ tests cover response, three-band/filter persistence,
stereo separation, bypass, low-rate/extreme settings, queue saturation and zero
host realtime allocations. Desktop tests cover live preview/commit/Undo/Redo with
unchanged device opens and advancing transport. GUI exercises actual point drag,
Q wheel, cancellation, handle-only faders/pan and existing flicker/DPI regressions.
Physical/user acceptance: MRS_STAGE_3B_CHECKLIST.md. #16 remains deferred.
