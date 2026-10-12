# 0.2g upd3 fix6 — project-name hover and flat mixer buttons

2026-10-09. Source f09eadf, local branch mrs/0.2g-fix1-editor-space-local.
Accepted prerequisite fix5: user «Теперь все отлично. Принимай».

ProjectHome now has a filename-sized accessible TextButton per visible row.
Hover/down changes filename to skyblue; pointing hand belongs only to that
button. List blank area uses normal cursor. Existing row click, Enter, filter,
async scan/load, New and Resume behavior retained; callbacks use SafePointer.

Mix/Master channel TextButtons and plugin-name surfaces use a scoped flat
style: solid fill brighter(.15f), no idle border/bevel. Hover/down and keyboard
focus remain visible; toggle colors remain active. Panel backgrounds unchanged.
Arrange mini-strips and global toolbar unaffected. Transparent power icon and
accepted blue/neutral colors unchanged. No project state/schema/DSP/Undo changes.

## Validation

Windows VS18 2026 x64 Release, cached JUCE9.0.3, ASIO/VST3 enabled; existing
build/asio-local. Configure with FETCHCONTENT_FULLY_DISCONNECTED=ON PASS;
MoonRiverStudioJuce build PASS. Focused CTest4/4 PASS (25.26s): crash_diagnostics,
juce_j2_smoke, juce_j3_smoke, juce_diagnostics_smoke. Full118 not rerun for UI.
Initial hover regression failed because painting queried cursor location while
the test set Button state. Paint now follows Button state (also real hover).
Regression covers cursor/filename bounds, hover pixels, filename click, Mix/
Master style flags and brighter solid fill. Software100/150 previews reviewed.
Physical DPI, live ASIO and screen-reader checks NOT RUN for this scoped patch.

Separate immutable package:
MR-Studio-0.2g-upd3-fix6-Home-Hover-Flat-Mixer-JUCE-ASIO-Windows-local.
EXE SHA256: 4E07E072E3BC6C8E67E44136A1926C1E6FD3BF35B723C3980B90A7B8FB4E1E74.
Matching symbols/helpers, assets/licenses, README, focused test log, previews.
Three packaged J3+diagnostics runs exit0; packaged diagnostics helper tests
PASS (main/worker/abrupt/clean/acknowledgement/restart/new report/retention).

READY WITH MANUAL CHECK: hover name versus blank area; inspect flat lighter
Mix/Master buttons, toggle colors and unchanged backgrounds. User acceptance
received 2026-10-09: «Проверка прошла». Previous packages preserved; no next
sub-issue/Stage5, push or release.
