---
name: mr-studio-build-test
description: Validate a local MR Studio code change with CMake/JUCE compile, focused CTest, GUI/app smoke checks and release acceptance. Use for build failures or explicit test/build requests; not for a docs-only planning task.
---

# MR Studio Cloud Build & Test

**Never invent preset/target/path/test commands.** Discover canonical local scripts, CMake presets and target names from the *actual checkout*. Use Codex Cloud for development and GitHub Actions for supported Windows builds/tests/artifacts. Follow docs/CLOUD_DEVELOPMENT.md; do not merge without separate user acceptance. Protect existing accepted packages/user projects.

## Choose proportional validation

- **L0 Docs/metadata only:** diff/link/schema sanity; no application build.
- **L1a Runtime-only visual asset:** if SVG/JSON/theme resource is consumed without compilation or generated/embedded source, validate syntax/manifest; user performs visual acceptance; **no configure/build/CTest**. If embedding/generation changes compiled code, use L1b.
- **L1b Cosmetic C++/compiled asset:** one **incremental build of affected GUI target only** in existing configured tree + diff/source check; user performs visual acceptance, no automatic smoke/screenshots/DPI for cosmetic-only work. No full CTest or unrelated product tests. Avoid touching a widely included header when an existing `.cpp` or runtime theme implementation can safely own the color.
- **L1c Local UI layout/control change:** incremental affected target + specific GUI/state/interaction tests and relevant scale/layout previews only, not the entire matrix by default.
- **L2 Product feature:** affected build, targeted model/Undo/SaveOpen/regression tests, relevant app/GUI smoke.
- **L3 Engine, hosting or RT:** L2 + relevant audio/host/PDC/loop/record tests, no RT allocation/lock, rate/buffer/CPU max callback; physical device tests only if available and relevant.
- **L4 Acceptance/release (only when explicitly in scope):** local clean/appropriate build, full **current required** CTest suite plus touched GUI/integration/smoke/packaged checks. Don't rerun full CTest after every small iteration, but don't claim new source validated by an older suite.

**Hard anti-overvalidation rule:** A request like “change the background color” defaults to L1a/L1b, not L4. Never start full rebuild, full CTest, all-target Build step, packaging or GUI DPI 100/125/150/200 sweep on such a task without either explicit user direction or a concrete technical reason from changed dependencies / failing target. Record the reason before escalating.

## Build and CI workflow

1. Identify current branch/HEAD/dirty changes; actual Windows MSVC/JUCE/ASIO toolchain and paths; find repository script → CMake preset → documented command (in that order). See [command discovery](references/command-discovery.md) only as needed.
2. **Choose target-scoped build explicitly.** In the current workspace `tools/Build-Local.ps1 -Step Build` is documented without a target and may build many unrelated projects, while `-Jobs` defaults to 2. Before invoking it, inspect whether the script accepts an explicit target. If not, derive the correct `cmake --build <existing-build-dir> --config Release --target <actual-target> --parallel <safe-jobs>` from the **current local** configuration; do not guess target names or silently increase jobs beyond machine resources. Select tests that exercise the change; reuse build tree when safe. Reconfigure/clean only if build files, SDK, toolchain, dependencies or stale cache demand it. [JUCE/Windows notes](references/windows-juce-validation.md) if toolchain problems occur.
3. Before executing any step, state scope (e.g. `L1b / changed app GUI target / one smoke`). For an L1 task, stop after one successful affected build and visual verification; do not continue into packaging, shortcuts or a release gate by default.
4. On failure capture **first meaningful** configure/compiler/link/runtime error, fix root cause, rerun failing step and downstream invalidated tests; do not chase cascades, delete whole build trees reflexively or mutate unrelated code to make green.
5. Check startup/editor lifetime and project Save/Open/Undo if affected. Audio device/recording/ASIO checks only when relevant; unavailable hardware → **NOT RUN**, never PASS. GUI software scaling is not proof of physical mixed-DPI focus/window behavior.
6. Preserve previous version folders and matching symbols. Verify package/launcher hashes and working directory when delivering release candidate; exclude logs, source, fixtures and debug symbols from compact user-facing runtime package unless release policy explicitly says otherwise.
7. Record exact local commit, build configuration, commands, PASS/FAIL/NOT RUN, runtime smoke, hardware limits, package/manifest hash, manual acceptance needs and scope. Optional [report](assets/build-report-template.md), [validation matrix](assets/validation-matrix-template.md) and [stage checklist](assets/stage-acceptance-checklist.md) only when useful.

## Acceptance rules

- Code is **READY** only if required checks pass; **READY WITH MANUAL CHECK** if only explicit user/hardware checks remain; otherwise **BLOCKED** or **NOT VALIDATED**. Do not close an Issue before user acceptance of that scope; a passed build alone isn't acceptance.
- Use task branches and configured CI within the assigned scope; no merge without separate user acceptance. Updating GitHub docs/issues as agreed is not code publication. Preserve Studio/Live shared engine invariants rather than presuming separate products and never impose Live low-latency goals on Studio mixing by default.
