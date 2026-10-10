# MR Studio — Pre-Spatial Routing (architectural contract)

**Status:** user-approved design direction / architecture backlog; **not implemented**.  
**Decision date:** 2026-10-10.  
**Tracking issue:** [#133 Pre-Spatial Routing](https://github.com/vladleng/MR-Studio/issues/133) (architecture backlog, no code started).  
**Scope:** SHARED Audio Engine, Project Model, Mixer, future MR Strip, MR Spatial and FX bus/send integration. Applies to Studio Mix and future Live Mode through **one common processor/routing graph**, not two audio backends.

## 1. Intent and non-negotiable rules

The user wants the ability to shape instruments and **dry group buses before acoustic-room processing**, while independent sends may feed reverbs or other FX **without passing through MR Spatial**. A group compressor must not be *forced* to compress the spatial room's reverb tail. Post-reverb processing is a valid artistic option, but never an unavoidable consequence of normal grouping.

**Product principle:** `Source / dry processing → dry group processing (optional) → spatial scene (optional) → final mix`; independent `pre-spatial sends → FX buses → final mix`. This is a **routing topology contract**, not a fixed new plugin chain; users can explicitly bypass Spatial, route effect returns into Spatial or process the summed mix when they choose.

In particular:
1. MR Strip: future channel-strip UI/native DSP for instrument **dry** tone/dynamics, within existing insert architecture; not a separate engine.
2. Dry Group Bus: real audio subgroup sum for shared EQ, compression, saturation etc., **before** MR Spatial. The group compressor receives no room tail unless the user explicitly routes a wet source into that group.
3. MR Spatial: a **spatial mixing stage/scene service** with several independent sources sharing one virtual room; it must not be implemented as a mandatory normal track insert placed before/after arbitrary buses. Sources can also be routed directly to destination without spatialization.
4. Sends/AUX: draw from a defined **pre-spatial dry tap** of source tracks **or** processed dry groups, with independent pre/post-fader settings; send signal may reach FX inserts while bypassing MR Spatial.
5. FX Return: **Direct / bypass Spatial by default** for traditional wet reverb/delay returns; explicit opt-in to Spatial is a future routing option with cycle safeguards and no double counting.
6. Master: collects Spatial output, Direct paths and FX Returns; optional final mastering dynamics on the combined signal is allowed and distinct from dry group processing.
7. No implicit copy of wet room tails into a dry group compressor; no unrequested dry signal duplication; no gratuitous cross-routing between scenes/returns.

This design **extends** existing #22 sends/buses and SHARED #13; does not reopen completed #16/#22 or change accepted existing audio behavior.

## 2. Logical signal flow (target architecture, not current implementation)

```text
 Audio/MIDI Instrument Track A                         Track B
           │                                               │
           ▼                                               ▼
   Native Inserts / MR Strip                      Native Inserts / MR Strip
           │                                               │
           ├───── pre/post-fader SEND ─────────────────────┬──► FX BUS 1
           │                                              │    MR Reverb (wet)
       Fader/Pan                                      Fader/Pan │
           │                                              │    FX Return:
           └────┬─────────────────────────────────────────┘      Direct
                ▼                                                   │
      DRY GROUP BUS (optional)                                     │
        EQ / Compression / Saturation                              │
        Fader/Pan                                                  │
           │                                                       │
           ├── Group pre-spatial SEND ───► FX BUS 2 (Delay etc.) ──┤
           │                                                       │
           ▼                                                       │
     [Output Route]                                                │
      │          │                                                 │
      ▼          └──────────────► DIRECT ─────────────────────────┤
   MR SPATIAL                                                      │
   Source(s) → scene →                                            │
   Direct + early/late room output                                  │
      │                                                            │
      └──────────────────────► FINAL SUM ◄─────────────────────────┘
                                        │
                              MASTER INSERTS / GAIN
                                        │
                                  Hardware / Export
```

**Illustrative ordering**, not an exact current fader position contract. The default post-fader send must actually be sampled *after* that source's fader; the pre-fader option must retain the currently accepted MRS behavior. For FX return processors requiring a 100%-wet insert, e.g. a reverb on a traditional send bus, the user configures Mix=100% or equivalent to avoid a duplicated dry path. A send with 0 level is not an extra audible path.

**Critical:** Spatial output is an agreed combined scene output (**direct source + scene reflections/reverb**), not merely an additional wet tap sent alongside an unchanged dry copy. The final sum must **not** receive a second duplicate of the same direct source. The engine may internally split/direct-process independently as long as the observable result is identical. With Spatial bypass, the source goes to Direct at matched gain/latency without causing a duplicate or truncating unrelated FX tails.

## 3. Grouping and positioning — do not promise impossible behavior

Nonlinear processing of the **sum** of instruments (e.g. bus compression) and retaining all the instruments' exact **independent** spatial-input signals after that single mono/stereo processed sum are generally incompatible. Do not silently claim both.

The routing model must explicitly distinguish:

### A. Group As Source (first implementation candidate)

```text
Track A dry ─┐
Track B dry ─┼─► Group EQ/Compressor ─► One MR Spatial source ─► Scene
Track C dry ─┘
```

A **genuine subgroup compression** (full summed signal) occurs before Spatial; the group occupies **one location** in the virtual room. Internal A/B/C independent positions are no longer available at that spatial input. This is suitable for e.g. an instrument stack, multiple drum mics treated as one acoustic source, or stereo stems.

### B. Individual Spatial Sources (independent instruments)

```text
Track A dry ─► Spatial source A ─┐
Track B dry ─► Spatial source B ─┼─► Shared room scene
Track C dry ─► Spatial source C ─┘
         + linked grouping via VCA / shared parameter / optional detector
```

Independent source positions are preserved. **VCA-like gain control or a linked sidechain/detector** can be applied before Spatial without converting to a summed-bus audio compressor. Linked per-track gain control is **not mathematically equivalent** to full mixed-bus compression in general; communicate this honestly in GUI/docs.

A mixed scheme is valid (some grouped-as-one, some individual), provided each track reaches the scene exactly once by the selected output path. No implicit duplication just to preserve per-track panners.

**Decision required before implementation:** UI name and exact semantics of group-as-source vs independent sources; define upgrade-safe defaults and test both. For MVP implement one straightforward path rather than fake both.

## 4. Send semantics and bus routing contract

Existing MRS baseline ([SENDS.md](SENDS.md)) supports ordinary pre-fader/post-fader sends and bus returns. Those fader settings **do not automatically mean pre/post-Spatial**; this proposal adds an **independent spatial boundary**. Do not redefine legacy send terminology or silently migrate old sessions.

- Source tap may be in a track after native inserts/MR Strip, or in a dry **group** after group processing. At either location, use the existing pre/post-fader distinction so a post-fader send follows that channel's gain. Determine exact ordering relative to pan and insert chain by local code audit.
- Main output path is **not disabled** by choosing a send. Default source output may go to a dry bus, individual Spatial input, or direct destination according to explicit route.
- A group send sees **processed dry group audio** at its source tap, not final acoustic output from Spatial.
- FX sends and returns use **existing validated directed acyclic graph** rules. New spatial routes must participate in cycle rejection, mute/solo admission, lifecycle and plugin state dependencies; do not introduce feedback by accident.
- FX return's normal Direct destination bypasses Spatial; a user-selected spatialized effect return is optional follow-up, not an implicit default, and never fed recursively to its own return path.
- A mono track/bus to stereo room must have an explicit and tested channel-layout conversion. There is no blanket assumption that any stereo signal can be restored to individual internal sources.
- Concurrency/PDC: both split paths must align at the final sum across plugin reported latencies, spatial render latency, send/return latency and parallel routing. Do not silently compromise raw recording, playback timing or offline tails.
- Mute/solo: preserve existing semantics for upstream sources and return buses; test scenes/Direct/return admission together. Spatial bypass must be local to its selected route/scene, not a global FX bus mute.

## 5. Separation of responsibilities

| Area | Owns | Must not own |
|---|---|---|
| SHARED audio/routing graph | ordered taps, buses, processing edges, safe graph preparation, PDC, mix sum | a second Live-only audio engine |
| MR Strip (future native DSP/UI) | instrument/channel tone/dynamics and automation | global room positioning or hidden wet tails |
| MR Spatial (future scene service) | source coordinates, shared virtual acoustic space and room output | mandated dry group compression or ordinary AUX/FX return |
| FX buses/returns | auxiliary inserts e.g. MR Reverb/delay, optional Direct/Spatial destination | forced traversal through room or double dry mixing |
| Mixer UI / Project Model | editable routing selectors, shared command validation, Undo/Redo, persistence/migration | callback-side file operations or separate unsaved UI-only route model |

**MR Reverb #128** stays an independent native convolution reverb for artistic FX; it is **not MR Spatial** and must not acquire new mandatory spatial-routing requirements while its current DSP work is in progress. MR Strip and MR Spatial are future independent product directions built on SHARED routing.

## 6. UX contract and defaults (proposal, not implemented)

- A normal **Dry Group Bus** visibly remains a bus with standard inserts, gain and sends; processing its audio must be upstream of Spatial.
- Source or group **Output** has future explicit destination choices: `Spatial / Direct / another dry group` with legal DAG validation. Avoid ambiguous stacked sends that appear as destinations.
- A send has `Pre-fader / Post-fader` **plus** a clearly determined `Pre-Spatial` tap (the default for FX sends). Do not overload the existing Pre/Post toggle to mean placement in the room.
- FX Return: `Direct` default; `Spatial` advanced opt-in if supported. When Spatial disabled, Direct paths + effect returns keep behaving.
- Where a grouped source is one spatial object, label it **Group as one source**; where multiple tracks keep independent positions, make clear that group linked controls are not a summed-bus compressor.
- Existing projects: **identical routing/audio when reopened** with no MR Spatial used. New spatial routing serialization must be versioned and Undo/Redo-supported, with sensible Direct fallback when the module is unavailable and no silent duplicate paths.
- An acoustic on/off button is an explicitly testable bypass, not automatic sends/returns bypass.

## 7. Engineering gates before claiming implementation

### Routing correctness
- [ ] Audio references: single track Spatial/Direct, dry group → Spatial, track/group sends → direct FX return, effect wet returns independent of dry group compression.
- [ ] Stereo & mono routing, nested buses, inserted processors, overlapping sends, bus removal/reconnect and DAG cycle errors.
- [ ] Verify **the group compressor does not react to MR Spatial's reverb tail**. Render a transient with very long scene tail and compare group dynamics with Spatial on/off.
- [ ] Verify **no double dry path** at final output in Spatial on/off and external reverb mixes; gain/phase tests and mono fold-down.
- [ ] Test group-as-source spatial image, and independent-source mode (if implemented) without falsely claiming real summed compression on same input.
- [ ] FX Return via Direct maintains level & reverb tail while moving the dry source in Spatial.

### Engine/UX reliability
- [ ] No RT allocation/locks/disk access. No graph rebuild in callback. PDC and latency on Spatial/Direct/FX merge, including long tails, transport, seek, offline exports.
- [ ] Existing accepted pre/post-fader Sends, channel/bus/Master inserts and Undo/Redo remain intact. No behavior change to legacy projects.
- [ ] Routing automation or control changes respect current safe hot-update semantics; graph structural changes require the engine's existing Pause/Stop rules until explicit validated hot-swap is delivered.
- [ ] Save/reopen/migration/preset states, missing processors, reloading a project on the other machine, Live Mode use of same SHARED graph.
- [ ] Report real Windows/ASIO measurement results rather than assumed low-latency fitness. Studio Mix high-buffer use is not constrained by Live's monitoring targets.

## 8. Implementation boundaries and work order

**Do not implement as part of ongoing MR Reverb #129–#132.** This is a future architecture requirement, not authorisation to refactor the active local branch.

1. **Architecture/code audit:** compare existing local mixer graph/tap sites with [SENDS.md](SENDS.md), [BUSES.md](BUSES.md), [NATIVE_INSERTS.md](NATIVE_INSERTS.md), [ARCHITECTURE.md](ARCHITECTURE.md), and [ACOUSTIC_SPACE_PROTOTYPE.md](ACOUSTIC_SPACE_PROTOTYPE.md). Propose additive state model with validated migration and exact processor order.
2. **Dry routing feature:** introduce explicit Spatial/Direct boundary and dry track/group sends and group-as-source contract, preserving accepted legacy output/sends behavior.
3. **MR Spatial prototype integration:** one scene with independent sources, after-dry group source when chosen, one nonduplicated direct+room output, direct FX-return exclusion by default. Benchmark and listen.
4. **GUI and user acceptance:** compact visible controls for output/spatial routes, project persistence, QA/regression; defer complex advanced grouping UX.

All new code/build/tests/packages occur in the local checkout. GitHub docs/issues only; documentation commits `[skip ci]`; no GitHub Actions, code push, PR or merge without explicit permission. At implementation time consult local `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, realtime safety, performance plan and UI-design skills.

## Related sources / scope separation

- [#13 SHARED Core](https://github.com/vladleng/MR-Studio/issues/13), [#16 Audio Engine](https://github.com/vladleng/MR-Studio/issues/16) and [#22 Mixer/Routing](https://github.com/vladleng/MR-Studio/issues/22) — existing base. This new feature does **not** reopen accepted or closed stages.
- [#78 FEATURES](https://github.com/vladleng/MR-Studio/issues/78) — appropriate track for routing and MR Spatial capabilities, distinct from [#111 NATIVE PLUGINS](https://github.com/vladleng/MR-Studio/issues/111).
- [Acoustic Space Prototype](ACOUSTIC_SPACE_PROTOTYPE.md) — scene modelling idea; future prototype must follow this routing contract.
- [MR Reverb #128](https://github.com/vladleng/MR-Studio/issues/128) — independent convolution insert for FX returns, not equivalent to MR Spatial.
