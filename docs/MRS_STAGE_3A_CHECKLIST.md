# MRS Stage 3a / 0.1k — native inserts

Local 0.1k accepted by the user, 2026-10-04. Stage 3a completed.
Accepted baseline 0.1k. Stage #23 stays open; see MRS_STAGE_3_PLAN.md.

- [x] Ordered chains on audio tracks/buses/Master through shared processor graph.
- [x] Gain, high-pass, low-pass and one-band parametric EQ; parameter editor.
- [x] Add/remove/reorder/bypass; common Undo/Redo and persistence.
- [x] Snapshot v8 reads v1-v7; stable IDs, limits and validation.
- [x] Monitoring processed, raw capture unaffected; direct output bypasses Master.
- [x] Plain wheel: 32 logical pixels/notch, fractional deltas and partial rows.
- [x] Space: Play/Stop and return to start, ignore held-key repeats.
- [x] Local offline-dependency ASIO configure/build, 77/77 CTest, expanded GUI smoke.
- [x] Main and insert editor visual previews reviewed.
- [x] User acceptance of local 0.1k / Stage 3a: «Проверил, вроде все работает, закрывай под-этап.»

1. Copy a project, open 0.1k and connect your interface at the project rate.
2. Mix -> track Inserts. Add Gain, High-pass, Low-pass and EQ. Verify audible
   parameter changes after Pause/Stop, independent L/R and correct bypass.
3. Reorder/remove, Undo/Redo; edit another track, a bus and Master. Verify chains
   are independent. Direct Cue hardware output must bypass Master inserts.
4. Save/reopen and verify chain order, parameters and bypass. Saved format is v8.
5. Record a monitored track with effects: monitoring is processed, recorded take
   stays raw. Pause/Stop then edit; device/position remain intact.
6. Scroll vertically: small sub-row increments with matching headers, clip hits,
   waveform and bounds. Test all existing Ctrl/Shift wheel modifiers.
7. Start from a nonzero position. Space starts; next Space stops and returns.
   Held Space must not toggle repeatedly. Pause button retains current position.
8. Confirm fader/menu/settings controls still do not flicker.

VST3/IR/model hosting and latency compensation belong to later slices; #16 remains
deferred. Code/builds local; GitHub issues/docs only.
