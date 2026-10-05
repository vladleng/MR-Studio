# 0.1p / JUCE J3 Р Р†Р вЂљРІР‚Сњ Windows polish and parity review

Follow-up 0.1p fix2: user accepted TH-U state/sound after project reopen; special TH-U/Nuro build checks disabled by policy. VST3 preset loads keep the editor/processor instance; reference palette and Files breadcrumbs/details added. See [JUCE_FIX2.md](JUCE_FIX2.md).

Follow-up 0.1p fix1: upd2 embedded plugin GUI accepted; explicit preset restoration, in-place Save list refresh, visible/resizable browser and WAV drops onto existing tracks. See [JUCE_FIX1.md](JUCE_FIX1.md). TH-U audible review remains pending.

Follow-up 0.1p upd2: offline native editor rebuild/lifecycle corrected; per-plugin preset library and Files browser added. See [JUCE_UPD2.md](JUCE_UPD2.md). TH-U audible-state user review remains pending because upd1 GUI blocked testing.

Follow-up 0.1p upd1: user confirmed the UI and Nuro state restore work, but reported
TH-U audible-state mismatch. See [diagnosis, plugin presets and editor update](PLUGIN_PRESETS.md)
and [local build/test policy](LOCAL_BUILD_POLICY.md). General J3 review confirmation
does not enumerate the outstanding physical monitor/accessibility checks below.

J1 / 0.1n and J2 / 0.1o were accepted by the user. J3 implementation is ready
locally for user review. JUCE remains an opt-in parallel shell; the preserved
Win32 package remains available. No default-target switch is made before the
remaining physical acceptance gates are verified.

## Changes

- Save/restore workspace, sidebar visibility/width, snap, horizontal zoom,
  track height, and main window placement/maximized state. JUCE restores window
  state with its platform-aware window implementation.
- Audio settings initializes saved ASIO device/rate/buffer/inputs/outputs.
  Editable rate/buffer fields reject malformed numbers. Connect remains explicit.
- Saved audio profiles: Save/replace, Load, Delete with the existing capture/
  resolve/validation API. Load stages settings without opening hardware; Connect
  re-enumerates devices and resolves a loaded profile against current channel names.
  Existing profile format supports at most one monitor input. Manual Connect can
  select multiple inputs and persists them separately in the JUCE view settings.
- Opt-in reconnect checkbox for startup and project open. Re-enumerate by device
  name rather than stale driver index. Mismatched project rate, missing device or
  changed channel-name layout leaves the offline clock and an actionable error.
  Failed hardware activation resets the offline clock. Offline selection clears
  reconnect; the Offline button disconnects without altering the opt-in preference.
- Atomic per-file settings publication through a temporary sibling file.
  Existing JUCE preferences retain the MRS_DESKTOP_CONFIG format (despite historical
  .json filename). Separate juce-view.json holds bounded view/multiple-input state.
  Both live under %LOCALAPPDATA%/MoonRiverStudio. Legacy desktop.cfg can seed prefs.
- Focus containers, explicit toolbar/footer tab order, meaningful names for M/S/R/I,
  channel gain/pan and device/profile/search fields. Custom handles expose ranged
  numeric UI Automation slider values (normalized 0..1), value-change notifications
  and writes through shared model commands/Undo. Arrows adjust; Ctrl is fine.
- Global arrangement/transport shortcuts are blocked in text/ComboBox input,
  modal components and external editor windows. Unassigned modified keys no longer
  fall through to unmodified Record/Split shortcuts. Return activates focused buttons;
  Space keeps shared Play/Stop and repeat protection in the workspace.
- Native VST3 editor initial sizing converts HWND physical pixels to JUCE logical
  pixels using the peer scale; small editors are no longer stretched by the default
  document-window minimum. Existing backend/editor cleanup remains in place.
  Plugin-directed resize/content scaling while crossing physical monitors still
  requires the explicit acceptance check below.

## Local verification Р Р†Р вЂљРІР‚Сњ 2026-10-05

- Cached, fully disconnected Windows MSVC/SDK configure and full Release build passed.
  No new dependencies installed/downloaded; no GitHub Actions.
- 90/90 CTest passed: existing backend suite plus J1/J2/J3 UI component/engine checks.
- Actual profile-control callbacks tested with a synthetic device enumeration:
  save/load/delete, non-activating Load, changed output-label rejection, unsupported
  multi-input profile rejection without mutation and malformed buffer rejection.
- Hidden native HWND permits querying actual named UI Automation handler/range;
  value write commits one shared history command and Undo restores it.
- Text/external-editor shortcut guards, view state serialization, malformed JSON
  rejection, repeated atomic publication and narrow layout with widest BROWS tested.
- Reviewed full software-rendered main snapshot; rendered dimensions at 100/150/200%.
  Fixture native client area checked at current OS DPI, logical content sizing tested
  under forced peer scales 1/1.5/2. Forced scale is not a physical monitor transition;
  Windows non-client metrics do not follow a forced JUCE peer scale.
- Installed Nuro Audio Flexion and TH-U: timed software processing, native editor
  reuse/cleanup, opaque-state save/reopen and reopened editor passed on this EXE.
  This is not a listening test of the user's selected preset or the full plugin matrix.
- Core/audio/processing/persistence source and project schema unchanged.

## Package and user acceptance

Local branch: mrs/0.1p-juce-j3-local.
Package: MR-Studio-0.1p-JUCE-J3-ASIO-Windows-local in the chat Builds folder.
Launch Moon River Studio JUCE.exe; keep mrs_vst3_scan.exe beside it.
EXE SHA256: 3B18C7563DFC522D3FD5F7E241D4CCDD3367FFD3AA6936EED514E3212BB98F95.
Build/test logs and validation record are bundled.

- [ ] User review: saved view/window state after restart, ASIO profile load/connect,
      reconnect after project open and chosen physical input/output mappings.
- [x] User TH-U project-state/sound restore and Nuro state restore accepted.
- [ ] Remaining physical ASIO playback/monitoring/recording acceptance on intended hardware.
- [ ] Real 100/150/200% monitors and monitor transitions, plugin-driven resizing,
      focus/Tab/Return/Space interactions, Narrator/UI Automation session.
- [ ] Decide whether JUCE becomes the default shell after these checks.

Whole Stage 3/#23 and deferred #16 remain open. No Amp/Preamp/PDC/isolation work
is implied. Code/commits/packages local; GitHub issues/docs only, no code push,
PR/merge/Actions. Existing local/private licensing boundary remains unchanged.

