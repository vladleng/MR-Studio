---
name: mr-studio-build-test
description: Choose proportionate local MR Studio / MR Live builds and tests, diagnose failures and prepare acceptance packages. Use for build failures or requested validation; not for docs-only planning.
---

# MR Studio Local Build & Test

Development, builds, tests and artifacts run on the local Windows PC (user decision 2026-11-16). Tasks, status and acceptance live in DSH Task Manager; load `dsh-task-manager-workflow` before journal operations. Follow root `AGENTS.md` and [local policy](../../../docs/LOCAL_BUILD_POLICY.md). Do not push source/rules, create PRs or dispatch Actions automatically. Preserve local Git history, SDKs, accepted packages and user projects.

**Never invent preset/target/path/test commands.** Discover scripts, CMake presets and targets in the actual checkout. Installed tooling does not prove that SDKs, configuration or compilation work.

## Choose proportional validation

- **L0 Docs/metadata:** diff, links and schema/frontmatter sanity; no application build.
- **L1a Runtime-only visual assets:** syntax/manifest checks and user visual acceptance; no configure/build/CTest.
- **L1b Cosmetic C++/compiled assets:** source/resource review; build the affected GUI target only when an updated application is requested. No automatic smoke, previews, screenshots or DPI sweep. Otherwise explain that changes appear in the next build.
- **L1c Layout/control behavior:** focused state/interaction checks, then affected Windows JUCE target when needed to validate the change or provide a runnable candidate.
- **L2 Product feature:** targeted model/Undo/SaveOpen/regression and affected app/GUI checks; scoped local Windows build for changed integration or a requested candidate.
- **L3 Engine/hosting/RT:** L2 plus relevant host/PDC/loop/record regressions and available allocation/lock/callback timing checks. Physical ASIO/MIDI/vendor behavior requires actual hardware checks.
- **L4 Acceptance/release:** agreed local integration/release build and necessary CTest suite; full CTest only when explicitly agreed or justified by concrete risk.

A cosmetic edit does not justify full rebuild, all-target build, full CTest, packaging, shortcut changes or mass DPI checks. Preserve Studio-only / Live-only scope. Studio uses ARA2 by default; Live standalone is always without ARA2, and a joint build keeps ARA2 confined to Studio. Studio ARA-OFF requires an explicit diagnostic request.

## Local execution

1. Check status, branch, exact HEAD and dirty diff. Inspect installed MSVC/Windows SDK/CMake and required SDK paths. See [command discovery](references/command-discovery.md) only as needed; current files and root policy override historical examples.
2. State L0–L4 scope and actual targets/tests. For bugs establish RED before fixing, then GREEN using the same regression. Use the documented manual exception only where automated reproduction is impractical.
3. Read `tools/Build-Local.ps1` before invoking it. It initializes `Enter-DevEnvironment.ps1` automatically and accepts `-Step Configure|Build|Test|All`, `-Product Core|Studio|Live|Both`, `-Target`, `-BuildDir`, `-Configuration`, `-Jobs`, `-TestRegex`, `-Full`, `-NoAra`, `-AraFixture` and `-DryRun`. Default is a scoped Studio build with ARA ON and Live OFF, using `build/sdk`, `build/private-sdk` and separate `build/local-windows/<profile>` trees. DryRun returns the command plan without configure/build/test. Test requires explicit selection and a matching configured tree; build the required test targets before running it. ARA render/model tests require the explicit fixture option; Full with ARA rejects a missing fixture. Do not invoke all-target builds merely to obtain one target.
4. `tools/ci/Prepare-Sdks.ps1` and `tools/ci/sdk-lock.json` provide pinned SDK preparation. Their location under `tools/ci` does not require Actions. Downloads/installations are a separate environment-preparation step, never part of docs-only validation. Check pinned SDK files before execution; the local build script does not download them.
5. If scripts cannot express the scope, derive scoped CMake commands from the actual configured tree and target definitions. Reconfigure only when required; preserve existing build trees. Use conservative job counts; if parallel MSBuild dies without a useful error, retry once with one job and inspect resource pressure.
6. On failure read the first substantive configure/compiler/link/test error, form a new hypothesis, change one cause and rerun affected checks. Never weaken tests or delete unrelated build trees to obtain PASS.
7. Record branch/HEAD, dirty state, exact commands, configuration, SDK pins and PASS/FAIL/NOT TESTED. Keep logs outside Git; record meaningful task evidence in the assigned DSH task within the agreed scope. A check on a different revision does not validate current code.

Visual Studio 18 Community may supply CMake/Ninja outside PATH; use its developer shell or verified absolute paths. Machine-specific paths are examples, not proof of availability. [Windows/JUCE notes](references/windows-juce-validation.md) are optional troubleshooting material.

## Delivery and acceptance

- Before assigning a build version or preparing a package, read [versioning rules](../../../docs/VERSIONING.md). Prepare acceptance packages outside checkout with matching component version, manifest and symbols; preserve earlier versions. Exclude SDKs, fixtures, secrets, proprietary plugins and user files.
- Check startup/editor lifetime and Save/Open/Undo when affected. Report listening, ASIO/MIDI, physical DPI and vendor checks only when actually performed; unavailable hardware remains NOT TESTED.
- READY requires necessary checks; READY WITH MANUAL CHECK means only stated user/hardware checks remain; otherwise report NOT VALIDATED or BLOCKED. Build PASS is not acceptance, marking the DSH task Done or permission to merge into local `main`.
- Historical CI configuration is retained but is not a required local gate. Actions or remote publication require a separate request; then inspect real workflows rather than bypassing gates.
- Optional [report template](assets/build-report-template.md), [matrix](assets/validation-matrix-template.md) and [stage checklist](assets/stage-acceptance-checklist.md) are aids, not parallel status journals; root policy overrides historical CI wording in them.
