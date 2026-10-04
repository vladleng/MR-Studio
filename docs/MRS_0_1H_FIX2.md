# 0.1h fix2 — stable buttons on fader release

User verified fix1: buttons stay stable while dragging, but blink on release.
Release called the complete model/sidebar refresh, which resent unchanged button
labels, redrew the menu bar and rebuilt the track list after every mixer gesture.

Fader/pan release now commits the same single shared Undo command and updates
only history availability, the dirty title when changed, and the custom canvas.
It does not rebuild the sidebar or relabel transport/record/arm/monitor buttons.
Full model refreshes also compare button text/enabled state and menu availability
before changing native controls, preventing unnecessary redraw on other edits.
Fix1's paint/layout separation remains in place. Audio/model/persistence unchanged.

GUI smoke observes native WM_SETTEXT/WM_ENABLE on unrelated buttons during
release. The regression failed on fix1 and passed on fix2. Release checks cover
mixer gain, master gain and track mini gain; prior preview/idle/gesture/Undo and
150% layout checks remain enabled. Verification on the user's display is pending.

Check several repeated drag/release gestures on track, bus, master and mini
controls. Only a genuinely changed Undo/Redo availability or dirty title should
update; other buttons stay stable. Recheck Undo/Redo and save/reopen.

Package: MR-Studio-0.1h-fix2-ASIO-Windows-local. Code/build/tests stay local.
GitHub receives issue/documentation updates only; no code push, new PR or merge.
