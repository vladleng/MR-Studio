# MR Rig — Native GUI / UX Contract

## ACTIVE UX CONTRACT — 2026-10-17: NAM-first ([#226](https://github.com/vladleng/MR-Studio/issues/226))

**Overrides older IR-combo gallery-first layout descriptions below for future GUI development.** MR Rig remains compact, flat and modular under MR UDS/UI Kit, with one editor and **PRE FX → NAM Amp → Cabinet IR (optional) → POST FX**. Existing GUI from accepted #199/#200/#201 is historical as-built, not a mandate to keep Full Combo WAV as the primary amp browser.

- **Primary browser:** NAM amplifiers / capture variants (tags, clean/jazz/rock, favourites, licensed cover art, source status) and **My Rigs**. Import NAM as secondary workflow. Do **not** continue building a first-class `IR Models / Full Combo WAV` amp gallery.
- **Central card:** named NAM amp/full-rig capture with input/output trim, status/loading/errors and clear `Full Rig / Amp Only / Unknown` role. No imaginary analog amp controls: captures have fixed original hardware settings; level before NAM changes drive.
- **Cabinet slot:** one minimal tile **after** NAM: `Cabinet: Included in NAM` (Full Rig; no external DSP), `Cabinet: Select IR / Bypass` (Amp Only); Unknown shows manual type selection before auto Cab. Cabinet mini-browser only for actual `cabinet_only` WAV; variant and safe external source relink; no advanced mult-mic/room library backlog.
- **Workflow:** choose NAM → (if Amp Only) choose optional Cab IR → Pre/Post pedals → save My Rig; accurately restore NAM ID, capture class, optional Cab variant, module state and image. Toggle/bypass must match audio DSP with no invisible stacked convolver.
- **No unapproved redesign** of accepted standalone components or other MR Studio GUI. Scope is [#226](https://github.com/vladleng/MR-Studio/issues/226), after [#224](https://github.com/vladleng/MR-Studio/issues/224), separate user sign-off. Prior GUI plans below = archive/as-built details where they conflict.

---


> **New approved visual direction (2026-10-15):** [MR Studio Unified Design System](MR_UNIFIED_DESIGN_SYSTEM.md). MR Rig illustrations should be original thin pale-blue contour art on dark blue-graphite, sharing controls via #189 when ready; keep existing PRE FX / Rig Core / POST FX hierarchy. Existing implemented accepted UI stays untouched until scoped GUI work and user signoff. **Do not change actual DSP/RigPreset in a visual-only redesign.**


**Status:** #200 accepted/closed; #201 implemented locally / awaiting acceptance; later DSP/NAM planned, 2026-10-15; [#197](https://github.com/vladleng/MR-Studio/issues/197), [canonical architecture](MR_RIG_ARCHITECTURE.md), [model library](MR_RIG_LIBRARY_PRESETS.md). Uses current **accepted MR Studio JUCE `ui::Theme`**, compact flat graphite; align with [MR UI Kit + Gallery #189](https://github.com/vladleng/MR-Studio/issues/189) once components approved, don't block GUI on Kit's unfinished status.

**Local #200 / 0.4d:** compact model gallery, PRE/Core/POST shell with empty side lanes, real IO/wet controls, model IR selector, Save IR and My Rigs are implemented. Expand library retains full curation and Manage files is secondary. New preset load/Undo/close/reopen retain native editor ownership/geometry. Initial bounds fit the owner's display work area; minimum logical window940x640. Software100/125/150/200 snapshots are separate from untested physical mixed-DPI/ASIO. [Delivery and exact boundaries](MR_RIG_SHELL_200.md). No active pedals or neural controls before their stages.


**Current #201 / 0.4e:** PRE/POST now contain real ordered utility modules. Add Pedal offers Gain/HP/LP/Bell EQ; other pedal families are disabled. Selecting a tile shows its parameter inspector, module bypass, Up/Down, Move across Core and Remove; context menu adds Duplicate. Core controls returns to the IR/Rig inspector. The same native window and accepted Theme remain in use. #200 is accepted; #201 awaits user acceptance. [Implementation, screenshots and checks](MR_RIG_CHAINS_201.md).

## 1. Main window: hierarchy, not a replica of rack plugins

```text
┌ MR Rig ────────────────────────────────────────────────────────────────┐
│ [IR Models] [NAM Models] [My Rigs]   Search... [Genre] [★] [Import ⋯] │
│ ┌────────────────── Library cards with artwork ─────────────────────┐ │
│ │ [Polytone*] [Fender*] [Hempback cabinet*] ... (*examples, not stock) │ │
│ └───────────────────────────────────────────────────────────────────┘ │
│ Active: Warm Jazz       [★ Favorite] [Save Rig] [Bypass]              │
│ ┌ PRE FX ─────────┐ ┌──────── RIG CORE ───────────┐ ┌ POST FX ──────┐│
│ │ [Compressor]    │ │  [picture or placeholder]   │ │ [Chorus]      ││
│ │ [Drive]         │ │  MODEL / variant / status   │ │ [Reverb]      ││
│ │ [+ Add Pedal]   │ │  IR / NAM / full rig badge   │ │ [+ Add Pedal] ││
│ └─────────────────┘ └─────────────────────────────┘ └────────────────┘│
│ ┌ Selected module inspector (contextual actual params) ────────────┐ │
│ │ knobs/faders/toggles/value entry | source info | advanced ...     │ │
│ └───────────────────────────────────────────────────────────────────┘ │
│ IN peak/gain                                                OUT peak │
└───────────────────────────────────────────────────────────────────────┘
```

`*` examples are **not** assertions that a given user's library contains those WAV/NAM or that commercial amplifier photos are licensed. Display actual available imported models.

## 2. Layout and sizing

- On desktop: central Rig Core visual card receives priority; PRE/POST chains are **compact lanes** on left/right, not wall-sized hardware stompboxes. Responsive resize: hide detailed labels or scroll chains at minimum window size rather than truncate values. Cards use bounded aspect-ratio thumbnails, image fallback, UI primary focus on source state.
- Adjustable module selection → inline/bottom inspector only, not many independent pedal windows. Knobs standardized via future UI Kit; preserve current native Undo/gesture semantics.
- GUI behaves as a single editor/window with plugin editor owner/Space bridge as in accepted #185 and #196 local follow-ups. Switching model or preset must not create/spawn a second editor window, steal focus or reset browser tab/geometry; no raw file-dialog modal on first open.
- Import own WAV/NAM is **secondary action**, accessible top-right and in library context menu. Basic workflow: select card, play, tweak, save rig.
- PRE/POST each has `+ Add Pedal`, tile bypass, optional drag handle, right-click menu Duplicate/Remove/Move where valid; reorder feedback visible. Selected tile inspector shows parameter values, units and confidence/status. Copy/paste module settings optional future.
- Header: Preset title editable, Unsaved (*) indicator, Undo/Redo, save, reference image (if licensed), **role badge** for Cabinet IR / Full Combo IR / NAM Full Rig / NAM Amp Only / Unknown. Global Rig bypass vs individual module bypass visually distinguishable.

## 3. Library tabs and navigation

- `IR Models`: curated **WAV** model cards; category `Full Combo` vs `Cabinet-only` and Unknown. Optional sources hidden behind advanced “Import/Manage Files”; existing #185 user decision: filter menus **only filter list**, not auto select an IR.
- `NAM Models`: curated/neural cards. Before NAM backend ready, tab is explicitly **Unavailable / Planned** and cannot claim processing/sound. If NAM binary missing → Missing with relink options, not an animated fake amp.
- `My Rigs`: whole-chain saved presets; a rig may reference either IR model or NAM when supported. Order/favorites/search/genre/tags, no duplicate audio media copied to save file. Preset thumbnail may override base model thumbnail.
- Live preview is actual selection and loading into the host **only after user selects** card; browse/filter/hover should not silently switch currently sounding model. Async Ready/Loading/Error/Missing shown. Stop/menu and audio change controlled by existing project command and undo.

## 4. Model details and source provenance

Model card: display name, type, maker/model, variant, source/capture notes when verified, installed/missing/not supported, optional thumbnail, favorites. WAV Full Combo cannot show gain/treble/bass knobs pretending to control amp modeling; `Bright`/`Clean` label is a distinct static source variant if file exists. Unknown capture role stays visible and user-editable; NAM amp-only must not silently add extra cab if asset missing. Cabinet-only card warns **"Requires amp processing for full guitar sound"**; do not secretly synthesize one.

Thumbnails: user-picked original/legally permitted image (local path / managed cache if allowed), safe relative path, validation size/type/decoding, missing fallback. Do not auto scrape product photos or ship them without rights. Future generated branded art optional by separate request. Can be placeholder with gradients/shapes/icons until real licensed art exists.

## 5. States/accessibility

Explicit Normal/Hover/Pressed/Selected/Focus/Disabled/Error/Loading/Ready/Missing. Keyboard nav Tab/Enter/Space context (DAW transport Space remains in non-text contexts), accessible text labels for icons, zoom 100/125/150/200% + physical monitor DPI test separately. Avoid permanent neon, unnecessary skeuomorphic images, photoreal control panels; **photographic model cards**, not photoreal knob skins. Level meters real DSP snapshots only or explicitly DEMO in Gallery. Confirmation for unsaved preset before destructive overwrite or global rename.

## 6. Initial MVP gates

A. Library card → exact existing WAV/Full Combo as current #185/#196 convolver, non-destructive source switch; model label/thumbnail/favorite.
B. Save/reopen RigPreset with no pedals yet, then include ordered pedals when real DSP is ready.
C. PRE/POST empty and plus slots visible only if feature supported; no fake active pedals.
D. Neural tab shown as pending until tested NAM runtime in separate stage.
E. Gallery visual approval independent from production shell acceptance.

GUI does not own live audio graph; all edits via host/model commands, scoped UI preview only for transient knob gestures; shutdown/editor rebuild cannot drop DSP or reset state.
