# MR UI Kit + Gallery — stages, checks, adoption plan

**Parent:** [#189](https://github.com/vladleng/MR-Studio/issues/189), category [#78](https://github.com/vladleng/MR-Studio/issues/78). **Статус:** backlog, 2026-10-14; не запускать реализацию автоматически. **Spec:** [MR_UI_KIT.md](MR_UI_KIT.md), [Gallery](MR_UI_GALLERY.md), [Codex handoff](MR_UI_KIT_CODEX_HANDOFF.md). Issues #190–#195 связаны этим checklist; **GitHub-native sub-issue links not yet verified**.

| Этап | Issue | Результат | Gate |
|---|---|---|---|
| P1 | [#190 — Audit](https://github.com/vladleng/MR-Studio/issues/190) | Локальная инвентаризация Theme/Controls/Reverb/host/editor/gestures, accepted colors & metrics | Аудит + migration plan/user signoff |
| P2 | [#191 — Tokens/LookAndFeel](https://github.com/vladleng/MR-Studio/issues/191) | Semantic tokens + scoped gallery preview/adapters без глобальных изменений | Theme isolation, defaults, DPI paint |
| P3 | [#192 — Core controls](https://github.com/vladleng/MR-Studio/issues/192) | Knobs/faders/buttons/selector/value field, tested states & keyboard | Design showcase acceptance |
| P4 | [#193 — Gallery](https://github.com/vladleng/MR-Studio/issues/193) | UI window catalog/inspector/live preview, DPI 100/125/150/200, safe drafts | User visual acceptance, no audio dependency |
| P5 | [#194 — Binding](https://github.com/vladleng/MR-Studio/issues/194) | One host parameter adapter, gesture commit, Undo/Redo, automation readiness | No host regressions and RT violations |
| P6 | [#195 — Pilot/QA](https://github.com/vladleng/MR-Studio/issues/195) | One opt-in native editor uses Kit, full UI/host regression + user acceptance | Final release/user signoff |

**Preferred order:** #190 → #191 → #192 → #193 → #194 → #195; #194 can be prepared earlier when needed for pilot but must not alter currently accepted controls before test gates. Gallery P4 is useful independently for approving GUI styles **before** a full plugin editor migration.

## Acceptance checklist

- [ ] Existing `ui::Theme` / `Controls.h` is the authoritative base; #145/#146 accepted font/color exceptions not regressed, no parallel theme.
- [ ] All controls from real single reusable implementation, not simulated lookalike mocks.
- [ ] Gallery window opens without creating audio device/processing and has component tree, preview, inspector.
- [ ] Size/density, palette/font/icon/border, styles and meaningful states Normal/Hover/Pressed/Disabled/Focused/Selected/Error visibly testable.
- [ ] 100/125/150/200% zoom with min/normal/max window, no illegible labels or clipped units; physical DPI evidence separate.
- [ ] Sandbox preview values isolated from project/DSP, default reset, local draft save/reopen, whitelisted import/export; no automatic global-theme apply.
- [ ] Knob/fader precise semantics: Ctrl/fine, drag, wheel policy, double-click reset, editable units, accessibility/keyboard; one host Undo commit per accepted gesture.
- [ ] Shared parameter binding does not allocate/lock/I/O/audio callbacks and does not change accepted transport/editor close behavior.
- [ ] One pilot native editor accepted, remaining plugins can adopt incrementally. No forced external VST3 GUI restyling.
- [ ] Tests/GUI snapshots/local builds correctly recorded; staged acceptance test matrix complete, full CTest before P6 user acceptance.
- [ ] User accepts each sub-stage; issue #189 closed only after P6 is accepted.

## Coexistence with current work

MR Cab #182/#183–#188 independently continues. **Project context 2026-10-14 has MR Cab P2/#184 work recorded with real Hempback 68/68**, so do not rewrite MR Cab stages as future-only or change local project state. #185 GUI may adopt Kit after P4/P5 if ready, **but it is not a dependency/blocker**. Reverb #128 accepted/closed, #145/#146 accepted/closed and reference UI must remain stable. Future MR Strip, Saturator, Amp and Harma Waves can use Kit as they start.

## Source/collaboration rules

Read [AGENTS.md](../AGENTS.md), [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [LOCAL_BUILD_POLICY.md](LOCAL_BUILD_POLICY.md), [UI_REFERENCE_UPD9.md](UI_REFERENCE_UPD9.md), [REVERB_EDITOR_IMPLEMENTATION.md](REVERB_EDITOR_IMPLEMENTATION.md), [NATIVE_INSERTS.md](NATIVE_INSERTS.md).
Source/configure/build/test/GUI snapshots/packages **locally**, docs/issues on GitHub only [skip ci], no Actions/source push/merge/PR. Stage P1 audits before modifying. Track native GitHub attachment separately; Markdown checklist is not native sub-issues.
