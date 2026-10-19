# Command and target discovery

Use this reference before inventing build commands.

## 1. Inspect root files

Look for:
- `CMakePresets.json`
- `CMakeUserPresets.json`
- `CMakeLists.txt`
- build helper scripts
- README/build docs
- CI files only as secondary evidence

## 2. Prefer canonical entry points

Priority:
1. project script
2. CMake configure/build preset
3. documented command
4. derived command

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

## 5. CI is not the local source of truth

CI may help reveal intended commands, but when the project workflow is local-first:
- validate locally;
- use CI only if the user asks or if CI is itself part of the task.
