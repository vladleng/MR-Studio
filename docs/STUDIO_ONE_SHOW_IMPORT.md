# MR Live 0.8.11 — Studio One Show Page context import (#305)

**Development state: native read-only .show parser and destination adapter exist in source; item picker/preview/import UI NOT IMPLEMENTED.**
This is an independent MR Live compatibility feature owned by #305 under roadmap #14.
The accepted baseline is MR Live **0.8.10**, from `main`. No change to its
audio engine, VST3, mixer, render graph, or existing `.mrlive` schema is part of
the investigation step.

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
| Chords | `performance.chords[itemId][]`, symbol/start/length | **RAW PARSED** (root/type/intervals/bass and time spans); no verified chord symbol decoder, so Chords import stays disabled |
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
- Retains chord `root / intervals / type / bass` encoding in a separate
  `rawChords` preview DTO. **Does not invent chord symbols** or enable
  Chords for import yet. Missing lanes are void (unavailable), never
  interpreted as an empty lane to overwrite the destination.
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
(Python only)**. The actual Windows/JUCE C++ reader and synthetic C++
smoke checks are **NOT TESTED** until the next targeted Windows run.
The private file is not stored in GitHub.

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
   decoded optional fields, raw encoded chords, availability and warnings.
   Next validate actual C++ behavior on Windows; decode chord symbols only
   against independently verified musical mappings.
3. **TODO:** native source SetList selector and explicit MR Live target-item
   mapping **before** four independent checkboxes and per-item preview; unavailable
   data must be disabled. No automatic selection or name-only matching.
4. Apply only selected, present fields to a **cloned** `.mrlive` model through
   `editLiveInterface`, using `live::setMusicalRows` for validated non-overlap,
   `live::setItemView` for the item end, one Undo entry, and normal Save/Open.
   Reject ambiguous bar/beat/meter units rather than silently converting.
5. Tests: real, redistributable fixture or a private on-device acceptance
   sample; selected-field isolation; non-destructive Cancel; invalid archives,
   Undo/Redo/Save/Open; unchanged channels, playback, FX and no ASIO work.

## Safety, version and release

`0.8.11` is the **active issue version**, not an accepted binary.
This source stage adds the standalone probe, native `LiveStudioOneShowReader.h`, `LiveShowContextImport.h` (atomic selected-field destination adapter with mandatory source ordinal), and synthetic tests wired into native Live smoke. The source item selector is specified but **not yet rendered as a working JUCE control**. This is `SOURCE ONLY`, not a Windows build, UI feature,
hardware test or user acceptance. Avoid changes to `main`, new releases,
unrequested Windows Actions or merge until user approval. No proprietary
Show, Song, media or vendor plugin assets in the repository.
