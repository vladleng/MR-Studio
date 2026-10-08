# Project home and smooth playheads — 0.2f upd6

User request: 2026-10-08. The user accepted upd5: «Тесты прошли успешно».

## User contract

On normal startup the JUCE app shows Project home before the workspace. It starts
with an empty internal project and connects saved hardware audio after New/Open.
The list finds regular `.mrsproject` files recursively under the Windows Documents
folder's `MR Studio` directory. Directories and symbolic links are excluded. File
extensions are case insensitive. Newest modified projects appear first; relative
paths distinguish duplicate names. Searching filters names/relative paths.
Single left click or Enter opens through the existing Desktop/Application loading
path. New project, Open chooser and Refresh remain available. File → Project home
returns to the list and pauses transport while retaining the current project.
Continue current project returns to that same session. Replacing/closing a
session retains the existing unsaved-change guard.
Cancellation or missing/corrupt files keep the home visible and show the error.
Closing the initial home does not offer to save an invisible empty project.

Discovery uses a cancellable background worker; painting performs no file scans.
No user file is created, moved or deleted by discovery. A missing folder is an
empty library. Save/Open choosers start in `MR Studio/Projects` for a new project.

Arrange, piano roll and controller lane use separate transparent cursor components
with 60 Hz message-thread timers. Moving a cursor invalidates its narrow old/new
strips. Content synchronization, 30 Hz metering and MIDI preview rates stay as
before. Fractional-pixel drawing avoids truncating every position to an integer.
All three cursors use one message-thread VisualTransport fed by the existing
bounded coherent engine state mailbox. It interpolates toward the observed sample,
never extrapolates beyond it. Normal display trails by roughly one UI frame; the
interpolation window is capped at 50 ms. This introduces no audio buffering.
Pause/Stop, backward movement/loop wrap, large forward seeks, project replacement
and device reset re-anchor the display. Small forward seeks converge within that
short display window. If callbacks stall, the display stops at the last sample.
Tempo-segment conversion preserves fractional ticks for the MIDI cursor.

## Layer impact and realtime boundary

| Layer | Impact |
|---|---|
| UI and notifications | AFFECTED: home list, cursor components and safe async opening |
| Commands/actions | AFFECTED: existing New/Open and discard guard, File home entry |
| Model/domain/engine | NOT AFFECTED: no transport or recording semantics change |
| Persistence/schema | NOT AFFECTED: schema 13; library/filter/display positions transient |
| Undo/redo | NOT AFFECTED: opening retains existing history policy; viewing is not an edit |
| Project files | AFFECTED: read-only discovery and chooser initial directory |
| Realtime audio | RT-ADJACENT read only: existing try_state; no callback changes |
| Studio/Live | Studio JUCE UI; shared Application load path reused |
| Tests/docs | AFFECTED: J3 home/cursor regressions and this contract |

All added scanning, sorting, interpolation, UI timers and painting run off RT.
Existing DSP/worker ownership, PDC, raw capture and graph teardown stay unchanged.
No new callback allocation, lock, file access, GUI call or plugin query.

## Verification

J3 fixtures cover an initial empty home, nested projects, uppercase extensions,
directories with a project suffix, unrelated files, duplicate names, async scan,
search, click/open, return home, missing-file error and New. Fixtures are temporary;
the user's project directory is never changed. Software snapshots cover 1200×700
and 1400×850 at 100%/150%.
Deterministic visual-clock checks cover 44.1/48/96 kHz, callback blocks
128/256/2048, 60 Hz updates, bounds/monotonic motion, Pause/Stop, Seek, loop wrap
and a stalled device. Physical ASIO motion and mixed DPI need user verification.
Build/test/package results are recorded in PROJECT_CONTEXT after completion.
