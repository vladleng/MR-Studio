# 0.1h fix3 — Audio settings flicker

User accepted fix2's fader-release fix and reported continuous Audio settings
flicker. The shared 33 ms UI timer invalidated the entire audio dialog. Its
paint handler filled the window directly, without buffering or WS_CLIPCHILDREN,
allowing background redraw to interfere with native edit/combo/button controls.

Audio settings now has WS_CLIPCHILDREN, double-buffered painting and a handled
WM_ERASEBKGND. The timer polls displayed status at most every 250 ms, compares
cached formatted values, and invalidates only the lower status region when they
change. Labels, fields and buttons are excluded from these periodic updates.
Latency/CPU display uses two decimal places. Open/Connect/Disconnect/Refresh
updates status immediately. Paint reads cached strings instead of querying the
backend. Existing field text is changed only if it differs.

GUI regression failed on fix2: timer invalidated static labels/input controls.
On fix3, changed status updates the lower region, unchanged status invalidates
nothing, WS_CLIPCHILDREN protects children, and background erase is suppressed.
Existing partial-input/typing, fader-release, preview/idle and 150% layout smoke
checks also passed. Local Windows x64 ASIO Release build passed. Shared audio,
model and persistence unchanged; prior 65/65 CTest baseline remains applicable.

User display verification of fix3 is pending: keep Audio settings open while
playing, type values without connecting, expand the device list and drag main
faders. Fields/buttons/labels should remain stable while CPU/latency can update.
Connect/Disconnect/Refresh and DPI changes must still work normally.

Package MR-Studio-0.1h-fix3-ASIO-Windows-local. Development and binaries stay
local; GitHub issues/docs only, no code push/new PR/merge/GitHub Actions.
