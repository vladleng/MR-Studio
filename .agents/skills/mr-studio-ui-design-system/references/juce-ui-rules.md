# JUCE implementation and visual verification

## Use current owners

Start from the [project map](project-map.md) and inspect the relevant source.
Reuse `ui::Theme`, existing controls and JUCE ColourIds rather than introducing a
parallel MRTheme/MRLookAndFeel hierarchy. Extract a reusable component where there
is repeated drawing/behavior or a clear independent responsibility. Avoid turning
a one-component change into a folder reorganization.

Use the same calculated geometry for painting, hit testing, dragging and focus
indicators. Keep layout in resized/layout helpers; paint should draw current state,
not resize children, modify the model or rebuild assets. Clamp small dimensions and
account for viewports, clipping and overlay intercepts. Changing visual handle size
must preserve the intended gesture travel, value mapping and neighboring targets.

## Theme, assets and lifetime

Use existing semantic colors/font helpers; introduce reusable roles deliberately.
Keep simple transport/shape icons as JUCE Paths where appropriate. Parse SVG or
create fonts/drawables outside hot paint paths; cache stable expensive geometry,
and refresh caches when size, scale or theme changes. Avoid caching dynamic meters
or waveforms in ways that hide fresh state. Embed only licensed assets.

LookAndFeel must outlive its attached components; detach references before its
owner disappears. Preserve existing teardown order. Async chooser/menu/dialog
callbacks that outlive an owner need the existing safe-pointer/lifetime pattern.
Do not capture a raw component indefinitely or let a styling edit change native
plugin child-window ownership, preset restore, editor attach/detach or focus rules.

## Audio and UI boundary

Use existing message-thread timers and immutable/atomic snapshots for meters and
transport. Never call UI painting/listeners or allocate UI assets from audio work.
Avoid adding plugin queries, synchronous disk work or graph mutation to repaint.
Do not change meter decay, peak hold, CPU normalization or timer rates merely to
make an animation attractive. Bound repaint regions/work; measure suspected costly
paths before adding broad caching or changing renderer/backend.

## Accessibility and states

Preserve control titles/names, keyboard activation and custom AccessibilityHandler
value interfaces. Test focus independently of selected/toggled state; disabled
controls must both look disabled and reject unintended actions. Retain contextual
shortcut guards so typing in fields/native plugin editors does not trigger transport
or deletion. Use text/shape as well as color for critical record/fault/clip states.
Check long Latin/Cyrillic labels, font fallback, tooltips and value units.

## Scaling and evidence

Use logical JUCE bounds, retaining the existing font-height convention. Do not
multiply every coordinate by OS scale or force all strokes to integer logical
coordinates: inspect how fractional scaling actually renders. Check resized layouts,
menus, scrollbars, clipping and hit regions as well as paint. A 1.5x software
snapshot validates that rendering path, not physical monitor transfer or native
plugin-window DPI. Record which was actually exercised.

Prefer existing fixture smoke/preview hooks; discover them in source/CMake. Never
use a normal app launch or an installed plugin session as an incidental screenshot
harness when it could change real preferences, projects or device connections.
On Windows, wait for GUI smoke process completion and check its exit code; a launch
alone is not a passing test. Use a task-specific output directory for previews and
logs, and compare equivalent states. Reuse local cached build dependencies and the
project build/test skill for code verification. Report unsupported physical or
screen-reader checks honestly without making them a blanket blocker.
