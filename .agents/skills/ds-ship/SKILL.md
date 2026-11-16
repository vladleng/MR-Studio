---
name: ds-ship
description: "Takes a change all the way from code to a merged pull request without supervision: build it, prove it, review it, file the PR, watch it and merge it. Use when the user says ship it, send it all the way, merge when green, or /ds-ship."
disable-model-invocation: true
user-invocable: false
---

Own the work from its current state to a merged PR while nobody is watching.

Only act this way when the user clearly asked for it, and only within project permissions. In MR Studio, a request to implement, ship, file a PR or 'merge when green' does not replace separate user acceptance and an explicit command to merge the accepted scope. Force-push/history rewrite needs separate approval. Until then, behave like `ds-ready` and leave the PR open.

## Steps

1. If you are on the default branch, create a new branch.
2. Make the smallest change that meets the goal. Fix the case that was reported, not every case that might exist.
3. Prove every part of the diff. Reproduce the failure, apply the fix, and run the same reproduction again. Run the checks CI runs. For UI or runtime behavior, run the app and look. Paste the command and its output into your notes.
4. Review the change with `ds-review-loop`. Re-prove anything the review changed.
5. Check whether the task branch is current. Update without rewriting shared history unless separately authorized; report conflicts rather than force-pushing. Then file the PR with `ds-pr`, noting actual proof.
6. Watch the PR with `ds-watch-pr`. Fix real findings and CI failures, prove each fix, push, and watch again.
7. Check the merge gate on the latest commit. For MR Studio merge only after separate explicit user acceptance and a merge instruction for that scope; otherwise leave the PR open and report ready/waiting for acceptance. Never bypass gates with `--admin`.
8. Report the PR link, merge commit, proof, and every decision you made for the user.

Merge gate, all true on the latest head: proof covers the full diff, required checks are green, review bots are finished, each finding is fixed or answered with a reason, no human review asks for changes, and the branch is current and mergeable.

## Stop when

- The PR is merged with required user approval and reported; or for MR Studio it is ready and waiting for user acceptance.
- You cannot prove the change, or the gate cannot pass and you cannot fix it. Ask the user.
- The next step deletes data, touches production, closes a PR, or pushes another branch. Ask the user.
- Six fix-and-watch cycles pass without a green gate. Stop and report what blocks it.

## Rules

- If supported, await the required checks with a single watcher. If the environment cannot monitor completion, report PENDING with links; do not claim PASS or promise unattended monitoring.
- Ask with the harness question tool, not a plain message. Put the PR link, the blocker and your recommended option in the question.
- If no question tool exists or nobody answers, leave the PR open and unmerged, and start your final message with the question.
- Evidence is output you saw in this session. A claim without output is not proof.
- Keep scope fixed. Decline extra work from reviews and list it as follow-ups.

## MR Studio override

Read `mr-github-workflow`, `mr-studio-build-test` and current `AGENTS.md` before destructive actions. GitHub Actions owns CI ordering and release gates. A passing PR does not replace hardware/manual ASIO/MIDI/DPI/user acceptance. Do not close the assigned Issue before acceptance.
