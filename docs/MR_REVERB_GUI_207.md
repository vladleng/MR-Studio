# MR Reverb visual modernization #207

Authorized 2026-10-16 after native GUI inventory #206. This extends the accepted
#195 opt-in composition; it does not reopen accepted Reverb DSP #128/#131.

## Entry and presentation

Normal sessions retain the accepted Reverb editor. Launch with `--reverb-kit`
to use the modernized editor. `--reverb-kit-preview` remains an isolated offline
Desktop with generated IR fixtures, no normal preferences/project/device reconnect.
The opt-in window is identified as `MR Reverb / UDS Preview #207`.

Default1040x720, min900x650, max1500x1000 logical pixels retained. Existing IR
browser occupies the upper area. The lower parameter area has original line art
at the left and shared MRKnob controls at the right; Options reveals the existing
Input Gate and Threshold. No Decay/Size or other nonexistent DSP controls added.
Existing Gallery import/reset actions remain available, not audio parameters.

## Reuse / ownership

- Canonical original asset `reverb/plate_unit`, path
  `assets/ui/mr-uds/svg/reverb/plate_unit.svg`, already present in manifest.
  It illustrates a reverberation device, not the currently loaded IR type or an
  actual waveform. No copied device branding, raster art or external asset.
- Separate compiled `mrs_reverb_art` / MRReverbArt resource embeds that SVG;
  no generator run or duplicate asset. Only the app links it. CMake regenerates
  the current project because a resource target was added; no SDK replacement.
- FxPanel owns an instance of the existing Theme for this opt-in Reverb only.
  Its toolbar, browser/search and selected IR/folder rows share the DAW palette;
  selected pale rows use dark text. Desktop/default/vendor themes stay separate.
- ReverbKitControls continues using the same MRKnob/MRButton/MRControlBinding,
  physical units, metadata, Mix mapping, preview/cancel and Undo. Theme borrowers
  detach before their owner disappears. Space bridge, presets/IR loading,
  state schemas and worker/DSP/callback code are unchanged.

## Verification

Capture old pilot first using existing juce_reverb_kit. Build only the app target
in the existing Release tree; run the existing Reverb browser/editor/Kit tests.
Kit fixtures cover software100/125/150/200 at min/default/max bounds, Options,
real IR loading, thresholds, gestures/Undo, presets/project round-trip, profile
import isolation, Space and repeated retirement/reopen. Compare fresh old/new
screenshots. Exact outcomes are recorded in #207 and local validation/ui207.
No full CTest/package/shortcut changes by default. Physical ASIO/listening/xruns,
hardware mixed-DPI and screen reader remain NOT TESTED.

Visual acceptance pending. No default switch, other native migrations or next
Rig stage starts automatically. Code/assets/builds/images remain local.
