# MR Studio Unified Design System (MR UDS)

**Approved visual direction: 2026-10-15. Implementation and GUI migration are NOT yet complete.** This standard applies to **the entire MR Studio desktop DAW, native plugins, app controls, browsers, inspectors, dialogs, and integrated Live Mode**. New icons, SVG illustrations and controls must extend it.

## 1. Authority and relationships

- **THIS DOCUMENT** is the canonical visual language: flat graphite/navy surfaces, light cool outlines, restrained status accents, compact premium instrument-panel design and original line-art.
- [MR UI Kit](MR_UI_KIT.md) / [#189](https://github.com/vladleng/MR-Studio/issues/189) (stages #190–#195) own reusable JUCE controls, theme integration, Gallery, parameter binding, automation and acceptance. **Do not create a parallel implementation/theme.**
- [MR Rig GUI](MR_RIG_GUI.md), [MR Reverb](MR_REVERB.md), and each plugin spec own screen hierarchy and functional behavior. [Live UI](UI_UX_CONCEPT.md) owns live performance information hierarchy.
- Local JUCE ui::Theme/Controls.h, [UI_REFERENCE_UPD9](UI_REFERENCE_UPD9.md), user-accepted issues #145/#146 are the **as-built baseline** until a scoped new implementation has been reviewed and accepted. Draft colors in this document MUST NOT silently repaint accepted screens.
- [AGENTS.md](../AGENTS.md) and [mr-unified-gui skill](../.codex/skills/mr-unified-gui/SKILL.md) govern work; real local code may be ahead of GitHub docs.
- Inspiration was a screenshot of Synth Anthology 4, **not a license to copy** another product's interface, logo or artwork. All new SVG elements must be original.

**Conflicts:** current explicit user decision > accepted issue scope > this visual language for style > module specifications for behavior > legacy visual samples. Report conflicting design or existing accepted colors instead of guessing.

## 2. Style principles

1. Deep **blue-graphite / dark navy flat panels** with subtle separators and measured corner radii, no skeuomorphic hardware surfaces.
2. **Fine light blue-gray contour artwork on dark background** for amp/cabinet/microphone/pedal, reverb units and acoustic spaces. Characteristic shapes and small technical details, never photorealistic default art.
3. **Compact modular UI**. Small rotary controls, thin faders, segmented tabs, readable values/units, simple power/bypass indicators; real interaction zones bigger than drawn control.
4. Bright pale blue for selected/focus, green for ready/On, red for error/record, amber for warnings and Live cues; never use red or green purely decoratively. Non-color shape/label indicators always available.
5. Restrained animations only for feedback; no bright neon glow, glass, 3D bevel, gratuitous gradients, heavy shadows, giant decorative waveform or unnecessary ornamental hardware panels.
6. **Unified visual identity, differentiated workflow**: MR Rig, MR Reverb, MR Strip, Harma Waves, Arrange, Mixer, Browser and Live Mode share controls/tokens/icons but not an identical fixed layout.
7. Fonts: neutral installed/fallback sans-serif, readable values, stable digit alignment; respect existing accepted menu/mixer font exceptions until explicitly migrated.

## 3. Reference visual tokens

Palette adopted from **MR Plugin GUI SVG Kit 01** as a **target for future MR UI Kit theme mapping**. It is NOT confirmation that the running UI already uses these values.

| Token | Hex | Role |
| --- | --- | --- |
| canvas | #111720 | root background |
| window | #171E28 | editor / DAW window |
| surface | #222C3C | panels / cards |
| raised | #2D394E | selected card / header |
| deep | #192333 | embedded module surface |
| bright | #DAE5F4 | main text / bright outline |
| stroke | #A9BFDC | hardware line-art |
| muted | #7188AB | secondary text / strokes |
| subtle | #4B5F7B | dividers / grids |
| accent | #91BBEE | selection, focus, important value |
| green | #7AB69E | active / ready |
| red | #E68F88 | error / record |
| amber | #E2C070 | warning / cue |
| violet | #AB92DD | optional musical accent |

Existing accepted colors such as #212830 (tree), #2D353E (card), #2F3841 (button), and #262E37 (Arrange) remain authoritative **in the existing implementation** until their specific migration is approved. Integrate via **semantic roles in a single existing JUCE theme**, not hardcoded per-view colors. Validate contrast and focus, not just swatch aesthetic.

**Metrics:** 4-logical-px spacing unit, 4/8/12/16 px gaps; conceptual larger panel radius 12–15 px; major illustration strokes about 1.1–1.6 (320×200 SVG), icon strokes 1.5–1.8 (24×24 SVG), dividers about 1 px. Plugin concept header ~48 px, NOT a required height of all DAW headers. Compact visuals must keep readable text and keyboard/pointing hit targets; a 32 px effective target is only an initial compact concept, increase where needed and for Live.

## 4. Asset catalog and extension

- **Core application:** transport, tracks/mixer/automation, routing, sends, browser, inspector, menu, folder, drag/reorder, clip statuses, notifications, meters and dialogs.
- **Control family:** knob mini/standard/large, fader H/V, toggle, A/B, power/bypass, segmented/tab, selector, numeric value with units, peak/RMS/GR meter, EQ/dynamics curves and focus/error/loading/missing states.
- **MR Rig:** original outline combo amp, head, cabinets, tube, dynamic/ribbon mic, pedal families, rack. Large central visual is an *illustration*, not a copied commercial amplifier face. Preserve PRE FX → Rig Core → POST FX and only real implemented DSP controls. Static Full Combo WAV is not a dynamic amp.
- **MR Reverb:** spring/plate/rack, room/chamber/hall/cathedral and reflection/IR illustration; distinguish diagram art from real measured IR data. Preserve accepted actual UI/parameters and DSP.
- **Other native modules:** MR Strip, MR Spatial, MR Saturator, Harma Waves use the same design grammar with feature-specific diagrams.
- **Live Mode:** same palette/icons but larger stage-distance readability, lower density, cue/ready/error priority; moving Chord Track and immediate transport remain primary.
- **Third-party VST3-owned GUIs are excluded**; only MR host chrome and our generic parameter editor may be restyled.

**SVG rule:** Original vector paths, viewBox, transparent background, descriptive stable snake_case name, no scripts/foreignObject, external URLs, bitmap embeds, proprietary branding or redistributed fonts. Icons primarily 24×24 with compact 16/20 and 32 variants as needed; equipment illustrations 320×200, controls art examples 160×140. For very small sizes draw reduced-detail LOD; do not merely squash long fine-line drawings.

**New asset process:** search and reuse existing icon → define semantic meaning + on/selected/disabled/focus variant → draw matching line weight/colors/radii → register in catalog with source and license note → preview in Gallery at 100/125/150/200% and small size → compare to baseline → user acceptance. Never create near-duplicate graphic assets for the same meaning or silently alter global palette.

## 5. Semantic states, real behavior

Normal, hover, pressed, focused, selected, on, off, disabled, recording/armed, loading, ready, missing, warning and error must be distinguishable. Selected ≠ enabled ≠ actively processing; bypass ≠ mute/solo. Numeric editing supports units/default/range; controls reuse actual host gesture/automation/Undo semantics, one undoable commit per gesture. UI painting on UI thread; no SVG rasterization, allocation, locks, I/O or UI calls in audio callback. Avoid fake meters and fake unimplemented pedals.

## 6. Delivery roadmap and acceptance

**Infrastructure**: #189 with #190 audit → #191 tokens/LookAndFeel → #192 shared controls → #193 Gallery → #194 parameter binding → #195 pilot/QA. Do not claim any stage implemented merely by writing this design spec.

**DAW rollout:** separate stages for app shell/navigation → Arrange/Track headers → Mixer/routing → Browser/Inspector/dialogs → Live performance. User approves each visually; preserve edits/commands/host focus/presets/recording.

**Native plugins:** MR Rig may use this visual direction during future #200 GUI work, without blocking its feature roadmap on unfinished Kit. MR Reverb #128 is **closed and accepted** and gets a new separate *GUI-only* task for optional modernization; do not reopen its DSP. Other future modules adopt from the beginning, no forced batch migration. Native plugin adoption does not change module DSP or project persistence.

**Validation per screen:** before/after screenshots, component and state inventory, mouse/keyboard/Space navigation, real controls and value/Undo binding, min/nominal/max sizes, logical DPI 100/125/150/200%, physical DPI separately (NOT TESTED unless checked), focus and accessibility, native editor repeated open/close/preset behavior, no audio callback/ASIO regressions. Screenshot concept ≠ working GUI; visual approval ≠ automatic acceptance of runtime code.

## 7. Provenance

The original 2026-10-15 **MR Plugin GUI SVG Kit 01** contains **65 independent conceptual SVGs** (23 controls, 12 Rig, 11 Reverb, 19 general icons) plus two concept screens. The ZIP was supplied **in conversation** and is **not committed to GitHub**; do not claim it is available in a new Codex session without checking the local workspace. The vector visuals describe styling, not complete interactive JUCE widgets. Supporting repository-only reference: [MR UDS swatch JSON](design/MR_UNIFIED_TOKENS.json). See also the skill and the shared UI Kit.

**Documentation and issues on GitHub only with [skip ci]; source/assets, builds, tests and runtime GUI previews locally unless user explicitly changes publishing policy. Do not begin migration without separate user instruction.**

## 8. Canonical local SVG library and kit distribution

**Required local layout:** `assets/ui/mr-uds/` is the **single canonical asset library** for future MR Studio GUI illustration/icons (separate from existing `assets/ui/icons/transport/`, which must be audited/reused, not moved blindly).

```text
assets/ui/mr-uds/
├── svg/
│   ├── controls/   # SVG control visual sources
│   ├── icons/      # additional MR UDS application/plugin icons
│   ├── rig/        # amps/cabs/mics/pedals line-art
│   └── reverb/     # rooms/plates/racks/space line-art
├── previews/       # GUI previews and asset contact sheets
├── tokens/         # kit's original design token files
├── build_kit.py    # kit generator (inspect before running)
├── manifest.json   # inventory/asset metadata
└── README.md       # kit usage, generator instructions
```

**Placement:** unzip kit, copy the original `svg/` directory with its four child folders, `previews/`, and all four root items (`tokens/`, `build_kit.py`, `manifest.json`, `README.md`) into `assets/ui/mr-uds/`. Keep generator and manifest next to their referenced folders to avoid broken relative paths. `docs/design/MR_UNIFIED_TOKENS.json` remains a **documented draft design reference**, not a replacement for the kit's local `tokens/` source. Do not conflate the two until an audited explicit sync process exists.

**Agent behavior:** first inspect the local path and `manifest.json` (paths such as `svg/controls/knob_arc.svg`), reuse assets, and when adding a new icon update the manifest and generator source if needed; validate whether `build_kit.py` regenerates or overwrites SVGs **before executing**. Running generator is opt-in after audit. Avoid copying generated assets into `.codex/skills/` or `docs/`. The remote GitHub may only contain the documentation, not this local library; do not claim otherwise.


**Verified source archive (2026-10-15):** `manifest.json` uses `svg/controls/...`, `svg/rig/...`, `svg/reverb/...`, and `svg/icons/...`. `build_kit.py` resolves `ROOT = Path(__file__).resolve().parent` and emits `ROOT / 'svg' / category`. Thus the *extra `svg/` level is REQUIRED* when copying the unmodified kit. If files were previously flattened into `mr-uds/controls` etc., move them back under `mr-uds/svg/` rather than keeping two competing copies. Keep original `previews/`, `tokens/`, `manifest.json`, and script next to `svg/`.
