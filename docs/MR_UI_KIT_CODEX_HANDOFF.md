# MR UI Kit — Codex handoff / first stage #190

**Status:** documentation/backlog; no implementation. **Parent:** [#189](https://github.com/vladleng/MR-Studio/issues/189). Canon: [MR_UI_KIT.md](MR_UI_KIT.md), [MR_UI_GALLERY.md](MR_UI_GALLERY.md), [MR_UI_KIT_ROADMAP.md](MR_UI_KIT_ROADMAP.md). **First stage after user's explicit "start" command:** [#190](https://github.com/vladleng/MR-Studio/issues/190).

## 0. Before touching the local checkout

1. Read `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, `docs/LOCAL_BUILD_POLICY.md`; inspect `git status --short`, branch/HEAD, pending uncommitted edits/other active chats. No forced checkout/reset/stash that destroys other work. GitHub main docs may not mirror current local source; local checkout defines implementation state.
2. Read `docs/UI_REFERENCE_UPD9.md`, `docs/REVERB_EDITOR_IMPLEMENTATION.md`, `docs/MR_REVERB.md`, `docs/NATIVE_INSERTS.md` and issues #145/#146/#128; inspect accepted usage of `ui::Theme` / `Controls.h`. Read local `mr-studio-ui-design-system` and `mr-feature-implementation` skills if present.
3. Resolve actual file/classes/layout, host parameter/model/mailbox/Undo gesture contract and existing GUI test/snapshot entrypoints. Do not infer any class names not verified locally; folder layout in spec is proposal.
4. Preserve 2026-10-14 MR Cab project work as reported in PROJECT_CONTEXT (P1 accepted; P2 68/68 classified; current issue status from live GitHub must be checked). **No interactions with Celestion media needed for UI Kit.**
5. Treat accepted #145/#146 as immutable baseline unless user later accepts change; `ui::Theme` is source, don't create a new unconnected theme. No new DSP or JS/web UI.

## 1. #190 output is AUDIT ONLY

Deliver real local inventory of:
- Existing `ui::Theme`, accepted palette/text, Layout, LookAndFeel and widget reuse boundaries, JUCE/component ownership, widget variations (mixer vs native editor).
- User gesture paths: mouse drag/wheel, Ctrl fine, double click defaults, keyboard and Enter/Escape, focus/transport Space; parameter IDs, host mailbox, preview/commit/Undo.
- MR Reverb and native insert existing editor/preset shell, VST3-host chrome (not vendor GUI), UI test harness and GUI preview evidence.
- State/version/lifetime constraints for Gallery per-window scoped preview tokens vs global theme.
- Proposed thin wrapper/adapters for MRKnob, MRFader, MRButton/Toggle/Selector, MRValueField, Graph/Meter/Panel extension, common host binding. Identify potential code duplication/regression.

**No implementation at P1**. Report impact estimates, file/classes mapping, choose pilot native plugin with user's input, exact local test plan. If local source missing/unavailable, ask user; do not claim completed audit.

## 2. Future implementation slices (do not begin without acceptance)

- [#191](https://github.com/vladleng/MR-Studio/issues/191): semantic design tokens + safe scoped LookAndFeel preview. No global repaint of accepted DAW.
- [#192](https://github.com/vladleng/MR-Studio/issues/192): reusable real JUCE controls with size/density/variants and consistent mouse/keyboard/states; not duplicate preview-only controls.
- [#193](https://github.com/vladleng/MR-Studio/issues/193): in-app gallery with live preview, inspector palette/font/spacing, 100/125/150/200% software zoom, simulated states, Draft, Reset/Import/Export. Gallery demo values not DSP/project.
- [#194](https://github.com/vladleng/MR-Studio/issues/194): parameter binding gesture/update/commit/cancel through current host model, consistent automation, Undo and RT safety.
- [#195](https://github.com/vladleng/MR-Studio/issues/195): opt-in native pilot editor + comprehensive affected GUI/CTest + user visual acceptance.

## 3. Development and testing policy

Cosmetic change: build + relevant GUI smoke and screenshots (100/125/150/200 with visual checks). Functional gestures: targeted parameter/model/history/automation tests + editor focus/ownership. Shared LookAndFeel: all affected screens/acceptance matrix. **Full CTest at stage candidate/final acceptance**, not on every intermediate paint build. Record last current source revision and test pass count, no older result as new. Physical Windows mixed-DPI, ASIO, real plugin editor manual comparisons must be labelled NOT TESTED unless done.

Never allow component repaint, font allocation, file access, mutex or UI thread references in audio processing callback. Ignore unsupported VST3 vendor GUI restyling. Preserve project/Undo/transport and accepted look.

## 4. Report template

```text
MR UI Kit #190 — AUDIT / BLOCKED / READY FOR ACCEPTANCE
Local branch/HEAD/dirty:
Accepted UI baseline: Theme/Controls/LookAndFeel and exact fonts/colors:
Current native controls + reuse sites:
Current GUI parameter/Undo/automation gesture flow:
Proposed Gallery sandbox Theme ownership, without audio dependency:
Proposed architecture and P2 files/classes:
Risk/regression and test targets:
Code edits/build/CTest/GUI/ASIO: NONE / results with exact evidence
Next decision required from user:
```

**Hard stop:** after P1 audit wait for user acceptance. GitHub docs/issues allowed [skip ci], source/build/tests/packages local only, no Actions/source push/PR/merge. Avoid bulk UI migrations while other chats develop MR Cab.
