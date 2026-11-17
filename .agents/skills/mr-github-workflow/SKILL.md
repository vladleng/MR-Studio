---
name: mr-github-workflow
description: Coordinate MR Studio / MR Live GitHub Issues and task discussion alongside local development, checks and user acceptance. Use for issue tracking and cross-harness handoff; remote source changes and Actions require a separate user request.
---

# MR GitHub Issues workflow

User decision 2026-11-16: code, rules, Git history, builds, tests and packages live on the local Windows PC. GitHub is used for Issues and task discussion. This adapter does not authorize source publication, PR creation, Actions, force-push or merge.

## Entry and evidence

1. Read current root `AGENTS.md`, the assigned Issue/sub-issue when present, and local status/branch/HEAD. Use [local policy](../../../docs/LOCAL_BUILD_POLICY.md) and `mr-studio-build-test` for build/test decisions; load other skills only for affected areas.
2. Respect precedence: current request and accepted decisions; `AGENTS.md` and Issue; relevant `mr-*` skills; generic `ds-*` techniques. Generic ship/watch/PR workflows do not override the local process.
3. When Issue updates are part of the task, keep requirements, acceptance criteria, meaningful results and blockers there. Merely assigning an Issue as a requirements source does not authorize comments; otherwise report in chat. Do not introduce PLAN.md/PROGRESS.md or parallel task journals by default. Without an assigned Issue, follow the specific request and report in chat.
4. Use real `gh` or available GitHub connector/API tools for Issues. Verify access without printing tokens. If access fails, continue independent local work and report the missing Issue update; do not claim it was posted.

## Local change to Issue evidence

- Preserve local branch/HEAD and dirty work. Do not replace them with a published branch. Use local commits within the task; merge into local `main` requires separate explicit permission.
- For bugfixes capture RED, GREEN and relevant regression results; otherwise state actual acceptance checks. Report exact commands, local SHA/dirty state, results, package location and NOT TESTED items. Do not upload code, sensitive logs, SDKs or user data to Issues.
- Issue comments on the user's behalf require authorization from the task or root policy; use `ds-pr-comments` when actually posting a GitHub comment. Existing PR/Actions links are historical evidence, never validation of new local changes.
- After explicit user acceptance close only the accepted Issue/sub-issue as Completed, following `AGENTS.md`. Build PASS is not acceptance or permission to close the parent.
- For docs/skill-only changes check text, frontmatter, links and diff; do not run application builds.

## Explicit remote exceptions

Only when separately requested, inspect actual tools, permissions, branch SHA and workflows for remote code/PR/Actions work. Do not invent commands, bypass gates or auto-merge. Historical remote instructions do not constitute current authorization.
