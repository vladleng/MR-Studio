# 0.1p upd2 — embedded plugin editors, preset library and file browser

Local Windows JUCE J3 follow-up, 2026-10-05. User could not audition TH-U in upd1
because its native editor briefly appeared and then the generic insert panel opened.
TH-U audible-state acceptance remains pending. Nuro state was already accepted;
no special installed Nuro tests are required for routine builds.

## Plugin editor correction

Offline insert edits formerly rebuilt mixer controls without preparing VST3 graphs.
The application therefore had no hosted instance for editor/parameter/state access.
Offline rebuilds now prepare insert graphs using the same host as hardware mode,
while retaining the offline clock and silent output. Opening a current editor also
synchronizes its graph generation so the next UI timer cannot retire it as stale.
The editor shell stays hidden until the native view is attached; the plugin is a
child HWND below the insert controls. A plugin with no supported native view still
uses the generic parameter editor. Native DSP editors retain their existing UI.

Regression checks cover a fixture after offline insert rebuild, real TH-U opening
and surviving UI timer turns offline, project save/reopen, and the reopened native
editor. No physical ASIO hardware or manual preset audition is claimed.
The upd1 activation-before-restore TH-U DSP correction is retained.

## Preset library

Library: `%USERPROFILE%/Documents/MR Studio/Presets/Plugins/` (Windows Documents
special folder, which may be relocated). Each processor has a separate directory;
VST3 directories use a legal plugin name plus class ID to avoid vendor/name clashes.
`Save preset...` asks for a new name and writes `.mrspreset` atomically in that folder.
Existing names are rejected; choose another name to preserve earlier presets.
The `Saved presets...` dropdown lists that processor's saved files and loads the
selected preset. `Load preset...` can also load an existing file from another folder.
Both the native VST3 shell and native/generic parameter editors expose the dropdown.
State includes opaque component/controller data and sparse parameter overrides;
slot identity, installed module path and destination bypass remain unchanged.
Loading requires Pause/Stop and uses the shared project Undo command.

Real file roundtrip testing exposed Windows CRLF headers in upd1 preset exports;
the decoder now accepts both LF and CRLF. Existing upd1 files remain readable.
Malformed, oversized and wrong-plugin files are rejected.

## Files tab and background

The right browser retains VST3 vendor groups and adds `Files`. It starts in Documents,
shows folders and WAV files, supports `Up`, `Folder...`, and an editable folder path
including another drive. Directory listing runs on a dedicated background thread.
Select one or more WAVs and drag into the arrangement. This uses the existing WAV
import command: each file creates a new audio track and clip at the project start;
it does not replace an existing clip or alter source media. Unsupported formats and
folders cannot be imported. Other sample formats are outside this update.
Arrange/Edit/Mix/BROWS remain at the bottom right.

Only one screenshot was attached although the request mentioned a second one.
The editor background visible in that screenshot was lightened approximately 20%
(#292d31 to #31363b; graph area #1d2328 to #232a30). Main dark browser/track/clip
backgrounds retain upd1 colors; the new Files tree uses the same dark background.

## Validation and delivery

Cached offline configure and local Windows x64 Release build; 90/90 CTest.
Expanded JUCE checks exercise real internal file-tree selection/drag/import and
preset file publication/discovery/reload with separate processor folders.
Installed TH-U editor/state checks pass; no special Nuro run. Main and Files
snapshots are included in the local package. User acceptance and remaining physical
J3 gates remain open, as do whole Stage 3/#23 and deferred #16.

Branch: `mrs/0.1p-upd2-editor-file-browser-local`.
Package: `MR-Studio-0.1p-upd2-JUCE-J3-ASIO-Windows-local` in local Builds.
Keep `mrs_vst3_scan.exe` beside `Moon River Studio JUCE.exe`.
Previous packages are preserved. Code/builds stay local; GitHub issues/docs only.
No code push, PR, merge, GitHub Actions, installations or downloads.
