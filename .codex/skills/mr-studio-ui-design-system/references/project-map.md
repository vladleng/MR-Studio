# MR Studio source and evidence map

Observed 2026-10-06; rediscover symbols if the tree changes. This is navigation,
not a prescribed architecture or proof that every existing value is approved.

## Workspace discovery

The unified source/build/chat workspace since 2026-10-15 is
`C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio`, including Git,
`.codex/skills`, `assets`, `build`, `Builds` and `Saves`.
The former `Documents/GitHub/MR-Studio` checkout is retained only for rollback;
do not develop or build there. Confirm the checkout with Git status, its
CMakeLists and UI sources. See `docs/LOCAL_WORKSPACE.md` for build commands and
migration evidence. Do not edit a packaged copy as though it were source.

Read applicable AGENTS.md and `docs/LOCAL_BUILD_POLICY.md`. Current project policy
keeps source/builds/tests/packages local, with GitHub reserved for authorized
issues/docs updates and no Actions. A UI task alone does not require GitHub updates.

## Implementation owners (paths relative to checkout)

| Area | Start here | What to inspect |
|---|---|---|
| Shared drawing/font/gestures | `apps/studio-juce/src/Controls.h` | `ui::Theme`, palette constants, `Handle`, focus and accessibility |
| Shell/layout and channel views | `apps/studio-juce/src/Desktop.h`, `Desktop.cpp` | `Desktop`, `Strip`, `Arrangement`, `Browser`, resize and event handlers |
| Editors/settings | `apps/studio-juce/src/Editors.cpp` | `AudioPanel`, `FxPanel`, plugin shells and ownership |
| App window | `apps/studio-juce/src/Main.cpp` | window construction, title and app lifecycle |
| UI regressions | `apps/studio-juce/src/Smoke.cpp`, `J1Smoke.h`, smoke helpers in UI files | actual screenshot/output paths and supported scenarios |
| Shared application operations | `apps/studio-desktop/src/application.cpp` | command, Undo, preview/commit and device/state contracts |
| Build/test definitions | `cmake/Juce.cmake`, `CMakeLists.txt` | current JUCE targets and CTest smoke entries |

Useful bounded searches: `rg --files apps/studio-juce`,
`rg -n 'Theme|Handle|paint|resized|setLookAndFeel' apps/studio-juce/src`,
`rg -n 'smoke|preview|createComponentSnapshot' apps/studio-juce cmake/Juce.cmake`.
Do not recursively scan every historical build for a component change.

## Observed baseline, not new mandates

`Controls.h` currently has `background=0xff303438`, `panel=0xff393e43`,
`surface=0xff25282b`, `accent=0xff006dcc`. Theme embeds Noto Sans, uses
`FontOptions(...).withPointHeight(size)`, and draws buttons with radius 3.
Inspect actual usage before mapping these into semantic roles. Do not replace the
existing radius with 4/6/8/12 or change point-height semantics simply because a
candidate schema suggests them. Several local colors remain outside the shared
palette; consolidate only the categories touched by the task or a requested audit.

Relevant history: `docs/JUCE_FIX1.md`, `JUCE_FIX2.md`, `JUCE_FIX3.md`, `JUCE_J3.md`.
Read only the documents relevant to affected behavior. Working source + a verified
current screenshot take precedence over historical implementation descriptions;
explicit user preferences govern the intended design.

## Interaction contracts to preserve when touched

- `Handle`: pointer gestures start on the handle; relative dragging, Ctrl fine
  adjustment, preview during drag, one commit on release, cancel on Escape/focus loss.
  Preserve its keyboard and ranged-value accessibility interface.
- Space transport handling guards repeats and editing contexts; Backspace/Delete
  deletion and application shortcuts must retain their existing focus guards.
- Mixer/arrangement share track identities/order; Master is fixed. Splitters,
  scrolling, equal fader/meter geometry and insert overflow remain functional.
- Plugin views are externally owned native children inside MR shells. Style the
  shell without replacing native editor HWNDs, changing focus/lifetime or restoring
  plugin state as a side effect of painting.
- Existing polling/peak-hold diagnostics are functional. The observed Desktop
  refresh is 30 Hz; changing polling, meter decay or DSP definitions is separate
  behavior work, not a necessary consequence of changing colors.
