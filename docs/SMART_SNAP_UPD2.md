# Arrangement grid / Smart Snap — 0.3m upd2, 2026-10-14


## Current correction / 0.3m upd4 — 2026-10-14

User screenshot shows crowded grid at wide zoom; wants half/whole divisions, and
Shift held after LMB drag begins to bypass snap for audio/MIDI clips and piano notes.
Nominal display/snap spacing10px instead5px; same1/16 minimum/ladder/bar boundaries.
At80BPM/7pixels-per-second choose half-note step1920 (bar21px, half10.5px), matching
the supplied scale; quarter/eighth lines disappear. Paint and Smart Snap stay shared.
During clip move/trim and piano note move/resize use current mouse-event Shift state
as temporary snap bypass. Plain mouse-down starts gesture; later Shift disables,
release during drag restores snapping. Piano small-horizontal deadzone also bypassed
with Shift; horizontal tick precision, vertical semitone and range limits retained.
Existing Shift-at-mouse-down selection toggles, Alt velocity/copy, cancel/preview/
common Undo/Redo and persistence retained. Snap flag/piano grid preferences untouched;
modifier is transient and never stored. UI/gesture tests affected, existing commands/
model/engine/serialization reused; DSP/network not changed.
Source `66b712fa2a66b6c8e3d2c1627e6caacf167c4348`,5source/test files; branch unchanged.
Release build PASS; final affected6/6 CTest PASS91.01s (MIDI/native Arrange/clip/nonplaying
and J3+diagnostics GUI) after all changes. Audio/MIDI Shift-after-down free drag,
preview-only before commit, unchanged Snap setting, one Undo/Redo; releasing Shift
mid-drag resumes grid; piano7px free movement/deadzone bypass and release-to-snap
without grid-setting mutation PASS. Free MIDI still uses natural tick rounding;
fixture expectations account for that. Grid ladder10px/bar boundaries and existing
Undo/SaveOpen/audition/selection/cancel tested. Previews7/30 and other scales100/150
inspected. Logs grid-shift-upd4-*; no test failures/timeout changes. Full145 last
passedupd2/160.58s, not rerun in full for this UI-only change.
Package `MR-Studio-0.3m-upd4-Grid-Shift-Drag-JUCE-ASIO-Windows-local`,13files/12hashes
verified, packaged AI smoke exit0; GUI SHA256
`3A0964BD615FBA1BA21BC2C30FC4421796A86487F0B20EB8159E5B1551C86BC0`.
PDB deliveries/0.3m-upd4-grid-shift-drag, MR shortcut target/workingdir/emptyargs/icon
verified; previousupd3 manifest12/12 unchanged. FEATURE READY WITH MANUAL CHECK:
actual user-project grid/Shift and physical DPI pending. Save/restart via MR Studio,
repeat supplied zoom, LMB then Shift clips/notes, release Shift, Undo/Redo/SaveOpen.
Native parent26 remains open5/11; no extra acceptance/source push/PR/merge/Actions.

## Current correction / 0.3m upd3 — 2026-10-14

User: minimum grid1/16 and missing half notes before whole notes; asks conventional
DAW progression. New shared display/snap ladder starts at ppq/4 (1/16), doubles
through1/8→1/4→1/2→measure→2measures→4measures.4/4 measure is a whole note.
Odd meter measure caps the last smaller subdivision; subdivision phase restarts
at each bar, so3/4 still shows/snap captures its bar boundaries with a half-note grid.
Meters shorter than1/16 use whole groups of bars, never a finer note division.
Nominal5px threshold/faint subdivisions, labels60px, Smart snap membership and
cursor anchor/bounds unchanged. Meter boundaries may form a shorter remainder.
UI/helper only; commands/history/persistence/engine/network unchanged, no migration.
Piano roll own grid unaffected. Previousupd2 minimum/beat→bar rules below superseded.
Source `fd7785325e5aa606da0d3755f11a05de2415dd8f`,3source/test files, branch unchanged.
Release build PASS, final affected10/10 CTest PASS91.74s after all source changes:
GUI J3+diagnostics/native Arrangement/clip/chord/marker/section/tempo/meter/MIDI;
explicit1/16 high-zoom floor, all ladder steps, half in3/4 with bar boundaries,
short meters, rendered spacing/Smart membership/zoom anchor/bounds/history checks.
Initial8/10 (45.59s): old4pps bar expectation now correctly half; fixture changed to
3pps bar. Next8/10 (44.56s): universal5px assertion failed mandatory odd-meter bar
remainder; test now requires5px unless either line is an actual bar boundary.
Nominal spacing assertions retained; no timeout changes. Logs smart-grid-upd3-* retained.
Previews30/15/12/6/3zoom at100/150 inspected. Full145 suite last passedupd2/160.58s;
not rerun in full for this isolated UI helper change.
Package `MR-Studio-0.3m-upd3-Smart-Grid-JUCE-ASIO-Windows-local`,13files/12hashes
verified, packaged AI startup smoke exit0. GUI SHA256
`0F3FEDAAAFD04177AC09602FC400F89648C1298BB156641949909273CD9D4E28`.
PDB deliveries/0.3m-upd3-smart-grid; MR shortcut target/workingdir/emptyargs/icon
verified, previousupd2 manifest12/12 unchanged. FEATURE READY WITH MANUAL CHECK:
user-project zoom/grid and physical DPI pending. Native parent26 remains open5/11;
source/build/package local, docs/issues GitHub, no source push/PR/merge/Actions.

User confirms Split/Merge G/Duplicate D in0.3m upd1 and explicitly accepts #167/#157.
Native parent26 completion5/11 (#164/#165/#166/#167/#157), parent and other6 remain
open. «Все вроде работает» general feedback recorded, no blanket acceptance inferred.
No additional ASIO/recording/error/DPI measurements claimed.

## Contract / layer impact

Arrangement grid stays subdivided longer: nominal5 logical px minimum instead14;
faint subdivisions, stronger beat/bar lines. Meter-aligned subdivision→eighth→quarter
(or denominator beat)→bar→power-of-two bars. One sequence, no overlapping second grid.
Resolution uses the fastest authored tempo and each meter segment origin, shared by
paint/snap; scrolling alone does not change the chosen step.
60px ruler label spacing retained; faint subdivisions reduce visual noise.
Enabled existing Snap toggle now means Smart Snap, caption Snap smart; off stays free.
Same resolution and meter-segment origin for paint and snap, nearest by sample distance
(lower on ties). Includes ruler, audio/MIDI clip move/trim/create/import/drop and
chord/marker/section/tempo dragging; group relative spacing retained. Existing7px
object-edge capture retained for musical groups, clamp/range/overlap constraints
still apply. Meter events legally bar-aligned. Piano roll keeps its own grid control.
Snap-off ruler retains its legacy6px bar capture; other Snap-off edits remain free.
Cursor-anchored wheel zoom uses the raw sample under the mouse, independent of Snap,
so resolution changes do not move the anchor.
No hidden finer fixed1/16 snap in Arrangement. Grid changes from zoom are transient;
existing view preferences store zoom and Snap bool; no schema migration.

UI/display/gesture projection and tests/docs AFFECTED; existing command/history/
serialization paths reused, no new project fields. Core/domain/engine/audio callback/
DSP/network NOT AFFECTED. UI thread calculates coordinates from immutable snapshots,
existing Application commands apply edits/history/SaveOpen. Non-RT only.

## Validation / delivery

Source33777d3b6d322f2d5969f407cca0c9d4d855f44b,10source/test files; branch unchanged.
Release build PASS. Initial focused8/9 (21.55s): old six-pixel ruler capture asserted
for Snap-on; updated to Smart grid contract while keeping Snap-off capture checks.
Initial full143/145 (130.95s): second old tempo/meter ruler capture assertion; corrected.
Next GUI failed tick out of range on boundary drag; snapTick now clamps0..max_tick,
regression added for negative/overflow input. Raw wheel anchor regression added.
Final GUI1/1 PASS44.67s; final full145/145 PASS160.58s after all source changes. No timeout/assertion weakened:
changed fixed-grid expectations explicitly superseded by requested Smart behavior.
Historical failures/logs smart-snap-* retained; previews15/12/4 at100/150 inspected.
No user project/settings/keys changed by tests. Existing Undo/SaveOpen path covered by
GUI/native desktop regressions. Package `MR-Studio-0.3m-upd2-Smart-Snap-JUCE-ASIO-Windows-local`,13files/12hashes
verified, packaged AI startup smoke exit0. GUI SHA256
`62B95483D171B6C63F3A15002BDBCBA188085F4430B63DE938252A2B5C5199F7`.
Matching PDB deliveries/0.3m-upd2-smart-snap. MR Studio shortcut target/workingdir/
emptyargs/icon verified; previous0.3m upd1 manifest12/12 verified unchanged.
FEATURE READY WITH MANUAL CHECK: physical DPI and user-project grid/Snap acceptance
pending; no new hardware measurement. Source/build/package local, docs/issues only
GitHub; no source push/PR/merge/Actions. User should check
both zoom steps from screenshots and Smart snap/move/trim/import at several zooms.
Physical DPI/user ASIO not measured. No next roadmap issue started/source push/Actions.
