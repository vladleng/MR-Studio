# 0.1p fix1 — preset restoration and browser/drop corrections

Windows local JUCE follow-up, 2026-10-05. User confirms the native plugin UI is now
embedded correctly in upd2. TH-U audible-state acceptance remains pending.

## Presets

Saving no longer closes/reopens the plugin editor. The library dropdown refreshes
in place after atomic publication, preserving the native child window and DSP.
Loading uses an explicit authoritative preset command rather than ordinary insert
editing: the current live state cannot overwrite the requested saved component and
controller state, even if those bytes equal the last project snapshot. This case
matters when a plugin has been edited since project save. Other inserts retain their
current captured state. Pause/Stop is required. Loading rebuilds the processing graph
and reopens its embedded editor, with the restored preset; this remains the safe
hosting lifecycle rather than in-place opaque mutation during audio callbacks.

A model-identical load still restores live DSP; the shared model intentionally does
not create a redundant Undo entry when its serialized project is unchanged.
Different model states use the existing project command/history path. Preset library
location, format and destination/class validation are retained from upd2.

Regression: save/capture fixture state, change its real native editor/DSP to another
gain, then load the file whose state still equals the project snapshot. Capture of
the restored active processor must match the requested saved state. Library refresh
must keep the original native child alive. No special installed Nuro run.

## Browser

- Vendor folder disclosure chevrons are light gray, white on hover, in both trees.
- Browser has a visible 8-pixel divider on its left edge and a resize cursor. Drag
  left to widen/right to narrow. Width persists on release; range 200..700 logical
  pixels, further bounded by window width to retain usable arrangement space.
- Files selection uses a brighter blue-gray background #375a80 and white text,
  explicitly set for both directory row rendering and TreeView selection rendering.
- Arrange/Edit/Mix/BROWS remain in the lower right.

## WAV drop placement

Internal Files-tab drops and external WAV drops use the cursor's timeline position
(including horizontal scroll and the existing snap setting) and the visible track
row (including vertical scroll/zoom). Drop on an existing audio track to add clips;
no new track is created. Multiple selected files are placed consecutively there.
Drop in an empty arrangement area to create one new audio track per selected WAV,
with clips at the cursor time. Bus/MIDI targets are rejected with an explanatory
message; source media is not altered. The whole batch is one import command,
including media copying/cache validation and Undo/Redo. Existing File-menu WAV import
retains its new-track/project-start behavior.

## Validation/delivery

Cached offline configure, full local Windows x64 Release build and 90/90 CTest.
Tests exercise actual Files-tree selection/drag, existing-track cursor placement,
no extra track, one-command Undo, automatic new track in empty area, actual divider
mouse coordinates, width limits and the preset/DSP regression above. Main, Files
selection and vendor disclosure previews are included in the package.

Local branch: mrs/0.1p-fix1-presets-browser-drops-local.
Package: MR-Studio-0.1p-fix1-JUCE-J3-ASIO-Windows-local in chat Builds.
Keep mrs_vst3_scan.exe beside Moon River Studio JUCE.exe. Previous packages retained.
Code/builds stay local; GitHub issues/docs only. No code push, PR, merge, Actions,
installations or downloads. User fix1 review and physical J3/TH-U audition gates
remain open; Stage 3/#23 and deferred #16 are not closed by software tests.