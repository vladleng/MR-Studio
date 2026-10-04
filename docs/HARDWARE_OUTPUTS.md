# Hardware outputs and device profiles — 0.1i

Implemented locally on the shared Audio Engine; hardware/user acceptance pending.

## Routing

Audio tracks and buses have one main destination: Master, another bus, or a
physical mono/stereo output. Sends remain independent and can feed other buses.
Master also chooses a physical mono/stereo destination. An empty Master route
uses the first one/two outputs selected in Audio settings; extra outputs stay
silent unless explicitly routed. Channels are physical zero-based indices in
the project, displayed one-based in the UI. Selected stream order may differ
from physical order; the application maps indices when preparing the graph.

Mix → Out offers buses, physical mono outputs, adjacent stereo pairs and
non-adjacent/reversed pairs selected together in Audio settings. Inactive
physical channels are disabled. Pause/Stop is required for routing changes.
The same handle, position and loop survive graph rebuilds; Undo/Redo share the
project command history. Deleting a bus reconnects its incoming tracks to that
bus's main destination, including hardware destinations; Undo restores it.

Use Master for Main/FOH and ordinary named buses for Monitor, Click and Cue.
Create a bus, rename it and choose its Out; feed it with track outputs or sends.
These buses route existing audio/monitoring. Click generation, cue events,
show/setlist UI and simultaneous multi-input capture belong to later stages.
Each mixer channel remains mono/stereo even with more physical outputs enabled.

Direct hardware destinations bypass Master processors and gain. Their own
track/bus gain, pan, mute/solo and meter still apply. Master sends only its own
processed sum to its destination. Overlapping physical destinations add and
the final hardware sum clips at ±1; this is counted in engine metrics. Mono
hardware destinations average L/R; a single-channel render retains unity.
Meters display channel/Master signals before physical summation; a hardware
aggregate meter and separate bus processor inserts are future work.

## Availability and persistence

All explicitly routed outputs must exist and be selected when connecting.
Missing channels report the channel number and mixer channel name; routes
are never silently redirected. Validation errors for channels, rate and buffer
are detected before closing the existing connection. Driver open failures can
still leave the device disconnected and are reported by the existing error UI.
An offline clock can open, edit and save projects with unavailable hardware
routes; it is a silent transport clock. Resolve routes before connecting sound.

Core snapshot v6 writes per-track hardware routes and Master routes and reads
v1–v5 with empty routes/default destinations. Old applications cannot read v6;
keep an older project copy if switching back to 0.1h. The project archive
container schema is unchanged. Cached device channel names avoid enumeration
of an open ASIO stream during input/output selection and Undo/rebuilds.

## Device profiles

Audio settings stores up to 16 named profiles in local desktop preferences.
Each profile contains the exact device name, sample rate, buffer size, ordered
physical output selection, optional mono monitor input, and selected channel
labels. Save captures validated fields; the same name replaces that profile.
Load validates availability/labels and restores fields. Press Connect to apply
them; loading alone never interrupts audio. Delete removes the selected profile.
Refresh re-enumerates devices after connection/hardware changes.

Profiles resolve session-local device indices by name. Missing devices/channels,
changed channel labels or a project-rate mismatch produce an error. An unchanged
loaded profile is rechecked against fresh device enumeration before Connect.
Manual field edits are treated as a new user-selected configuration. Actual
driver support for a rate is confirmed by opening the stream; no resampling is
provided. Profiles do not clone project routing; routes travel with the project
and remain undoable. Preferences config v4 reads v1–v3 and has a 512 KiB limit.

## Local verification

Windows x64 ASIO Release configure/build used cached dependencies with
FETCHCONTENT_FULLY_DISCONNECTED=ON; no installation or GitHub Actions.
69/69 CTest passed. New suites cover snapshot migration, Undo/delete routing,
physical selector permutation, independent Master/Cue, mono fold-down,
overlap/clipping, stopped monitoring, callback allocation checks, missing-route
rejection without losing the connection, profile roundtrip and invalid profiles.
The manual test driver rejects enumeration while open. GUI smoke covers real
profile Save/Load/Delete controls, partial typing, 150% DPI bounds and all prior
fader/menu/Audio settings flicker regressions. Real ASIO multi-output acceptance
and the deferred long performance benchmark #16 remain pending.
