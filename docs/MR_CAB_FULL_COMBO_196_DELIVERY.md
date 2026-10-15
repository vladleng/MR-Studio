# MR Cab Full Combo IR / #196 — local delivery

**Current follow-up, 2026-10-15:** [0.4b upd2 — stable Cab window/preset switching, Gain explanation](MR_CAB_WINDOW_UPD2_196.md).

**Previous follow-up, 2026-10-15:** [0.4b upd1 — browser menus/filter persistence](MR_CAB_BROWSER_STATE_UPD1_196.md). New package/targeted checks there; base results below remain unchanged.

Date: 2026-10-14. User authorised implementation: «Переходи к следующему шагу».
Version: **0.4b Full Combo IR**. Status: **READY WITH MANUAL CHECK**, user acceptance pending.
Local source: `2fbe90ef6ce1877ca77d6fbffdf51d3a209a59ef`.
Baseline #185 remains accepted/closed. #186 and full #187 importer are not started.
Requirements: [Full Combo contract](MR_CAB_FULL_COMBO_IR.md), [#196](https://github.com/vladleng/MR-Studio/issues/196).

## User workflow

1. Open MR Cab, choose **Collection folder...** and select
   `D:/Samples/Past to Future/Polytone Mini Brute Jazz Guitar Amp` itself.
2. Wait for scan, then **Map collection... → Polytone Mini Brute profile → Apply mapping**.
   This is an explicit user classification, not an automatic filename guess.
3. Choose **Full Combo IR**. Menus show Combo Model / Capture Variant / Length.
   Ten captures appear, with rate versions grouped. Click a capture to load it.
   Menus still filter; automatic loading from menus remains deferred by user decision.
4. Recommended guitar DI → this one MR Cab at Mix 100% → output/other FX.
   Change Mix yourself if needed; classification/selection preserves all existing controls.
   Additional cabinet processing inside MR Cab is not added. Existing external inserts
   remain under user control; UI explains avoiding a second cabinet in the signal chain.
5. Save a project or MR preset to preserve the selected external WAV identity,
   role, model/notes and controls. Vendor samples are never embedded in the saved state.

Other collections: choose All IR / Cabinet IR / Full Combo IR / Unclassified,
then manually map active WAV or the whole current collection. Optional rate filename
suffix explicitly groups versions of a capture, preserving parent-folder identity.
The Polytone profile uses `_441`; it does not label B as Bright or assign gain settings.
Supplier PDF describes amp IR, dark/bright settings and gain stages; exact chain unknown.
WAV IR remains static linear colour, not nonlinear amp modelling or NAM/AIDA-X.

## Implementation and state ownership

- Shared Core stores bounded `NativeInsert::cab_metadata` (role/capture snapshot).
  Existing SetInserts commands provide project Undo/Redo; metadata changes retain
  DSP graph/loader/device, including during playback. Controls are not reset.
- Catalog remains **CabManifest v1**, using existing optional tags and overrides.
  Metadata-only publication validates current scan revision, increments generation
  and publishes an immutable snapshot. It performs no WAV I/O or re-decoding.
  Stale/scanning mapping is rejected; sequential mapping retains earlier overrides.
- Library mappings are global preferences, like library root/favorites, and persist
  independently of project Undo. Undo reverses the active IR's project metadata.
  Project/preset snapshot stays authoritative for its saved active role/description,
  even when the catalog changes, moves or is missing.
- Capture grouping is opt-in; explicit group plus parent path/channel mode is stable
  when role/notes change. Unchanged Celestion mappings keep their IDs. Cabinet role
  derives from recognised Celestion/confirmed cabinet metadata; arbitrary filenames
  such as Fender/Polytone do not automatically become Full Combo.
- **Schema 23 / archive 1 unchanged.** Cab-only `CAB_SELECTION 2` adds a bounded
  quoted metadata map; reader still accepts `CAB_SELECTION 1`. Writer emits v1 for
  empty metadata. Existing schema23 Cab projects/presets continue loading.
  Older 0.4a readers reject the new subrecord rather than silently losing its fields.
- UI retains actual loaded WAV/digest as selection truth and owned native window.
  Full Combo rate versions share one row; exact project rate wins, otherwise the
  existing deterministic selector and off-thread resampling apply. Length/stereo
  are preserved. Missing/unsupported entries retain visible error semantics.
- Same `mrs.cab-v2`, shared ReverbLoader and Cab-mode ReverbProcessor; no additional
  convolution stage, room kernel, fake amp knobs, callback metadata or audio DSP edits.
  Current supported Cab bound remains mono/stereo, 8–192 kHz, <=1 second.
  Real Polytone 632–857 ms fits; Celestion 48k/200 and 500 remain unaffected.

## Checks and physical limits

Synthetic tests cover role safety, opt-in grouping/rate selection, duplicate validation,
stable IDs, overrides, reindex/missing, stale mapping rejection, sequential edits,
project/preset round-trip, Undo/Redo, root move/relink, missing saved metadata,
unchanged Mix/output/device generation, GUI active/Ready identity and native ownership.

Read-only vendor tests scan all20 Polytone PCM24 stereo WAV and map10 captures;
Application loads/processes a real capture at project44.1/48/96k. Cab-mode offline
profile uses `Polytone 5 B.wav` (96k,82249 frames), projects44.1/48/96k and blocks128/256/512.
Callback allocations/deallocations0/0; algorithmic latency0; full-tail one-WAV
oracle maximum error5.96046e-08. Final offline measurements: mean62–254us,
max141–518us, prepared memory5.1–8.5MB; these are not ASIO CPU or Live acceptance.

Release configure/build PASS; final targeted6/6 PASS3.54s and final Cab GUI1/1 PASS1.64s.
Full current CTest ran once on ready candidate: **149/149 PASS149.03s**.
GUI snapshots100/150/200 and mapping150 reviewed; native main/editor ownership/z-order
regression PASS. Physical mixed-DPI remains unverified. Initial compiler shadow-warning
and exact-float GUI-test assertion were corrected before these final passing results.

Package: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.4b-Full-Combo-IR-JUCE-ASIO-Windows-local`.
13files/12SHA256 entries verified; packaged Cab editor/Combo smoke **exit0**.
GUI SHA256 `F3DA80610DC3C62C57D2986C5968BF0A13F00A8E62885B7BEB76B9A6652AD190`.
Existing MR Studio shortcut updated, icon retained; previous0.4a/0.4a upd1 preserved.
Prior upd1 package hashes and20 source Polytone WAV SHA256 hashes rechecked unchanged.
PDB: `build/asio-local/deliveries/0.4b-full-combo`.
Logs/profile/hash metadata: `build/asio-local/validation/combo196`.

Physical ASIO playback, listening/level-matched A/B, physical mixed-DPI transfers
are **NOT TESTED** and remain manual acceptance. Existing Live safety gate is unchanged.
Vendor WAV are not downloaded, copied, modified or distributed.
Code/build/tests/package stay local; GitHub receives docs/issues only, with [skip ci].
