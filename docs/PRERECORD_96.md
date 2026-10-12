# #96 — скрытая предзапись при pre-count

## Пользовательская приёмка — 2026-10-09

«Тесты прошли, принимай» — принят #96 в 0.2g upd4 fix1 (source77c9abd),
включая исправление преждевременной остановки Count. Issue закрывается completed.
Parent55 остаётся open; следующий #97 не начат. Детали устройства/длительности
не сообщены; полная hardware/sustained матрица и P4 этой приёмкой не закрываются.
Ниже сохранены исторические результаты доставки. Immutable пакеты/хеши не меняются.

2026-10-09: пользователь повторно поручил «Переходи к #96» после предложения
контракта. Рабочий контракт: до2 секунд в конце отсчёта, ограниченных доступным
pre-count и позицией старта. Видимый клип начинается в выбранной позиции;
left trim раскрывает source prefix, timeline не бывает отрицательным.
Stop до фактического старта не добавляет клипов. Loop overdub не входит в scope.

## Layers / state flow

SHARED CORE. AFFECTED: engine RT capture, application take finalization/preview,
audio media and MIDI source-offset projection, command/Undo tests, persistence
round-trip tests, docs. NOT AFFECTED: schema15 (existing source_offset), existing
trim commands, routing/PDC/plugin state, visual styling. UI reads corrected
preview; no new widget/settings. NOT APPLICABLE: separate Live app capability.

Control stops callback before preparing immutable recorder starts and count
metadata. Recorder source start is earlier than visible start; callback captures
only overlap with the prepared window while transport remains frozen during
count. Existing audio SPSC disk writer handles raw samples; no new RT file I/O,
allocation, mutex, UI calls or graph ownership change. MIDI capture uses existing
preallocated journal, retains notes/CC crossing visible start. NON-RT finalization
sets existing audio/MIDI source_offset, commits all takes in one AddTakes command.
Undo/Redo uses existing ProjectStore; Save/Open retains source material.

## Required validation

Audio+MIDI window and exact count boundary, raw input/click separation, early
note crossing visible start, no-count regression, zero-start, short count,
Stop/cancel before start, capacity/error prefixes, left trim/Undo/Redo/Save/Open,
RT allocation probes, configure/build/core+GUI tests and immutable local package.
## Local delivery / validation

Source2b0331f, branch mrs/0.2g-fix1-editor-space-local, version0.2g upd4.
Windows VS18 2026 x64 Release, cached JUCE9.0.3/ASIO/VST3 build/asio-local.
Configure FETCHCONTENT_FULLY_DISCONNECTED=ON and full build PASS.
Full serial CTest118/118 PASS(88.53s); then GUI-only coverage addition and rebuild,
focused GUI3/3 PASS(26.60s). Core new tests: combined audio/MIDI 2s prefix,
early note crossing start, raw samples unaffected by click, existing offsets,
left MIDI trim reveals source, one take command Undo/Redo, serialized roundtrip
and actual Save/Open, cancel before start, zero-origin, short exact audio window
crossing irregular count boundary, callback allocation probes0. Existing
transport/worker/PDC/mixed/anticipation/dropout/persistence tests remain green.
First extended fixture omitted transient arm after Open; fixed fixture, not model.
GUI new nonzero-origin fixture asserts preview offsets/visible starts for both
types. Preview paint uses source offsets for MIDI notes and audio peaks.
Software100/150 reviewed (short post-count clips, no hidden material drawn).

RT boundary: device callback owns capture/count time; disk worker consumes
SPSC; MIDI recorder fixed journal release/acquire published prefix. Application
prepares/finalizes after callback stop/join. No new queues or object lifetimes,
blocking, allocations or logging on callback. Existing fault-prefix retention
unchanged: if Stop happens after prefix capture but before visible start, no clip
is committed; a valid audio prefix file can remain available outside the project
clip list. No automatic deletion of captured material. No-count behavior retained.

Package MR-Studio-0.2g-upd4-Prerecord-JUCE-ASIO-Windows-local.
EXE SHA256:A06A065D1A0D3BB2D2412A2A9E015F24CAF8769B6C4A4D6427A02ECC92261858.
Matching symbols/helpers/assets/licenses, core recording test executable,
full+GUI test logs, previews and SHA256. Three packaged J3+diagnostics exit0;
packaged MIDI recording test PASS; packaged diagnostics helper tests PASS.
FEATURE READY WITH MANUAL CHECK / READY WITH MANUAL CHECK / RT SAFE WITH MANUAL
CHECK. Physical MIDI/ASIO/sustained/DPI NOT RUN.
Manual: Count on, seek to third bar, play/sing during final2 seconds of count,
record beyond start, Stop; original start stays aligned, left trim reveals early
material. Check early held note, Undo/Redo and Save/Open. MIDI keyboard suffices
for MIDI acceptance; audio audition if input available. Issue96 remains open
until user acceptance; no Stage5, next sub-issue, push, Actions or release.

## upd4 fix1 — premature stop during pre-count

User reported a few clicks followed by no recording. Reproduced by running
Application::poll after every count callback: old code fails app.recording().
Hidden capture now contains frames/events while playback intentionally stays
paused; the previous automatic-stop condition confused count with a real pause.
NON-RT poll reads one coherent engine state and only treats non-playing capture
as stopped when count_remaining is zero. Fault/device-loss checks remain active.
No callback, capture ownership, schema, command or visual-style changes.
Regression coverage: combined audio/MIDI poll on every callback; MIDI-only
one/two-bar count, Pause cancellation before start, Pause after real recording;
GUI combined count/prefix poll. Required final validation and manual ASIO retry
are recorded in PROJECT_CONTEXT. Previous upd4 acceptance is not claimed.

Source77c9abd, cached configure/full Release PASS; full serial CTest118/118
PASS(88.67s). Separate fix1 package J3+diagnostics exit0, packaged MIDI recording
and crash diagnostics helper checks PASS. EXE SHA256:
AEDB78FBC0C6086E66D7CA9E876502EB4E04618DD770D57C6B84150CCB510928.
RT SAFE WITH MANUAL CHECK: only NON-RT poll changed, one existing coherent
engine snapshot; capture callback/queues/lifetimes unchanged. Physical ASIO/MIDI
retry remains NOT RUN, issue96 open pending user acceptance.
