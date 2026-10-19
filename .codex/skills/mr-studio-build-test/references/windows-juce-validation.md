# Windows + JUCE validation notes

Use only when the repository is built on Windows.

## Toolchain

Do not assume MSVC unless the repository indicates it.

If MSVC is required, verify the environment is initialized correctly and that:
- compiler is accessible;
- Windows SDK resolves;
- architecture matches the configured build.

## CMake generator mismatch

A cached build directory configured with one generator/toolchain should not be reused blindly with another.

If generator mismatch is proven:
- use the correct existing preset/build directory, or
- create/clean the intended build directory according to project policy.

## JUCE

Check that the actual JUCE integration resolves:
- submodule;
- FetchContent;
- package;
- local source path;
- other project-specific mechanism.

Do not "fix" a missing JUCE dependency by embedding a random new JUCE copy.

## ASIO

Treat ASIO/device validation as hardware/driver dependent.

When the repository has ASIO-specific tests:
- run them only when the expected driver/device is present;
- report exact device/sample rate/buffer used;
- distinguish automated pass from manual playback confirmation.

Never claim ASIO behavior was validated when only a compile succeeded.
