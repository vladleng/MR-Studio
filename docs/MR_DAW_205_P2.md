# MR Studio #205 P2 — Arrange and track headers

User-authorized 2026-10-16 after acceptance of Palette upd2. This is the Arrange/track-header stage plus the three explicitly requested Mixer surface/control refinements. P3 routing/Mixer restructuring, P4 Browser/dialogs and native-plugin rollout remain separate.

## Surfaces and selection

- Channel bodies (Mixer and Arrange headers): #2F3841, the previously accepted text-button background. Channel text/letter buttons and send controls: #39434E, slightly lighter than the body. Other buttons retain the accepted upd2 surface.
- Channel, clip, lane and range selections use the existing scoped Theme focus role #91BBEE. Legacy appearance retains its earlier palette. Musical current/selected states remain distinct through fill versus outline and dark text on light selections.
- Fader rail outline: 0.65 logical px; thumb: 0.7; rotary balance outline: 0.75. Value fills, white centre-deviation arc/segment, input ranges, thumb hit testing, keyboard focus and gesture semantics are preserved.
- Icon-only lane toggles have no button plate, using the glyph and restrained active/focus feedback. The text-labelled meter toggle keeps its button surface. Existing icon paths/meanings and hit areas are reused.

## Arrange density and presentation

The existing timeline axis, fixed left boundary, track order, musical-lane heights, source offsets, zoom/snap and editing coordinates are retained. Track-header layout adapts to height: normal headers keep the established controls; compact headers place gain, pan and input on one row, with no overlaps. Numeric gain/type metadata and meters are drawn only when vertical space permits; gain/pan/input and channel action buttons remain accessible even at the supported minimum track height. Full track names/type and input descriptions remain in tooltips.

Clip labels have a bounded 20 px header, an actual Audio/MIDI type glyph, and a text type badge when width permits. Narrow clips omit the optional badge/glyph before crowding their title. The actual waveform and MIDI-note preview remain below that header and use the original sample/tick mapping. No static waveform art, replacement audio decoding, PCM processing or decorative notes are introduced. Full clip names/types are available on hover.

Audio/MIDI/instrument/bus/FX-return track roles come from actual model fields; they are not guessed from names. The 8 base /24 clip-color scheme from upd2 and all saved RGB/templates remain intact. No project schema, commands, Undo policy, audio callback, device or plugin editor changes.

## Validation and handoff

Reuse the actual Desktop asset smoke, adding real generated stereo WAV and MIDI notes, long Latin/Cyrillic names, Audio/Instrument/Bus/FX-return tracks, five track heights (56/80/100/128/200), software 100/125/150/200%, and three window sizes. Check header control bounds, tooltip content, absence of project/history mutation during layout and exact Undo from the compact gain control. Existing full regression covers clip move/trim/split/copy/group, musical-lane actions, Save/Open, editor lifetime and transport.

QA, before/after images and test logs stay local under `build/asio-local/validation/ui205-p2/`. Matching source commit/package and actual PASS/FAIL are recorded in #205. Physical ASIO/listening/xruns, screen reader and hardware mixed-DPI require separate actual testing; software snapshots do not establish them. User acceptance of P2 is required before advancing to P3.
