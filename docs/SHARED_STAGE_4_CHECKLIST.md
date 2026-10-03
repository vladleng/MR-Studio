# SHARED Stage 4 — Windows acceptance
Issue #19. Stage 3 accepted; deferred Stage 1 hardware/performance work stays in #16.

1. Download and extract MR-Studio-SHARED-Stage-4-Windows from Core contracts.
2. Double-click Start-Persistence-Test.cmd in the extracted folder.
3. Expect four PASS lines and "SHARED Stage 4 check passed".
4. Send the output or confirm all checks passed.

The checker uses a unique temporary folder, creates/reopens project/show/autosave
files and removes its own test folder afterwards. It does not modify your music.
No ASIO device, plugin installation or MIDI keyboard is required. On reopen the
transport is stopped and no authored MIDI action executes.

Automated CI additionally tests corruption, migration, unavailable plugin state,
backup preservation, pre-replace fault injection and background autosave failures.
Acceptance authorizes closing #19 and moving to the next SHARED stage; GUI/shell
and actual hardware/plugin hosting are separate stages.
