# 0.1h / MRS Stage 2c — local Windows acceptance

Stage 2c accepted locally in 0.1h fix3 on 2026-10-04: user confirmed no flicker
and everything works. Code 0911855dce7d179e03df33f05a120684e4637d37.
The historical pending notes below are superseded. #22 stays open for hardware
output routing/multi-output/device profiles; no automatic code publication.

Local configure/build, CTest 65/65 and expanded GUI smoke passed.
User verified 0.1h functionality and reported native button flicker. The local
0.1h fix1 addresses this; fix1 verification is pending. See MRS_0_1H_FIX1.md.
Run the local MoonRiverStudio.exe; no GitHub build.

1. Open/save several projects. Files → Open recent project lists them newest
   first, without duplicates; restart and open one from this menu.
2. In Arrange, use each left mini panel: drag the horizontal gain, inspect the
   horizontal level meter, drag the pan knob, double-click gain/pan to reset.
   Check the same gain/pan values in Mix and Undo/Redo.
3. Toggle Mix: it overlays the lower arrangement in the same main window.
   Resize the window; use uncovered arrangement lanes while the mixer is open.
   Drag gain/master vertically; stereo level bars are vertical. Wheel scrolls
   channels. Mix again hides the panel. Escape cancels a gesture.
4. Pause and create two returns with Sends → Create return and send, or add
   sends to existing buses. Each source can feed its main output and several
   returns. Confirm each send's destination, level and pre/post mode.
5. Play a WAV. A post-fader send follows track gain/pan; a pre-fader send remains
   when the track fader is lowered. Mute silences both. Return gain/mute/solo and
   nested output routing affect the return sum; inspect meters and master.
6. Change send levels during playback, Undo/Redo. Pause to add/remove a send or
   switch pre/post. Self/duplicate/cyclic choices are disabled/rejected. Attempt
   return A → B and B send → A; the invalid route must not alter the project.
7. Delete a return while paused: its sends disappear; Undo restores them.
   Save/reopen and verify every route, mode, level and input choice.
8. Connect ASIO. Choose different physical inputs on two audio tracks, arm each
   in turn and monitor the intended input. Switching an armed input while paused
   retains transport position. Off disables input; Default uses Audio settings.
9. Record a mono take through a muted/attenuated track and return. The recorded
   WAV remains raw. Record sequential takes on different selected inputs/tracks.

Later slices: hardware multi-output/device profiles and multi-input recording.
DSP/plugin inserts are a later stage; these returns route audio without effects.
Code push/PR/merge occurs only on a separate user request. #22 remains open.
