# 0.2g upd2 fix2 — #93, mixer insert bypass

Requested 2026-10-08 after accepted #92. Only #93; not Stage 5 or #94–100.

## Contract and state flow

Each mixer insert row (audio/instrument track, bus, Master) has a separate power
icon and an editor-name button. Power changes bypass only; the name opens the
existing editor. In fix3 the power hit area is 13×13 logical pixels, half the
initial 26px width, centered vertically. No idle background/border; a cached JUCE
Path draws the power symbol. Blue means active (!bypass), neutral grey means off /
bypass. Hover/press brighten the glyph; only keyboard focus shows an outline.
Tooltip and accessible button title expose the full plugin name/state/action.
Row height
18 px, spacing 20 px, existing channel font/theme and viewport retained.
The state is read from NativeInsert.bypass, not an independent widget toggle.

Click → queued message-thread action (Strip SafePointer + ProjectStore identity +
slot existence guards) → existing Desktop::applyChain → Application::set_inserts /
SetInserts → central history / project model → existing engine transfer → refresh.
Queueing avoids destroying a clicked button inside its callback during refresh.
No plugin calls or directory operations in drawing, and no new audio-thread code.

Native bypass uses the existing bounded parameter queue and may change during
playback. VST3 bypass remains Pause/Stop-only: it may change fixed latency, so
existing stopped capture/rebuild/reconnect handles PDC and native editor lifetime.
Rejected playing/recording changes retain model/history/control state and show
the existing error. No seamless latency-changing transition is introduced.
An already open native editor is reconnected, not opened as a bypass side effect.

## Layer and RT review

UI, notifications, tests, docs: AFFECTED. Existing Application/commands/history/
model/persistence/engine paths: AFFECTED by a new entry point, unchanged semantics.
Project files, schema15, preferencesv7, DSP/PDC algorithm, callback scheduling:
NOT AFFECTED. Shared model/engine, Studio mixer UI; Live-specific presentation:
DEFERRED. Hover/focus state is transient, bypass already persists in projects.

UI is NON-RT; applyChain is RT-ADJACENT. Existing native parameter publication and
VST3 stop/join/reprepare remain the only transfer/ownership boundaries. No new
allocation, locks, I/O, plugin state capture or UI calls on RT-CRITICAL paths.
No separate DSP owner or plugin-specific rule. Undo/Redo retains existing editor
closure policy, and bypass still uses central SetInserts history.

## Validation

Added GUI regression checks: disjoint pointer hit areas; actual queued Button
activation; native state, single revision and unchanged graph generation; Undo /
Redo refresh; project serialization; native playback; bus/Master targeting;
queued removed-slot cancellation; 100% / 150% software snapshots. Synthetic VST3
checks assert 7-sample PDC removal/restoration, existing two-editor reconnect and
playing rejection without state/history drift. Existing plugin editor bypass,
transport, native EQ, project persistence and audio allocation tests retained.

Cached Windows x64 Release configure/build PASS; focused J2 GUI smoke PASS (2.67s).
Full sequential CTest: 118/118 PASS (82.01s). Initial -j2 run had one existing
audio_ahead_recovery failure (heard=256, after=256, maximum=1); its recovery check
uses a fixed sleep before expecting producer progress. Ten isolated repeat runs
and the full sequential run passed. Engine/test code was not changed for this
timing observation. Eight sequential packaged J3+diagnostics smokes exit 0;
packaged crash diagnostics PASS. Source 9697c54; separate local package
MR-Studio-0.2g-upd2-fix2-Mixer-Bypass-JUCE-ASIO-Windows-local.
EXE SHA256 76A3DDDB75897AC85578252F695BF8B7495440788BFEDBC4AD752AE79784AF61.
Physical ASIO audition, installed plugins, physical mixed-DPI and screen reader:
NOT RUN. User manual acceptance remains required. RT SAFE WITH MANUAL CHECK;
FEATURE READY WITH MANUAL CHECK. Issue93 stays open pending user acceptance.

Manual check: click power on a stopped project for a native effect and VST3 on track /
bus / Master; verify audible bypass, no new editor, clear state, Undo/Redo and
save/reopen. While playing, native bypass remains available; VST3 requests Pause/
Stop. Check an already open VST3 editor survives stopped bypass.

Source/build/package/docs local; issues updated, no Actions/source push/release.

## fix4 shared plugin surface

2026-10-08: user supplied a reference screenshot and requested the power symbol
on the plugin button itself, without overlapping the label, and a smaller font.
Fix4 makes the name button fill the entire row and parents the power button
inside it. The icon remains 13×13 at x=2; label begins at x=17 (2px gap after
icon), uses 11px font and left alignment. One theme surface/border encloses both
hit regions; the icon has no independent background. Child hit testing retains
separate bypass/editor activation, accessible title and blue-active/grey-off.
Only UI/version/test assertions changed; DSP/Application/model/schema unchanged.
Release build PASS; focused GUI3/3 CTest PASS(28.45s), shared parent/full-row
geometry and separate hit targets asserted. Snapshots100/150 inspected; no label
overlap, compact Channel EQ label now fits the narrow strip. Three packaged
J3+diagnostics smokes exit0. Source f96bff5; matching EXE/PDB and SHA256 manifest.
User accepted final fix4 on 2026-10-08: «Принято!». Issue93 closed completed;
functional acceptance preceded this visual acceptance. Earlier packages immutable.

## fix3 visual follow-up

2026-10-08: user accepted function («Все отлично, работает») but requested the
smaller invisible square power button and blue=active / colourless=off semantics.
Only Controls rendering/layout, version and UI smoke colour/size assertions change;
Application, model, persistence, DSP, queues and bypass/PDC policy unchanged.
Windows Release build PASS; focused J2/J3/diagnostics CTest 3/3 PASS (26.67s),
including 13×13 geometry, no button text, blue active and neutral off assertions.
100% / 150% software snapshots inspected. Full 118-test suite belongs to fix2;
not rerun for this isolated visual follow-up. Three packaged J3+diagnostics
smokes exit 0. Source 2240af0; separate fix3 package preserves fix2 unchanged.
Final visual acceptance pending. No new RT review required: bypass path unchanged.
