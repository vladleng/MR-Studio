# MR Studio 0.2g — MIDI workflow release

Windows x64 / JUCE / ASIO / VST3. Version 0.2g intentionally follows 0.2h upd1.
Includes all current local development: MIDI input/instruments, MIDI clips and
recording, piano roll and musical editing, CC/Program Change, metronome/precount,
clip Duplicate/Alt-copy, crash diagnostics and external Windows MIDI output.

User acceptance: «Все работает!» (2026-10-08), available MIDI workflow accepted.
External receiving hardware is unavailable. Physical external MIDI routing/timing
is NOT RUN, not PASS. Stage 4g/#69 and Stage 4/#24 retain this validation gate.
Stage 5 is explicitly paused before implementation by user instruction.

Source implementation: `959ffbc`; acceptance: `88c2e99`. Local Windows x64 Release
validation: 118/118 CTest PASS (76.42 s), three packaged J3/diagnostics smoke runs
exit 0, packaged crash tests PASS, software previews 100%/150% inspected.
Package EXE SHA256: `89E8E76EE6282E8D1B7930F4AA52C9110DA85402FE4CB753ECFCC0E8D0C8D1FA`.
Published ZIP uses the preserved tested package, matching symbols and manifest.
Post-merge cached configure and clean-first Release PASS, repeated CTest
118/118 PASS (47.46 s), including GUI/crash smoke. No GitHub Actions.
Windows ZIP SHA256: `3377515AE685404B0EC752F867287E743A718BF83586806DAAF6371451A9087A`.
Symbols ZIP SHA256: `2FAEBDAFF5CE4AA7F1DCB856E632629C88650F79055529F7F095380BD6074EC8`.

Project schema 14 reads 1–13; earlier builds cannot read newly saved schema 14.
Save As a copy. Preferences v7 unchanged. No user projects or settings bundled.
No universal crash-free or sample-accurate hardware-output claim.

[External MIDI contract](MIDI_EXTERNAL.md) · [Checklist](MRS_STAGE_4G_CHECKLIST.md)
