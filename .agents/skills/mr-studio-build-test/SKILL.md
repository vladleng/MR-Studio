---
name: mr-studio-build-test
description: Choose proportionate MR Studio / MR Live tests (fast Core first, targeted regression checks, Windows CI only when justified), diagnose failures and prepare acceptance builds. Use for build failures or requested validation; not for docs-only planning.
---

# MR Studio Cloud Build & Test

**Never invent preset/target/path/test commands.** Discover canonical local scripts, CMake presets and target names from the *actual checkout*. Development is online (user decision 2026-11-08): GitHub `main` owns rules and specs, topic branches hold the working code, and GitHub Actions produces Windows builds, tests and artifacts. The local Windows PC is for user acceptance, real ASIO/MIDI/DPI/vendor checks and running the produced artifacts; local builds are allowed but are not the source of truth. Do not merge into `main` without separate user acceptance. See `docs/CLOUD_DEVELOPMENT.md`. Protect existing accepted packages/user projects.

Optional local Windows diagnostic toolchain (not the default CI path): Visual Studio 18 Community with bundled CMake/Ninja at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake\bin\cmake.exe,Ninja\ninja.exe}`; they are **not** on `PATH`, so use the VS developer shell or absolute paths. Git ships portably under the DSH tools directory.

## Choose proportional validation

- **L0 Docs/metadata only:** diff/link/schema sanity; no application build.
- **L1a Runtime-only visual asset:** if SVG/JSON/theme resource is consumed without compilation or generated/embedded source, validate syntax/manifest; user performs visual acceptance; **no configure/build/CTest**. If embedding/generation changes compiled code, use L1b.
- **L1b Cosmetic C++/compiled asset:** check diff and correctness of changed source/resources; user performs visual acceptance. Build the affected GUI target **only when an updated application is requested** (use a scoped Windows CI profile; local incremental build is optional diagnostics). No automatic smoke/screenshots/preview/DPI or full CTest. If no build is requested, explain that the change will appear in the next build. Avoid touching widely included headers when a narrower source can own the change.
- **L1c UI layout/control change:** focused GUI/state/interaction contract checks and any available lightweight compilation first. Verify the affected Windows JUCE target when the change cannot be validated by these checks, is Windows-specific, or a runnable acceptance candidate is requested. No entire matrix or automatic packaging by default. Physical DPI remains a user-side check.
- **L2 Product feature:** targeted model/Undo/SaveOpen/regression tests and relevant app/GUI smoke first. Run a scoped Windows build when Windows/JUCE/ARA/VST3/ASIO integration needs verification or an executable acceptance candidate is due; do not equate every intermediate commit with a Windows Release.
- **L3 Engine, hosting or RT:** L2 plus relevant audio/host/PDC/loop/record checks, RT allocation/lock and callback timing checks where supported. Windows-specific audio/host behavior still requires a relevant scoped Windows check before the affected stage is reported validated; real device, rate/buffer and xrun performance require appropriate local hardware testing.
- **L4 Acceptance/release (only when explicitly in scope or technically justified):** agreed Windows CI integration/release profile, current required CTest suite and touched GUI/integration/smoke/package checks. User acceptance and physical ASIO/MIDI/DPI/vendor validation remain local. Don't rerun full CTest after small iterations or claim new code validated by an older suite.

**Hard anti-overvalidation rule:** A request like “change the background color” defaults to L1a/L1b, not L4. Never start full rebuild, full CTest, all-target Build step, packaging or GUI DPI 100/125/150/200 sweep on such a task without either explicit user direction or a concrete technical reason from changed dependencies / failing target. Record the reason before escalating. CI scheduling and prerequisite gates are enforced by the repository workflows; do not reproduce trigger rules in prompts or bypass a failed gate.

## CI-driven validation (default)

1. Read the assigned Issue and the actual topic branch/HEAD; inspect the changed diff and select L0–L4. Check available tests and workflow definitions in the **current checkout**. Do not invent targets or assume an older topic branch has the same YAML as `main`.
2. Prepare the smallest applicable regression/contract evidence; respect RED → GREEN for bugs. A Linux Core PASS is **not** proof of Windows-specific JUCE/ARA/VST3/ASIO behavior. Report those aspects as NOT TESTED until the relevant check is performed.
3. **CI owns scheduling and ordering:** `.github/workflows/` plus its `tools/ci/` gate scripts enforce prerequisite checks for the exact commit. If a release gate blocks, inspect its reason, complete or fix the missing contract check, and rerun only when justified. Never bypass a gate by changing the trigger, job condition or tested SHA.
4. **Authorization:** follow the current user request and `AGENTS.md` exceptions. Already agreed source commits, scoped checks and justified retries do not need repeat permission; merge into `main` and Issue closure require separate user acceptance.
5. On failure, find the first meaningful compiler/link/test/configure error in the relevant logs and diagnose the failing target first. Follow `AGENTS.md` for sensible CI status checks and repeated-attempt limits. Report the workflow/run/commit, matching product version and artifacts, PASS/FAIL/**NOT TESTED**, and the physical acceptance limits.
6. Downloaded Windows runtime artifacts are tested by the user **outside the checkout**. Do not claim actual ASIO, native DPI, listening, MIDI or proprietary-plugin results from CI; preserve existing builds, projects, settings, SDKs and secrets.

## Optional local Windows diagnostic workflow

Use the local procedure below only when it materially helps debug, or local validation is specifically requested. It does **not** replace a required CI gate, and it must not trigger an unnecessary Windows Release.

1. Identify current branch/HEAD/dirty changes; actual Windows MSVC/JUCE/ASIO toolchain and paths; find repository script → CMake preset → documented command (in that order). See [command discovery](references/command-discovery.md) only as needed.
2. **Use the canonical local script, scoped to the change.** `tools/Build-Local.ps1` now owns the local toolchain: it reads SDK sources from `build/sdk/{juce,vst3,portaudio,ara}` and the licensed ASIO archive from `build/private-sdk/asio.zip`, configures VS 18 into `build/local-windows`, builds the ARA render fixture when ARA is enabled, and accepts these switches:
   - `-Step Configure|Build|Test|All` (default `Build`), `-BuildDir <dir>` (default `build/local-windows`);
   - `-Target <name>[,<name>]` — pass the real target; without it the whole tree is built, which is slow and can exhaust memory;
   - `-Ara` default ON, `-NoAra` for a diagnostic Studio build; `-NoLive` for Studio-only work;
   - `-Jobs <n>` (default 2) — prefer `-Jobs 1` when MSBuild child nodes die silently;
   - `-PrepareSdk` downloads the pinned SDKs through `tools/ci/Prepare-Sdks.ps1` when they are missing;
   - `-Step Test -TestRegex '^(core_|ara_)'` or `-Full` for the acceptance suite;
   - after a successful `Build`/`All` the script refreshes the desktop shortcut `MR Studio.lnk` to the built exe (working directory = exe folder, icon = exe); `-NoShortcut` skips it, `-ShortcutName` renames it.

   If the script cannot express what a task needs, derive `cmake --build <build-dir> --config Release --target <actual-target> --parallel <safe-jobs>` from the current local configuration; never guess target names or raise jobs beyond machine resources. Select tests that exercise the change; reuse the build tree when safe. Reconfigure or clean only when build files, SDK, toolchain, dependencies or a stale cache demand it. [JUCE/Windows notes](references/windows-juce-validation.md) if toolchain problems occur.

   **Known machine limit:** parallel MSBuild on this PC can silently return exit code 1 with "0 errors" when the build exhausts memory. Rerun the same project with `-Jobs 1` before treating it as a code failure. A full cold configure+build of the JUCE GUI target takes roughly 25–40 minutes single-threaded; incremental target builds take seconds to a few minutes.
3. Before a requested local step, state scope (e.g. `L1b / changed app GUI target`). For purely cosmetic work, local build is optional and visual acceptance belongs to the user; do not proceed into packaging, shortcuts or a release gate by default.
4. On failure capture **first meaningful** configure/compiler/link/runtime error, fix root cause, rerun failing step and downstream invalidated tests; do not chase cascades, delete whole build trees reflexively or mutate unrelated code to make green.
5. Check startup/editor lifetime and project Save/Open/Undo if affected. Audio device/recording/ASIO checks only when relevant; unavailable hardware → **NOT RUN**, never PASS. GUI software scaling is not proof of physical mixed-DPI focus/window behavior.
6. Preserve previous version folders and matching symbols. Verify package/launcher hashes and working directory when delivering release candidate; exclude logs, source, fixtures and debug symbols from compact user-facing runtime package unless release policy explicitly says otherwise.
7. Record exact commit, build configuration, commands, PASS/FAIL/NOT TESTED, runtime smoke if performed, hardware limits, package/manifest hash if applicable, manual acceptance needs and scope. Optional [report](assets/build-report-template.md), [validation matrix](assets/validation-matrix-template.md) and [stage checklist](assets/stage-acceptance-checklist.md) only when useful.

## Acceptance rules

- Code is **READY** only if required checks pass; **READY WITH MANUAL CHECK** if only explicit user/hardware checks remain; otherwise **BLOCKED** or **NOT VALIDATED**. Do not close an Issue before user acceptance of that scope; a passed build alone isn't acceptance.
- Use task branches within the assigned scope; follow the existing approval to run scoped Actions, push task-branch source and create a PR where applicable. Do not merge into `main` or close an Issue without the required user acceptance. Docs-only rule updates may be published to `main` with `[skip ci]` under `AGENTS.md`; do not treat them as a product build. Preserve shared Studio/Live engine invariants; Live low-latency requirements do not automatically apply to Studio mixing.
