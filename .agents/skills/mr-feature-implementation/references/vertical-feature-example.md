# Example: implementing Track Mute

This is an example of feature completeness, not a required architecture.

## User action
User toggles Mute on a track.

## Possible vertical flow

`TrackMuteButton`
→ command/action
→ `TrackModel::muted`
→ realtime-safe projection to mixer/engine
→ UI observers update
→ project serialization writes `muted`
→ undo transaction restores previous value

## Checks
- button reflects model, not only local widget state;
- mute survives save/load;
- undo/redo works;
- audio path changes without unsafe locking;
- Studio and Live show the same semantic mute state if they share tracks;
- tests cover model + persistence;
- build/test skill validates targets.

The implementation may differ; always follow the actual repository patterns.
