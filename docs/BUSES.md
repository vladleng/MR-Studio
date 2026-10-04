# 0.1g / MRS Stage 2b — buses and subgroups

Uses the same shared ProjectStore, AudioEngine, transport, device and persistence.
TrackKind::bus is a mixer channel without clips or an armed input. Audio tracks
and buses have one output: master by default, or a bus. Bus outputs can feed other
buses. Project candidate validation rejects missing/non-bus destinations,
self-routes and cycles before publishing changes or creating an Undo entry.

The shared renderer prepares a source-before-destination order off the callback.
Per sample it sums voices/monitor into tracks, applies track gain/pan/mute/solo,
adds each channel to its destination and processes downstream buses once.
Master processing/gain/clamp remain at the end; raw recording remains before mix.
No allocation, locks, file I/O or model access in the callback. At most 128 audio
tracks and buses together; existing fixed meters/parameter queue serve both.

Bus gain/pan use the existing 5 ms ramps and stereo balance (unity center).
Solo on a bus admits its upstream tracks/subgroups and its downstream bus chain.
Solo on a track admits that track and its downstream buses, excluding siblings.
Multiple solos combine; mute on any channel overrides solo at that channel.
Channel meters are post-fader and pre-destination; master is pre-clamp.

Creating/removing buses and changing outputs require Pause/Stop and no recording.
The existing device handle and sample position are retained while rebuilding.
Gain/pan/mute/solo may change during playback/recording; Undo of parameter changes
works during playback. Structural Undo needs Pause/Stop. Deleting a bus reconnects
its direct inputs to its own output; one Undo restores bus, controls and routes.

Core snapshot v4 stores bus kind and optional output IDs, reading snapshots v1-v3
with default master destinations. Archive envelope and MIXR layout are unchanged;
PROJ holds routing and MIXR mirrors shared controls. Older apps reject v4 rather
than silently dropping buses. Unknown archive chunks remain intact.

Mix provides + Bus, visibly labelled bus strips, Out: destination menus and Del
for bus deletion. Cyclic destinations are disabled. Output selection/deletion are disabled
during playback/recording; parameter gestures retain one-command commit/cancel.
Bus channels also appear in the project list for rename/reorder, without clips.

Sends/returns, arbitrary physical outputs, multi-input capture, routing/device
profiles and individual bus plugin inserts remain subsequent work in #22/#23.
All build/test/package validation is local, using cached ASIO/PortAudio sources.
