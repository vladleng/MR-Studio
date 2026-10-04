# 0.1h / MRS Stage 2c — sends, returns and track controls

Status: implemented and tested locally; user acceptance pending. 0.1g is the
last user-accepted version. Code publication, PR creation and merging require
a separate user request; GitHub holds issue/documentation updates only.

## Shared routing

Audio tracks and buses have up to eight sends, each to a distinct bus. A return
is an ordinary shared bus with its own gain, pan, mute, solo, meter and output.
The main output remains active alongside sends. Send levels are linear 0..16.
Pre-fader taps the channel sum before gain/pan; post-fader taps after them.
Both obey channel mute and solo admission. Return controls affect the combined
return signal. A solo source admits its destinations; a solo return admits its
upstream contributors. Mute overrides solo. Zero-level sends remain graph edges.

Main outputs and sends share one validated DAG. Self routes, missing/non-bus
destinations, duplicate sends and cycles involving either edge kind are rejected
before model mutation. Deleting a return removes sends to it; main inputs follow
its former output, as in 0.1g. Undo restores the return and all original routes.

The engine prepares a topological order outside the callback. At runtime it
uses fixed channel/signal/control arrays. Live send levels, gain/pan/mute/solo
and master changes use the bounded SPSC queue and 5 ms ramps. Callback tests
verify zero allocations. Route addition/removal and pre/post switching require
Pause/Stop; the transport position and open device remain intact.

## Input per track

Audio tracks store a physical mono input index, Default (-2), or Off (-1).
Default uses Audio settings. The choice is applied when that track is armed;
choosing an input on an unarmed track does not switch the current recording input.
Only one armed audio track and one mono hardware input are recorded at a time.
Selecting another physical input while armed requires Pause/Stop and reopens
the ASIO stream, retaining the stopped/paused position. Missing inputs are
rejected; the app does not silently substitute another physical channel.
Recorded WAV data remains the raw input, before track/return/master mixing.
Multi-input capture and hardware output profiles belong to later slices.

## Project and preferences

Core snapshot v5 stores sends and input selection; versions 1–4 migrate with
no sends and Default input. The archive envelope remains unchanged. PROJ stores
routing while MIXR continues mirroring shared gain/pan/mute/solo. Older apps
reject v5 rather than discard routing. Undo history remains runtime-only.
Send level Undo/Redo works during playback; topology/input changes need Pause.

Desktop preferences v3 keep up to ten recent absolute project paths, bounded to
4096 UTF-8 bytes each; the complete config is bounded to 64 KiB. Config v1/v2
still loads. Successful Open/Save/New/Save As updates the MRU list. Missing or
invalid recent projects report an error without replacing the current session.

## Interface

Files → Open recent project lists saved/opened projects and survives restart.
Mix toggles a lower panel inside the main arrangement window. The uncovered
arrangement remains editable; no extra mixer window or engine is created.
Chord/section strips compact while Mix is open. Resize the main window or hide
Mix when more arrangement height is needed. Mouse wheel over Mix scrolls channels.

Mixer gain faders and stereo meters are vertical; drag vertically to change gain.
Double-click restores unity gain or center pan. Sends opens per-send menus with
destination names, pre/post mode, level choices, Off and Remove. Add send chooses
an existing bus; Create return and send creates a new return. Structural choices
are disabled during playback/recording; send levels remain adjustable.

Each arrangement lane has a left mini panel: horizontal gain fader, horizontal
peak meter, round pan control dragged horizontally, and an audio input menu.
Mixer and mini panels edit the same channel values. One fader/pan gesture commits
one Undo command; Escape or capture loss cancels its preview.

## Local verification

Windows x64 ASIO Release: configure with cached dependencies and disconnected
FetchContent, build, CTest 65/65 and expanded GUI smoke passed. Coverage includes
pre/post sums, nested returns, mute/solo, cyclic edges, zero callback allocations,
archive roundtrip, return removal/Undo, live send Undo, input rebind using a test
device, MRU/config migration, vertical gestures, mini controls and 150% layout.
Physical ASIO/user acceptance is pending; the deferred long benchmark #16 stays
pending. GitHub Actions were not used.
