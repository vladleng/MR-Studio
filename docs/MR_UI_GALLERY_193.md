# MR UI Gallery #193 — implementation contract

Scope: design Gallery using the accepted #191 Theme/tokens and #192 reusable
controls. User visual acceptance pending. Host binding #194 and pilot #195 are
separate stages. The Gallery does not apply themes or parameters to products.

## Entry and ownership

- Standalone `MRUIGallery` / MR UI Gallery 0.3: GUI-only target linked through
  `mrs_ui_controls`, without audio/desktop/processing/ASIO dependencies.
- Explicit MR Studio developer entry: `Moon River Studio JUCE.exe --ui-gallery`.
  This branch returns before diagnostics/normal Window/Desktop/audio setup.
  `--ui-gallery-smoke` verifies this entry using the same Gallery implementation.
  Normal Studio startup and the accepted runtime shortcuts remain unchanged.
- `Gallery.cpp` implements both entries. `Gallery.h` exposes the window factory;
  `GalleryMain.cpp` is only the standalone JUCE application wrapper.
- Each window owns its chrome, preview and accepted-reference Theme instances.
  No global LookAndFeel or product settings are changed. The reference is not
  mouse/keyboard editable and follows the preview's scratch parameter value.
  Destruction detaches viewports, cancels controls, destroys tooltip/chooser and
  uses safe component pointers for asynchronous file/dialog callbacks.

## Design workflow

The left keyboard-accessible navigator selects knobs, faders, buttons, switches,
selectors, unit fields, scroll panels or explicit meter/graph extension markers.
All rendered widgets are the actual `ui::kit` classes, not painted mock widgets.
Available variants are finite: three knob sizes, horizontal/vertical/centred
bipolar faders, text/icon/compact buttons, dropdown/segmented selectors. A disabled
variant inspector indicates that only one form is supported for that category.
No decorative alternative knob skin or unsupported processing is invented.

The central scrollable comparison supports compact/normal/wide canvas widths,
100/125/150/200 logical zoom, optional accepted reference and actual hit/component
bounds. Bounded control scale 1.0..1.5 and padding 4..32 change layout. Token density
changes spacing without shrinking native hit targets; large controls expand the
scrollable sample rather than clipping value units. Minimum/nominal/wide root
windows retain independent preview and inspector scrolling.

The inspector edits the existing 12 semantic RGB roles and 10 whitelisted metrics:
control/compact/mixer-label/menu typography, font width, density, radius, stroke,
icon size, gap. The established Theme owns drawing. Gallery applies the control
font to editable unit fields and `MRIconButton::setIconSize` supplies an optional
bounded 12..32 logical-px SVG size; its default preserves the #192 renderer.
No new assets, global Theme rewrite or arbitrary fonts are introduced.

Real input keeps JUCE mouse/keyboard/Tab/focus behavior and the #192 numeric,
fine/reset/cancel contract. Footer B/P/C/X/A are begin/preview/commit/cancel/action
counts on scratch values. The explicit Simulate gesture button uses that same
sandbox API and labels its result SIMULATED. Sandbox values/counters are transient,
not project state or preset metadata.

Forced hover/press/disabled/focus/selected/warning/error states are labelled
SIMULATED. Native buttons get the corresponding visual state without invoking
callbacks. Fields demonstrate actual invalid text for Error. Generic diagnostic
outlines do not claim that a knob acquired a new hover/pressed skin, a real OS
focus, an engine error or a project selection. Warning/error include text cues.
MeterSource/GraphSource are explicitly DEFERRED; no numerical DSP meter/graph,
continuous animation or audio callback is created for the Gallery.

## Draft format and persistence

Gallery JSON schema1 has exactly these fields: galleryVersion, kitBase, tokens,
kind, variant, layout, zoom, state, compare, hit, size, padding. `kitBase` is
accepted-0.4f; `tokens` is the strict #191 schema1. Integer/boolean/numeric types,
finite ranges, category-specific variants and all token roles/metrics are validated.
Maximum input is 32KB; there are no executable, font, resource or file-path fields.
Unknown fields/versions/base names and invalid token data are rejected, without
modifying the current design. Existing #191 token-only schema1 drafts are accepted
through their original strict validator and migrated to Gallery defaults plus
those tokens. A subsequent save writes the new Gallery envelope.

Save Draft atomically writes an explicitly selected local metadata file and marks
it saved. Export writes the same validated metadata without clearing the current
unsaved status. Open/Import validates before applying; dirty designs require an
explicit discard/open choice. Closing dirty windows offers Discard/Keep editing.
No automatic preference writes or global Apply action exist. Reset accepted
restores the accepted tokens and default Gallery selections; it resets design,
not scratch demo parameters or a plugin preset. Token diff and dirty state are
visible; selections/geometry are included in dirty detection and draft persistence.

## Verification

`juce_ui_gallery`: standalone catalog, strict roundtrip/invalid input/migration,
atomic save/reopen/export, reset/diff/dirty, per-window/reference/global isolation,
actual MR control gesture/cancel, cancellation on design rebuild, native focus/Tab,
tooltip, read-only reference, states without commits, transformed hit tests and
scroll extents at 100/125/150/200, snapshots and repeated lifetime teardown.
`juce_ui_gallery_entry` exercises the same workload through Studio's early entry.
Screenshots and logs remain local in `build/asio-local/validation/ui193/`.

Physical mixed-monitor DPI transitions and screen-reader operation are NOT TESTED
by software snapshots/native-peer event checks. The visual design, file-dialog
workflow and keyboard feel require user review of the packaged Gallery. No new
physical ASIO/latency/xrun measurement is claimed for this GUI-only stage.

