# MR Studio — Pre-Spatial FX Sends + MR Strip Link (architecture contract)

**Decision:** 2026-10-10, revised after design discussion. **Status: agreed future architecture / backlog, NOT implemented.**  
**Issue:** [#133 — Pre-Spatial Routing / MR Strip Link](https://github.com/vladleng/MR-Studio/issues/133).  
**Companion specification:** [MR_STRIP_LINK.md](MR_STRIP_LINK.md).  
**Applies to:** SHARED Audio Engine, Studio Mix, future MR Spatial, native MR Strip and ordinary FX Returns. All DAW workspaces and Live Mode use the **same** backend.

## 1. Superseding decision — what is changing

The previous proposal to use dry **audio group buses** as the preferred pre-Spatial group processor, add general `Spatial / Direct` per-track/bus selectors, or **return** the processed bus sum back into separate channels is **rejected for this architecture**. It would unnecessarily complicate the routing graph and cannot generally reconstruct each independently positioned instrument after nonlinear bus processing.

The user-approved, simpler plan:

1. **Each instrument has its own MR Strip DSP on its own audio channel.**
2. **MR Strip Link** lets one MR Strip editor control the parameters of multiple independent MR Strip instances, **without audio summing**. This is the preferred group-control workflow.
3. Main audio from individual instruments enters a **shared MR Spatial scene**, preserving each instrument as an independent source and spatial position.
4. **Only an FX Send → FX Return has a special bypass around MR Spatial.** Track/channel FX send taps are taken **before spatial processing** (after the relevant dry insert stage); they reach MR Reverb, delay and other AUX effects outside Spatial. FX Returns merge after the scene.
5. **Existing traditional audio buses/subgroups remain supported**, with unchanged semantics. They are not the solution for keeping separate instrument positions, are not automatically redirected back into individual tracks, and are not given any special new `return from bus to Spatial sources` function.
6. The master final sum may deliberately include mastering processors acting on direct sound and reverberation together. This is not the same as forcing reverb tails through every instrument/group dynamics processor.

This revision **supersedes all earlier proposals in this issue/document** concerning Group-as-Source as the **primary** workflow, dedicated `Direct / Spatial` channel-output selector, and linked group-audio signal return. Group-as-Source remains a *normal consequence* of intentionally using an ordinary audio bus, not a new special topology.

## 2. Target signal flow (conceptual; no code is implemented yet)

```text
  GUITAR TRACK                    KEYS TRACK                   BASS TRACK
  Audio/MIDI                      Audio/MIDI                   Audio/MIDI
      │                                │                            │
  MR Strip A                      MR Strip B                   MR Strip C
      │                                │                            │
      │           MR STRIP LINK GROUP  │                            │
      │      (control-only, e.g. EQ / Dynamics / Saturation)         │
      │<·········· shared parameter updates ·······················>│
      │                                │                            │
      ├── FX Send ───┐                 ├── FX Send ───┐             ├─ FX Send ──┐
      │              │                 │              │             │            │
      ▼              │                 ▼              │             ▼            │
  Spatial A          │             Spatial B          │         Spatial C        │
      │              │                 │              │             │            │
      └──────────────┼─────────────────┴──────────────┼─────────────┘            │
                     │                                │                          │
             SHARED MR SPATIAL SCENE                   │                          │
             (one combined dry + room output)          │                          │
                     │                                │                          │
                     │     [ALL FX SENDS bypass Spatial]                           │
                     │                                │                          │
                     │           FX BUS / MR Reverb / Delay (wet) ◄───────────────┤
                     │                     │                                      │
                     │                FX RETURN                                   │
                     │                     │                                      │
                     └─────────────────────┴──────────────────────────────────────┘
                                           │
                                    FINAL MIX SUM
                                           │
                                MASTER inserts / gain
                                           │
                                 Hardware / Mixdown
```

The diagram is a **logical** diagram, not an assertion that the current renderer supports post-insert taps or spatial-scene nodes. A real implementation will establish accurate insert, pre/post-fader tap and pan ordering by a local code audit.

**Signal conservation:** spatial scene produces the intended direct instrument + room contribution exactly once. A normal FX Return should contribute **wet-only** signal (e.g. MR Reverb Mix=100% when used as send effect); otherwise adding another dry copy is an expected audible doubling that must be prevented by design/user guidance. Sends are intentional parallel effect branches, not an additional dry subgroup output. Spatial bypass/off must not cut off FX tails.

## 3. MR Strip Link — the preferred grouped-processing interface

See [MR_STRIP_LINK.md](MR_STRIP_LINK.md) for the authoritative detailed specification.

- A Link Group comprises **several regular MR Strip instances on separate channels**. One participating MR Strip's **editor** can be used to adjust linked modules on all members. The project owns linkage; no permanent special master audio channel is needed.
- Link can cover selected compatible **EQ, Dynamics, Saturation** controls and parameters. Each strip **processes its own audio only**. Link is a control/data relationship, not an audio graph edge. Therefore it does **not** duplicate audio or change its spatial destination.
- **MVP: Link Parameters** with per-module link mask; **Relative Link** to preserve per-track adjustments, optional **Absolute Link** when compatible. Member-local parameters not linked continue to operate independently.
- Compressor instances with shared **settings** have **separate sidechain detectors**. This is **not the same as a real summed-bus compressor**. Separate saturation is likewise not saturation of the group sum. Do not market it as equivalent bus processing.
- A more complex **Link Dynamics / shared detector** could be considered later as a separate DSP feature, not included in routing MVP. Do not use it as a pretext to implement audio send-back.
- Link group membership/settings belong to the **shared Project Model**, with per-group automation conflict handling, Undo/Redo, save/restore, safe missing-member behavior and sample-aligned bounded parameter updates. No allocation, file I/O, locking or editor calls from audio callback.

## 4. Traditional audio buses — retain, do not special-case

Existing buses and sends from [BUSES.md](BUSES.md) and [SENDS.md](SENDS.md) continue behaving as previously accepted. They may be used where a **true submix** is intended (stems, drum groups, specific mastering chains etc.). Their bus inserts may legitimately EQ/compress/saturate the summed sound.

However:

- When instruments are *summed* to an audio bus, that result is **one composite source** to a subsequent Spatial scene. Individual member positions cannot be recovered from an arbitrary bus output.
- **Do not invent** a `bus → return to member channels → Spatial` pathway or use it to claim the original sources are independent.
- **Do not create** an alternative general-purpose per-track/bus Direct output toggle for bypassing Spatial as part of this feature. The **specific intentional bypass** is for parallel **Sends → FX Returns**, as requested.
- Ordinary bus-to-bus/output routing, sends from a bus if already supported, mute/solo and graph cycle rules remain conventional. A bus FX Send may follow the same pre-Spatial FX rule, but it does not split its composite output back into members.
- Legacy projects with no Spatial or Link features must retain identical audible routing.

## 5. FX Sends: sole special spatial bypass

1. Send audio is tapped from the dry channel/group **before MR Spatial**; exact post-insert/pre-/post-fader position is defined by existing send semantics and verified in code.
2. Existing **Pre-Fader / Post-Fader** are **not** synonyms for `Pre-Spatial / Post-Spatial`: both send types bypass Spatial in the proposed architecture, but differ in their current relationship to the originating channel's fader.
3. FX Bus hosts ordinary insert effects, e.g. [MR Reverb](MR_REVERB.md), Delay, Chorus. Its normal **FX Return bypasses MR Spatial and goes to final sum**, preserving artificial-effect processing independent from acoustic scene coloration.
4. The main signal of a track **continues along its normal path** to MR Spatial. Sends do not steal/mute or create a second copy of the audible dry source. An FX Return with Mix<100% can generate unwanted duplicate dry; define clear wet-only guidance.
5. Do not insert the FX Return implicitly into MR Spatial. Optional advanced spatial placement for returns is **out of scope**, not part of this requested simple default topology.
6. Master may process the final summed material; any compression of added reverb at that point is an intentional mastering action, not an accidental dry-group compressor.
7. No feedback loops via FX Bus/Return and no unbounded fanout; preserve validated routing DAG and mute/solo admission. Maintain tail playback and latency compensation at merge.

## 6. MR Spatial boundary and implementation constraints

MR Spatial is envisioned as a **shared scene/spatial render stage**, not as a mandatory plugin inserted into each source chain. Each individually routed track should be positionable separately. A regular audio bus, when used as a submix source, supplies one source. Spatial renders the scene's direct component plus early/late reflections coherently, without summing an extra uneffected direct copy.

No requirement to invent HRTF/immersive formats; [ACOUSTIC_SPACE_PROTOTYPE.md](ACOUSTIC_SPACE_PROTOTYPE.md) retains its stereo listening/microphone prototype and real listening/CPU tests. **MR Reverb #128 is a separate convolution FX insert**, not the Spatial renderer.

MVP routing is deliberately narrow:
- No per-instrument Direct/Spatial selector just to achieve bypass.
- No audio return from a group bus to all original channels.
- No forced FX Return pass through Spatial.
- No requirement to emulate arbitrary nonlinear bus processing in Linked Parameters mode.
- No separate Studio-vs-Live core; Studio Mix may use higher audio buffers than Live recording/monitoring.

## 7. Project, lifecycle and safety contract

**Shared ownership:** Link groups and new routing state use the same Project Model, Commands, Undo/Redo, plugin/native insert state, automation engine and snapshot migration. No private cross-plugin messaging or UI-owned link state. An MR Strip editor may close without terminating group linkage.

**Real-time safety:** prepare graph/state changes on control thread; bounded parameter queues, no audio-thread allocations, locks, file operations, non-RT destructor work, or callbacks into GUI. Structural topology edits obey existing engine Pause/Stop constraints until a separately audited safe hot-swap exists.

**Latency and rendering:** account for every FX branch and Spatial render latency with PDC; verify sample coherence at final sum, correctly report plugin/native DSP latency and tail lengths on playback, seek/stop and offline export. Test mono/stereo and differing block sizes.

**Compatibility:** projects created before this feature play identically when reopened; missing or incompatible linked MR Strip does not stop another channel's audio. Do not automatically link users' existing strip instances, buses or routing.

## 8. Acceptance / Codex handoff checklist — NOT STARTED

### Phase A — audit / architecture
- [ ] Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, [SENDS.md](SENDS.md), [BUSES.md](BUSES.md), native inserts, [ARCHITECTURE.md](ARCHITECTURE.md) and this doc. Check actual **local** implementation versus GitHub documentation.
- [ ] Trace main and sends graph/fader/insert ordering, find safe pre-Spatial FX tap, choose single scene input/final mix topology with no dry double count.
- [ ] Keep accepted #22 behavior; spec optional project-state versioning and PDC rules without reworking the full mixer.

### Phase B — MR Strip Link control group (independent future feature)
- [ ] Confirm native MR Strip instances/modules exist before implementing; design group IDs, membership, masks, Relative Link and optionally Absolute Link.
- [ ] One member's MR Strip editor controls the group; each DSP instance keeps **separate audio** and separate Spatial coordinates.
- [ ] Undo/Redo one gesture, parameter compatibility, automation conflict handling, missing members and project reload tests. No recursive link events.

### Phase C — Spatial + parallel FX sends integration
- [ ] Multiple independent Spatial sources sharing one room; no source duplication.
- [ ] Track (and compatible existing bus) sends pre-Spatial to FX, **return direct** to mix; no extra channel Direct output workflow.
- [ ] Check FX wet-only path, Spatial bypass/off without loss of FX tail, mono/stereo, PDC/latency and protection against cycles.

### Phase D — regression / manual listening / acceptance
- [ ] Link group control changes strip parameters but **not** audio paths, track faders/pans or Spatial positions. Audio sum/level matches nonlinked case for identical effective DSP values.
- [ ] Linked compressor settings do **not** claim bus-compressor parity; same for nonlinear saturation.
- [ ] FX send can add Bricasti-style reverb while dry track stays in Spatial; no dry boost, wrong phase, or unintended extra compression.
- [ ] Existing buses, pre/post-fader sends, mute/solo, realtime reliability, cross-project persistence and Studio/Live shared graph remain intact.
- [ ] Capture local CMake/build/tests, Windows ASIO profiling, GUI observations and **explicit user acceptance** before closing #133.

**Do not start or scope-creep the active MR Reverb #129–#132 implementation.** This is documentation/backlog only. All code/build/tests remain local; GitHub documents/issues only; commits `[skip ci]`, no GitHub Actions, PR or source merge without instruction.

## 9. Ownership / related work

- [#133 Pre-Spatial Routing / MR Strip Link](https://github.com/vladleng/MR-Studio/issues/133) — architectural tracking.
- [#78 FEATURES](https://github.com/vladleng/MR-Studio/issues/78), [#13 SHARED Core](https://github.com/vladleng/MR-Studio/issues/13), [#22 Mixer](https://github.com/vladleng/MR-Studio/issues/22) — routing infrastructure, no reopened completed stages.
- [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111) — future native MR Strip module; Link project-state contract must align with its plugin architecture.
- [MR_STRIP_LINK.md](MR_STRIP_LINK.md) — Link specific control/automation/QA details.
- [ACOUSTIC_SPACE_PROTOTYPE.md](ACOUSTIC_SPACE_PROTOTYPE.md) — room renderer concept.
- [MR Reverb #128](https://github.com/vladleng/MR-Studio/issues/128) — reverb effect for parallel sends; no scope changes.
