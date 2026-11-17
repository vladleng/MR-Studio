---
name: ds-ready
description: "Runs the full ds-ship workflow but stops with the pull request ready for a human to merge, and never merges. Use when the user says get it ready, make it mergeable, stop before merge, or /ds-ready."
---

For an explicitly requested remote PR workflow, do `ds-ship` except the merge. In MR Studio, a local readiness request ends with a checked local diff/commit and acceptance report; it does not require a PR.

For MR Studio follow `AGENTS.md`: source push, PR creation and Actions each require authorization in the user request. Do not infer it from "get it ready". Force-push/history rewrite and merge require separate explicit permission; do not change auto-merge or merge-queue settings automatically.

## Local MR Studio readiness

1. Verify the assigned checkout, HEAD and dirty diff; preserve existing work.
2. Finish only the requested scope and run proportional local checks from `mr-studio-build-test`, including RED/GREEN for bugs and the documented exceptions.
3. Report the local diff/commit, exact checks and remaining acceptance items. Stop without push, PR, Actions or merge unless those actions were explicitly requested. A required FAIL/NOT TESTED prevents claiming fully verified readiness.

## Steps for an authorized remote PR workflow

1. Read `ds-ship` and follow its steps 1 to 6: branch, change, prove, review, file, watch.
2. Check if auto-merge or a merge queue is already enabled. For MR Studio do not change either setting without explicit permission; if an automatic merge threatens required user acceptance, stop and report a blocker. Recheck before final handoff.
3. After the last fix, re-prove any behavior change and any non-trivial conflict resolution.
4. Fetch the base branch. Read the live PR: head SHA, base, checks, reviews, mergeable state. Confirm it is open and not a draft.
5. Apply the `ds-ship` merge gate and pick one verdict:
   - **Ready**: the gate passes. Leave the PR open.
   - **Waiting on approval**: only a required human approval is missing. Confirm that this is the real blocker.
   - **Blocked**: say what stops it, what you tried and what is needed.
6. Report the PR link, final head SHA, verdict, proof, review outcome and remaining risks. State that the PR is open and unmerged.

## Stop when

- You reach one of the three verdicts and the report is written.
- Someone else merged the PR. Report that.
- Six fix-and-watch cycles pass without a green gate. Report Blocked.

## Rules

- Never merge, turn on auto-merge, join a merge queue or push to the default branch.
- Do not ask another agent to do any of those things for you.
- Show evidence for the verdict: the check list and mergeable state you read.

## MR Studio status

- A local result or PR can be technically READY but still be **WAITING FOR USER ACCEPTANCE**. CI PASS is not permission to merge. Verify the exact SHA and actual check results.
- Follow `mr-github-workflow` and `mr-studio-build-test`; generic `ds-ship` wording never authorizes unnecessary full CTest or Windows Release.
