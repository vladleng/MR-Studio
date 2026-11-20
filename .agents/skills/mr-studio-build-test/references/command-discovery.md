# Command and target discovery

Use this reference to find real local build commands, not to infer them from historical CI.

## 1. Inspect the affected local entry points

- `tools/Build-Local.ps1` and `tools/Enter-DevEnvironment.ps1`
- `CMakePresets.json`, `CMakeUserPresets.json` when present, and `CMakeLists.txt`
- Target definitions and the matching configured build tree
- [Local build policy](../../../../docs/LOCAL_BUILD_POLICY.md) and root [AGENTS.md](../../../../AGENTS.md)

Read only the files needed for the selected product and check. Tool installation alone does not prove configuration or build readiness.

## 2. Choose the narrowest supported command

Priority: current local build script → applicable CMake preset → documented local command → derived command after checking actual targets and configuration. Inspect the script interface before invoking it; do not invent parameters or target names.

Keep Studio-only / Live-only / Core scope. Studio defaults to ARA ON; Live standalone is ARA OFF. Studio ARA-OFF needs an explicit diagnostic request. Full CTest requires an agreed integration/release candidate or a concrete risk justification. Docs-only and cosmetic work follow the exceptions in AGENTS.md.

## 3. Record reproducible evidence

Record the actual local branch/HEAD and dirty state, configure/build/test entry points, targets, configuration, SDK pins, commands and PASS/FAIL/NOT TESTED. Include executable/package paths when created and identify hardware checks that remain unperformed. Keep logs outside Git; record meaningful results in the assigned DSH task within the agreed scope and report in chat. Load `dsh-task-manager-workflow` before journal operations.

If a historical target or path no longer exists, use the actual local checkout. Missing local tools or SDKs are a validation limitation, not authorization to install dependencies or dispatch Actions.

## 4. Explicit remote exceptions

Source push, PR creation and Actions are outside the normal local process and require an explicit user request. Only then inspect the current workflow inputs, scope and gates. `docs/CLOUD_DEVELOPMENT.md` and older Actions runs are historical evidence; they do not override local rules or validate a different revision.

Merge into local `main` requires separate explicit permission. Passing checks do not establish user/hardware acceptance or authorize marking the DSH task Done.
