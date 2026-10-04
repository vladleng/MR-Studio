# MRS Stage 3b / 0.1l — local acceptance checklist

Accepted by the user on 2026-10-04: local 0.1l / 3b.
Source `95c719fcd0893652f92a93beb9b20cb538cd84b9`; branch `mrs/0.1l-vst3-eq-local`.

- [x] Timed child-process VST3 scan, metadata/cache/change reconciliation and failed-module rejection cache.
- [x] Mono/stereo effect loading, removal/reorder/bypass and native editor / generic parameters.
- [x] Live parameters, bounded queues, state save/reopen, prepared-instance latency reporting.
- [x] Scanner isolation and in-process runtime boundary documented before delivery.
- [x] Unified three-band Channel EQ plus HP/LP, interactive summed response, frequency/gain point drag, Q wheel and per-band enables during playback.
- [x] Relative handle-only gain/pan gestures, no rail jumps, Ctrl fine drag, cancellation and single Undo commit.
- [x] Core v9 reads v1–v8; plugin/native parameter persistence.
- [x] Local offline-dependency Windows x64 ASIO configure/build, 85/85 CTest, hidden GUI smoke and reviewed previews.
- [x] Installed Blue Cat Gain 3 Stereo: software processing/automation/state/editor check.
- [x] User acceptance of local 0.1l / Stage 3b, confirmed 2026-10-04.

1. Copy a project and open it with 0.1l. Start playback; add/reorder effects after Pause/Stop.
2. In a track Inserts add Channel EQ. Enable HP/LP as needed, drag three bell points
   for frequency/gain and filter points for cutoff. Wheel changes Q; Ctrl gives fine
   control. Try exact numeric values, band enables/bypass, Undo/Redo, save/reopen.
3. Adjust all EQ parameters during playback. Audio/transport must continue; audition
   independent stereo channels and verify monitoring effects while capture stays raw.
4. Click gain/pan rails away from handles: no jump. Drag handles/knobs, try Ctrl,
   cancellation and Undo on mini panels, mixer tracks/buses and Master.
5. Scan a VST3 folder. Add an appropriate mono/stereo effect; open its native editor
   and adjust parameters during playback, or use generic normalized parameters.
   Try two tracks, a bus and Master, bypass/remove after Pause/Stop, then save/reopen.
6. Tweak the plugin, Pause/Stop, add/reorder an adjacent insert and verify plugin state
   persists. Check the displayed prepared latency. Dynamic bus/latency mode changes
   currently require reload; PDC/full runtime isolation belong to 3e.
7. Confirm no fader/menu/audio-settings flicker regression and Space Play/Stop return.

Whole #23 remains open. 3c Cab IR follows acceptance; 3d/3e later. #16 matrix remains
deferred and is not implicitly passed. Code/builds local; GitHub issues/docs only.
