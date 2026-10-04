# 0.1h fix1 — stable native buttons during mixer updates

User verified the local 0.1h functions and reported that other buttons blink
while dragging faders or opening menus. The fix is local; user verification of
fix1 is pending. Code push/PR/merge requires a separate request.

The mixer paint path called MoveWindow(+ Bus, repaint=true) on every frame.
The main paint path also called ShowWindow and EnableWindow for native buttons.
These window operations generated layout/erase traffic during fader previews
and during the timer messages dispatched by native menu loops.

Paint now draws only. Workspace/model/layout updates synchronize native controls;
visibility, enabled state and bus-button bounds are applied only when changed.
The timer checks control state outside painting. Selecting the already selected
channel no longer rebuilds the sidebar and relabels native buttons.
Double buffering and WS_CLIPCHILDREN remain in use.

GUI smoke instruments native button WM_WINDOWPOSCHANGING/owner-draw traffic.
It explicitly exercises rendering through WM_PRINTCLIENT, including with a
hidden test window, during repeated fader previews and timer/idle frames.
The regression failed before the fix (fader repaint triggered native layout),
then passed after the fix. Existing gesture, routing and 150% layout smoke
checks remain enabled. Shared audio/model/persistence code is unchanged.

For user verification: drag mixer/master and track-mini faders repeatedly;
leave Files/Recent/Sends/Out/Input menus open while meters update. Other buttons
should remain stable. Also switch Arrange/Edit/Mix, resize the main window,
and start/pause playback: + Bus must move once with layout changes and become
disabled during playback, enabled again while stopped/paused.
