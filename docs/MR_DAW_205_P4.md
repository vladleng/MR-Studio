# MR Studio #205 P4 — Browser, preset menus and dialogs

Authorized 2026-10-16 after explicit P3 acceptance.

## Scope

- Reuse the existing scoped Theme for Browser search/path editors, quick folders,
  VST3/Files lists, AI tab, scan action and breadcrumbs. Preserve frameless icon
  tabs and framed text actions. Selected rows and text use the shared pale focus
  color with dark readable text. Browser background remains the accepted transport
  window color. Existing filtering, navigation, drag/drop, scanning and file
  preview workers remain unchanged.
- Preset menus own a shared instance of the existing Theme, retained by the
  asynchronous popup callback. Scanning, folders, preset persistence and retirement
  guards are preserved. Legacy mode retains the previous preset-menu Theme.
- MR-owned text input, unsaved-project confirmation and replace-instrument
  confirmation use AlertWindow with an owned Theme, detached before destruction.
  Return/Escape, callbacks, save/discard decisions and lifetime guards remain.
- The Windows native file chooser stays native; no replacement file workflow.

There is no separate Inspector in the current runtime. Existing track headers,
Mixer and audio/MIDI/plugin parameter editors continue to provide those functions.
This slice does not invent a second editing panel or change parameter binding;
a dedicated Inspector would require a separate product workflow decision.

## Verification

Incremental MoonRiverStudioJuce Release target only. Existing GUI asset smoke
adds Browser Theme ownership, selected-row/Files/dialog screenshots, Unicode text
and no-model-mutation checks. Existing activation/detachment checks exercise the
new Browser borrowers. Existing J3 covers file/preset async retirement and editor
behavior. No full CTest, package generation or shortcut update by default.

Results and local source are reported in #205. Physical ASIO/listening/xruns,
screen reader and hardware DPI are NOT TESTED. P4 user acceptance is separate;
P5 does not start automatically. Code/builds/screenshots remain local.
