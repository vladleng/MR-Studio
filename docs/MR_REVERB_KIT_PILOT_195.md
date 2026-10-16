# MR Reverb Kit pilot #195

User-selected pilot: a separate opt-in MR Reverb editor. #194 was accepted by the user on 2026-10-16. #195 and parent #189 await separate final acceptance.

## Entry and isolation

`--reverb-kit` enables the pilot for Reverb editors in a normal Studio session. The flag is transient; no settings or project migration is introduced. Without the flag the accepted editor is unchanged. It does not opt other native or external VST3 editors into new styling.

`--reverb-kit-preview` opens the same real Desktop/native editor in a private testing session: no persisted preferences, recent projects, device reconnect or user project load. Its synthetic IR is original test data generated in a unique temporary folder. Audio uses the existing offline clock, with no physical output. Close the preview to leave it; ordinary Studio operation is available through the existing launcher.

## Composition and ownership

The existing FxPanel/native EditorWindow, preset selector, bypass/save/load/remove toolbar, IR browser, Options/gate and native Space bridge are preserved. The opt-in parameter area composes the exact MRKnob, MRButton, Theme and MRControlBinding classes shared with Gallery and the accepted binding stage. It has no replacement knob renderer, DSP, IR loader, automation scheduler or engine.

Pre-delay, Tone, Mix and optional Threshold use actual prepared native metadata and physical units. The wet-input gate remains the existing model command; threshold is disabled when gate is off. The existing finer Reverb Mix travel is preserved independently from physical percent values. Structural/IR/preset/bypass operations retain their existing host policy.

A Desktop owns one lifetime-checked shared parameter host, outliving its editor controls and preceding Application teardown. Bindings cancel on close and retire before controls/LookAndFeel. Timers and asynchronous chooser callbacks remain on the UI thread, use safe ownership and never enter audio processing. Compatible preset changes keep the native window, geometry and controls. The existing native owner/Space hook is installed after final peer configuration. No global Always On Top is introduced.

Default pilot size1040x720; minimum900x650; maximum1500x1000 logical pixels. The taller controls area keeps the full browser and toolbar usable. Accepted Reverb retains its existing sizes/layout.

## Explicit Gallery transfer

Gallery and pilot share GalleryDraft schema validation in GalleryProfile.h (same v1 schema, whitelist, bounds and old token-profile compatibility). Gallery is still a project-free sandbox. Export a draft in Gallery, then explicitly choose Import Gallery in the pilot. Only validated DesignTokens are applied to this editor. Simulated values, selection/layout/zoom/state and audio parameters are never imported.

Import/reset cancel an active preview before changing the scoped Theme. Failed validation preserves the previous profile. Neither style operation creates a project Undo command or changes the model, DSP, Desktop Theme or other editors. Reset Kit restores approved MR UDS defaults. Imports are session-local to the current editor and survive compatible presets, but are not persisted on close; reopened editors use approved defaults.

## Validation boundaries

Local CTest juce_reverb_kit exercises real IR selection, native control/physical units/keyboard/wheel/preview/one Undo/live history, compatible preset A/B/A/bypass/geometry, project round-trip, Gallery profile validation/isolation, repeated teardown/reopen midgesture, accepted editor opt-out and Space text focus. Software snapshots cover min/nominal/max pilot bounds and100/125/150/200 percent rendering; comparison covers accepted DAW surfaces and old Reverb/Kit/Gallery. UI refresh and snapshot+PNG timings are diagnostics, not audio callback metrics.

Full current CTest is required for the final candidate. No physical ASIO/listening/xrun, monitor-transfer DPI, screen-reader or leak-tool result is claimed without running that check. The audio callback/processing source is not changed. Previous user ASIO acceptance is retained. No source push, Actions or PR; local builds/packages/saves remain local. Product adoption #205/#206 and Rig work do not start automatically.
