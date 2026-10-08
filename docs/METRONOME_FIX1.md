# 0.2h fix1 — toolbar, blank tempo map and note-edit lifetime

2026-10-08. Local Windows JUCE/ASIO correction after the user tested 0.2h.

## Contract and layer impact

Click and Count are toolbar toggles, not popup launchers. Transport settings remain
unchanged. Click toggles both playback/recording off or on; the menu still provides
independent switches. Count toggles Off / last enabled bar count (one bar initially).
The last enabled count while Off is transient per Desktop session; the active
settings retain the existing Preferences v7 persistence. Neither action dirties
the project or enters Undo. Recording still disables settings changes.

New projects use the default TimeMap: one 120 BPM point at tick zero, 4/4 from bar
one. Demo tempo/meter data must not leak into New. Existing saved projects preserve
their tempo maps, schema 13 is unchanged. New resets the document/history normally.

Note replacement and musical note transforms are clip-only edits and retain the
prepared insert instances. The stopped callback/producer boundary still rebuilds
MIDI playback data; plugin chains/device configuration are unchanged. One command,
Undo/Redo and Save/Open semantics remain the existing shared Application path.

Affected: UI, Application/actions, model initialization, RT-adjacent plugin lifetime,
notifications, tests and docs. Persistence uses existing formats (no new fields).
Undo is affected only by the existing note commands, not toolbar preferences.
Project/media file structure and RT-critical DSP are not changed. Application logic
is shared; the toolbar is Studio UI. No Live-specific implementation is introduced.

## Crash evidence and limits

The user identifies Omnisphere 3; the crash was noticed after releasing a moved
note and switching to ChatGPT, not at mouse-down. Previously every note commit
captured/recreated the VST3 chain unnecessarily. Retaining instances removes that
lifetime transition, but is not proof of the reported crash's root cause.
Read-only Windows Application/WER inspection did not yield a matching current
crash stack/dump. The older 0.2h intermediate heap-corruption exit remains
unexplained. Do not equate passing fixture tests with Omnisphere crash resolution.

Regression coverage: toolbar callbacks toggle without a popup, Count restores three
bars, New after Demo has uniform timing and saves its clean TimeMap, note movement
keeps insert_generation stable, and 32 alternating auditioned note drags return
notes to their original positions/pitches. Existing full suite covers Undo/Redo,
persistence and audio/MIDI recording. User Omnisphere/ASIO and delayed-crash
reproduction remain manual checks.

Validation results and package provenance: PROJECT_CONTEXT.md.
