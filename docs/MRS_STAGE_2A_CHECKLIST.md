# 0.1f / MRS Stage 2a — Windows acceptance

Artifact: `MR-Studio-0.1f-ASIO-Windows`. Run `MoonRiverStudio.exe`.
Automated CI covers audio mixing, RT allocation checks, persistence and GUI smoke.
User accepted the local ASIO build on 2026-10-04. Local configure/build,
59/59 CTest and GUI smoke passed; no GitHub Actions used for that build.
PR #48 and included #47 merged. Sustained performance benchmark remains deferred.

1. Open an existing 0.1e project with two audio tracks, or import two WAVs.
   Connect the usual vendor ASIO driver and main output pair. Open Mix.
2. During playback drag track gain: sound and channel/master meters change,
   transport continues and ASIO remains connected. Double-click returns to 0 dB.
3. Drag mono pan fully L/R, then double-click to center. For stereo WAVs verify
   balance preserves the surviving side. One output should ignore pan.
4. Mute each track independently. Solo one, then both; mute a soloed track.
   Empty-track solo should silence other tracks. Clear Mute/Solo afterward.
5. Change master gain. Overload meters turn red before the output safety clamp.
   Lower gain restores normal output. Track meters remain independent of master.
6. Drag a fader, release and Undo/Redo during playback. One gesture is one edit.
   Drag then Escape: original value and audio return without a new Undo entry.
7. Stop, save, close/reopen. Track gain/pan/Mute/Solo and master gain restore.
   Delete a track and Undo: its mix returns with the track. Scroll a larger mixer.
8. Arm a track while stopped, select one ASIO input, enable Monitor. Gain/pan/mute
   affect monitoring. Record while changing mix; the WAV must retain raw input,
   with backing clips audible but absent from the captured file. Stop and replay.
9. Switch Arrange/Edit/Mix during playback; verify no transport/device restart.
   Recheck Pause/seek, long WAV playback and the existing recording workflow.

Full #22 acceptance (buses/sends/routing/profiles) is outside this slice.
