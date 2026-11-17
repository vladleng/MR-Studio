---
name: ds-unstick
description: "Break out of a repeating failure: name the assumption behind all the failed fixes, test it directly, list three other approaches, pick one, and restart clean if needed. Use when the same error or wrong behavior comes back after two fix attempts, or when you notice you are retrying small variations. Invoke as /ds-unstick."
---

# ds-unstick

When two fixes fail the same way, the problem is usually a wrong belief, not a wrong edit. Find that belief and test it.

## Steps

1. Stop editing. Do not try a third variation of the same fix.
2. Write down each failed attempt in one line: what you changed and what happened. Include the exact error text.
3. Write the assumption that all of those attempts shared. Typical examples: "this file is the one that runs", "the config is loaded", "the function receives a string", "the cache is cleared", "the error comes from my code". Name one, in one sentence.
4. Test that assumption directly with the cheapest probe that gives a yes or no answer: a print statement, a log line, a `which` or `--version`, a grep for the real call site, a minimal script, or a read of the docs for the exact version installed. Paste the result.
5. If the assumption is false, you have found the real problem. Plan the fix from the new fact.
6. If the assumption holds, list 3 different approaches. "Different" means another mechanism, library, layer or data path, not a tweak of the same edit. Give each approach one line on cost and risk.
7. Pick one approach and say why in one sentence.
8. Inspect the diff and remove only your identified failed-attempt edits with targeted changes, preserving pre-existing work. In MR Studio, do not reset, stash, switch branches or discard files without agreement. If ownership is unclear, preserve the changes and report the uncertainty.
9. Record the false assumption and chosen approach in the session handoff. In MR Studio update the Issue only when that publication is part of the task; do not create PROGRESS.md by default. For other repositories follow their handoff policy.
10. Return to ds-verify-loop with the new approach.

## Stop when

- The probe in step 4 explains the failure, or the new approach passes its check.
- Two rounds of this skill have failed. Stop and report to the user: the attempts, the assumptions you tested, and the evidence. Ask for direction.

## Rules

- Do not repeat a fix that has already failed, even with small changes.
- Evidence beats reasoning. If a probe takes a minute, run it.
- Keep the failed-attempt list, because it is the most useful part of the report.
- Use a clean state before a new approach, so that old edits cannot hide the result.
