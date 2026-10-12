# 0.2g upd3 fix5 — crash notice and brighter plugin power

2026-10-09: user accepted #94 / upd3 fix4, then requested two corrections before
the next sub-issue. This is a follow-up fix, not a new stage or crash-cause fix.

## Contract and implementation

The startup notice repeatedly selected the same retained crash report. Its
existence did not prove that the most recent session crashed. The latest report
now has a zero-byte `mr-session-*.seen` sidecar after the user dismisses the notice.
The callback captures only the report path, not application/session objects.
Later clean launches do not repeat that notice or surface older retained reports;
a newer report still triggers a notice. On the first launch of fix5 an old report
may appear once: dismiss it to acknowledge it. Logs, reports and dumps are retained.
The File > Open diagnostics folder action remains available.

Seen markers follow the existing per-session retention group, not a separate
unbounded cache. Reads/write failures are nonfatal; failed acknowledgement is
journalled and may repeat the notice. Symlinks are not accepted as report/marker
files. Discovery follows retention, avoiding a notice for a report just pruned.
Older versions ignore sidecars and can still repeat old notices; report format
and crash capture protocol are unchanged. No actual user logs are modified by tests.

Power icons are slightly brighter: active blue #3098ee, bypass neutral #bdc6cf.
13x13 hit region, 11px inset label, invisible idle button background, hover/focus,
state projection and separate editor/power clicks are unchanged.

## Layers

Affected: diagnostic startup/notice API, local acknowledgement persistence,
retention, GUI callback, icon drawing, tests, version, packaging and docs.
Project model/schema15, preferencesv7, editing commands/Undo, project files,
routing, plugin state/lifetime, transport, audio callback and fatal-path capture:
NOT AFFECTED. Diagnostic acknowledgement is not a project edit or project Undo.
Windows diagnostic infrastructure and existing Studio JUCE shell only;
no new Live scope, background queries or network upload.

## Validation

Configure/Release app, diagnostics probe/tests build PASS. Focused CTest4/4
PASS(29.38s). First test fixture accidentally used Windows text-mode copying,
changing CRLF bytes; corrected to binary writes, then preservation check PASS.
No crash-report content was changed by production acknowledgement.
Source9af6ff2, branch mrs/0.2g-fix1-editor-space-local. Full118 PASS belongs to
accepted upd3 fix4; not rerun for this scoped diagnostic/UI follow-up.
Software100/150 previews reviewed (100 from package,150 from build).
Three packaged J3+diagnostics runs exit0; packaged diagnostics helper tests PASS,
including acknowledgement/restart/new-report regressions. Separate immutable
package: MR-Studio-0.2g-upd3-fix5-Crash-Notice-Bright-Power-JUCE-ASIO-Windows-local.
EXE SHA256:32F53CD94AA90D4C621AFEB7BA5D165CFD0B37F52368B11E4507C03AB25F56F5.
Matching EXE/PDB/helpers, README, focused test log, previews, SHA256 manifest.
READY WITH MANUAL CHECK / FEATURE READY WITH MANUAL CHECK. User acceptance
of fix5 received 2026-10-09: «Теперь все отлично. Принимай»; no next sub-issues,
code/docs push, Actions, merge or release.
Checks passed: first notice,
acknowledge/idempotence, restart twice without repetition, older reports not
resurfacing, later crash triggers notice, preserved reports, blocked/missing
acknowledgement, marker retention; existing main/worker/abrupt/clean diagnostics.
GUI smoke retains bypass click/Undo/VST3 fixtures and screenshots at100/150.
Physical DPI and screen-reader checks NOT RUN.

Manual check: dismiss the old notice, quit normally and launch again; it should
not return without a new report. Review brighter power on Track/Bus/Master.
