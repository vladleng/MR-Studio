# MR Rig — Working Codex Handoff (start #198 P0 audit)

**Owner:** [#197 MR Rig](https://github.com/vladleng/MR-Studio/issues/197). **Current status:** design/specification only, 2026-10-15. Not evidence of completed MR Rig implementation. **First task after explicit user request:** [#198 — P0 local audit](https://github.com/vladleng/MR-Studio/issues/198); stop afterward for acceptance. Next stages [roadmap](MR_RIG_ROADMAP.md).

## 0. Required reading and source-of-truth order

1. `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, `docs/LOCAL_BUILD_POLICY.md`, `docs/LOCAL_DEVELOPMENT.md`; verify current local `git status --short`, branch, HEAD and dirty files **before changing anything**. Local checkout may be ahead of GitHub docs; never reset or stash unfinished work of other Codex chats.
2. `docs/MR_RIG_ARCHITECTURE.md`, `docs/MR_RIG_GUI.md`, `docs/MR_RIG_LIBRARY_PRESETS.md`, `docs/MR_RIG_AUDIO_PEDALS.md`, `docs/MR_RIG_ROADMAP.md`.
3. Existing `docs/MR_CAB_COLLECTIONS.md`, `docs/MR_CAB_SIMPLE_185.md`, `docs/MR_CAB_FULL_COMBO_IR.md`, latest `docs/MR_CAB_FULL_COMBO_196_DELIVERY.md`, `docs/MR_CAB_WINDOW_UPD2_196.md`, `docs/MR_CAB_ROADMAP.md`, `docs/NATIVE_INSERTS.md`, `docs/REVERB_EDITOR_IMPLEMENTATION.md`, accepted `docs/UI_REFERENCE_UPD9.md`.
4. Issues [#182](https://github.com/vladleng/MR-Studio/issues/182), [#185 accepted](https://github.com/vladleng/MR-Studio/issues/185), [#196 pending approval](https://github.com/vladleng/MR-Studio/issues/196), [#187](https://github.com/vladleng/MR-Studio/issues/187), [#188](https://github.com/vladleng/MR-Studio/issues/188), [#189](https://github.com/vladleng/MR-Studio/issues/189), [#197](https://github.com/vladleng/MR-Studio/issues/197), [#198](https://github.com/vladleng/MR-Studio/issues/198) and [#199–204](https://github.com/vladleng/MR-Studio/issues/199).
5. DSP or graph tasks also require `docs/ENGINE_PERFORMANCE_PLAN.md`, `docs/PROCESSING_RELIABILITY.md`, local skill `mr-realtime-audio-safety`, `mr-feature-implementation`, `mr-studio-ui-design-system` if present.

## 1. Do only #198: read-only audit

**Do not write code or schema during audit without further command.**

- Inventory current MR Cab / version / source: existing IR convolver async loader/relink/transitions; IR classifications Full Combo; preset/state/project migration; model versus Rig metadata, pipeline and native editor window ownership, Gain location, current GUI.
- Map actual current host `Processor`/native insert graph, ownership of preset/Undo/automation/PDC and available DSP module interfaces. Determine whether a single MR Rig container can reuse native processor classes inside existing SHARED host graph without a second audio engine.
- Investigate two plugin identity/migration routes:
  (A) preserve MR Cab insert identity and extend it via Rig shell while retaining old projects;
  (B) separate MR Rig insert identity, migrate via explicit tool/manual assignment; evaluate ABI/state/host compatibility.
  **Propose a recommendation to user; do not choose prematurely.**
- Plan safe semantic libraries SourceAsset/ToneModel/RigPreset and artwork rights/path migration; no auto-scraped Fender/Polytone images.
- Evaluate DSP role distinctions: WAV Full Combo linear (no extra Cab), WAV Cabinet-only (not amp), NAM Full Rig (no extra cab), NAM Amp Only (optional one Cabinet) and Unknown (manual decision).
- Verify UI shell/kit integration boundaries with existing Theme/#189 (no dependency gate), single native plugin window, PRE/CENTER/POST zone layout and what can be reused.
- Identify first pilot rig (e.g. one user-owned/licensed Polytone Full Combo WAV with image) and realistic unit/GUI/ASIO test approach; do not assume user owns any particular WAV/NAM or has permission to redistribute.

**Expected report:** source/package/branch/dirty and confirmed current acceptance statuses; actual classes/files; proposed migration/ID compatibility decision options; data schema delta and source identity; module host contract; issues/prereqs, risk register; precise P1 scope, which parts need user approval; exact TESTED/NOT TESTED. Do not claim build/ASIO/audio acceptance from docs.

## 2. Implementation stage boundaries after #198 acceptance

- [#199](https://github.com/vladleng/MR-Studio/issues/199) — curated Tone Model Library, IR/NAM/My Rigs tabs/cards with optional pictures, legal placeholders, optional Import My IR, correct source mapping, model != rig.
- [#200](https://github.com/vladleng/MR-Studio/issues/200) — actual compact MR Rig shell GUI and minimal WAV-backed RigPreset: left PRE, center model photo, right POST, bottom inspector. No fake pedals or NAM Ready state. Single editor ownership.
- [#201](https://github.com/vladleng/MR-Studio/issues/201) — actual ordered PRE and POST FX graph/node/presets/bypass/reorder/Undo, PDC; no second DAW core.
- [#202](https://github.com/vladleng/MR-Studio/issues/202) — first **user-approved minimal native pedal subset**, then compressor/drive/EQ/chorus/delay/reverb progressively, no dummy processing.
- [#203](https://github.com/vladleng/MR-Studio/issues/203) — own NAM engine, supported architecture tests, NAM Full Rig/Amp Only optional single cab, performance and no silent fallback.
- [#204](https://github.com/vladleng/MR-Studio/issues/204) — entire RigPreset SaveOpen/Undo/GUI/live/studio performance/end-to-end QA and user acceptance.

**Stop between stages**; separate user acceptance required, no new tasks based on an unaccepted intermediate build. #196 can continue independently and requires its own approval.

## 3. Hard boundaries

- Never relabel static WAV IR as full nonlinear amp emulation, never load NAM via WAV convolution, do not auto-apply a second cabinet/room to Full Combo/Full Rig and do not bypass external inserts without host command.
- No proprietary protected WAV/NAM/media/photos bundled or copied into GitHub/package/tests by default; synthetic test fixtures and licensed user-owned files only when explicitly authorized.
- Reuse approved source state/model/command graph and established RT safety (no alloc/lock/I/O/UI in audio callback). Studio mixing may use higher buffers than recording/Live. NAM CPU/latency must be measured per model.
- Prefer established native GUI/theme and reuse MR UI Kit when ready. No VST3 vendor editor restyling.
- Code, configure, builds, tests, screenshots, packages in **local** MR Studio checkout; GitHub only docs/issues commits with [skip ci], no source push/PR/merge/Actions. Never overwrite dirty local changes.
- Test requirements during development = affected targeted tests/GUI; at candidate = full latest CTest + affected GUIs; manual ASIO/physical mixed-DPI/listening not claimed unless performed.

## 4. Ready-to-run prompt for working chat

> Начинаем проект MR Rig, parent issue #197. Прочитай AGENTS.md, свежий PROJECT_CONTEXT.md и документы MR_RIG_ARCHITECTURE, MR_RIG_GUI, MR_RIG_LIBRARY_PRESETS, MR_RIG_AUDIO_PEDALS, MR_RIG_ROADMAP и MR_RIG_CODEX_HANDOFF. Проверь локальный checkout MR Studio, актуальный статус #185 (принят) и #196 (0.4b upd2 — ожидание приёмки, проверь изменения). Выполни **только аудит #198**: существующий Cab/processor ID/state/конвольвер, возможность Rig shell/PrePost chain внутри единого host графа, выбор безопасной миграции, карта данных/GUI и поэтапный план. Код/сборку не трогай. После отчёта остановись и жди моей приёмки. Не скачивай и не публикуй коммерческие IR/NAM/фотографии.
