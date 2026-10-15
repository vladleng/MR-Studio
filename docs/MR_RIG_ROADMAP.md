# MR Rig — Roadmap, Acceptance and Dependencies

**Status:** planned 2026-10-15. **Parent:** [#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197) under [#111 Native Plugins](https://github.com/vladleng/MR-Studio/issues/111). **Implementation is NOT started by these docs**. Sources: [architecture](MR_RIG_ARCHITECTURE.md), [GUI](MR_RIG_GUI.md), [library/presets](MR_RIG_LIBRARY_PRESETS.md), [DSP/pedals](MR_RIG_AUDIO_PEDALS.md), [handoff](MR_RIG_CODEX_HANDOFF.md).

**Latest state read from GitHub:** [#185](https://github.com/vladleng/MR-Studio/issues/185) accepted/closed, [#196](https://github.com/vladleng/MR-Studio/issues/196) local 0.4b upd2 READY awaiting acceptance; linked [#182 Cab](https://github.com/vladleng/MR-Studio/issues/182) still active. Before any implementation, Codex must re-read latest PROJECT_CONTEXT and local source/branch; GitHub may be stale.

## 1. Stages / issue checklist (logical sub-issues)

| Stage | Issue | Outcome | Exit gate |
|---|---|---|---|
| **P0** | [#198](https://github.com/vladleng/MR-Studio/issues/198) Audit / migration | MR Cab 0.4b/host/GUI/source and ID compatibility | User accepts specific integration option |
| **P1** | [#199](https://github.com/vladleng/MR-Studio/issues/199) Tone Model Library | IR/NAM/My Rigs conceptual separation, curated WAV cards/images and optional raw WAV import | Card/metadata/source identity and rights tests |
| **P2** | [#200](https://github.com/vladleng/MR-Studio/issues/200) MR Rig Shell GUI | Pre/Core/Post layout, real WAV sound source, minimal RigPreset and one-window GUI | Real model card + saved/opened rig, no fake DSP |
| **P3** | [#201](https://github.com/vladleng/MR-Studio/issues/201) Modular DSP Slots | Actual PRE/POST chain graph, add/remove/reorder/bypass, model/history integration | RT-safe chain 0/1/N & Undo/PDC |
| **P4** | [#202](https://github.com/vladleng/MR-Studio/issues/202) Pedal DSP | Small accepted set Compressor/Drive/EQ/Chorus/Delay/Reverb incrementally | Real signal processing and user-approved subset |
| **P5** | [#203](https://github.com/vladleng/MR-Studio/issues/203) NAM Engine | Full Rig vs Amp Only neural models, optional Cab if amp only, CPU strategy | Supported models tested; explicit no-double-cab |
| **P6** | [#204](https://github.com/vladleng/MR-Studio/issues/204) Final integration/QA | Presets including whole chain, Studio/Live benchmarks, migration, errors | Latest full CTest + GUI + user acceptance |

**Recommended order:** P0 → P1 → P2 → P3 → P4; P5 may start after the required P1/P2/P3 prerequisites independently of all pedal models. P6 only closes when user accepts implemented scopes **and any deferred promised capabilities are marked incomplete**. Don't treat placeholders as passing NAM/pedal DoD.

**GitHub-native sub-issues:** these issue hyperlinks and Markdown checkbox states are **logical relationships only**, not verified native GitHub Sub-issues. The current GitHub connector has no POST/PUT native sub-issue parent mutation action. To get automatic progress, in issue [#197](https://github.com/vladleng/MR-Studio/issues/197) choose **Sub-issues → Add existing issue** #198–204; for category relationship add parent #197 to [#111](https://github.com/vladleng/MR-Studio/issues/111) if GitHub permits; **do not relocate #182/#196 from their existing category tree without careful review**. Verify native hierarchy after manual linking, and don't claim GitHub native progress until then.

## 2. Milestones and what is already reusable

**Approved:** #185 existing MR Cab Simple (async WAV IR load/prepared kernel/crossfade, basic editor/source/preset/Undo). **Pending user acceptance:** #196 Full Combo WAV IR metadata and current window/browser updates. **Planned/independent:** #187 generic WAV importer; #188 Cab future modular contracts/QA; #189 UI Kit and Gallery.

Do not change status or scopes of those issues retroactively. **#182 owns the Cab collection/convolver reusable backend**, **#197 owns the new MR Rig product**, and [#196](https://github.com/vladleng/MR-Studio/issues/196) contributes Full Combo WAV source semantics; keep their tasks separate. Migration/rename paths decided by P0 and user approval; no blind hard rename of `mrs.cab-ir`.

## 3. Cross-cutting acceptance criteria

- [ ] Models appear as curated cards with optional licensed/user-supplied image; first workflow does not require choosing raw WAV.
- [ ] Model (external WAV/NAM asset + metadata/role/image) separate from RigPreset (selected model + actual PRE/POST modules/settings).
- [ ] Library clearly distinguishes **IR Models**, **NAM Models**, **My Rigs** and WAV `Full Combo` vs `Cabinet only`; unavailable neural backend not advertised as working.
- [ ] Full Combo WAV is one **linear** existing convolver (no double cabinet), CAB-only is properly labelled; NAM Full Rig includes cab; NAM Amp Only can use one optional CAB, unknown state requires explicit choice.
- [ ] PRE FX left, center model and photo, POST FX right, per-pedal selection inspector and compact design. Third-party VST3 host GUI not restyled; #189 UI Kit only after approved.
- [ ] Add/remove/reorder/bypass actual native pedals; no decorative DSP claims, no separate audio engine. Save/Open/Undo/Redo handles entire ordered RigPreset.
- [ ] Existing MR Cab #185 projects/presets and MR Reverb remain stable according to P0 user-approved migration; #196 and user IR external assets remain usable; no unauthorized commercial content.
- [ ] No RT callback locks/heap/I/O/UI; output latency/PDC measured, live buffer performance measured separately from higher-buffer Studio mixing, no hidden double DSP.
- [ ] Local targeted tests during development and full current CTest at candidate before user acceptance; software GUI 100/125/150/200 and physical mixed-DPI/ASIO testing marked separately.
- [ ] User acceptance per stage and final P6. Underlying #182/#196 not closed by completing MR Rig documentation alone.

## 4. Codex first implementation task

**Start strictly with [#198 P0 audit](https://github.com/vladleng/MR-Studio/issues/198)** after user's explicit command. Read [handoff](MR_RIG_CODEX_HANDOFF.md) and corresponding architecture, current project context, local source. P0 is read-only audit, no changes/code/build/download; user chooses whether to use a wrapper over existing `mrs.cab-ir` or separate insert ID. Then stop for signoff.

**Build rule:** UI paint changes targeted tests, module/model bindings appropriate unit/GUI integration tests, shared audio graph wider DSP regressions. Full CTest once per accepted candidate; do not claim previous full CTest verified newer source. All code/builds/assets remain local, GitHub docs/issues only [skip ci], no Actions/PR/merge/source push.
