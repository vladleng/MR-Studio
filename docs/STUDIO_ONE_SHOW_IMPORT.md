# MR Live 0.8.11 — Studio One Show Page context import (#305)

**Development state: investigation / schema probe complete in source; importer NOT IMPLEMENTED.**
This is an independent MR Live compatibility feature owned by #305 under roadmap #14.
The accepted baseline is MR Live **0.8.10**, from \`main\`. No change to its
audio engine, VST3, mixer, render graph, or existing \`.mrlive\` schema is part of
the investigation step.

## User scope and UX contract

A user picks a Studio One / Fender Studio Pro \`.show\` file and independently
selects **Sections**, **Tempo**, **Chords**, or **Length in bars**. Preview must
show supported/missing fields per source song, explicit target MR Live item mapping,
and conflict consequences. Import/Cancel is a transaction, standard show Undo
and Save/Open follow existing MR Live workflow. Do not add a separate Save channel
or any link back to Studio Pro.

No audio tracks, media, MIDI clips, plugin state, presets, patches, channel state,
routing, .song project migration, round trip, or auto-sync.

## Verified vs unverified

External Studio One documentation describes Show Page Song headers displaying
tempo/meter and Show Page Arranger and Chord tracks (which can be transferred
from Song Page). It **does not define the binary/internal .show file layout**:
https://www.soundonsound.com/techniques/studio-one-show-page-revisited

| Property | MR Live destination | Availability inside a real .show |
| --- | --- | --- |
| Sections | \`items[].sections[]\`, start/length in bars, name/color | **UNKNOWN** until real archive inspection |
| Tempo (constant BPM) | \`items[].tempo\` | **UNKNOWN** |
| Tempo changes | SHARED TimeMap supports points, but current Live projection uses one item BPM; requires explicit integration after verified source | **UNKNOWN; NOT SUPPORTED by current Live projection** |
| Chords | \`performance.chords[itemId][]\`, symbol/start/length | **UNKNOWN** |
| Song length | \`items[].bars\` and \`performance.arrangement[itemId].length\` | **UNKNOWN** |

**Never claim import compatibility based on a synthetic ZIP/XML fixture.** Do
not hard-code guessed paths like \`Show/show.xml\` as authoritative. A real,
non-copyrighted test Show (or locally generated schema report) is required
to confirm container layout and actual metadata positions and units.

## Read-only schema probe (first source slice)

\`tools/studio_one_show_probe.py\` is a standalone Python 3 standard-library
tool that inspects only a ZIP-formatted \`.show\` and reports XML element paths and
attribute **names**, with counts. All text/attribute **values** are omitted.
It never extracts members, executes plug-ins or opens/reads linked media.

Run on a locally owned copy of a .show file:

\`\`\`powershell
python tools/studio_one_show_probe.py "D:\Shows\My Show\My Show.show" --output show-schema.json
python -m unittest discover -s tests -p "test_studio_one_show_probe.py"
\`\`\`

The report stays local. Before sharing it, inspect \`xml_documents[].archive_path\`:
archive entry filenames may themselves contain private names. If the tool
rejects the .show as non-ZIP, that is an honest **unsupported format** result,
not evidence that the Show is empty. The inspector has ZIP member/size/XML
complexity limits and rejects suspicious paths. This is a research utility,
**not** the final parser or a working GUI import.

## Required next implementation slice (after format confirmation)

1. Confirm one or more compatible Studio One versions on an actual \`.show\`,
   record XML paths/types/units or non-ZIP container signature. Source
   \`.show\` is always read-only. Ask explicitly before opening external
   linked \`.song\` files; avoid any automatic traversal or extraction.
2. Implement a small versioned \`ShowContextImportDTO\` carrying source song
   identity, optional musical fields, detected availability and warnings.
   Keep source file parsing independent of MR Studio \`.song\`/.dawproject
   importer (#3) and reuse the shared musical model.
3. Show the four independent checkboxes and per-song mapping/preview in native
   MR Live, initially disabling fields that cannot be decoded reliably.
4. Apply only selected, present fields to a **cloned** \`.mrlive\` model through
   \`editLiveInterface\`, using \`live::setMusicalRows\` for validated non-overlap,
   \`live::setItemView\` for the item end, one Undo entry, and normal Save/Open.
   Reject ambiguous bar/beat/meter units rather than silently converting.
5. Tests: real, redistributable fixture or a private on-device acceptance
   sample; selected-field isolation; non-destructive Cancel; invalid archives,
   Undo/Redo/Save/Open; unchanged channels, playback, FX and no ASIO work.

## Safety, version and release

\`0.8.11\` is the **active issue version**, not an accepted binary.
This first step is \`SOURCE ONLY\`, not a Windows build, UI feature,
hardware test or user acceptance. Avoid changes to \`main\`, new releases,
unrequested Windows Actions or merge until user approval. No proprietary
Show, Song, media or vendor plugin assets in the repository.
