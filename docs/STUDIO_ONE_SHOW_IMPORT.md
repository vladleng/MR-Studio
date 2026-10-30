# MR Live 0.8.11 — Studio One Show Page context import (#305)

**Development state: Windows #401 PASS and user confirmed import of Fever into an existing item without losing audio (partial acceptance). New Browser-only creation and remembered-source UX in 0.8.11-Upd2 are SOURCE ONLY / NOT TESTED on Windows.**
This is an independent MR Live compatibility feature owned by #305 under roadmap #14.
The accepted baseline is MR Live **0.8.10**, from `main`. No change to its
audio engine, VST3, mixer, render graph, or existing `.mrlive` schema is part of
the investigation step.

## 0.8.11-Upd2 — Browser-only imported items and remembered Show source

After successful Windows #401, the user confirmed import of `Fever` over an
existing MR Live item **without losing its audio**, and requested two UX
additions before Issue #305 acceptance:

1. **Create new:** source items are inserted into `items[]` (Items Browser)
   only. The importer must **not** call `insertEntry`; a Setlist is a
   performance selection, never the automatic destination. Existing items
   retain their current Setlist membership and playback/mix. Immediately after
   applying the selected musical context to a newly created item, call
   `saveItem` so the **Browser library snapshot** shows imported BPM, sections
   and length rather than the creation defaults. User can add Browser items
   to a Setlist through the existing explicit action.
2. **Remember source:** `ViewSettings.lastStudioOneShow` is persisted only in
   the local view preferences. On Files → Import Studio One Show, open the
   persistent dialog directly and read that file if it still exists and
   parses successfully. There is **no automatic import**. If the source
   is missing, corrupt or never selected, leave the dialog open with a
   **Choose .show...** button. That button opens a file chooser within the
   dialog; a valid replacement resets the source-only draft and becomes the
   remembered file. Cancellation or invalid selection preserves the prior
   usable source/draft. The path does not enter `.mrlive` or GitHub.

Contract checks were extended: Browser-only new item, unchanged Setlist,
up-to-date `libraryItem` snapshot, and view settings roundtrip with
backwards compatibility (old settings lacking the path).

**Status:** changes committed to existing Draft PR #306 under `0.8.11-Upd2`;
native Windows compilation, actual GUI lifecycle and Save/Open acceptance are
**NOT TESTED** for this update. Do not trigger extra Windows Actions or merge
until explicitly requested. No new issue or change to shared audio engine.

## 0.8.11-fix2 — Windows user-acceptance findings (30.10.2026)

The user reported and illustrated three defects in the first native GUI build
(Windows #400): checked source items could show non-zero Sections/Chords counts
but their checkboxes were disabled; clicking outside closed the CallOutBox and
discarded all selections; Import remained disabled without a clear explanation.

**Changes in the current source branch (no new build yet):**

- The parser retains raw **detected** counts and now discloses **usable** event
  counts. It normalizes only floating-point roundoff of exact 1/4-bar events
  (e.g. four Fever chords), never actually shorter musical events.
- Studio One may retain arranger markers at or after the end of the SetList
  item: these non-playable section markers are skipped with an explicit warning
  including the number excluded. An absent section length can be inferred
  **only** from the following section start or known item end. Unknown source
  data is not fabricated; truly short chords and competing events remain
  unavailable with an actionable reason.
- `LiveStudioOneImportDialog` now launches as a persistent **modal
  `juce::DialogWindow`**, not the auto-dismiss `CallOutBox`. Outside clicks
  cannot discard selections; Cancel/close still discard draft choices without
  touching MR Live.
- Choosing source, target or fields updates validation automatically against
  the cloned destination document. When the whole explicit mapping is valid,
  Import becomes enabled; otherwise the reason names the source ordinal and
  missing destination/field. The Preview button remains available.
- The user-provided private `.show` was re-analyzed **locally and read-only**:
  75 source items; **48/48** Section-bearing items compatible, with 5 trailing
  section markers disclosed as skipped; **16/19** Chord-bearing items compatible
  (4 near-quarter-bar values normalized in `Fever`, 91/91 usable). Remaining
  incompatible chord lanes cannot be faithfully represented under MR Live's
  current minimum 1/4-bar / no-overlap model, and are not silently truncated.
- Synthetic regression cases added for floating-point quarter bars, out-of-item
  section markers and genuinely short source chords.

**Verification**: private structural check **PASS (Python, read-only)**;
new native C++ compile, JUCE smoke, modal behavior, Undo/Redo, Save/Open and
physical ASIO **NOT TESTED** until the next explicitly requested Windows run.
Windows #400 PASS applies to the preceding, faulty GUI version; not this fix.
This remains within Issue #305; version `0.8.11-fix2`.

## User scope and UX contract

A user picks a Studio One / Fender Studio Pro `.show` file, **selects the source
Show Page item(s)**, explicitly maps each selected source item to a target MR
Live item, then independently selects **Sections**, **Tempo**, **Chords**, or
**Length in bars**. Preview shows detected fields, mappings and consequences.
Import/Cancel is a transaction; normal show Undo and Save/Open apply. Do not add
a separate Save channel or any link back to Studio Pro.

**Mandatory item-selection stage (0.8.11-Upd1):** source items are listed in the
original SetList order with **1-based number + original title**, search/filter
and initially no selection. Support selecting one or multiple source items.
Internal identity is original 0-based ordinal (plus stable source ID where
available), **never the title**: the real sample contains duplicate song names.
No import when zero source items are selected. For every selection, user must
choose an existing MR Live item or explicitly consent to creating a new one.
Do not match by name or overwrite other items. A duplicate destination mapping
or missing target is an error. Four content checkboxes stay independent; preview
reports availability per selected source item. Import must not touch any
unselected source/target item.

No audio tracks, media, MIDI clips, plugin state, presets, patches, channel state,
routing, .song project migration, round trip, or auto-sync.

## Verified vs unverified

External Studio One documentation describes Show Page Song headers displaying
tempo/meter and Show Page Arranger and Chord tracks (which can be transferred
from Song Page). It **does not define the binary/internal .show file layout**:
https://www.soundonsound.com/techniques/studio-one-show-page-revisited

| Property | MR Live destination | Availability inside a real .show |
| --- | --- | --- |
| Sections | `items[].sections[]`, start/length in bars, name/color | **PARSED** into bars on compatible source items; Studio One ARGB→MR UDS color map not verified |
| Tempo (constant BPM) | `items[].tempo` | **PARSED** as `60 / secondsPerQuarter`, guarded against unsupported values |
| Tempo changes | SHARED TimeMap supports points, but current Live projection uses one item BPM; requires explicit integration after verified source | **UNKNOWN; NOT SUPPORTED by current Live projection** |
| Chords | `performance.chords[itemId][]`, symbol/start/length | **DECODED in source** using host-neutral Smart Improviser `ChordModel`; invalid/overlapping/short spans stay unavailable with diagnostics; C++ build NOT TESTED |
| Song length | `items[].bars` and `performance.arrangement[itemId].length` | **PARSED** into bars using item-specific tempo and meter; no silent rounding |

**Never claim import compatibility based solely on structural discovery.**
The observed paths are verified for **one** real user Show. Other Studio One
versions and malformed or missing fields must fail safely. The user's file
must not become a redistributable fixture.

## Evidence from real Studio One .show (private user sample, 30.10.2026)

The provided, locally inspected .show **is a ZIP**, containing
`Show/show.xml` with
`Show / Attributes[x:id=Model] / Attributes[x:id=setList] / List[x:id=Items] /
SetListItem` entries. **75 items** in source order, with titles in
`PersistentAttributes[x:id=metaInfo] / Attribute[id=Media:Title]`.
Some titles repeat. The first real sample contains 549 section events spread
across 48 items and 948 chord events across 19 items. Every item carries
`tempo` and `length` attributes; lack of a chords/sections list on other
items means **not available**, not an instruction to clear target data.

Source XML is **not strictly namespace-well-formed**: the document uses
`x:id` without a declared `xmlns:x`, and tags such as
`MixerSnapshot::ChannelState` use double colons. A carefully scoped,
bounded compatibility adapter is necessary. Do not turn off validation or
silently accept arbitrary malformed XML.

For this observed format, `tempo` is seconds per quarter note (BPM is
`60/tempo`), `length` and section/chord `localOffset`/duration are seconds,
and source meter uses `timeSignature.numerator` and
`timeSignature.denominator`. Convert per item into bars based on tempo and
meter. Some lengths are fractional bars; do not round silently. Chord symbols
are encoded in `Attributes[x:id=chord]` as numeric root/intervals and require
a verified musical decoder before enabling the Chords checkbox.

**This is structural research evidence, not a claim of successful native
import.** No actual user Show or excerpts are committed or uploaded to GitHub,
PRs or CI; any future regression fixture must be synthetic or independently
redistributable.

## Native C++ reader source slice (30.10.2026)

`apps/studio-juce/src/LiveStudioOneShowReader.h` implements
`live::readStudioOneShow(const juce::File&)` plus a synthetic-testable
`live::readStudioOneShowXml(std::string)`. It reads **only**
`Show/show.xml` from the `.show` ZIP. It neither extracts nor opens
audio, preset, VST, media or linked .song references.

- Reads SetList in original order, including **duplicate titles**, with a
  zero-based `sourceItemOrdinal`. User selection is **not inferred**.
- Reads source meter, converts seconds-per-quarter to BPM, and converts
  item length, section boundaries and encoded chord boundaries into bars.
  Fractional bars retain precision.
- Validates Sections against `LiveTimelineEditState.h` rules. For
  unsupported or overlapping spans, **the entire category is unavailable**,
  not partially imported. Retains count and warnings; no source is modified.
- Parses the chord `root / intervals / type / bass` fields into a separate
  `rawChords` diagnostic DTO, **and** maps supported data to Live `chords`
  using the **exact accepted Smart Improviser ChordModel**. The imported
  harmonic normalizer is a pinned, unchanged source snapshot from
  `vladleng/Smart-Improviser` commit
  `28f6bb3a4df06bb179a6a51a0415b99fc1b1a1f1`, under
  `core/harmony/si_snapshot`. Source `intervals` are 12 hex values
  (including `B` for 11 and `D` for 13) and `root` / `bass` are circle
  of fifths indices; the Studio One adapter is in
  `LiveStudioOneChordDecoder.h`. This avoids inventing an alternate harmony
  engine in MR Live. Unknown symbols or invalid/overlapping/short events
  disable the **entire chord category**, with a warning; missing lanes
  remain void, never interpreted as an instruction to clear destination.
  Studio One's `type` is preserved as raw source evidence but the canonical
  symbol comes from normalized intervals/quality.
- Bounds ZIP entries (4096), expanded archive metadata (1 GiB), source
  size (512 MiB), XML size (8 MiB), XML depth (64), XML node count
  (100000) and source item count (500); rejects DTD/entities and unsafe paths.
  Normalizes the source's `x:id` / `::` quirks **in memory only**.
- The downstream `ShowContextSong` mapping still requires an explicit target
  item ID. A source/target meter mismatch blocks Sections/Chords/Length
  unless a later separately agreed meter-handling UI resolves it.

**Private read-only validation:** real uploaded `.show` parsed with an
independent Python equivalent of the XML normalization and time conversion:
75 items / 48 with Sections / 19 with Chords; **43** section-bearing
items satisfy the current section-span limits, **5** require a clear
compatibility warning. Structural extraction and time conversion: **PASS
(Python only)**. The earlier native reader and synthetic checks passed in Windows
[run #41018601841](https://github.com/vladleng/MR-Studio/actions/runs/41018601841).
The subsequently added Smart Improviser linkage, decoder and multi-item
plan remain **NOT TESTED** in Windows (a new build has not been requested).
The private file is not stored in GitHub.

### Smart Improviser integration and source selection contract

The user identified the existing [Smart Improviser](https://github.com/vladleng/Smart-Improviser)
harmonic normalization layer. We checked the accepted
`src/core/context/HarmonicContext.h` and `src/core/model/ChordModel.h/.cpp`
rather than building a competing chord vocabulary. The copied source snapshot
is isolated as a non-RT static library `mrs_smart_chords`, linked only to
the JUCE Studio/Live applications. Any later shared ecosystem dependency
consolidation requires its own architectural decision; this task does not
alter Smart Improviser's repo or its released plugin.

The supplied private Show contains 948 chord records across 19 items.
All 948 have 12 interval tokens and correspond to recognized qualities in
Smart Improviser's harmonic model. **16 items** satisfy the present Live
minimum-span/no-overlap constraints, **3 do not**. No private source
songs/chord charts were committed. Preview must show the availability and
warnings per source item.

`LiveStudioOneImportPlan.h` adds atomic multi-source transactions: each
requested source ordinal maps to a single explicitly chosen MR Live item
(or explicit **create new**), duplicate source/target selections are rejected,
and no mutation is committed on an invalid plan. A newly created item may
adopt the source meter, whereas existing target meters are protected.
One complete plan is intended to be applied inside
`Desktop::editLiveInterface` for a **single Undo step**. This plan has synthetic C++ contract tests; the native user-facing selector/dialog
is now implemented in source, but its build, visual review and Save/Open
acceptance are **NOT TESTED**.

### Native import dialog — current source stage

`apps/studio-juce/src/LiveStudioOneImportDialog.h` implements the real
`MR Live → Files → Import Studio One Show (.show)...` entry, integrated in
`LiveWorkspace.h`. The file chooser parses the selected Show **read-only**
using the existing native reader, then displays its source items with
**1-based original positions, titles, search/filter and initially no selection**.
The source ordinal stays stable even with duplicate names and filtering.

Each selected source item has separately configured MR Live destination
(existing item ID or explicit creation), and four independently chosen
Sections / Tempo / Chords / Length checkboxes. Unavailable fields are disabled;
the right side displays the parsed values and compatibility warnings.

`Preview` validates the **whole selection atomically** against a cloned
in-memory show and lists source-to-target mapping and chosen fields. `Import`
stays disabled until a successful Preview. After explicit Import, the same
batch is applied through **one** `Desktop::editLiveInterface` operation
(one Undo transaction). Cancel/dismissal only discards UI state. A changed
MR Live document, concurrent edit, missing destination, duplicate mapping,
invalid source data, unsupported chord/section or meter mismatch blocks the
commit without partial data. Normal MR Live Save/Open handles persistence;
there is no separate channel or import save operation.

**Verification state:** native C++ GUI has **NOT TESTED** compilation,
interactive behavior or Windows acceptance. Previous run #399
[`41022966852`](https://github.com/vladleng/MR-Studio/actions/runs/41022966852)
was PASS for the parser/decoder/import-plan source **before** adding this GUI.
No build was launched for the dialog changes. Private sample remains off GitHub.

## Read-only schema probe (first source slice)

`tools/studio_one_show_probe.py` is a standalone Python 3 standard-library
tool that inspects only a ZIP-formatted `.show` and reports XML element paths and
attribute **names**, with counts. All text/attribute **values** are omitted.
It never extracts members, executes plug-ins or opens/reads linked media.

Run on a locally owned copy of a .show file:

```powershell
python tools/studio_one_show_probe.py "D:\Shows\My Show\My Show.show" --output show-schema.json
python -m unittest discover -s tests -p "test_studio_one_show_probe.py"
```

The report stays local and includes `source_item_count` when the ZIP contains the known
`Show/show.xml` structure. The inspector now accounts for Studio One's `x:id`
/ double-colon XML syntax without rewriting the source. Before sharing a report,
inspect `xml_documents[].archive_path`:
archive entry filenames may themselves contain private names. If the tool
rejects the .show as non-ZIP, that is an honest **unsupported format** result,
not evidence that the Show is empty. The inspector has ZIP member/size/XML
complexity limits and rejects suspicious paths. This is a research utility,
**not** the final parser or a working GUI import.

## Remaining work in the same #305

1. **DONE (source):** verified the observed ZIP/Show XML structure and
   implemented a bounded, read-only native reader; linked `.song` paths
   are never traversed and media/plug-in data is never opened.
2. **DONE (source):** `ShowContextSong` contains ordinal, title, source meter,
   decoded optional fields, raw chords, normalized chord symbols, availability
   and warnings. Smart Improviser model is used; verify new linkage and decoder
   with a future targeted Windows build.
3. **DONE (source only):** native file picker, original-ordinal source selector,
   search/filter, explicit target mapping, four independent field checkboxes,
   preview validation and non-destructive Cancel. Per-item availability is shown.
4. **DONE (source only):** one atomic `editLiveInterface` Undo transaction
   using validated source rows, cloned model and unchanged shared persistence.
   Windows/interactive verification still required.
5. **NEXT:** addressable Windows compile+Live smoke for new UI (no full CTest);
   user acceptance on private `.show`, duplicate titles, Cancel, partial selection,
   bad values, Undo/Redo and Save/Open. Playback, FX and ASIO remain untouched.

## Safety, version and release

`0.8.11` is the **active issue version**, not an accepted binary.
This source stage adds the standalone probe, native `LiveStudioOneShowReader.h`, `LiveShowContextImport.h` (atomic selected-field destination adapter with mandatory source ordinal), and synthetic tests wired into native Live smoke. The source item selector/dialog is now implemented in JUCE source but is
**not yet a tested Windows binary**; physical hardware, interactive runtime,
Save/Open and user acceptance are **NOT TESTED**. Avoid changes to `main`, new releases,
unrequested Windows Actions or merge until user approval. No proprietary
Show, Song, media or vendor plugin assets in the repository.
