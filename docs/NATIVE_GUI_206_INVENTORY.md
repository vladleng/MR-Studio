# Native GUI adoption #206 — inventory and pilot decision

2026-10-16; first stage authorized by “закрывай и переходи к 206”.
Local runtime source baseline: dbbadcad1ac5daaae7d1f48c4e06b32ff2010c52.
Shared Kit #189 and pilot #195 are closed/accepted on GitHub. The older local
pilot document still contains historical pending-acceptance wording; it does not
override actual acceptance or the current proportional-validation policy.

## Actual local editors

| Editor | Actual composition / adoption boundary |
|---|---|
| Gain, High/Low pass, EQ, Channel EQ | FxPanel in Editors.cpp; existing DSP-backed fields/curve and preset shell; not yet uniformly migrated to Kit controls |
| MR Reverb | FxPanel + ReverbBrowser + optional ReverbKitControls; accepted editor stays default, transient --reverb-kit enables accepted #195 pilot |
| MR Rig | Existing RigPanel with product-specific shell; continue #200, do not duplicate or infer NAM implementation from visual controls |
| MR Rig Models / Cab collection | CabBrowser / ToneBrowser with actual Gain/Mix/Low cut/High cut controls; preserve existing WAV/model semantics |
| Legacy Cab IR | Existing FxPanel controls; preserve embedded IR behavior |
| External VST3 | PluginPanel host toolbar adopted in #205 P3; vendor HWND is outside native visual migration |

No dedicated Harma Waves/Saturator/MR Strip/Spatial editor was found in the audited
JUCE source. Roadmap names are not evidence of an implemented editor or DSP.

## Existing reusable infrastructure

ReverbKitControls already uses MRKnob, MRButton, Theme and MRControlBinding against
Desktop's lifetime-checked parameter host. It binds real Pre-delay, Tone, Mix and
gate Threshold metadata; threshold is conditional on existing Input Gate options.
Do not build another knob/binding/Theme framework.

FxPanel and EditorWindow own window/Space and preset behavior. Reverb loader is
off-thread; controls must retain real Loading/Ready/Missing/error and True Stereo
metadata. One completed gesture is one Undo command; compatible preset reloads
retain the editor, and retirement cancels previews before owners disappear.
Gallery imports are session-local visual tokens, not audio/project state.

## Pilot decision and next implementation slice

Use MR Reverb as the first adoption candidate under linked #207. Extend the
accepted #195 editor rather than create a second editor:

1. Reuse the existing preset/bypass toolbar and current IR browser.
2. Compose one compact view: IR identity/status, browser, real Pre-delay/Tone/Mix,
   optional Gate/Threshold; keep file and True Stereo states legible.
3. Reuse original plate/room/hall/spring artwork from assets/ui/mr-uds/svg/reverb/;
   illustrations are graphics, not simulated waveform or DSP controls.
4. Preserve the accepted/default editor and opt-in boundary until the new scope
   receives its own visual acceptance. No automatic default switch or migration
   of MR Rig, other editors, DSP, schemas or automation.

#207 explicitly requires a separate implementation request. This inventory marks
only the first #206 checklist item complete, not Reverb modernization complete.

## Verification for the next slice

Use an incremental affected GUI target if compiled code changes, then the existing
juce_reverb_kit and affected Reverb browser/editor checks. Reuse existing fixtures
for IR selection, physical units, preview/Undo, presets, reopen/Space and Theme
lifetime. Inspect relevant new layout/state/DPI views when geometry changes.
No full CTest or packaging by default. Current inventory is docs-only: no build,
test, normal app launch, device reconnect or user preference modification needed.

#205 is closed by explicit user instruction after P6 acceptance, with Live/P5
excluded/deferred. Main shortcut now targets the current compiled DAW EXE.
Native adoption #206 remains open; per-editor acceptance is still required.
