---
name: mr-studio-build-test
description: Validate a local MR Studio code change with CMake/JUCE compile, focused CTest, GUI/app smoke checks and release acceptance. Use for build failures or explicit test/build requests; not for a docs-only planning task.
---

# MR Studio Local Build & Test

**Never invent preset/target/path/test commands.** Discover canonical local scripts, CMake presets and target names from the *actual checkout*. Development is local (user decision 2026-11-07): source, configure/build, CTest, binaries, GUI screenshots and packages stay on this machine. Do not launch GitHub Actions, push source, open PRs or merge without a separate explicit request. `docs/CLOUD_DEVELOPMENT.md` is the closed online-development stage, not current policy. Protect existing accepted packages/user projects.

Local toolchain baseline: Visual Studio 18 Community with bundled CMake/Ninja at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake\bin\cmake.exe,Ninja\ninja.exe}`; they are **not** on `PATH`, so use the VS developer shell or absolute paths. Git ships portably under the DSH tools directory.

## Choose proportional validation

- **L0 Docs/metadata only:** diff/link/schema sanity; no application build.
- **L1a Runtime-only visual asset:** if SVG/JSON/theme resource is consumed without compilation or generated/embedded source, validate syntax/manifest; user performs visual acceptance; **no configure/build/CTest**. If embedding/generation changes compiled code, use L1b.
- **L1b Cosmetic C++/compiled asset:** one **incremental build of affected GUI target only** in existing configured tree + diff/source check; user performs visual acceptance, no automatic smoke/screenshots/DPI for cosmetic-only work. No full CTest or unrelated product tests. Avoid touching a widely included header when an existing `.cpp` or runtime theme implementation can safely own the color.
- **L1c Local UI layout/control change:** incremental affected target + specific GUI/state/interaction tests and relevant scale/layout previews only, not the entire matrix by default.
- **L2 Product feature:** affected build, targeted model/Undo/SaveOpen/regression tests, relevant app/GUI smoke.
- **L3 Engine, hosting or RT:** L2 + relevant audio/host/PDC/loop/record tests, no RT allocation/lock, rate/buffer/CPU max callback; physical device tests only if available and relevant.
- **L4 Acceptance/release (only when explicitly in scope):** local clean/appropriate build, full **current required** CTest suite plus touched GUI/integration/smoke/packaged checks. Don't rerun full CTest after every small iteration, but don't claim new source validated by an older suite.

**Hard anti-overvalidation rule:** A request like “change the background color” defaults to L1a/L1b, not L4. Never start full rebuild, full CTest, all-target Build step, packaging or GUI DPI 100/125/150/200 sweep on such a task without either explicit user direction or a concrete technical reason from changed dependencies / failing target. Record the reason before escalating.

## Local build workflow

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
3. Before executing any step, state scope (e.g. `L1b / changed app GUI target / one smoke`). For an L1 task, stop after one successful affected build and visual verification; do not continue into packaging, shortcuts or a release gate by default.
4. On failure capture **first meaningful** configure/compiler/link/runtime error, fix root cause, rerun failing step and downstream invalidated tests; do not chase cascades, delete whole build trees reflexively or mutate unrelated code to make green.
5. Check startup/editor lifetime and project Save/Open/Undo if affected. Audio device/recording/ASIO checks only when relevant; unavailable hardware → **NOT RUN**, never PASS. GUI software scaling is not proof of physical mixed-DPI focus/window behavior.
6. Preserve previous version folders and matching symbols. Verify package/launcher hashes and working directory when delivering release candidate; exclude logs, source, fixtures and debug symbols from compact user-facing runtime package unless release policy explicitly says otherwise.
7. Record exact local commit, build configuration, commands, PASS/FAIL/NOT RUN, runtime smoke, hardware limits, package/manifest hash, manual acceptance needs and scope. Optional [report](assets/build-report-template.md), [validation matrix](assets/validation-matrix-template.md) and [stage checklist](assets/stage-acceptance-checklist.md) only when useful.

## Acceptance rules

- Code is **READY** only if required checks pass; **READY WITH MANUAL CHECK** if only explicit user/hardware checks remain; otherwise **BLOCKED** or **NOT VALIDATED**. Do not close an Issue before user acceptance of that scope; a passed build alone isn't acceptance.
- Use task branches within the assigned scope; no merge without separate user acceptance. GitHub Actions, source push, PR and merge only on a separate explicit request; updating GitHub docs/issues as agreed is not code publication. Preserve Studio/Live shared engine invariants rather than presuming separate products and never impose Live low-latency goals on Studio mixing by default.
