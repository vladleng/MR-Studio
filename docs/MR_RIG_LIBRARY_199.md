# MR Rig P1 / #199 — Model Library

**READY WITH MANUAL CHECK**, 2026-10-15. **0.4c MR Rig Model Library**.
Local source: `806c875b46118eab2d3bbfb1de205a7765395dde`.
P0 #198 accepted by user («принимаю 198 и поехали 199»). No legacy test-project
migration, Convert or fallback compatibility is required. This slice implements
model curation/gallery on the accepted WAV backend; the new mrs.rig processor and
Pre/Core/Post shell remain #200/#201, actual pedal DSP #202 and NAM runtime #203.

## Scope and state ownership

| Layer | Change / owner |
|---|---|
| Shared model metadata | ToneLibrary1: separate SourceAsset and ToneModel, stable references/roles/curation |
| Global library/preferences | Model bookmarks, titles, tags, favorites, external image/rights references; no audio Undo |
| Project / preset | Exact verified source and selected model ID/title/role/provenance snapshot in existing external-selection state |
| Commands / Undo | One verified card/source transaction, current sonic controls retained; failed load does not commit |
| UI preferences | Per-insert search/tab/role/tag/favorites/files view, independent of active audio |
| Runtime / async | Cancellable source validation, existing scanner and ReverbLoader; bounded artwork preparation off UI/audio thread |
| DSP / audio callback | Existing WAV core unchanged; no pedals, second convolver, device or neural fallback |
| Host/editor lifecycle | Compatible host-only native editor retained after source Undo/Redo graph generation; external editors retire |
| Full RigPreset / chain | Deferred to #200 onward; model bookmarks are explicitly not full Rig presets |

IR Models is the default gallery in the existing WAV insert, labelled MR Rig Models.
Cards select exact WAV captures; search/role/tag/favorites/tab navigation never load
a source. NAM Models is explicitly unavailable until its runtime; My Rigs states
that full-chain saving belongs to #200. No fake active pedal or amplifier controls.
One model can reference actual same-capture rate alternatives; duration/channel
layout must agree, exact project rate wins, otherwise deterministic source choice
and the current off-thread SRC apply. Source WAVs are never copied or modified.

The existing installed collection contributes metadata bookmarks. Optional Import
menu adds a WAV, a folder, the user-applied Polytone Mini Brute profile or versioned
model bookmarks. Unknown WAV remains unknown; profile application is explicit.
Profile capture grouping uses the accepted opt-in _441 suffix, not brand heuristics.
Edit model changes name/maker/variant/tags/notes/provenance/role/cover/rights.
Cabinet-only is labelled as requiring amp processing. Full Combo WAV is static linear
colour, without nonlinear dynamics or imaginary live amplifier tone controls.

## Persistence and source safety

ToneLibrary is bounded metadata (max1000 models/8000 source records/16MiB), serialized
as MRS_TONE_LIBRARY1, and retained in optional view preferences. Model/source IDs
are separate from editable title, cover and tags. Reindex preserves user curation;
same exact-file reimport does not duplicate cards. Source bookmark has relative path,
root, absolute fallback, content fingerprint/bytes and real format metadata. The
current FNV1a64 identity detects changes; it is not a security or rights guarantee.

Relink accepts the exact original bytes/format, preserves source/model identity and
never substitutes a similarly named capture. Missing/changed content stays visible.
Source loading and selected model snapshot commit together only after loader success;
global library edits do not rewrite project sound. A selected active source can be
explicitly reloaded after relink to update its project reference.

Model metadata export (.mrmodels) contains bookmarks only. Existing .mrspreset and
project selection persist the exact WAV/selected model metadata; they do not claim to
save a future full Pre/Post Rig chain. Input/output gain and full Rig state are #200.
Multiple editor caches observe the same global library. Optimistic publication and
job generations reject stale worker results rather than overwrite another editor's
curation. Display titles may duplicate; IDs remain distinct. Fingerprint validation
also checks real WAV format metadata and absolute fallback. Source byte budget64MiB.

## Artwork

Local user-selected JPG/JPEG/PNG only, with a nonempty rights/provenance note; no web
scraping, bundled vendor photos or media. Worker checks compressed size <=8MiB and
header dimensions <=8192/16MP before decoding, then reduces to <=512x384; thumbnail
cache budget64MiB. Missing,
invalid/oversized art falls back to a neutral drawn speaker tile and reports the
reason. Artwork cannot make the WAV source invalid. No image decoding in callback
or on the message thread. Stored image references are not proof of distribution rights.

## Validation / delivery

Release configure and all-target build PASS. Final **full current CTest152/152 PASS,
157.70s**, run once on the ready candidate. Earlier targeted9/9 PASS42.74s during
development; final suite includes the updated tests and rebuilt test executables.

New tests cover metadata/roles/IDs/reimport/relink/Unicode/duplicate names/format
limits; exact same-WAV model metadata changes; one Undo per verified source selection;
failed selection retaining previous audio/history; preset and preferences roundtrip;
filter/tab independence from sound; compatible native HWND/geometry across Undo;
shared library race vs independent per-insert UI state; artwork fallback/oversize
rejection before decode. Existing Cab/Reverb/host/PDC/RT and GUI regressions pass.
The first GUI fixture used a wrong test collection ID and was corrected before PASS.

Read-only real Polytone test **PASS:20 WAV /10 rate-paired model captures**, actual
44.1/96k sources with48k fallback selection. All20 vendor SHA256 values remain
identical to the #196 baseline. No asset files were downloaded, copied or modified.
Software gallery snapshots100/150/200 exported,100/150 inspected; no physical DPI claim.
No source-specific reference gain or artwork was supplied/measured: none invented.
User may edit capture notes/levels manually and provide their own permitted cover.

Local package: `MR-Studio-0.4c-MR-Rig-Model-Library-JUCE-ASIO-Windows-local` under
the MR Studio project Builds directory. **13files/12SHA256 entries verified**;
packaged `--tone-library-smoke` **exit0**. GUI SHA256:
`230068EBC3296002C419AEFCAA036096940AA2765E31B891DC1EA83FD0BF5B70`.
Existing desktop shortcut updated, icon retained; previous0.4b upd3 preserved and
all its hashes verified. Symbols: `build/asio-local/deliveries/0.4c-rig-model-library`.
Logs: `build/asio-local/validation/rig199`, build logs `rig199-build*.log`.

Manual workflow: restart via the MR Studio shortcut, add/open **MR Rig Models**.
Existing installed collection yields model cards. If empty: **Import... → Polytone
Mini Brute profile...** and choose the already supplied local folder. This explicit
profile creates ten capture cards, labels them Full Combo and applies the user-assigned
Jazz tag; it does not interpret B/numbers as physical amp settings. Click a card to
load, Edit model for role/tags/own cover, Relink WAV for an exact-content replacement.
The source browser remains secondary under Manage files; Models returns to cards.

Hardware ASIO/listening and physical mixed-DPI are **NOT TESTED** for this release;
automated offline tests do not establish those results. Paid WAV/photos are absent
from the runtime package. Code/build/tests/package remain local; GitHub docs/issues
only. #199 stays open until user acceptance; #200 has not started.
