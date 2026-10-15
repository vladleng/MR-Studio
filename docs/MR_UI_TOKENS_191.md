# MR UI Kit #191 — scoped tokens and native preview

2026-10-15. **READY FOR VISUAL REVIEW**, separate MR UI Preview0.1.
#190 audit accepted by user; #191 explicitly authorized. #192–#195 are not
implemented by this slice. The first native preview is now runnable.

## How to open

Desktop shortcut **MR UI Preview**, or
`Builds/MR-UI-Preview-0.1-Tokens-Windows-local/MR UI Preview.exe`.
This is a separate JUCE process; existing MR Studio0.4f shortcut/package stays.
Left: accepted0.4f Theme reference. Right: editable scoped MR UDS preview.
Inspector: profile,100/125/150/200% zoom, semantic RGB role, bounded metric,
Reset profile, Save draft, Load draft. Native demo sliders accept typed values,
keyboard and drag; these values have no audio/project destination.
Comparison scrolls at small sizes/high zoom; inspector remains accessible.
Close with unsaved token changes offers discard/keep editing. Profile switching
retains each profile's in-memory edits. Saved files are explicit user-local JSON.

## Implementation owner / isolation

- `apps/studio-juce/src/DesignTokens.h`: validated semantic token metadata.
- `Controls.h`: extension of the existing **final ui::Theme** with explicit
  instance-only applyPreview/role/font/geometry resolution and preview rotary
  painting. No second global theme; default construction retains accepted style.
- `ThemePreview.cpp`: plain JUCE component tree, scoped Theme owners, comparison,
  inspector, bounded draft I/O and smoke fixture. No Desktop/FxPanel/RigPanel
  constructed; no project, shared audio engine, ASIO or DSP state is accessed.
- `cmake/Juce.cmake`: dedicated MRThemePreview target, linked to existing binary
  assets and juce_gui_basics only. Generated project dependencies were checked:
  no mrs_desktop/mrs_audio/mrs_processing/mrs_asio linkage.

No global setDefaultLookAndFeel. Each Theme lives longer than attached children;
root teardown cancels its file chooser, detaches viewport/LAF; SamplePane detaches
its LAF. Native colour/metric edits invalidate the affected subtree, and density/
gap adjust content bounds before the viewport scales the comparison.
Default constexpr product paint/helpers remain available and unchanged in meaning.

## Schema1 / exact boundaries

Base ID `accepted-0.4f`, profile ID `accepted` or `mr-uds`; font family is fixed to
system Segoe UI with JUCE/system fallback. No new fonts/assets are imported.
Accepted defaults are the existing #145/#146/#148 palette and typography. MR UDS
colour targets are from the approved design system/local kit; geometry initially
retains compact accepted metrics for comparison. A successful preview does not
make experimental user edits approved global defaults.

Twelve opaque RGB roles: canvas,window,panel,raised,deep,text,muted,border,focus,
on,error,warning. Twelve-colour MR UDS defaults map to #111720/#171E28/#222C3C/
#2D394E/#192333/#DAE5F4/#7188AB/#4B5F7B/#91BBEE/#7AB69E/#E68F88/#E2C070.
Selection uses focus/accent; power/status uses on. Labels also identify states.

Ten metric roles and ranges:

| Role | Default | Bounds |
|---|---|---|
| controlFont |14 |10..20 logical |
| compactFont |13 |10..18 logical |
| mixerFont |12 |10..16 logical |
| menuFont |14 |10..20 logical |
| fontWidth |1.04 |0.8..1.2 |
| density |1 |0.8..1.4 |
| radius |6 |0..16 logical |
| stroke |1 |0.5..2 logical |
| iconSize |20 |12..32 logical |
| gap |12 |4..24 logical |

Mixer/strip button roles remain distinct from normal buttons/menu roles. Actual
accepted default popup/Combo14/1.04 and mixer12/1 follow #148, not older menu12
text. Existing product defaults are not overwritten by an experimental profile.

All required keys/role names/counts/schema/base/profile/font are whitelisted;
colours require #RRGGBB, metrics finite numeric values within bounds. Unknown,
missing, malformed or unsupported drafts reject before replacing current state.
Parser limit16K characters; file import limit16KB. Export uses JUCE TemporaryFile
and replacement, no project/settings/theme auto-apply. Diff/reset compare with
the selected profile's defaults. Draft metadata has no scripts, font paths,
external references, executable data or audio/plugin parameters.

## Validation / delivery

- All-target local Release PASS: `build/asio-local/ui191-candidate-build.log`.
- Targeted GUI CTest **6/6 PASS43.05s**: juce_theme_tokens, juce_rig_shell,
  juce_native_editors, juce_reverb_editor, juce_reverb_browser, juce_j3_smoke.
- Full current CTest **156/156 PASS201.76s**, including ai_authorization.
- New token smoke: schema exact roundtrip, future schema/malformed/oversized/
  opacity/NaN/bounds rejection, dirty/diff/reset, atomic draft roundtrip,
  independent context teardown, unchanged global/default Theme and exact
  accepted-subtree rendering while editing the right subtree: PASS.
- Mixer font exception, UTF8 Cyrillic sample, maximum density/gap layout,
  minimum inspector separation, scaled geometry100/125/150/200: PASS.
  Hover/pressed/disabled/selected sample render captured; actual keyboard focus
  is available through native JUCE controls, user/manual verification remains.
-9 baseline PNGs exact across accepted0.4f vs new default Theme: Rig4,
  Arrange2, Mixer2, preset menu1. Reverb3 differ only in the random temporary
  fixture-folder GUID text; every pixel outside that path field is identical.
  The actual before/after screenshots and comparison JSON remain local.
- Preview window/minimum images visually inspected after UTF8 correction.
  Earlier smoke caught text encoding display and minimum-layout issues; fixed
  before final candidate/full checks, not counted as final PASS results.
- Packaged preview `--smoke`: PASS exit0. Package contains only preview exe,
  existing JUCE/OFL licences, README and SHA256 manifest; no audio/media data.

QA: `build/asio-local/validation/ui191/` (focused/full logs, details, PNGs,
before/after comparison and packaged smoke). Source and screenshots stay local;
GitHub receives only this contract and issue status with [skip ci].
Existing MR Studio package, shortcut, user Saves and audio sources are preserved.
No audio callback code or DSP/schema/preset/Undo behaviour changed.

Physical mixed-DPI, screen-reader session and manual keyboard/window interaction:
**NOT TESTED by agent**. ASIO measurements are outside this no-audio preview.
#191 awaits user visual acceptance. This is an initial tokens/preview slice,
not the full #193 Gallery or the #192 shared control catalog, #194 host binding,
or #195/#206 product editor migration.

