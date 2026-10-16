# MR Fader System — approved design direction (2026-10-16)

**Scope:** MR Studio's future DAW mixer and track control GUI. Visual family chosen by user. **Not yet implemented or runtime-accepted.** Reuse accepted MR UI Kit `ui::kit::MRFader` from [#192](https://github.com/vladleng/MR-Studio/issues/192); #192 is CLOSED/accepted and must not be reopened. The planned visual variants are a new adoption/extension in [#205](https://github.com/vladleng/MR-Studio/issues/205) P2/P3, with shared artwork in [#208](https://github.com/vladleng/MR-Studio/issues/208).

## 1. Approved family

| Variant | Orientation | Primary use | Styling |
| --- | --- | --- | --- |
| **MR Micro** | Vertical | Normal mixer channels, buses, master in compact density | Preferred main mixer style. Narrow understated precision rail and small rectangular thumb; subtle cool-blue active cue, matte graphite; disciplined and serious. |
| **MR Slide** | Horizontal | Arrange track headers, Inspector, Sends and compact track strips | Horizontal derivative of earlier third/wider concept, made *slimmer and stricter*: thin short capsule/thumb, clear center line, cold pale contour, minimal glow. |
| **MR Precision** | Vertical | Expanded/large Master, Bus, detailed channel inspection | Enlarged MR Micro with extra scale/detail, not a separate casual/large skeuomorphic skin. Secondary variant; lower priority than Micro and Slide. |

Priority order: **MR Micro → MR Slide → MR Precision**. Reuse one control implementation and gesture/binding pipeline, differing only in rendering geometry, density and scale. Do not create three independent controls, audio algorithms, DSP parameters or app-wide changes in #192.

## 2. Visual constraints

- Follow [MR Unified Design System](MR_UNIFIED_DESIGN_SYSTEM.md): navy/graphite flat surfaces; fine cool light-blue outlines, restrained active accent, understated visually precise controls, no neon/gloss/textured chrome.
- **Volume fader ≠ signal meter**: separate signal metering from fader value; no moving backlight that suggests measured RMS/peak. Numeric dB value and clear **0 dB unity** mark, with legible min/max references if layout permits.
- Compact visible thumb with **larger effective mouse/focus target**; fader track/handle hit testing respects accepted #192 relative drag model. Thumb should read at normal, hover, active drag, automation/read/write (only if actually supported), disabled/focus, unity.
- Avoid too-bold Master difference; use density/scale/detail of same family. Color never communicates enabled/error state alone.
- Concept images are visual direction only, not exact pixel requirements; final dimensions, thumb form and stroke require screenshots/approval from **actual JUCE preview**.

## 3. Interaction & engine boundaries

- Existing #192 supports vertical/horizontal `MRFader` with mouse/keyboard gestures, Ctrl fine, double-click reset, localization and bounded linear/log/bipolar mapping. Reuse **as-is**, preserving undoable gesture lifecycle.
- Follow #194 host binding/automation acceptance. Automation graphics must reflect actual host state; no invented write/peak status.
- In the main DAW, new fader variants must not change mixer level/pan/routing, track selection, project save, host automation, fader law, dB taper or real DSP/metering.
- Do not expand product scope to a new Inspector feature without separate accepted workflow (#205 audit found no standalone Inspector). 
- SVG can be used for **reference** or static icons but actual scalable/interactable JUCE fader must be a shared painted control; avoid shipping separate bitmap skins.

## 4. Design handoff assets (two different purposes)

1. **Editable vector geometry reference:** a compact, original `MR_FADER_FAMILY_REFERENCE.svg` with three orthogonal variants and states (no copyrighted third-party UI screenshots), stored under `docs/design/` as documentation; **not authoritative runtime artwork**.
2. **Rendered UI preview:** screenshots of real Gallery/MR UI Controls showing MR Micro and MR Slide inside narrow mixer/track strips. Put generated local previews in the local validation output; document paths in #205. **Needed before implementation signoff** and to verify practical density/scale; concept artwork alone is insufficient.

After visual acceptance, reusable production SVG/icon assets (if needed) belong **locally in** `assets/ui/mr-uds/svg/controls/` and must be registered in `assets/ui/mr-uds/manifest.json` after auditing original kit generator paths. Do not assert GitHub stores the actual local SVG kit.

## 5. Implementation checkpoints

- [ ] Confirm latest local #192 reusable fader APIs, existing track/mixer fader sizing/gestures and #205 P0 findings. No duplicate controls.
- [ ] Gallery/preview-only experiments: Micro vertical, Slide horizontal, Precision large; state matrix normal/hover/drag/focus/disabled/unity, 100/125/150/200 DPI, min/regular/expanded track widths.
- [ ] User accepts actual fader appearance, especially matte thumb geometry and subtle glow.
- [ ] #205 P2: implement MR Slide on Track Headers within existing header workflow, preserving commands and automation.
- [ ] #205 P3: implement MR Micro on Mixer/Bus, Precision as optional expanded/large variant; no separate Master DSP implementation.
- [ ] Test unit values, gesture/undo/automation, focus, keyboard, resize and accepted UI regressions. Record PASS/FAIL/NOT TESTED, including physical DPI.
- [ ] User signs off on each scope; #192 completion remains unchanged.

**Workflow:** #205 is ongoing; this spec is a visual decision, not permission to run P2/P3 early or reorder its active acceptance gates.
