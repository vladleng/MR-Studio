# 0.2g upd3 fix7 — shared popup theme and mixer background

2026-10-09. Local source1bd8ba7, mrs/0.2g-fix1-editor-space-local.
Fix6 accepted: user «Проверка прошла». Reference: supplied Studio Pro menu
screenshot; requested browser-colored popup surface, faint outline, mixer
channel background matching track channel. No unrelated redesign.

## Implementation

Controls.h configurePopupColours/drawPopupSurface share background=surface
(25282b, same as Browser VST3), border3e4348 at1 logical pixel, whitesmoke text,
existing accent highlight/white text. No gradient/bevel. Theme applies to
MenuBar/ComboBox popups. Explicit LookAndFeel binding for standalone plugin,
audio/MIDI input, output and Sends menus; submenus inherit parent theme in JUCE.
PresetMenuTheme shares the palette/background while retaining compact13 font
and340 width cap. Existing labels, shortcuts, disabled/ticked states, arrow/
keyboard activation and callbacks unchanged. No new global LookAndFeel lifetime.
Third-party plugin-internal UI is not owned/restyled by MR.

Strip body now panel393e43 for Mix Track/Bus/Master, same as mini track strip.
Selection header/accent and accepted flat lighter buttons remain distinct.
Arrange content, browser, meters, faders and Inserts sizing unchanged.
Theme state is transient UI, no project/preferences schema, Undo/DSP changes.

## Validation

Windows VS18 2026 x64 Release, cached JUCE9.0.3/ASIO/VST3,
build/asio-local. Configure FETCHCONTENT_FULLY_DISCONNECTED=ON PASS;
MoonRiverStudioJuce build PASS; focused CTest4/4 PASS(29.79s): crash_diagnostics,
juce_j2_smoke, juce_j3_smoke, juce_diagnostics_smoke.
Added assertions for Mix/Master pixel color and popup browser fill/faint border;
actual popup snapshots100/150 with disabled/ticked entries; existing preset menu
snapshots show folder arrows. Software previews reviewed. Full118 not rerun
for this scoped UI. First compile fixed shadowed test variable; first pixel
test corrected native deferred graphics read with SoftwareImageType/scoped
Graphics. Neither required production behavior changes.

Package MR-Studio-0.2g-upd3-fix7-Popup-Mixer-Theme-JUCE-ASIO-Windows-local.
EXE SHA256:F001654A4260CC4320A77DA9AE0A91793E160D639523578C32C595CEC58B11E0.
Matching symbols/helpers/assets/licenses, README/test log/previews/SHA256.
Three packaged J3+diagnostics runs exit0; packaged diagnostics helper tests
PASS (main/worker/abrupt/clean/acknowledgement/restart/new report/retention).

READY WITH MANUAL CHECK: File/Transport, Inserts plugin submenus, presets,
input/output/Sends; matching Track/Mix background. Physical DPI/screen reader
NOT RUN. User accepted 2026-10-09: «Отличная работа, принимай и переходим к сл.
sub-issues». Next #96 contract discussion, no recording code yet. Earlier packages
preserved; no Stage5, source/docs push, Actions, merge or GitHub release.
