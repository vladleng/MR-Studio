---
name: ds-watch-pr
description: "Watches a pull request through CI and review, fixes real findings and stops when it is ready, merged or blocked. Use when the user asks to watch, monitor or babysit a PR, or /ds-watch-pr."
---

Keep one PR moving until it is green and reviewed, with as little noise as possible.

The optional watcher is `scripts/watch_pr.py` in this skill's folder. Call it by absolute path only when the environment has `python3`, an authenticated `gh` and a process runner. In DSH without these, use available GitHub connector/API operations for PR checks, workflow runs, jobs and logs; never claim that the Python watcher ran.

```bash
python3 <skill>/scripts/watch_pr.py <pr> [--repo owner/name] [--until ready|green|any] [--interval 60] [--max-minutes 60]
```

It prints one line per new check failure, finished check run, comment or review, then a final `stop:` line. Exit 0 means the condition is met or the PR merged. Exit 1 means you must act. Exit 2 means bad input. `ready` waits for green checks, a mergeable branch and no change request. `green` waits for checks only.

## Steps

1. Handle what already exists: failed checks and comments newer than the last push.
2. Start the watcher. Run it in the harness's background or monitor tool if it has one, so each line wakes you. Otherwise run it as one long foreground command.
3. On each event, read the source before you change code. Decide if the finding is real.
4. For a real bug or CI failure: make the smallest fix, run the narrowest available check (local if supported, otherwise the scoped GitHub Actions workflow), push to the task branch, then resume watching. Follow `mr-studio-build-test` for MR Studio.
5. For a false positive or out-of-scope request: reply with a short reason through `ds-pr-comments`, resolve the thread and list it as a follow-up.
6. Tell real test failures apart from infrastructure flakes. Re-run a flake once before you call it a flake.
7. If the base branch moved or the branch is behind, inspect the delta. For MR Studio never force-push or rewrite branch history without separate explicit approval; prefer non-destructive updates or report a blocker.
8. Before you call the PR ready, `git fetch` and read the PR again. It must not be BEHIND or DIRTY.

## Stop when

- The watcher exits 0. Report the PR as ready, or merged.
- Five fix-and-push cycles pass without exit 0. Report what still fails.
- Another PR makes this one obsolete. Stop and ask before closing it.
- `gh` reports a rate limit. Check `gh api rate_limit` and wait for the reset once. Do not loop.

## Rules

- One watcher per PR. Never write your own sleep or polling loop.
- When an event needs no action, say nothing. No "still waiting" messages.
- Merge only if the user asked for it. Otherwise report ready.
- Keep the PR small. Prefer fixes that delete or narrow code.

## MR Studio / GitHub Actions constraints

- The example 60-second interval is a starting point, **not** a fixed policy. Prefer event-driven notifications, avoid redundant polling and never run overlapping watchers. If the environment cannot keep monitoring, report the last observed state as PENDING, not PASS.
- Judge checks on the PR's actual head SHA and current workflow definitions. Distinguish missing/pending/cancelled/failed checks, infrastructure flakes and failed evidence preflight. An older green SHA is not proof for a new head.
- Do not trigger full Windows Release on every push. Follow CI gate order and scopes from `AGENTS.md`, `mr-studio-build-test` and current `.github/workflows/`.
