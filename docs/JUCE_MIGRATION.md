# JUCE migration — selected direction, 2026-10-04

The user selected JUCE with future portability in mind, then explicitly narrowed
the active migration to Windows. Mobile work is outside the current plan.
This is the migration decision and plan; no JUCE integration, new app build,
dependency download or toolchain installation has been performed.
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
- No populated JUCE SDK was found in the inspected repository dependency cache.
  Pin an official release/commit and record licence/hash when adding the dependency.
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

Architecture selected; Windows J1–J3 planned, not implemented or tested. Current local package:
MR-Studio-0.1m-upd1-fix1-ASIO-Windows-local. Outstanding 3c user checks remain pending.
This decision does not close #23, pass deferred #16 or implement Amp/Preamp/reliability.

