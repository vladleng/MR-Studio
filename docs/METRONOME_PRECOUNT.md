# 4h / 0.2h — metronome and recording count-in

User authorized 2026-10-08, before 4g. Scope remains #70; no external MIDI.

## Contract

Transport menu and Click/Count controls: separate playback/record click switches,
first-beat accent, 10–100% click level, count-in Off or 1–4 bars. Defaults off,
accent on, 20% level. Application preferences v7 read v1–6 with safe defaults;
project schema remains 13. Preference changes are operational, not project edits
and not Undo operations; they do not dirty projects. Recording freezes settings.
Take creation retains one existing Undo/Redo and portable Save/Open.

Click follows project tempo/meter maps and loops at rounded sample boundaries.
Prepared tone buffers avoid callback synthesis allocations. Click goes to the
selected Master hardware outputs after inserts/PDC, before Master gain; it does
not pass through track FX. It is never injected into raw capture. Physical
acoustic/electrical input feedback is outside that guarantee; use headphones.

Count-in uses tempo/meter at the selected record position, repeats complete bars
without playing/recording project content or moving the project cursor. This
also works at project zero. Monitoring stays active. Visible remaining seconds;
record starts at the original cursor exactly after count-in, splitting a callback
at the boundary when needed. No counted samples/events become take content.
Stop/Pause cancels; empty temporary audio files are removed by existing finish.
Settings and armed-track validation occur before start. Loop recording remains
unsupported, as before. Count-in is audible even when recording click is off.

## Layer impact / ownership

UI, Application actions, engine, preferences persistence, notifications, tests,
docs AFFECTED. Project model/schema NOT AFFECTED; file conventions NOT CHANGED.
Undo AFFECTED only through existing finalized takes, settings NOT APPLICABLE.
SHARED CORE engine/Application; JUCE controls Studio UI; future Live uses shared
semantics, no new Live screen. External MIDI DEFERRED to 4g.

NON-RT prepares immutable timing/tone buffers and files. RT-ADJACENT uses existing
bounded SPSC controls and versioned state mailbox; stop/join before prepare.
RT-CRITICAL schedules clicks and gates capture/count transitions without heap,
locks, I/O or UI calls. Whole-graph anticipation renders clicks with its owner;
mixed producer disables clicks, device renders them once. Count status is transient
and cannot resume on Save/Open. Existing plugin ownership/PDC/raw capture retained.

## Validation

Required: variable blocks, sample rates, tempo/meter/loop/seek clicks, accented
beats and gain; no callback allocations; count boundary/input suffix, MIDI
precount exclusion, cancellation, multiple takes, Save/Open and one Undo;
preference migration/invalid values; GUI controls/count snapshots 100%/150%;
full local configure/Release/CTest and packaged smoke. Real ASIO audition and
physical mixed-DPI are separate user acceptance; no hardware PASS inferred.

During development, the new aggregate fields initially broke a legacy graph
initializer; moving them to the end preserved source compatibility. Enabling
click in anticipation regressions exposed a tail mismatch on same-position
seek/restart, fixed with explicit click reset at seek/rebase. One intermediate
J3 run exited 0xc0000374 without diagnostic text; a later completed rebuild and
three consecutive J3 runs passed. Cause of that isolated intermediate crash was
not established; do not describe it as proven fixed or as a hardware result.

Final cached configure/full Windows x64 Release PASS, no compiler warnings/errors.
Full **114/114 CTest PASS (64.41 s)**. New audio_metronome: 98 checks, 44.1/48/96
kHz, tempo/meter/accents, loop, level/off, cancellation and callback allocation 0;
193-frame count inside a 128-frame callback records only the 63-frame input suffix.
JUCE combined audio/MIDI: count exclusion, exact start at zero, repeat/cancel,
stable insert_generation, one Undo/Redo and portable Save/Open PASS. Preferences
v7 round-trip/older defaults/invalid values PASS. Click-enabled serial/ahead/mixed
sample equivalence/all-thread allocation tests PASS; seek/restart and J3 repeated
three times. Fresh software count UI 100%/150% reviewed. Hardware NOT RUN.
The mixed parameter test deliberately disables click in both references: its
fresh mid-beat DSP graph has no independent ongoing click tail. Other equivalence
and raw-capture suites retain click enabled; no production fallback was added.
FEATURE READY WITH MANUAL CHECK; RT SAFE WITH MANUAL CHECK. #70/#24/P4 not closed.
