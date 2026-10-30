# MR Live 0.8.11 — Studio One Show Page context import (#305)

**Development state: schema probe and schema-independent model application exist in source; native file import/UI NOT IMPLEMENTED.**
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
| Sections | `items[].sections[]`, start/length in bars, name/color | **FOUND** as `SetListItemSection`; seconds-to-bars converter still pending |
| Tempo (constant BPM) | `items[].tempo` | **FOUND** in each item as seconds/quarter; BPM converter pending |
| Tempo changes | SHARED TimeMap supports points, but current Live projection uses one item BPM; requires explicit integration after verified source | **UNKNOWN; NOT SUPPORTED by current Live projection** |
| Chords | `performance.chords[itemId][]`, symbol/start/length | **FOUND** as encoded `SetListItemChord`; musical symbol decoder pending |
| Song length | `items[].bars` and `performance.arrangement[itemId].length` | **FOUND** as item `length` in seconds; bars converter pending |

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

The report stays local. Before sharing it, inspect `xml_documents[].archive_path`:
archive entry filenames may themselves contain private names. If the tool
rejects the .show as non-ZIP, that is an honest **unsupported format** result,
not evidence that the Show is empty. The inspector has ZIP member/size/XML
complexity limits and rejects suspicious paths. This is a research utility,
**not** the final parser or a working GUI import.

## Required next implementation slice (after format confirmation)

1. An actual user `.show` has confirmed a ZIP and `Show/show.xml` structure
   (see evidence above). Implement a bounded reader supporting this observed
   schema and distinguish other versions. Source is always read-only, never
   automatically traverse linked `.song` paths or extract plug-in data.
2. Implement a small versioned `ShowContextImportDTO` carrying source item
   **index, title, optional stable ID**, optional musical fields, availability
   and warnings; list/filter/select source items separately from destination mapping.
   Keep source file parsing independent of MR Studio `.song`/.dawproject
   importer (#3) and reuse the shared musical model.
3. Show the source SetList selector and explicit target-item mapping **before**
   the four independent checkboxes and per-item preview. Disable fields that
   cannot be decoded reliably and disable Import until a valid source selection.
4. Apply only selected, present fields to a **cloned** `.mrlive` model through
   `editLiveInterface`, using `live::setMusicalRows` for validated non-overlap,
   `live::setItemView` for the item end, one Undo entry, and normal Save/Open.
   Reject ambiguous bar/beat/meter units rather than silently converting.
5. Tests: real, redistributable fixture or a private on-device acceptance
   sample; selected-field isolation; non-destructive Cancel; invalid archives,
   Undo/Redo/Save/Open; unchanged channels, playback, FX and no ASIO work.

## Safety, version and release

`0.8.11` is the **active issue version**, not an accepted binary.
This first step adds the standalone probe, `LiveShowContextImport.h` (atomic selected-field destination adapter), and synthetic tests wired into native Live smoke. It is `SOURCE ONLY`, not a Windows build, UI feature,
hardware test or user acceptance. Avoid changes to `main`, new releases,
unrequested Windows Actions or merge until user approval. No proprietary
Show, Song, media or vendor plugin assets in the repository.
