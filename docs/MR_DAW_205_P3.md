# MR Studio #205 P3 — Mixer and routing

Authorized after P2/upd1 by the user's instruction to continue #205 on 2026-10-16.

Preserve the accepted channel surfaces, frameless M/S/Arm, fader/pan geometry,
white balance deviation, shared selection and real stereo meters. Existing
12-unit Mixer typography, strip sizing, insert/send scrolling and dividers remain.

## This slice

- Insert/send power glyphs use the existing scoped Theme palette, restrained
  active glow and a keyboard focus dot without a button frame. Legacy rendering
  remains available.
- Sends retain the destination name normally and show their actual dB value on
  hover, focus or drag. Tooltips retain the full destination; existing gestures,
  fine adjustment, cancel, bypass, unity and Undo are preserved.
- Track and Master routing labels show the actual one-based hardware channel
  numbers. Output tooltips/accessibility describe the destination; the output
  menu checks the current Master/bus/hardware choice. Missing bus references
  are not presented as Master. No new route commands or graph changes.
- VST3 host toolbar uses an independently owned instance of the existing Theme,
  with explicit ACTIVE/BYPASSED text. Vendor HWND/editor rendering, owner,
  preset callbacks, pinning, Space bridge and retirement remain unchanged.

## Proportional verification

Incremental MoonRiverStudioJuce target in the existing Release tree; no configure,
all-target build or full CTest. Existing asset smoke checks M/S/Arm, gain, Undo,
Save/Open, lifetime and new hardware-routing labels/Undo; existing J3 checks
send interaction, bypass, routing, scrolling, preset/editor lifetime and host
behavior. Review fresh Mixer and host captures. Physical ASIO/listening/xruns
and hardware DPI remain NOT TESTED. Exact results are reported in #205.

P3 visual acceptance remains pending. P4 and native plugin migration do not start
automatically. Source, assets, tests and builds remain local.
