# #95 — send strips in the mixer insert area

Started locally 2026-10-09 from accepted 0.2g upd9/source73fd5f9,
branch `mrs/0.2g-fix1-editor-space-local`, initial tree clean (7056dd9).
User refinement takes precedence over the original issue decomposition:
send destination names on buttons, a top-edge level strip without a fader handle,
right click destination menu, sends inside the Inserts area beneath plugins and
above the fixed mixer faders. Candidate version: **0.2g upd10**.

## Contract and layers

UI AFFECTED: named send rows, top-edge gesture/level display, right-click menu,
bottom send viewport under the insert viewport, add-send affordance. Existing
theme/fonts, mixer width/fader length and accepted unrestricted mixer-height
behavior preserved. The minimum dock reserves one send row on tracks (28 additional
logical pixels); the default expanded content includes inserts, sends and Add Send. Separate scrolling handles overflow; Master has no sends.
Application/commands AFFECTED: transient send preview through the existing bounded
MixerUpdate queue; final SetTrackSends gain edit remains one shared Undo command.
Model/routing AFFECTED through existing source/send destination/gain/pre_fader
fields; no new persisted state. Serialization/compatibility code NOT AFFECTED:
schema15/preferencesv7/cachev1 retained, Save/Open through existing fields.
Engine/audio AFFECTED RT-ADJACENT only: message-thread preview construction,
bounded queue and existing RT send-gain application. Callback code unchanged.
Notifications AFFECTED: committed revision refresh, preserved widgets during gain
gestures; IDs and project-store identity protect stale gesture/menu callbacks.
Files NOT AFFECTED. Shared model/Application/engine, JUCE mixer entry point.
Tests/docs AFFECTED. Stage5 NOT IN SCOPE.

Gesture: left click/drag on upper edge sets level, Ctrl fine drag, keyboard arrows
adjust, Home/End select silence/max, double click resets unity. Escape/focus loss
cancels and restores actual model level. Preview never changes project/history;
timer retries full queue while gesture lives, final mouse-up commits once.
Range spans existing gain0..16 using a dB scale with an explicit silent endpoint;
tooltip/accessibility expose actual level, unity marker identifies 0dB.
Right click on a row chooses/replaces its bus (preserving gain/pre-fader), or
removes/toggles pre/post. Add Send chooses a new destination/new return.
Duplicate/self/cyclic routes are disabled/rejected using existing model validation.
Structural routing changes retain Pause/Stop guard and detach editors before
rebuild; gain changes work during playback. Async choices resolve current data
by source/destination ID, never write an old captured sends vector.

## Validation and delivery

Local source **15afe65**, branch `mrs/0.2g-fix1-editor-space-local`.
Cached configure / Windows x64 Release PASS. Final full CTest **118/118 PASS
(52.44s)**, including desktop sends/mixer/buses/workspaces, core sends, audio
send DSP/mix queues/parallel/ahead/RT, persistence, MIDI/VST3, J2/J3/diagnostics.
Application send regressions cover finite0..16 range, unknown source/bus, queue
saturation, cancellation without revision changes, one committed gain/Undo/Redo
while playing and untouched other sends. GUI regression observes real return
meters: selected bus silent after the existing5ms ramp, other bus remains audible,
cancel restores actual DSP. Mouse gesture/one Undo/Escape/focus loss/Home/End,
caption click, right click, checked/duplicate-disabled menu, destination replacement
with gain/pre-post retained, stale menu removal no-op, bus rename, Save/Open,
8 sends +8 inserts, default/minimum/expanded layout and overflow scrolling PASS.
Software100/150 previews inspected. Existing J2 waveform probe is positioned in
visible Arrange after the larger default send area; no waveform code changes.
Initial fixture failures (checking before gain ramp settled,12 inserts over the
existing8-effect limit, off-screen waveform probe) corrected; final suite green.

RT boundary: all new preview preparation/model reads occur on message thread;
fixed-size MixerUpdate transfers gains through the existing bounded SPSC queue.
Audio-thread send smoothing/routing stays unchanged. Queue-full return never blocks;
timer retries preview, cancel invalidates applied revision and poll retries actual
model publication. No new callback allocation/locks/I/O/UI or graph changes for
levels. Structural menu changes retain Pause/Stop and detach editor windows before
Application rebuild; SafePointer/project-store guards and current IDs reject stale
callbacks. RT SAFE WITH MANUAL CHECK / FEATURE READY WITH MANUAL CHECK.

Delivery: **MR-Studio-0.2g-upd10-Mixer-Sends-JUCE-ASIO-Windows-local** under chat
workspace/Builds. Matching EXE/PDB, helpers/assets/licenses/previews/full-test log,
README/manual steps and SHA256.json. Packaged J3+diagnostics PASS exit0; session
log confirms version0.2g upd10. EXE SHA256:
`64DFF00623BBEDC228EFF2DCAB04A71665C4459AAD37062C7FB3E9855E7B03BE`.

Manual check: add two sends, adjust each top rail with ASIO playback and verify
only its return level changes; right click replace bus/pre-post/remove; Undo/Redo
and Save/Open; shrink/expand insert area with many plugins/sends and scroll;
check real pointer hit area, Ctrl, Escape,100/150/mixed-DPI/screen reader.
Physical pointer/DPI/ASIO checks NOT RUN in the automated run; the subsequent
functional user acceptance is recorded below. Previous packages preserved, docs local only, no source
push/merge/Actions. Stage5 not started.

## User acceptance — 2026-10-09

User: «Все работает, закрывай sub-issues и весь issues».
#95 and parent #55 closed completed on direct request. All10 sub-issues #91–100
verified closed (nine already closed; #95 was the last open one).
Accepted current build **0.2g upd10**, source15afe65/docs88e1d6e; full CTest118/118
PASS(52.44s), packaged J3+diagnostics exit0 from delivery. No rebuild/retest required
for this documentation-only acceptance record; package and SHA256 left immutable.
Acceptance covers functional #95 and the whole #55 backlog, not a separately
measured ASIO/mixed-DPI or hardware/P4 matrix. Stage5 not started.
Next step: wait for a new user request. Docs local only; no push/merge/Actions.
