# MR Rig — Model Libraries, Curation, Images, Presets & Imports


**P1 / #199 implemented locally, 2026-10-15:** 0.4c upd1 MR Rig Model Variants; [implementation, boundaries and validation](MR_RIG_LIBRARY_199.md). SourceAsset/ToneModel bookmarks, WAV gallery/curation and exact selected-model snapshot are implemented. Full RigPreset/Pre-Core-Post shell and NAM runtime remain later stages. #198 accepted; #199 awaits user acceptance.

**Stage:** DESIGN 2026-10-15. **Parent [#197](https://github.com/vladleng/MR-Studio/issues/197)**. Technical baseline [MR_RIG_ARCHITECTURE](MR_RIG_ARCHITECTURE.md), GUI [MR_RIG_GUI](MR_RIG_GUI.md). Accepted baseline #185 plus **#196 Full Combo WAV 0.4b upd3**. **#182 and standalone importer #187 are superseded**; only a minimal optional user WAV import / manual mapping is in #199, not universal Cab catalog development.

## 1. Three library views / what is stored

| UI view | Stored item | What is loaded? | Edit options |
|---|---|---|---|
| **IR Models** | `ToneModel` referencing one validated WAV or set of **real alternative captures** | Existing single-WAV Cab convolution at a time | Title, tags, cover, semantic role `full_combo/cabinet_only/unknown`, version, variation |
| **NAM Models** | `ToneModel` referencing validated .nam model(s) with known compatible NAM engine | Dedicated **future** NAM runtime, no WAV convolver for neural processing | Name, tags, cover, `full_rig/amp_only/unknown`, model architecture/quality compatibility |
| **My Rigs** | `RigPreset` snapshot of **whole chain** | Selected ToneModel + ordered Pre/Post FX + real DSP parameters | Name/favorite/cover override/categories, duplicate/export/relink, Undo |

**Important:** A collection of IRs is not automatically a collection of playable amp emulations. Celestion cabinet-only remains labelled. Full Combo IR is the *linear measured complete chain if documented*. A WAV file alone cannot simulate input-dependent distortion/compression or moving physical tone-stack controls; do not create fake Gain/Bass/Treble for it.

## 2. Curation first / raw imports optional

**User-approved UX update / #199, 2026-10-15:** one gallery card represents one explicitly identified amplifier model, with its real IR captures selected in the model details. Sample-rate alternatives are not extra cards or capture choices. `Save IR` remembers the verified active capture as that model's library default. The exact sounding capture remains independently stored in project/MR preset with Undo; saved defaults never change other running instances. Search and filters never load audio. Polytone Mini Brute is one card with ten captures, not ten model cards. Current implementation keeps stable capture bookmark IDs and aggregates them by explicit family metadata; see [delivery/schema/validation](MR_RIG_LIBRARY_199.md).

Library first-run may show curated templates or legal example items with explicit **Not installed** status. User browses by cover/genre/type/model, not absolute files. MVP `Add My Model` secondary workflow:
1. Choose `Import WAV IR` (today) or `Import NAM` (only when NAM engine supported); point to existing local file or collection root; **do not copy vendor files** without explicit consent.
2. Validate WAV header/channel/rate/duration/integrity or NAM format/architecture (supported runtime list), detect/match known profile if trustworthy; otherwise mark `unknown`.
3. Ask manual classification with explanation: WAV cabinet-only/full-combo/unknown; NAM amp-only/full-rig/unknown. Optional maker/model/voicing/mic/settings. Missing fields stay missing; **not infer "Fender Full Combo" from filename**.
4. Choose cover image via file picker, optional crop/fit/thumbnail generator off-thread; rights warning; fallback generic clean tile. User image is not redistributed. Model may use existing image path; safe cache if ownership/relink policy vetted.
5. Save **ToneModel** metadata bookmark, not a full Rig, then `Create Rig` based on it and add pedals; no forced save of imported IR as Rig. `Save Rig` snapshots real chain.

**User-selected shortlist:** Jazz/Clean/Lounge etc are tagging choices, not hardcoded Polytone/Fender SKU catalog, commercial assets can be referenced by user. A sample 'Warm Jazz' Rig requires real source and valid license or obvious illustrative placeholder, never auto-download protected file.

## 3. Suggested normalized state (not current implemented schema)

```text
SourceAsset { assetId, kind: wav_ir | nam_model, externalRef, fingerprint,
              rawFormat, sourceRateHz?, frames?, channels?, namArch?, engineCompatibility }
ToneModel { modelId, sourceAssetIds[], selectedVariantId, sourceType: ir | nam,
            capturedRole, cabIncluded: yes | no | unknown,
            roomIncluded: yes | no | unknown,
            manufacturer?, modelName?, variantName?, captureSettings?,
            artworkRef?, artworkRightsNote?, tags[], favourite,
            provenance: vendorDocumented | userAssigned | unclassified }
RigPreset { rigId, rigSchemaVersion, name, modelId, selectedAssetId,
            coreMode, optionalCabAssetId?, preFx: ModuleState[],
            postFx: ModuleState[], inputGain, outputGain, rigBypass,
            slotBypass[], artworkOverrideRef?, customTags[], favourite }
ModuleState { moduleId, instanceId, typeId, version, bypass, parameters,
              order, latencyPolicy?, missingPlaceholderMetadata? }
```

This is **conceptual**, not a forced new schema; inspect already released CabManifest and project schema. Avoid non-deterministic ID derived from changing absolute path. `assetId` bound to validated content and metadata; `modelId` stable across relink where possible. Insertion chain positions and settings must round-trip. Permitted export `RigPreset` only references media; if sharing offline without source, missing media must remain recoverable.

## 4. Artwork policy

- User may choose **own** JPG/PNG or licensed PNG/WebP/SVG depending on actual decoder safety, size limits and local permission, optional thumbnail downsample off RT.
- No automatic online scraping or unauthorized Fender/Polytone trade dress/product photo packages. Prefer user photos, licensed originals, abstract generic cover fallback. Vendor logo/art claims stored separately from model signal metadata.
- Artwork does not block audio loading, and missing artwork must not make the Rig missing/invalid. Asset image references stable/relinkable; avoid loading huge images in audio callback or during transport.
- Gallery #189 defines reusable card/style primitives later, but does not require model files embedded in UI source.

## 5. Persistence, edits, Undo

- `Save Tone Model` = curation metadata (global user library, safe persistence) distinct from `Save Rig` = host/preset state. Uncommitted UI preview not saved accidentally, editor close retains audio state.
- For project portability, snapshot necessary manifest refs/role/preset data under **host project’s existing transactional model**; do not mutate global user library when automation/Undo changes a per-project Rig.
- Parameter input binding read→preview→commit once per gesture, add/reorder/remove pedals as structural host commands with proper Undo/Redo; bypass meaning/side effects clearly defined. Save/Open/duplicate/delete/favorite semantics tested individually.
- Import/reindex should preserve manually assigned role/image/tags/favourites; duplicate source files and moved roots resolved without silent substitution. A destroyed/wrong source shows Missing and keeps metadata; future NAM unsupported shows Unsupported with exact reason, not "Ready".

## 6. Test matrix / Acceptance

- Synthetic WAV tests for one cabinet-only (not real amp), one Full Combo, one unknown, missing/moved/corrupted, rate alternatives; NAM metadata fixture even before runtime but not Ready.
- Model and Rig entities round-trip without conflation, no byte copies of vendor files, source identity/fingerprint stable and auditable, unknown role handling preserved.
- Artwork fallback, huge/invalid image handling, broken external image relink, licensed metadata not enforced by automatic trusted assumptions.
- Rig duplicate/save/missing/relink/Undo/Redo, two instances independent, editor reopen retains active source/selection, library filters not auto sound changing.
- Manual user acceptance of available self-curated model cards and their internal IR choices **only with user-supplied/licensed source assets**; one card per model, no quota filled by duplicate capture cards; can't claim preset audio completeness otherwise.
