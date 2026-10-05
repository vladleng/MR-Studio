# 0.1p fix3 — audio reconnect, shared track order and Backspace

Windows local follow-up, 2026-10-05. User reports fix2 generally works;
remaining audio-settings/reordering/style requests are addressed here.

## Audio session

Opening a project used to connect the temporary offline clock with the project's
monitored physical inputs. Mono 2 or higher failed validation against its one
virtual input, preventing subsequent ASIO reconnect. Offline connection now uses
only explicitly requested virtual inputs, skips physical output-route validation
and prepares the offline mixer/inserts. Project physical mappings remain untouched.
Hardware connections retain channel validation and real media rendering.

When a hardware device was active, Open/New reconnects the saved device by name,
with saved buffer/default-input/output configuration and channel-name safeguards,
independently of the startup auto-reconnect checkbox. New retains the device rate;
Open requests the project's rate on that same interface, without resampling media.
Unavailable device, changed layout or unsupported configuration still reports the
specific error and falls back offline without erasing saved device preferences.

## Interface and editing

Main/menu/toolbar/footer/mixer background and mixer strips use the same #25282b
surface as the browser. Track-panel and clip-grid colors remain as in fix2.
Windows native title-bar appearance remains controlled by Windows.

Drag a track name to reorder it before/after another track in either arrangement
or mixer, including dragging between those views. Drop in empty arrangement/mixer
space to append. Track targets show a light-blue insertion edge. One ReorderTrack
command updates the common project order; both views rebuild from that order.
Stable IDs preserve clips, inserts, routes and sends. Undo/Redo and project saves
retain order. Same-position drops do not create an Undo entry. The Master stays
fixed. Structural changes require Pause/Stop, as before. Files/VST3 browser order
continues to describe folders/vendors, not project track positions.

Backspace deletes the selected clip through the shared command/Undo action.
Delete remains an alias. Text fields, combo boxes, modal and plugin windows keep
their keyboard guards so text editing does not delete clips.

## Verification and delivery

Cached offline configure/local Windows x64 Release build and 90/90 CTest.
Regression opens a saved project with monitored stereo inputs 3/4 and master
outputs 3/4, connects offline successfully, then reconnects a synthetic device
and checks retained channels and buffer. JUCE tests exercise track drag/drop in
both directions, synchronized views, retained clips, Undo, no-op drop and actual
Backspace binding. Packaged J3 smoke and software previews included.
Physical ASIO-device/project-switch audition remains user review, not claimed by
the synthetic checks. No special installed TH-U/Nuro tests, per accepted policy.

Local package: MR-Studio-0.1p-fix3-JUCE-J3-ASIO-Windows-local in chat Builds.
Keep mrs_vst3_scan.exe beside the main executable; previous packages retained.
Code/builds local; GitHub issues/docs only. No code push, PR, merge, Actions,
installation or downloads. Remaining J3 physical gates, Stage 3/#23 and #16 stay
open.
