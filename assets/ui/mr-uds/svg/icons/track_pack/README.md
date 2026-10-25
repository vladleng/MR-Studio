# MR Studio — Track Type Icons Pack v1

This additive set extends the canonical `assets/ui/mr-uds/` vector library. It does **not** replace the previously accepted 8 track identities (16 SVG variants) or the existing Vocal (`icons/mic`), Bus and Folder glyphs.

## Delivered

- 26 original **new** type identities, each with individually simplified 24×24 and 16×16 geometry (52 new SVG files).
- 8 existing type identities preserved and referenced, rather than duplicated.
- Existing mic/vocal, bus and folder SVGs reused for those types.
- Machine-readable catalogue at `../../catalogs/track_type_icons_v1.json`.
- Visual gallery at `../../../previews/track_type_icons_v1.html`.
- 289 SVG entries in the canonical manifest after addition.

The artwork uses a flat transparent canvas and UDS-compatible line work: 1.6 px at 24 and 1.3 px at 16, round joins/caps. Neutral lines use `#A9BFDC`; music-type accents are `#91BBEE`, `#00B8EF`, `#E2B44A`, `#B45DF0`. Accent tint is identification, **not** recording/ready/error state.

## Use in JUCE / MR Design Lab

Resolve catalogue IDs from `manifest.json`. Use a 24-px icon in regular track headers and genuine 16-px LOD in compact headers. Keep icon presentation frameless; the host supplies a larger pointer target, tooltip/accessibility name and keyboard focus. Selected/hover/disabled feedback is supplied by the host's semantic Theme and image tint, **not** by inventing static state SVGs. Clip, MIDI/audio type and track-role assignment must come from actual host metadata; no guessing instrument from title. Future editable user-assigned track icon is separate product behavior.

These files are an **asset catalogue only**. No C++ UI placement, persistence, command handlers, meter readings, DSP, or build/package behavior is changed. Runtime and end-user visual acceptance need a separate scoped integration check; no Windows/ASIO/DPI claim is implied.

## New identities

- `track_audio` — Audio
- `track_midi` — MIDI
- `track_instrument` — Instrument
- `track_acoustic_guitar` — Acoustic guitar
- `track_upright_bass` — Upright bass
- `track_electric_piano` — Electric piano
- `track_organ` — Organ
- `track_percussion` — Percussion
- `track_drum_machine` — Drum machine
- `track_violin` — Violin
- `track_cello` — Cello
- `track_harp` — Harp
- `track_trumpet` — Trumpet
- `track_trombone` — Trombone
- `track_saxophone` — Saxophone
- `track_flute` — Flute
- `track_clarinet` — Clarinet
- `track_marimba` — Marimba
- `track_sampler` — Sampler
- `track_loop` — Loop
- `track_audio_input` — Audio input
- `track_aux` — Aux return
- `track_automation` — Automation
- `track_video` — Video
- `track_reference` — Reference
- `track_accordion` — Accordion
