# 0.1p fix2 — in-place VST3 presets and reference UI/browser

Windows local follow-up, 2026-10-05. User confirms TH-U now retains the intended
settings/sound after project reopen. TH-U and Nuro restore are accepted: no special
installed-plugin checks on routine builds unless requested or a new specific issue
is reported. Synthetic VST3 regression checks remain part of normal testing.

## In-place VST3 preset loading

The active processor, controller, graph instance and editor HWND remain alive.
With transport paused/stopped, stop device callbacks, drain/capture pending controls,
apply the selected component/controller state to that same processor, flush sparse
explicit overrides, commit project state, and resume callbacks. No reactivation,
recreation, editor closure or window replacement. Old queued editor overrides are
cleared before restore. Full legacy parameter snapshots do not override opaque state.

If state restoration fails, restore the prior captured state and retain the editor;
the requested state is not committed to the project. A preset that changes processor
latency is rejected with an explanation and prior state restored, because dynamic
latency/graph reconfiguration belongs to the planned reliability stage. Structure
changes still use the existing rebuild path. Native DSP presets retain their prior
workflow; this correction targets hosted VST3 plugin windows.

Fixture checks modify real processor DSP through its native editor, restore a saved
preset, and assert both restored active state and identical native HWND/window object.
Invalid fixture state is rejected and restores previous DSP without closing the view.
No special installed TH-U/Nuro runs were performed.

## Reference-inspired Windows interface

The provided Studio Pro screenshot is the palette/layout starting point: graphite
chrome #303438, track panels #393e43, lighter arrangement grid #373b3f, dark browser
#25282b and blue active controls #006dcc. Smaller rounded controls, light disclosure
chevrons, stronger blue-gray file selection, and drawn Play/Pause/Stop/Record icons
retain existing actions/accessibility names. Existing arrangement/track/mixer controls,
zoom/scroll, transport/Undo and bottom-right Arrange/Edit/Mix/BROWS remain functional.
No speculative toolbar/workspace actions are added.

## Files browser

Folders dropdown provides Desktop, Documents, Music, MR Studio and available drive
roots (Volumes). Current folder tree remains lazy/asynchronous; expand subdirectories
without recursively scanning whole drives. Editable path, Up and folder chooser stay
available. A horizontally scrollable clickable breadcrumb row navigates ancestors.
Folder controls and drive selection retain the real Files tab and WAV drag/drop.

Selected WAV detail panel shows name, sample rate, bit depth, mono/stereo/channel
count, duration and modification date. A background worker builds a bounded sampled
waveform overview from 256 short blocks, including long WAV files without full preload.
This is a visual overview, not an exact full-file peak analysis. It does not modify
source audio or play an audition. No inactive audition/loop controls are added.
Existing WAV drop placement onto tracks/empty area and import Undo are preserved.

## Verification and package

Cached offline configure and local Windows x64 Release build; 90/90 CTest.
Expanded fixture checks cover state restoration, stable native editor identity and
failed restore rollback. Existing file-tree selection/drop, browser divider, shared
project/state/Undo, graph/audio/RT and JUCE checks pass. Current main, Files details
and vendor-tree software snapshots are included for review. These checks do not
replace the still separate physical monitor/accessibility/hardware J3 acceptance.

Local branch: mrs/0.1p-fix2-live-presets-reference-ui-local.
Package: MR-Studio-0.1p-fix2-JUCE-J3-ASIO-Windows-local in chat Builds.
Keep mrs_vst3_scan.exe beside Moon River Studio JUCE.exe. Previous packages preserved.
Code/builds remain local; GitHub issues/docs only, no code push/PR/merge/Actions,
installations or downloads. Whole Stage 3/#23 and deferred #16 stay open.
