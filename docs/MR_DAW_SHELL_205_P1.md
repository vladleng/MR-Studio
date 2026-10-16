# MR Studio #205 P1 — scoped shell/navigation/transport

P0 was accepted by the user on 2026-10-16. P1 is a separate opt-in candidate pending visual acceptance. P2–P6, #206 and Rig are not automatically started.

## Product boundary and entry

`--uds-shell` explicitly enables the new shell in a normal Studio session. The flag is process-local, with no preference/project/schema migration. Without the flag, the accepted appearance remains the default.

`--uds-shell-preview` opens the same Desktop in testing mode with the built-in private demo and existing offline clock. It does not read/write normal UI preferences, load recent projects or reconnect hardware. No ASIO setup is needed to inspect this candidate. Explicit user project/file/record commands still retain their normal command behavior; the preview is not a second DAW or audio engine. Existing accepted shortcuts/packages are preserved.

## Composition, Theme and assets

The shell uses real shared MRButton controls and the existing final ui::Theme class. An instance-scoped Theme is borrowed only by the main toolbar, transport/navigation buttons, navigation target and shell/context/status labels. Desktop.theme and its child Arrange/Mixer/Browser/native/external editor consumers stay accepted.

Controls remain children of Desktop: command callbacks, component titles, keyboard parentage, explicit focus order and native editor ownership are retained. No alternate LookAndFeel class, renderer hierarchy, button overlay or command proxy is introduced. The scoped Theme is detached from every borrower before teardown. Turning the opt-in off restores inherited Theme and the accepted layout.

Transport artwork is the same original embedded SVG already cached by Theme (play/pause/stop/record/previous/next/loop). There are no new SVG files, duplicate icons, generator runs or vendor-owned GUI changes. Only buttons explicitly tagged mrShellTransport use the new flat background and semantic icon contrast; accepted Gallery/controls/native transport styling is not globally altered.

## Layout and roles

The existing content starts at y94 and retains the same Arrange/Mixer/Browser geometry and dividers. Shell paint is restricted to the upper toolbar/context region and existing footer; the menu bar and all popup/dialog migration are deferred to P4.

The main toolbar has 32px-high Kit targets, compact 4px spacing and separated tool/history/edit/zoom groups. Click/count stay on the right. Footer transport is grouped with its section/marker selector; workspace buttons stay on the right. Text transport/device status and CPU/latency stay on the left, with a separate position value. No Live button or Inspector is fabricated.

The existing semantic DesignTokens target supplies window/deep/border/text/focus/on/warning/error roles. Play uses On, Pause uses warning, recording uses error; focus has its own border. Status also has literal text, not color alone. Mixer/strip font12/scale1 and accepted menus remain outside this scoped change.

## Host semantics and status

All button callbacks remain the original Desktop actions, including Play30/Pause31/Stop32/Record33, section/marker navigation, loop, Edit/Mix/BROWS, Pointer/Knife, Undo/Redo, zoom/snap, metronome and count-in. Space retains its existing pressed/released latch and stop behavior, with modal/text/ComboBox guards. F2/F3/F5 remain the existing workspace shortcuts. No new transport or recording command is introduced.

Status is read on the existing UI refresh path from the authoritative transport snapshot, recording state, device name, count_remaining and existing processing-fault metrics. It displays STOPPED, PLAYING, PAUSED, COUNT-IN, RECORDING, PROCESSING FAULT or NOTICE. Generic Desktop messages retain their unclassified NOTICE meaning rather than inventing an error severity. Existing full message/count-in painting remains available. The full status is also exposed as description/tooltip; Offline clock and Disconnected are not called Ready.

Record/loop enable/toggle policy, navigation restrictions while recording, device reconnect, model/Undo, raw recording, PDC and project Save/Open semantics are unchanged. Status/position creates no project history. No meters, smoothing rates, processing queues, DSP or callback code are modified. The existing 30Hz Desktop timer drives this UI; no new timer is introduced.

## Validation contract

CTest juce_uds_shell uses private Desktop/manual audio fixtures. It checks the actual common MRButton class, retained keyboard parent/focus order, non-overlapping32px targets, unchanged content geometry and pixel identity for Mixer+VST3 / Arrange+VST3 / Arrange+Files at1200x700/1400x850/1800x1000 and100/125/150/200 percent.

Additional checks exercise real Play/Pause/Stop, Space latch and text guard, F3/F5, Pointer/Knife, metronome, toolbar Undo/Redo, actual count-in/recording/navigation lock/Record stop, notice/disconnected states, project round-trip and repeated Theme detach/attach. Recording fixtures write unique files in the task QA directory; no physical device is opened. UI status timing is diagnostic, not a callback/xrun/FPS measurement.

The candidate requires the affected GUI checks, current full CTest, visual review and packaged smoke before user acceptance. Software screenshots do not establish physical monitor-transfer DPI or screen-reader behavior. Physical ASIO/listening/xruns/mixed-DPI/screen reader remain NOT TESTED unless separately exercised; previous user ASIO acceptance is retained.

QA and screenshot evidence remain local in build/asio-local/validation/ui205-p1/. GitHub receives docs/issues only [skip ci]. #205 stays open until its separately accepted later stages; P1 does not close it.
