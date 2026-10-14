# MIDI editing / 0.3m upd1 — 2026-10-14

User accepted #166 and requested this separate maintenance build. Parent #26
remains open; native completion3/11 (#164/#165/#166). Other issues not accepted.

## Contract and layer impact

Arrangement display uses one meter-aligned sequence: subdivisions when wide,
beats at medium zoom, bars / powers of two bars at narrow zoom, minimum14 logical
pixels between drawn lines. Visible-range generation; labels at least60px apart.
Display thinning never changes snap, musical ticks or clip positions. Meter/tempo
segments supported; main ruler and musical lane backgrounds share paintGrid.

S / Split MIDI, G / Merge MIDI Clips, D / Duplicate. Merge also in Edit menu.
G needs two or more selected MIDI clips on the same existing instrument track;
audio/mixed/cross-track/stale/duplicate selections rejected atomically. Result spans
earliest start through latest end, preserving gaps/overlaps, visible notes/pitch/
velocity/channel and supported channel events. Trimmed notes cropped; controller
chase materialized at original clip start. Sustain/bend releases retained at original
ends. Split retriggers retained (notes crossing a split remain separate); hidden
source outside original clip windows discarded by consolidation. New clip/note/event
IDs; old sources restored exactly by one Undo, Redo restores committed IDs.
No new project schema: existing MidiClip, project serialization/archive.

UI/actions, shared Core command, Application/engine projection, common Undo,
persistence/tests/docs AFFECTED. Studio/Live domain shared. Cache/preferences/file
layout NOT AFFECTED. RT-ADJACENT transaction; callback/DSP NOT AFFECTED.
Application builds candidate/TimingPlan off callback, validates unchanged map/rate/
routing and budgets, stops device and joins processing, swaps only MIDI plan,
stages/publishes Store. Backend restart failure swaps back/rolls back model/history.
S/G/D MIDI and their named history preserve processors/editors/head/loop/generations;
requires stopped/no recording/no count-in. Existing audio path retained.
Shortcuts use existing modal/TextEditor/ComboBox/plugin focus guards; Ctrl+D existing
musical-lane alias retained. D targets the existing Duplicate operation/selection.

## Validation and delivery

Source `4c1d5baf49a78da39fad7b36b41df9cd1e274e68`,9source/test files, branch
`mrs/0.2g-fix1-editor-space-local`. Release configure/build PASS. Focused5/5
PASS28.29s before final display-bounds independence from Snap; final full145/145
PASS147.23s after all code changes, including final GUI Snap on/off spacing tests.
Core trimmed/gapped source, chase/CC/program/bend, visible note timing/pitch/channel,
invalid/stale/duplicate/cross-track atomic rejection, retrigger boundaries,
Undo/Redo/serialization; Application VST3 fixture generations/loop/backend rollback/
SaveOpen; GUI D/G/real focused typing/min14px at7zooms/Snap off+on/meter transitions
PASS. Fake-device split/merge/history0.6049ms, not actual ASIO/project latency.
Grid24/30 at100/150 inspected; no new DSP/callback work.

Initial build failed strict unused-variable warnings in new test; corrected. Initial
GUI keyboard fixture lacked a native peer/focus; added temporary test peer and asserted
real focus. Native image pixels were read before deferred rendering finished; pixel
fixture now explicit SoftwareImageType with Graphics scope closed before reads.
Historical failure logs retained; no assertions/timeouts reduced. Logs
midi-edit-upd1-* in build/asio-local. No normal user project/device/settings mutated.

Package `MR-Studio-0.3m-upd1-MIDI-Editing-JUCE-ASIO-Windows-local`,13files/12hashes
verified; packaged --ai-assistant-smoke exit0. GUI SHA256
`E99E0C830CFEAB7C046C7F46A030E8CD876AB85E74F1AAF79B867B7D2843B465`.
Matching PDB deliveries/0.3m-upd1-midi-editing. MR Studio shortcut target/workingdir/
empty args/icon verified; previous0.3m package and earlier builds preserved.
FEATURE READY WITH MANUAL CHECK / RT SAFE WITH MANUAL CHECK: user should restart,
zoom out, Split S, select MIDI clips on one track→G, Duplicate D, Undo/Redo/SaveOpen.
Actual ASIO split latency and physical DPI NOT RUN. #166 accepted separately;
this maintenance build awaits user verification, no other roadmap issue started.
Source/build/package local; docs/issues only GitHub, no source push/PR/merge/Actions.
