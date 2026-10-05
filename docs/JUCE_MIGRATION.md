# JUCE migration — selected direction, 2026-10-04

The user selected JUCE with future portability in mind, then explicitly narrowed
the active migration to Windows. Mobile work is outside the current plan.
J1 was implemented locally on 2026-10-05 as the parallel 0.1n Windows prototype.
An official pinned JUCE source dependency was downloaded before offline configure;
no global toolchain installation was performed. See [J1 details](JUCE_J1.md).
The user accepted J1 and authorized J2 on 2026-10-05. The parallel target now builds
0.1o / J2 desktop workflow; see [J2 details and boundaries](JUCE_J2.md).
The preferred working interface remains 0.1m upd1 fix1. The upd2 skin is experimental.
Code/builds remain local; GitHub issues/docs only, without code push/PR/merge/Actions.

## Architecture

Retain mrs_core (model/timeline/commands), mrs_processing (graph/native DSP/Cab IR),
mrs_audio (engine/streaming/recording) and mrs_persistence (project/archive).
JUCE provides the presentation and platform adapters, not a second engine or project
format. Introduce a parallel apps/studio-juce target and presenter using existing
mrs_desktop/Application commands. Keep the Win32 shell during parity verification.

Separate presenter/model bindings, components, layout, theme/assets and platform
capabilities. Use custom LookAndFeel, packaged licensed fonts and vector icons.
Define hover/pressed/disabled/focus states, accessibility and DPI/touch behavior.
Keep Arrange/Edit/Mix/BROWS bottom right. Audio callbacks never draw,
touch UI objects or allocate presentation data.

## Repository findings

- CMake already separates model/processing/audio/persistence/desktop libraries.
- win32_main.cpp owns Windows controls, painting and dialogs.
- ASIO/PortAudio integration and existing VST3 host are Windows x64-specific.
- Persistence/media publication have conditional Windows/POSIX implementations;
  preserve the current Windows project/file behavior during this migration.
- J1 uses local official JUCE 9.0.3, commit be29c81492b6151c8ea8d14c840e1311963b3a83.
  Source archive SHA256 E12F3C39395480050A4FE50D820B248C0CFA32F65FD04D4C1B68EECFD4CC31F2.
  Configure must not silently fetch dependencies or install tools.

## Sub-stages

### J1 — parallel Windows JUCE shell and component sample

Add an opt-in GUI target from a pinned local JUCE SDK. First deliver one functional
track/mixer strip and Play/Stop using the existing model/offline engine. Demonstrate
custom fonts/buttons on this small surface before copying the theme everywhere.
Validate DPI/resize/focus, stereo meters, M/S, relative handle-only faders, Ctrl fine
adjustment and one-command Undo. The old UI remains the runnable baseline.
Assign a new build version when implementation starts.

### J2 — desktop UI parity

Move arrangement/waveforms, zoom/scroll, mini-panels, mixer/routing/sends, menus,
recent projects, browser and native insert editors. Preserve existing shortcuts,
Stop-return, footer navigation and BROWS resizing. Test project roundtrip/Undo.
Keep current ASIO/VST3 adapters initially so the UI port does not replace compatibility
fixes implicitly. Connect VST3 native editors through a platform-aware window bridge.

### J3 — Windows parity, polish and acceptance

Complete Windows accessibility, keyboard/focus, DPI, menus/dialogs and custom-component
behavior. Existing ASIO/VST3 adapters stay in place. Any later host replacement is a
separate tested change:
Nuro auxiliary buses, TH-U opaque state, mono/stereo, native editor lifetime and latency
reporting are parity gates. Require offline local configure/build/core tests and actual
Windows UI/device/plugin checks before choosing the JUCE shell as the default target.

Other operating systems remain future possibilities. No Android/iOS investigation,
toolchain setup, mobile UI or mobile plugin work is included in J1–J3.

## Licence and build constraints

JUCE modules are dual-licensed under AGPLv3 and the commercial JUCE licence. Determine
the intended product/source-distribution licence before distribution; do not silently
relicense MR Studio, buy a subscription or accept a commercial agreement.

Use JUCE CMake integration with the existing Windows MSVC/SDK toolchain. Its current
CMake API requires CMake 3.22+. Pin the local JUCE SDK and avoid silent downloads during
configure. No other platform build or portability validation is claimed here.

Official sources checked 2026-10-04:
- [Platforms/features](https://juce.com/features/)
- [Custom LookAndFeel](https://juce.com/tutorials/tutorial_look_and_feel_customisation/)
- [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
- [Exporter overview](https://github.com/juce-framework/JUCE/blob/master/README.md)
- [Licence options](https://juce.com/get-juce/)

## Status

J1 / 0.1n accepted by the user. J2 / 0.1o desktop workflow was accepted by the user on 2026-10-05:
arrangement/WAV/clip edits/zoom, shared mini-panels and docked mixer, routing/sends,
project menus/recents, ASIO settings/recording bindings, VST3 browser/drop/native
editor bridge and native DSP editors. Existing backend and project schema unchanged.
Offline configure/build and 89/89 CTest passed; final component checks rerun after
UI/test corrections. Existing Win32 GUI smoke exited 0. Reviewed waveform snapshot.
Nuro Flexion/TH-U editor and opaque-state software checks passed; intended preset
audition/hardware tests are not claimed. User J2 review is complete; detailed J3 gates remain pending.
Start/open uses offline mode; select ASIO explicitly. Full preference/profile polish,
physical multiple-monitor DPI, accessibility and hardware acceptance remain J3 gates.
Full DAW baseline remains MR-Studio-0.1m-upd1-fix1-ASIO-Windows-local.
Separate packages: MR-Studio-0.1n-JUCE-J1-Windows-local (accepted) and
MR-Studio-0.1o-JUCE-J2-ASIO-Windows-local (accepted). Outstanding 3c checks remain pending.
This decision does not close #23, pass deferred #16 or implement Amp/Preamp/reliability.


## J2 acceptance — 2026-10-05

Local 0.1o / J2 accepted on source 74d6e6f9580908b67eb08330e6ef112a49c3e1f5.
EXE SHA256 E1FB66D1486AAA721A4D39DFCEBE66FEF02E41BBAC2A12730021F4395DCABCE2.
Next planned slice is J3; implementation has not started. Existing test results
stand; no rebuild performed for this documentation-only acceptance update.

## J3 / 0.1p ready locally for review — 2026-10-05

The user authorized J3. Windows preferences/window/view persistence, audio profiles,
explicit ASIO reconnect, keyboard/focus guards and named UI Automation controls are
implemented locally; native editor DPI initial sizing corrected. Offline configure/
build and 90/90 CTest passed; installed Nuro Flexion/TH-U editor/state checks passed.
See [J3 changes, validation and remaining physical acceptance gates](JUCE_J3.md).
JUCE stays opt-in until hardware, physical monitor transitions and accessibility
acceptance are verified. Code/packages local; GitHub issues/docs only.
