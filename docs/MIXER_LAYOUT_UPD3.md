# #94 — freely resizable Inserts area (0.2g upd3 fix4)

## Contract, 2026-10-09

The user corrected the original content-dependent maximum: the empty Inserts
area may expand freely, plugin rows remain pinned to its upper edge, and faders
keep their length. Sends is unchanged. The window, not plugin count, limits the
dock; keep at least 140 logical pixels of arrangement visible. No Stage 5 work.

## Implementation and layer impact

- UI/actions: Desktop splitter uses the displayed height as its drag origin,
  avoiding a dead zone when saved height exceeds the current window's capacity.
  Strip keeps its existing top-aligned insert viewport and bottom controls.
  Fixed 145px mixer faders/meters match the previous default geometry.
- State/persistence: existing `ViewSettings::mixerHeight` remains base height;
  content allowance is derived from the longest insert chain, using the actual
  20px row pitch. The accepted automatic allowance for inserts is retained,
  but it never imposes an upper expansion limit. Minimum geometry protects pan,
  header and lower controls; insert lists still scroll in a constrained window.
  Mix/Edit use separate saved heights. The view-settings version stays 1;
  the upper decode bound for mixerHeight grows from 1400 to 2160 so tall-window
  heights survive reload. Older view JSON still loads, older apps safely clamp
  larger values. Project schema 15 and application preferences v7 are unchanged.
- Model, engine/domain, realtime/audio, project files: NOT AFFECTED. No plugin
  state queries, audio graph mutation, callback work or routing changes in layout.
- Undo/notifications: resizing is a UI preference, not a project edit and not
  project Undo. Existing plugin chain Undo/Redo still refreshes geometry.
- Studio/Live: current JUCE Mix shell only; no future Live implementation.
- Tests/docs: AFFECTED. Track/Bus/Master, empty/short/long chains, top anchoring,
  splitter gesture, minimum window, fixed faders, independent Edit height,
  tall-window settings reload and software 100/150 previews.

## Validation journal

Local VS18 2026 x64 Release, existing build/asio-local, cached JUCE 9.0.3,
ASIO/VST3 enabled. Configure PASS. Initial draft build hit a shadow-variable
warning in the new test (fixed). First GUI run failed because the test copied
insert IDs between channels; fixed fixture IDs, no production engine change.
Final Release build PASS. Focused GUI CTest 3/3 PASS (26.79s); full serial CTest
118/118 PASS (86.06s), including UI, project/Undo, synthetic VST3 and diagnostics.
Software 100/150 snapshots reviewed at 1200x700: top-aligned rows, fixed faders,
Master overflow scrollbar and usable arrangement/transport. Physical DPI is not
implied by these software snapshots. No real settings/projects were altered.

Source f318c6c on local mrs/0.2g-fix1-editor-space-local; EXE/PDB match that build.
Separate package: MR-Studio-0.2g-upd3-fix4-Free-Inserts-Height-JUCE-ASIO-Windows-local.
EXE SHA256: E224D8FDE7C088F83D3AE9DA4939FCA21E0DE4FAD98ABDB8D20A26023192A329.
Three packaged J3+diagnostics runs exit0. Packaged crash helper tests PASS:
main/worker dumps, exception/stacks/modules, abrupt/clean shutdown, retention,
bounds and graceful failure. Matching symbols, logs, previews and SHA256 manifest
are included. Previous packages retained; no GitHub code/docs publication,
Actions, merge or release. Issues #94/#55 follow the corrected contract.
2026-10-09: user accepted the build: «Да, отлично! Ты все правильно понял! Этот
тест прошел». #94 closed completed, parent55 updated. Follow-up crash notice and
power brightness work is separate fix5; the immutable fix4 package is preserved.

## Manual acceptance

Drag the Mix divider with zero, one and several inserts: empty space grows,
rows stay at the upper edge, and faders/meters retain their length. Check Master,
small window, insert overflow scrolling and independent Edit size after switching.
Physical mixed-DPI and screen-reader verification are NOT RUN.
