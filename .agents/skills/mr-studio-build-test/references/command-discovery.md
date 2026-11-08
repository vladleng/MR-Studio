# Command and target discovery

Use this reference before inventing build commands.

## 1. Inspect root files

Look for:
- `CMakePresets.json`
- `CMakeUserPresets.json`
- `CMakeLists.txt`
- build helper scripts
- README/build docs
- GitHub Actions workflow files and `docs/CLOUD_DEVELOPMENT.md` (primary for online Windows CI)
- local build scripts and CMake presets (for optional local diagnostics)

## 2. Prefer canonical entry points

Priority for **online Windows validation**:
1. current GitHub Actions workflow, inputs, CI markers and target scope
2. `docs/CLOUD_DEVELOPMENT.md` and applicable `AGENTS.md` rules
3. current build/test scripts invoked by those workflows
4. derived command only after checking actual targets

For **optional local diagnostics**: project script → CMake preset → documented command → verified derived command.

## 3. Record the discovered contract

Write down:
- configure entry point
- build entry point
- test entry point
- app executable path
- Studio target
- Live target
- shared/core target
- required configuration
- hardware-only tests

## 4. Do not hardcode stale names

If a prior stage used a target or path that no longer exists, trust the current repository.

## 5. Online validation policy (2026-11-08)

GitHub `main` is the authority for rules, specifications and Issues; topic branches contain the current work. GitHub Actions is the default for Windows builds/tests/artifacts, while the local Windows machine serves user acceptance and physical ASIO/MIDI/DPI/vendor checks.
- Select an affected Studio-only / Live-only CI profile when available; read the actual workflow before using `checks=studio`, `checks=live`, `[ci studio]` or `[ci live]`.
- Full integration is a separate agreed gate or requires a justified risk; do not run it for each small change.
- Follow the current `AGENTS.md` and task authorization: necessary scoped Actions runs/retries may already be approved, so no blanket requirement to request permission for every run. Cosmetic-only changes are checked without CI unless an updated application is requested.
- Task-branch changes may be pushed as agreed. Merge into `main` and closing Issues require user acceptance. Local scripts are optional diagnostics, not the mandatory development path.
