# MR Live edit architecture — Issue #31

This is the edit contract and source audit for the fix9 candidate, based on
fix8 (`2aafda76`). It does not replace the shared Core AudioEngine, introduce
another backend or migrate the `.mrlive` format. Windows/hardware acceptance is
separate from the data checks and software measurements below.

## Findings and disposition

| UI path | Existing cost / correctness risk | Contract after this change |
| --- | --- | --- |
| Notes, Item Name/Key, musical Name/Color | Separate Undo classes, size estimates and cache invalidation; scalar commits can rebuild unrelated UI | Typed validators → one ID-addressed patch action and shared byte budget; bounded metadata/paint notifications |
| Organizer field / task list changes | Separate map-creation/absence handling; copied dirty and limit logic | Same patch action; keep existing 200-task/depth/range validators; preserve absent/null and parent containers on Undo |
| Save for All | Large saved JSON string + a separate action and size estimate | Serialize the requested item once, then patch only its library entry; rebuild saved-item baselines on the explicit library-change notification |
| View scale/lead, stems disclosure, item fold, Show Organizer, Back to start, card layout | Some paths still enter the full show/graph transaction | Validated presentation patches; only affected layout domains update; no Core preflight for presentation |
| Folder/divider/setlist/channel rename | Full transaction for a bounded string | Typed rename target and name validation; no audio graph transaction |
| Timeline Section/Chord/region drag preview | Full `originalState.clone()` on every pointer update | Copy only containers on the edited path and the bounded row list; preview remains detached from the show and Undo |
| Browser search and metadata refresh | Immediate filter/rebuild on typing; heavy cards/opaque saved-content comparisons | Debounce search independently of activation; metadata refresh retains Setlist cards/assets; Browser name changes still invalidate search/order |
| Audio/MIDI/FX menu stale guards | Canonical JSON encoding of profiles containing opaque VST3 state | Independent value snapshot and recursive exact comparison; document, device, route and catalog checks remain |
| Structural add/delete/move/import, musical boundaries, WAV/Cue/routing, plugin/device changes | Full validation and snapshots are costly; some checks overlap | Keep the structural/Core path and current preflight/rollback. A local-field fast path is not authorization to bypass these checks |
| Dirty / size bookkeeping | Unrelated revisions and manual per-action clean-baseline resets can disagree | Central invalidation for all local patches; byte counts cannot be read without a matching edit stamp |

## Command boundaries

A local edit follows:

`UI draft → typed command validation → ValuePatch → size preflight → Undo perform → domain notification`

`performLiveValuePatch` is an internal commit mechanism, not a replacement for
typed command validation. Existing validators remain responsible for names,
notes length, musical lane constraints, playback/record/loop restrictions,
Organizer hierarchy and presentation bounds. New UI code must choose an
existing typed command or add a bounded validated command. It must not send a
plain text/color/bool edit to `editLiveInterface`.

The full `editLiveInterface` / `LiveInterfaceEdit` path remains for changes whose
constraints involve several collections or Core runtime state. Its validation,
input reconciliation and rollback must not be removed on the basis of a UI
latency symptom alone. Core channel/Playback mix actions retain their established
RT-safe preview/commit side effects.

## Patch representation and Undo

`LiveEditProtocol.h` is a JUCE Core data adapter used on the message thread:

- Object properties are addressed by key; array rows by stable ID, never a
  retained array position.
- The input state is not mutated. Only ancestor objects/arrays on the target
  path are copied. Unrelated channel/plugin subtrees retain identity.
- Undo owns independent before/after values for the bounded target. Installing
  a value also detaches its mutable leaf from the action snapshot because
  legacy structural/Core adapters can still mutate model nodes in place.
- Missing properties are distinct from explicit null. If a parent map is
  absent, the patch owns the first missing property; Undo removes that newly
  introduced parent, rather than leaving empty maps behind.
- Perform/Undo/Redo compare the expected target value before replacement and
  reject a stale target. Actions also retain the document generation.
- Undo history accounts for retained strings and bounded containers, instead of
  charging a whole task list the same constant size as a bool.
- Continuous typing can coalesce only for the same owner, generation, target,
  impact and contiguous before/after values. No-op edits publish no action.
- Preview uses the same detached path-copy adapter but publishes neither the
  model, size cache nor Undo. Release retains the existing full musical/WAV
  commit validation. Any intervening document mutation invalidates the gesture.

This retains a boundary between transient drafts/previews, persisted show edits
and Core runtime publication. The format on disk is unchanged.

## Exact byte budget

The 64 MiB limit still refers to the default JUCE JSON representation used by
show validation. Byte accounting is not a character-count approximation.

The byte counter builds a format-only shape with string values replaced by empty
strings, counts each string's exact UTF-8 and JSON escape expansion, and lets
JUCE format the remaining keys, objects, arrays and numeric values. This avoids
allocating/re-encoding opaque preset or library strings. Tests compare its output
with actual JUCE serialization, including all control escapes, Unicode, empty
containers, nesting and numeric values.

For an existing value, the delta uses that value at its actual nesting depth.
Adding/removing a property measures its parent so keys, commas, indentation and
empty-container transitions are included. A growing edit with no current budget
first counts the whole show. A shrinking edit does not need to fill an invalid
budget. A current budget is updated by the exact delta on perform, Undo and Redo.

`EditByteBudget` exposes an optional count only for a matching stamp:
document generation, structural UI revision, channel mix, Playback scalar,
Playback disclosure and local mutation revision. There are no Notes-only cache
fields or caller-readable stale byte counts. Unknown/legacy structural changes
invalidate the stamp conservatively. Initial counting remains proportional to
the document's size; it is measured separately from warm local edits.

## Update domains

| Notification | Consumers |
| --- | --- |
| Notes | Notes editor synchronization and cached saved-dot comparison |
| Metadata | Existing Setlist card data, workflow/progress, bounded saved-content comparison |
| Paint | Timeline labels/colors; retain lane geometry and controls |
| Timeline | Layout-changing presentation (scale, lead, fold) |
| Cards | Card layout/disclosure or library baseline changes that require rebuilding cards |
| Browser | Item-name search/order invalidation without rebuilding the Setlist |
| Editor | Item/Organizer view reconciliation |
| Structural revision | Conservatively refresh legacy/full document consumers |

The mutation revision protects gestures and the byte budget; it is not a blanket
request to rebuild every view. Mixer meters and Core preview continue through
existing code. Metadata or presentation does not reconstruct the audio graph.

Saved-item dirty comparison caches opaque/non-metadata content when a card is
created. Later metadata edits compare only names, Key, Notes, Sections/Chords
and the progress semantics already included in saved item content. This preserves
existing Organizer status/task inclusion rules; it does not silently redefine
what Save for All stores. Search and structural/library changes recreate the
baseline when needed.

## Verification and acceptance

- Data tests: actual patch apply/Undo, row reorder, stale guards, mutable-leaf
  isolation, absent/null, exact JSON deltas and 64 MiB boundary, every cache stamp
  domain, metadata equivalence to the canonical saved-item comparison.
- Native smoke: mixed Notes/name/workflow/presentation edits, unchanged Setlist
  card and opaque/channel identity, no repeated full budget scan after warmup,
  no-op/generation rejection, Undo/Redo and Save/Open. Existing musical and
  Playback regression checks remain.
- Software benchmark: small show and a synthetic 8 MiB opaque payload; report
  cold byte counting separately from warm patches and compare with a full
  clone/JSON path. This is a data-kernel measurement, not an ASIO or end-to-end
  UI latency claim.
- Windows MSVC/Live CTest, focus/menus/Undo during Play/Stop, real large shows,
  long stress, plugin/device lifecycle and physical ASIO/DPI remain required
  acceptance evidence. Source/syntax checks do not imply those checks passed.

No automatic Actions run, merge, parent Issue closure or user acceptance is
implied by publishing this candidate.

## Reproducible software sample

`tests/live_edit_protocol_bench.cpp` can be built manually against the pinned
JUCE Core modules (plus pthread/dl/rt/z on Linux). It is not a timed CI gate.
The source uses 12 iterations, `steady_clock`, no GUI and no real plugin. One
sample with GCC `-O1` in the cloud executor produced:

| Opaque payload | Initial exact budget | Full clone + JSON, median | Warm local patch, median | Warm local patch, maximum |
| --- | ---: | ---: | ---: | ---: |
| 0 MiB | 0.015 ms | 0.023 ms | 0.0035 ms | 0.0069 ms |
| 8 MiB | 6.01 ms | 223.52 ms | 0.0204 ms | 0.0212 ms |

These numbers isolate the document-edit kernel. They are not p95/p99 UI input
latency, a Windows performance claim, or evidence of physical ASIO stability.
The cold budget is still size-dependent; it is counted only on a missing stamp
when an edit can grow the document. The tests also compare final byte totals with
JUCE's actual serializer to keep this optimization subordinate to data integrity.
