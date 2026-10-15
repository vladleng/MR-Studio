# MR UI Kit: core controls #192

Local implementation on the accepted #191 token/Theme foundation. This is the
#192 reusable control slice and standalone demonstration, not #193 Gallery or
#194 host binding. User visual acceptance is pending.

## Ownership and reuse

`apps/studio-juce/src/MRControls.h`, namespace `ui::kit`, is the single control
implementation. CMake consumers link `mrs_ui_controls` (INTERFACE): GUI-only JUCE,
existing `mrs_juce_assets`, and `mrs_control_assets`. This supplies include paths,
C++20 and the canonical power/plus SVGs. No audio, processing, desktop or ASIO
library is linked. The showcase `MRControlsPreview` uses these exact classes.
Existing `ui::Theme` remains the only LookAndFeel; scope it to the owning subtree,
keep it alive longer than its children and detach before destruction. No change
to the accepted DAW, MR Rig or MR Reverb UI is made by including this library.

```cpp
#include "MRControls.h"
using namespace ui::kit;
MRKnob frequency({"frequency", "Frequency", Unit::hz,
                  20, 20000, 1000, 1, Mapping::logarithmic, 0});
// Attach to a component with the existing ui::Theme.
frequency.onEvent = [](const ControlEvent& event) {
    // GUI-only event consumer. Host/model/Undo binding belongs to #194.
};
```

## Public controls

- `MRKnob`: mini 80x104, standard 120x144, large 160x184 logical component size;
  slider hit rectangles 40/72/112, arc diameter approximately 24/56/96. All have
  an independently editable value field and label. These are preferred sizes;
  consumers must preserve the minimum hit/entry geometry when placing them.
- `MRFader`: horizontal or vertical, physical units, independent value field,
  linear, true logarithmic or zero-centred bipolar mapping.
- `MRButton`, `MRIconButton`: native JUCE button semantics; cached original MR UDS
  power/plus SVGs from `assets/ui/mr-uds/svg/icons/`. Accessible icon names are
  required. Power is a native toggle; its green dot supplements the state.
- `MRToggle` / `MRSwitch`: native switch state with explicit focus outline.
- `MRSegmented`: 1..8 native radio buttons, one selection, arrow/Home/End navigation.
  `MRSelector` / `MRDropdown`: native JUCE Unicode combo/popup, zero-based select API.
- `MRValueField`: units none/dB/Hz/ms/%/ratio, valid/error states, last good value.
- `MRPanel`: section/header plus non-owning JUCE viewport/content and bounded scroll
  extent; embedding children and ownership remain the caller's responsibility.
- `MeterSource`, `MeterFrame`, `GraphSource`: separate GUI snapshot extension
  contracts only. Provider owns a safe snapshot and its lifetime. No RT-safety or
  mailbox implementation is implied; no fake numerical DSP display is shipped.

## Values and gestures

`ParameterSpec` owns stable id/name, physical range/default/step, units, mapping
and 0..6 displayed decimals. Construction rejects non-finite/invalid ranges and
mappings; range must contain an integral number of steps, and log minima must be
positive. Defaults are snapped to the legal grid. Consumers must choose displayed
precision sufficient to represent their step. Log uses exp/log across the entire
range; bipolar maps zero exactly to its centre, including asymmetric ranges.

`parse` accepts a number with an optional matching unit, whitespace around the
number/unit and one decimal dot or comma. Wrong units, trailing garbage, NaN/Inf
and out-of-range values are rejected. It does not guess kHz, thousands separators
or locale-dependent grouping. A valid value is snapped to the step. Invalid text
keeps the last good value, shows a border plus `!` and an accessible explanation.
Enter commits; Escape restores text; focus loss commits valid changed text and
keeps invalid text visibly erroneous. Clearing an error clears its tooltip too.

Pointer drag is relative: 200 logical px spans the normalized range, Ctrl gives
one tenth travel. Knobs/vertical faders use vertical motion; horizontal faders use
horizontal motion. Arrows use ten steps, Ctrl+arrow one; Home/End set bounds.
Double-click restores the declared default. Wheel uses native JUCE normalized
wheel policy and completes a discrete gesture per delivered wheel event. Wheel
is ignored during a pointer drag. Sliders and fields participate in native Tab /
Shift+Tab focus; buttons, toggles and selectors retain native keyboard behavior.

`onEvent(ControlEvent)` is a synchronous **message-thread-only** event sink:

1. `begin` once with the origin.
2. `preview` for each changed legal value.
3. `commit` once if the resulting value differs from the origin; otherwise `cancel`.
4. Escape, pointer focus loss, disable and destruction cancel an active drag,
   restore its origin and emit one `cancel`. Further pointer motion is ignored
   until mouse-up. Numeric entry, keyboard, wheel and reset use the same event path.

`setValue` synchronizes visual state without emitting a user event. Before an
external authoritative refresh during an active gesture, call `cancelGesture`;
then apply the refresh. `beginGesture/change/endGesture/cancelGesture` allow a
future adapter to use the same transaction boundary. All callbacks are UI-only;
no processor pointers or project mutations belong in paint. An event sink must
outlive its control or be detached before teardown: destruction can emit cancel.
#194 must implement host identity/generation, Undo/Redo, automation, external
refresh arbitration and safe parameter mailbox integration. A UI commit is not
proof of a durable host command or undo transaction.

## Standalone showcase and checks

`MR UI Controls 0.2` / `MRControlsPreview --smoke`: scratch values only, no audio
initialization, no project load/save or settings writes. The interactive preview
has UDS/reference palettes, 100/125/150/200 logical zoom, actual panel scrolling,
reset, all size/orientation variants, selected/disabled/error examples, native
buttons/selectors and live begin/preview/commit/cancel counters. Reset resets demo
parameter/value fields and counters; it is not a plugin preset or a global theme
operation. The preceding MR UI Preview 0.1 remains a separate token editor.

CTest `juce_mr_controls` exercises actual control handlers for unit validation,
true log/inverse and bipolar mapping, relative/fine drag, cancel/focus/disable,
reset, numeric entry, wheel transaction, lifetime cancellation, segment keyboard,
Unicode selection, native peer focus/Tab traversal, scrolling and software scale
snapshots. Screenshots/logs remain in `build/asio-local/validation/ui192/`.

Physical mixed-DPI monitor transitions, screen-reader operation and human keyboard
feel are NOT TESTED by these programmatic handlers/snapshots. No ASIO measurement
is claimed for this GUI-only target. User design acceptance remains required.

