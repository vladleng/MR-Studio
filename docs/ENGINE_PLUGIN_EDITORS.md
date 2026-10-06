# Plugin editor lifetime: 0.1t fix1

Local Windows JUCE fix requested 2026-10-06 after positive P3 performance feedback.
The user clarified that editors went behind MR Studio rather than closing on hover.

- Native VST3 editor windows are owned top-level Windows windows of MR Studio.
  They stay above their application owner, without system-wide always-on-top.
- Pin is a window-local UI toggle. Opening another insert closes unpinned editors;
  pinned editors stay visible. Clicking the same slot reuses its existing window.
- Closing one native editor detaches only that slot's view. Project/device changes,
  removing/reordering inserts and other processor-lifetime changes still retire
  editors regardless of Pin. Pin is not project state and has no Undo/schema change.
- Host bypass retains its existing Pause/Stop boundary and PDC recalculation.
  Native views are detached before processor/module teardown; after a successful
  rebuild, new views attach to the same visible JUCE windows, including pinned
  windows on other channels. Pin and top-level window identity survive bypass;
  the native plugin child HWND is recreated. No stale blank native host remains.
- The project insert model and existing SetInserts command remain authoritative
  for bypass, project serialization and Undo/Redo. Undo/Redo still closes editors
  as before. Failed bypass validation restores views when the runtime remains available; failed attachment retires
  the affected view rather than presenting it as a working editor.

## Realtime and layer review

UI, JUCE/Windows ownership, non-RT editor APIs, regression tests and documentation
are affected. Bypass still uses the existing stopped rebuild/capture path; no
callback scheduling, DSP, buffering, PDC algorithm or processor ownership changes.
PreparedGraph/Application expose per-slot close in addition to close-all, called
only from the message thread. Project schema, assets, routing, persistence format,
Live UI, device settings and engine performance policy are unaffected. No plugin
brands or project-specific behavior are introduced.

The fixture now unregisters its editor window class when its final view detaches,
preventing an unloaded DLL window procedure from surviving module reload.

## Local validation

Release configure/build, full CTest and hidden JUCE J3 fixture smoke are required.
The added GUI regressions exercise actual bypass/Pin buttons, four bypass changes
with two native views, HWND owner identity, independent close, focus/mouse messages,
unpin/open-another, editor reuse and existing presets/DPI conversion checks.
The toolbar preview is a JUCE software snapshot and does not capture native child
plugin pixels or establish physical mixed-DPI behavior.

Actual ONE/TH-U/Xvox editor appearance, pointer/focus behavior and an ASIO audition
remain a user manual check. Existing generic VST3 fixtures run locally; installed
plugin-specific compatibility tests are not run for this fix. P3/#53 remains open.

Result: READY WITH MANUAL CHECK. Cached configure/full Release passed; 109/109
CTest passed in 57.09 s. Final packaged hidden J3 smoke waited exit 0, including
both J2 editor regressions and J3 UI checks. Original 0.1t package is preserved.
Package: MR-Studio-0.1t-fix1-editors-JUCE-ASIO-Windows-local.
JUCE EXE SHA256: 034C6FAA6F62ECB8C038F8AA8F0580BE9E7736E03EB77F04F82E6E956B5B83F8.
Code/builds/tests/packages local; GitHub docs/issues only, no Actions.
