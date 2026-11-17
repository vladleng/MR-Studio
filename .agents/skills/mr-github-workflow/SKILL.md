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

## Issue / Sub-issue lifecycle

Creation and native hierarchy are user decisions. Do not create Issues, Sub-issues or parent/child links on your own initiative, and do not rewrite an existing hierarchy "for order". When the task authorizes creation, follow the rules below.

**When a new parent Issue:** a new user-visible area or stage has requirements that no current Issue covers, or the work needs its own acceptance. A one-off check, review or small fix gets no own Issue: follow the request and report in chat, or comment in the current Issue when that is authorized.

**When a Sub-issue:** the parent stage needs an independent slice with its own acceptance, or a significant addition starts a new stage. The third version digit is the Sub-issue ordinal — see [versioning rules](../../../docs/VERSIONING.md). A small follow-up inside the same Sub-issue is a `-UpdN` suffix or a comment, not a new Sub-issue. Markdown checklists and links are not native hierarchy.

**Body:** goal and context; scope that is in; verifiable acceptance criteria (a command or an observation, not "looks right"); what is out of scope; checks (L0–L4 per `mr-studio-build-test`); version. An empty body, or "see docs" alone, is not enough for a Sub-issue. In the GitHub UI start from the repository forms: `.github/ISSUE_TEMPLATE/parent.yml` and `.github/ISSUE_TEMPLATE/sub-issue.yml`.

**Title:** `[Area] NN — short scope` for a Sub-issue, `[Area / Topic] short scope` for a standalone parent; keep the existing project style.

**Native attachment:** attach and verify only through the real API or UI. Read check: `gh api repos/{owner}/{repo}/issues/<parent>/sub_issues`. Claim counts or percentages only from an actual API/UI result, never from a checklist. If the user attaches native links personally, do not duplicate that step and do not claim it was performed.

**Closure:** a Sub-issue closes as Completed only after explicit user acceptance of its own scope; closing one child does not accept the parent. The parent closes after all children are completed and the parent scope is accepted. Closure is independent of merge into local `main`.

## Explicit remote exceptions

Only when separately requested, inspect actual tools, permissions, branch SHA and workflows for remote code/PR/Actions work. Do not invent commands, bypass gates or auto-merge. Historical remote instructions do not constitute current authorization.
