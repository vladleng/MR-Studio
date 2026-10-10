# MR Strip Link — linked control across independent channel strips

**Status:** approved design direction / backlog. **Not implemented.** 2026-10-10.  
**Architecture parent:** [#133 Pre-Spatial Routing](https://github.com/vladleng/MR-Studio/issues/133).  
**Integration:** [PRE_SPATIAL_ROUTING.md](PRE_SPATIAL_ROUTING.md); future native MR Strip belongs to [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111), but **this document is not an instruction to start that plugin**.

## Product purpose

Control **several native MR Strip instances from a single participating instance's UI**, while each instance continues to process *only its own track audio*. The group is a **parameter/control link**, not a summed audio bus, not an audio return, and not a new engine. Each instrument therefore keeps its independent source in the future MR Spatial shared room.

A regular mixer Bus remains available for traditional submixes, but MR Strip Link is the **default conceptual solution for adjusting a group without losing separate source positions**. No bus send-back or automatic restoration of stems after bus processing.

```text
 TRACK GUITAR           TRACK KEYS             TRACK BASS
     Audio                  Audio                 Audio
       |                      |                     |
   MR Strip A              MR Strip B             MR Strip C
   [Link: Band]            [Link: Band]           [Link: Band]
       |                      |                     |
       +----------+-----------+---------------------+
                  | CONTROL ONLY
                  v
      Open MR Strip A to edit 'Band' linked modules
      (shared parameter changes, not shared audio)
                  |
     Audio from each strip independently
       |                      |                     |
       v                      v                     v
   Spatial A              Spatial B             Spatial C
       \                      |                     /
        +--------- one shared room scene ---------+
```

**Key distinction:** 'one instance controls a group' means one editor controls existing separate DSP instances; it **does not** mean one processing instance processes all instruments. Linking GUI or parameters is different from mixing their audio.

## User-facing MVP

- Each installed native MR Strip can be assigned to a named Link Group (e.g. 'Band', 'Drums'), with **one group per instance in MVP**. Select a member MR Strip and control the linked group **from its current editor**; separate floating control-only windows are not required.
- Select which modules/parameters are linked: at least future **EQ, Dynamics, Saturation** at the supported parameter level; do not assume these native modules are already implemented.
- **Relative Link** (recommended default): a group gesture moves linked parameter values by a relative delta, preserving per-track offsets. Clamp safely to each parameter's legal range and present clipping/saturation of the control explicitly.
- **Absolute Link** (optional, clearly selectable): linked parameters take the same normalized/physical value where compatible. Never apply raw numeric values across different parameter definitions or mismatched plugin versions.
- A member's unlinked module or parameter remains fully independent. For an individually customized setting of a linked parameter, explicitly leave/unlink that parameter or provide a future local Trim; do not silently fight the group's setting.
- Clear visual indication of link membership and linked modules, with the list of participating tracks available from the current MR Strip. No rack-style extra bus channel or unnecessary complex group mixer.
- Only link **compatible** MR Strip instances/modules. A missing or incompatible member is shown as offline/unlinked instead of receiving a mismatched parameter.
- Do not automatically link track output routing, pan, sends, faders, mute, solo, or Spatial coordinates: the link is for agreed MR Strip DSP modules only.

## DSP semantics and limitations

**MVP = Link Parameters.** Each MR Strip has its **own audio input, detector, EQ, dynamics and nonlinear processing**. Sending the same settings to several independent compressors does **not** produce the output of a compressor on their summed bus. Likewise, separately saturating tracks is not equivalent to saturating their sum. State this explicitly in UX and tests.

**Potential later feature — Link Dynamics:** a shared sidechain/detector on a derived sum with a common gain envelope, applied individually to the member channels. This requires latency alignment, coherent channel layouts and careful routing analysis; it is **not MVP**, is not automatically equivalent to arbitrary summed-bus processing, and must be separately designed/tested before implementation. No audio copy to an audible group return.

**Existing conventional Audio Bus:** if tracks are actually summed into a bus and the bus routed into MR Spatial, that sum is at most **one Spatial source** (not independent positions). This remains standard audio routing behavior; it is not an alternative Link implementation, and this project does **not** add 'send bus output back to the individual channels'.

## State, events, automation, Undo

Store link group IDs/names, member strip identities, link mask and mode in the **shared project model**, not in private plugin-to-plugin sockets or the audio callback. The current editor is a control surface over that model: do not rely on the editor staying open, or on a particular 'leader' processor continuing to exist.

- User action on linked parameter => validate once, produce one project-level command / Undo step with a consistent set of member parameter changes. No unbounded cross-plugin notification loops.
- Host and project automation need an explicit conflict policy: **do not allow independent automation lanes to fight a linked group gesture**. Prefer a single group-owned source of truth for linked parameters and map member updates at a defined time; until implemented, reject/flag conflicting per-member automation rather than silently overriding.
- Dispatch bounded, sample/time-consistent parameter updates using existing SHARED host mechanisms. DSP callback is free of allocation, locks, file I/O, UI access, or plugin-to-plugin method calls.
- Save/reopen/copy/delete/move strips; Undo/Redo and presets; if a member is missing on restore, other instances keep playing correctly and link membership is repairable.
- Preserve existing native processor parameter conventions and compatibility with older projects: absence of link metadata == all strips unlinked.
- A temporary editor close/scene change should **not** break the link, which is project state rather than an open-window relation.

## Relationship to MR Spatial, Sends, reverb

```text
Input → MR Strip on each track → individual MR Spatial source → shared scene ─┐
        └─ Post-Strip / Pre-Spatial FX Send → MR Reverb (100% wet) → Return ─┼→ Master
```

The Link Group is **not on the audio path**. FX Returns bypass Spatial by default. Ordinary pre/post-fader choices on Sends remain distinct from their spatial bypass. Never duplicate the source dry contribution at Master, and preserve latency compensation across the spatial and FX branches.

## Acceptance / implementation stages

- [ ] Local audit of existing processor IDs, Project Model / commands, GUI parameter model, automation and routing; do not assume planned modules already exist.
- [ ] Create persistent compatible link groups; select one member MR Strip as group-control editor, with clean membership and module mask.
- [ ] Relative parameter linking first; optional Absolute mode if compatible, with boundary and Undo tests.
- [ ] Verify track-local audio and per-source Spatial positions remain independent; no extra audio summing or double dry.
- [ ] Verify group event ordering, automation conflict policy, project restore, missing instances, Undo/Redo, zero real-time allocations/locks.
- [ ] Measure actual CPU and user-approve behavior before declaring implementation complete.

This is a **design contract only**. It must not expand ongoing MR Reverb #129–#132 scope. Code, builds, tests and packages are local; GitHub docs/issues only, commits with `[skip ci]`; no Actions or PR without request.
