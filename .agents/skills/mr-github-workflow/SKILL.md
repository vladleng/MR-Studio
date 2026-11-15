---
name: mr-github-workflow
description: "Coordinate MR Studio/MR Live development from DeepSeek Harness using GitHub Issues, task branches, targeted GitHub Actions checks, PR reviews and explicit user acceptance. Use for remote GitHub changes, CI failures, PR readiness and cross-harness cloud development."
---

# MR GitHub workflow — DSH / remote agents

This is a **project adapter**, not a new CI engine, permission system or standalone project tracker.

## Authority and entry

1. Read current root `AGENTS.md`, the assigned GitHub Issue/sub-issue (when present) and the current task branch + exact HEAD. Read only relevant sections of `docs/CLOUD_DEVELOPMENT.md`.
2. Respect the precedence: user's current scope and accepted decisions; `AGENTS.md` + assigned Issue; relevant `mr-*` skills; generic `ds-*` techniques. When instructions conflict, do not apply the generic skill's stronger permissions.
3. Pick relevant project skills: `mr-feature-implementation` for behavior, `mr-realtime-audio-safety` for RT/host/audio, `mr-studio-ui-design-system` for visual changes, and **`mr-studio-build-test` for every build/test/CI decision**.
4. Record scope and verifiable acceptance criteria in GitHub Issue/PR. Do not introduce `PLAN.md`, `PROGRESS.md` or a parallel task/status ledger by default.

## Choose the actual execution surface

- **Writable checkout + authenticated `git`/`gh`:** verify status, branch, HEAD and upstream. Commit/push only to the authorized topic branch. Never assume SDK, builds, secrets or local files are synchronized.
- **GitHub connector/API only:** use available repository/branch/file/commit/PR/check/run/log actions, validating returned SHA and permissions. These tools may edit documentation directly, but code compilation still needs real CI. Never invent a CLI command or claim a local test ran.
- **Read-only or no usable remote tools:** perform analysis and give exact next commands; mark edits/checks NOT DONE. Tool access and the contents of a SKILL.md do not grant write/merge rights.

Use one assigned branch per change; compare its HEAD and diff with the target baseline before writing. Avoid force-push, rebase that rewrites remote history, deleting branches/data, modifying auto-merge or bypassing protection without separate explicit approval. Do not overwrite someone else's work.

## Issue → change → evidence

1. Reproduce an existing bug (RED before fix, then GREEN with the same regression test) or name measurable acceptance checks for a feature. Make only the scoped change.
2. Inspect the **actual workflow YAML and `tools/ci/` of the current branch**. Do not guess workflow IDs, trigger names, job inputs, build targets, profile names or runner OS. Classify scope L0–L4 using `mr-studio-build-test`.
3. Run the narrowest applicable check. Workflow gates and prerequisites belong to GitHub Actions, not to prompt instructions. Do not dispatch full Windows Release or all CTest on every small edit; use the relevant profile and exact candidate SHA.
4. When CI fails, read the first substantive error from the actual run/job logs, distinguish code failure from infrastructure and preflight/evidence failures, establish a new testable hypothesis, then change one cause. Follow `ds-verify-loop` and `ds-unstick` for repeat failures.
5. Read the check/run outcomes **for the current head SHA**. Record PASS/FAIL/PENDING/NOT TESTED accurately and link to the actual CI result. Linux Core PASS does not prove Windows/JUCE/ARA2/VST3/ASIO GUI or physical device behavior.
6. Create or update one scoped PR. Summarize the problem, change, actual checks, current SHA, remaining risks and required user acceptance. Use `ds-review-loop` where justified. A green CI run is never acceptance by the user.

## Watch PR / Actions without waste

- With `python3` and logged-in `gh`, `ds-watch-pr/scripts/watch_pr.py` is optional. Set a sensible interval for run duration and available events; do not run duplicate watchers. The skill's 60 s example is **not** a fixed policy.
- Without a process runner or `gh`, query GitHub connector/API for PR status, check runs, workflow runs, jobs and error logs. Prefer event-driven completion; do not create unbounded polling loops. Report PENDING if the session cannot continue monitoring.
- For GitHub rate limits, stop unnecessary retries. Never assert PASS from an earlier commit, missing evidence or a run still in progress.

## Completion / permissions

- Source changes and allowed scoped checks may proceed within the user's current instruction; do **not** auto-merge, force-push, modify merge queues, close Issues or publish a release without the required separate authorization.
- A technically READY PR can be **WAITING FOR USER ACCEPTANCE**. The user validates the Windows runtime/ASIO/MIDI/DPI/vendor behavior where relevant; CI is not a substitute.
- Deliver: Issue link, branch/head SHA, PR link, exact checked workflows/runs, PASS/FAIL/PENDING/NOT TESTED, blockers, and whether merge is awaiting approval.
- For documentation-only or skill-only changes: validate text/frontmatter, links and diff. Do not run application builds/CI merely to validate prose.
