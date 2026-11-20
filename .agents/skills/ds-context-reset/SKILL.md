---
name: ds-context-reset
description: "Keep a PROGRESS.md for long tasks and move to a fresh session or subagent that reads only that file when the context gets long or stale. Use when a task spans many steps or sessions, when the model starts repeating itself, forgets earlier facts, or re-tries old fixes, or when the context window is past about half full. Invoke as /ds-context-reset."
---

# ds-context-reset

Keep a compact, evidence-based handoff so work can continue without relying on a stale conversation summary.

## Choose the handoff location

- In MR Studio use the session handoff. The assigned DSH task supplies requirements and recorded acceptance; the actual local checkout, Git and logs supply implementation evidence. Load `dsh-task-manager-workflow` before journal operations and record meaningful handoff information in the assigned task within the agreed scope.
- Do not create or commit PROGRESS.md or another task journal in MR Studio by default. Temporary scratch, if needed, stays outside Git and contains no secrets.
- In other repositories follow their policy; a PROGRESS.md may be appropriate. The steps below apply to either location.

## Steps

1. Record the goal and acceptance check; completed steps with commands/results; next steps; open questions; important constraints and failed approaches.
2. Update after each verified step. Distinguish incomplete implementation, failed checks and pending user acceptance.
3. Reset context when it causes repeated reading, forgotten facts or repeated failed fixes. First record local branch/HEAD, dirty state and unfinished edits. Preserve existing work; in MR Studio do not reset, stash, switch branches or discard files without agreement.
4. Rewrite the handoff as a concise standalone account. Verify facts against the actual checkout and local Git history; remote history does not replace local Git.
5. Pass the handoff to a fresh context or authorized subagent. It must inspect actual state before continuing and read only the relevant source/contracts. Do not automatically create a user-owned chat.
6. Confirm previous evidence still applies to the current revision. Re-run a baseline only when drift or risk justifies it; docs-only/cosmetic work does not require application checks. Investigate discrepancies before relying on the baseline.

## Stop when

- The requested scope is complete and its actual checks/remaining acceptance items are reported.
- Three resets on the same item produced no progress: report the blocker, evidence and next diagnostic step. Do not claim completion.

Keep the handoff short, preserve the facts and skip conversational history. Pair with `ds-plan-loop` for next steps and `ds-unstick` for failed hypotheses, respecting the same storage and permission rules.
