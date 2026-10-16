# MR Studio — SVG pack runtime adoption

2026-10-16: the user approved the Fader/Control/Track/Utility artwork and explicitly requested integration into the working interface. This is a bounded adoption slice across #205 / #208, not completion or acceptance of all #205 P2/P3/P4. Prior #205 P1 acceptance remains separately recorded. Native/editor rollout #206 and Rig work are not started by this change.

## Entry and scope

Normal application startup enables the new assets and the existing UDS shell. This is the working DAW: existing project commands, input/device settings and recording remain available. `--legacy-ui` retains the former default appearance; `--legacy-ui --uds-shell` retains the earlier shell-only candidate. Flags are process-local, not project or preference fields.

`--uds-assets-preview` opens the actual Desktop with its existing private demo/offline device and testing preference isolation. This is a viewing convenience, not the normal launcher. `--uds-assets-smoke` runs the offline regression fixture.

## Rendering and ownership

The existing final `ui::Theme` owns a `ConceptSkin` drawable cache, populated once at activation from embedded canonical SVGs. Only toolbar actions, workspace buttons, Strip children and selected Browser action buttons borrow this Theme instance. Desktop.theme, native and third-party editors, menus, Home and Browser lists retain their accepted consumers. No second LookAndFeel, global palette mutation, loose asset directory or SVG parsing during paint is introduced. Borrowers detach before cache/Theme destruction, including on repeated activation.

Runtime SVGs are channel M/S/record-arm states, track-type glyphs and action icons. Track icons reflect actual audio / MIDI or instrument / bus / master types; named guitar/drum/vocal assignments are not inferred from track names. Mixer names keep their existing width; type icons sit beside the gain value. Arrange mini-strip names have a dedicated icon inset. Existing transport SVGs are retained. Buttons keep their original callbacks, titles, focus parentage and tooltip meaning.

Selected action icons use cached dark-ink copies over the light selected surface. Mini-strip meter/value/input bounds do not overlap. While asset mode is active, existing UI refresh also projects transient host Arm state to both Arrange and Mixer buttons: disarming may not increment project revision, so relying only on revision refresh left the other button stale. This changes UI projection only, not arming/monitoring policy or project history.

User refinement (2026-10-16): M/S/Arm/Monitor are 20×20 squares, centred as a two-column Mixer group and aligned as a compact Arrange row. Horizontal balance paints a white segment from centre to current value; rotary pan uses the equivalent white arc. Pointer focus adds no rectangular frame to a Handle; keyboard focus remains visible. Gain text/type icons have no numeric capsule border. Transport icons have no individual or enclosing group border in asset mode; active/pressed states retain their highlight. The navigation selector remains a selector, with its existing affordance. Fader/pan gestures, value ranges and commands are unchanged; button targets follow their resized bounds.

Gain faders use shared painted Micro/Slide geometry with real normalized values and the old -60..+12 dB mapping (unity at 60/72). The shared Kit MRFader delegates to the same painter under this scoped Theme; current DAW Strip Handle keeps its established interaction, preview/commit/cancel and accessibility adapter. This slice does not replace its host binding with a new control or add numeric editing/reset/wheel behavior. Numeric dB text stays supplied by the host. Mini-strip pan uses dynamic rotary geometry; Mixer pan retains its existing compact horizontal interaction.

LED segments consume the existing `peaks` / `masterPeak` data after the unchanged host level conversion and smoothing. Fader value is distinct from signal level. Static sample meters, decorative waveform SVGs, automation states and unused future icons are not presented as live functionality. Actual Arrange/audio preview waveforms remain drawn from existing audio data. No audio callback, routing, DSP, meter collection cadence, recording format, project schema or Undo policy changes.

## Validation

The `juce_svg_runtime` fixture uses the real Desktop with no physical audio device or user preferences. It covers activation/isolation, Mixer + Arrange + Files at 1200×700 / 1400×850 / 1800×1000 and 100/125/150/200%, actual gain drag with one history commit, Undo/Redo, fine-drag cancellation, arrow keys, M/S/arm callbacks, Arrange gain, project replacement, save/open and repeated Theme detach/attach. Injected meter values in one screenshot are explicitly fixture data.

The existing Kit/Gallery/native/host/shell checks and full CTest remain required for the delivery candidate. Test outputs, screenshots, preservation checks and package smoke remain local under `build/asio-local/validation/svg-runtime/`. Physical ASIO/listening/xruns/hardware mixed-DPI/screen reader are not established by software screenshots; earlier user ASIO acceptance remains valid for its tested build.

Source, assets, builds and tests stay local. GitHub receives documentation/issue status only, without source push, Actions or PRs.

