# MRS Stage 2d / 0.1i — hardware routing and device profiles

Local implementation ready, 2026-10-04. User/hardware acceptance pending.
Accepted baseline: 0.1h fix3. Full #22 remains open.

- [x] Physical mono/stereo destinations on audio tracks, buses and Master.
- [x] Multiple hardware outputs on the shared engine, independent Main/Monitor/Click/Cue routing using named buses.
- [x] Project commands/Undo/Redo, delete-bus reconnection and save/reopen.
- [x] Core v6 reads v1–v5; desktop preferences v4 reads v1–v3.
- [x] Named device profiles: device, rate, buffer, outputs, mono monitor input, channel labels.
- [x] Missing/changed hardware validation and cache of open-device channel names.
- [x] Offline-dependency Windows ASIO Release configure/build, 69/69 CTest and expanded GUI smoke.
- [ ] User acceptance of physical multi-output routing and profiles.

## Check on the intended ASIO interface

1. Keep a copy of a 0.1h project. Run the local 0.1i EXE and open the copy.
2. Audio settings: select the interface, the project rate and a supported buffer.
   Enable outputs, e.g. 1,2,3,4; press Connect.
3. Mix: set Master Out to 1/2. Add/rename a bus Monitor or Cue; set Out to 3/4.
   Route a track into it, or use a send to feed it alongside the Main signal.
4. Verify actual output pairs. Moving Master must not change the independently
   routed Cue bus. Verify bus fader, pan, mute/solo, input monitoring and sends.
5. Pause at a known position. Change an Out, Undo/Redo and delete/restore a bus;
   verify the position and sound routing. Save/reopen and verify routes.
6. Try Connect with a routed output removed from Physical outputs. An explicit
   missing-channel error must appear and the previous connection must survive.
7. Enter a profile name and Save. Change fields, select the profile and Load;
   verify restored fields, then Connect. Restart and verify the profile persists.
   Delete a temporary profile. Test a disconnected device if practical.
8. Open the project offline with the interface disconnected. Routes must stay
   stored; the silent clock still allows editing. Connect only after resolving
   missing hardware. Recheck fader/menu/Audio settings for flicker.

This slice adds routing, not a click generator or a show/cue event engine.
Recording still uses one armed track/one mono input. Device profiles are local
preferences; project routing is shared persistent state. See HARDWARE_OUTPUTS.md.
Code/builds remain local. GitHub contains issues/docs only; no new code push,
PR/merge or Actions without a separate user request. Benchmark #16 is deferred.
