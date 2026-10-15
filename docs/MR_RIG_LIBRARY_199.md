# MR Rig P1 / #199 - Model Library

**READY WITH MANUAL CHECK**, 2026-10-15. **0.4c upd1 MR Rig Model Variants**.
Local source: `124568b404bee81f0e4924c8c706fcae09a3ce03`.
This supersedes the original 0.4c gallery of individual capture cards, following
user feedback: one amplifier model card, select and save an IR inside that model.
#199 stays open for acceptance of the update. #200 has not started.

## Model cards and IR selection

- One card per explicitly identified amplifier model; **Polytone Mini Brute = one
  card, ten selectable captures, twenty rate-specific source WAVs**.
- Select the card, choose **IR variant** in its inline details, wait for Ready,
  then **Save IR** to remember that capture as the model's library default.
- A card recalls its saved capture; without a saved default it uses the active
  capture in that family, otherwise its first capture. Filtering never loads audio.
- The project and existing MR preset save the exact verified sounding capture
  independently of the library default. Save IR does not alter other instances.
- Reopen and Undo/Redo show the actual active capture. Loading is distinct from
  active; a failed load retains prior sound and returns the selector to it.
- Name, maker, tags, favorite, role and cover editing apply to the model family;
  individual capture names/notes/provenance remain capture metadata.
- Unknown imports are independent cards until explicit model metadata is available.
  No grouping of arbitrary files based solely on folder or manufacturer guesses.

## Implemented state and boundaries

| Owner | Stored / executed |
|---|---|
| External SourceAsset | Root, relative path, absolute fallback, fingerprint/bytes and actual format; no media copies |
| Global library | Stable family identity/title, capture bookmarks, one preferred capture per family, curation and cover references |
| Project / MR preset | Exact verified capture/source ID, title/role/provenance and existing WAV selection snapshot |
| Commands / Undo | One verified capture selection; gain/mix/cuts retained, failed load does not commit |
| Per-insert UI | Search/tab/role/tag/favorites/files view, separate from audio |
| Async runtime | Cancellable NON-RT source/thumbnail checks with generation and global-library conflict guards |
| DSP | Accepted single-WAV backend unchanged; no new callback work, pedals, extra cabinet or neural fallback |

For stable existing project/preset identity, the original ToneModel records remain
capture bookmarks. Explicit `family`/`family_title` aggregate these into gallery
models; `preferred` identifies the saved IR. Each capture's source list contains
only sample-rate alternatives of that same capture. Exact project rate wins,
otherwise deterministic selection and existing worker SRC apply. Different
captures may have different lengths or channel layouts; these must match only
within one capture's rate alternatives.

Metadata format is **MRS_TONE_LIBRARY 2**, bounded to 1000 capture bookmarks,
8000 source records and 16 MiB. Version 1 remains readable: its original generated
model/capture titles recover family metadata; existing source/capture IDs remain
unchanged. Reindex preserves curation, same-file imports do not duplicate bookmarks,
and exactly matching-content relink preserves identity. Missing/changed files are
not silently replaced. Fingerprint is FNV1a64, not a rights/security guarantee.

The secondary Import menu reuses the scanner for individual WAV/folder/profile
and .mrmodels bookmarks. The explicit Polytone profile uses the supplied local
folder and opt-in _441 rate pairing; numbers/B are not inferred amp settings.
Full Combo WAV is static linear colour. Cabinet-only is labelled accordingly.
NAM is explicitly unavailable until #203; My Rigs explains the future chain
preset scope. Existing MR preset is not advertised as a full Pre/Post RigPreset.

User-selected JPG/PNG covers require rights notes; worker limits compressed size
8 MiB, dimensions 8192/16 MP, thumbnail 512x384 and total cache 64 MiB. Missing or
invalid covers fall back safely; a family's available cover is shared by its card.
No vendor WAVs, photos or logos are bundled. New mrs.rig processor/Pre-Core-Post
shell belongs to #200/#201, actual pedal DSP #202. #198 is accepted and legacy
user test-project compatibility is not required.

## Validation of upd1

- Release affected app/audio/test targets built successfully.
- All **nine affected tests PASS**: audio_tone_library, juce_tone_library,
  juce_native_editors, juce_cab_editor, juce_reverb_editor, juce_reverb_browser,
  juce_j1_smoke, juce_j2_smoke, juce_j3_smoke. Eight GUI tests passed in the first
  44.44s run; one core negative-test fixture compared separate models and was
  corrected, rebuilt and rerun successfully (1/1, 0.03s final). No full CTest repeat
  for this scoped browser update; original 0.4c full suite is historical evidence.
- New coverage: one family/two distinct captures, selector exact capture, one Undo,
  unchanged wet controls and HWND/geometry, Save IR without project mutation,
  saved default after preferences roundtrip, card recall and active selector after
  editor reopen, version 1 family upgrade, duplicate-default and truncated metadata
  rejection. Existing preset, failure, filters, concurrent editor and artwork tests pass.
- Real Polytone read-only test PASS: one family / ten captures / twenty sources;
  actual source integrity and rate selection verified. All twenty original SHA256
  match the #196 baseline. No source WAV changes or downloads.
- Gallery snapshots exported at 100/150/200%; 100/150 inspected.
- Packaged --tone-library-smoke **exit 0**. **13 files / 12 SHA256 entries verified**.
- Hardware ASIO/listening and physical mixed-DPI **NOT TESTED** for upd1.

Local package under the MR Studio project Builds directory:
`MR-Studio-0.4c-upd1-MR-Rig-Model-Variants-JUCE-ASIO-Windows-local`.
GUI SHA256: `D16158DB68BE4884FF6C6FEF2C31AC2D2D8B37BB58B955BB4589448C34309C27`.
Desktop MR Studio shortcut points to upd1; icon retained. Previous 0.4c package
preserved and all its hashes verified. Symbols:
`build/asio-local/deliveries/0.4c-upd1-rig-model-variants`.
Logs/screenshots: `build/asio-local/validation/rig199-upd1`.

Restart through the MR Studio shortcut, open MR Rig Models, select Polytone Mini
Brute, choose IR variant and press Save IR. Existing original generated bookmarks
are grouped when read; optional Import > Polytone Mini Brute profile can apply
explicit model metadata to an existing user collection without modifying its WAVs.
