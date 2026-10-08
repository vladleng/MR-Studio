# 0.2g fix1 — #91: Space in native VST3 editors

Contract: unmodified Space while a hosted VST3 editor is focused invokes the
same existing main-window Space action. No new transport semantics: the current
main-window policy is Play / Stop, including its recording behaviour.
Native HWND descendants bypass JUCE component key routing; a WH_GETMESSAGE hook
on the UI thread handles only registered visible plugin editor roots/children.
Removed messages only; repeated keydown is consumed without a second action.
Transport runs via MessageManager::callAsync, not inside plugin dispatch.
SafePointer + attachment generation reject queued actions after close/retire/
reattach. Final editor retirement removes the thread hook. No global hotkey,
subclassing plugin HWND, polling keyboard or capture from other applications.

Text protection: JUCE text/combo focus, native Edit/RichEdit/ComboBox ancestors,
and native OS caret ownership keep their Space input. Modified
Space and modal contexts are untouched. Custom opaque text widgets without native
or OS caret cannot be recognized reliably; physical plugin-editor
testing is required, including Omnisphere text/preset search fields. Plugin UI
threads separate from the host message thread are not covered by this bridge.

Layers: UI/keyboard routing, action entry point, editor lifetime, tests/docs AFFECTED.
Model, engine/DSP/RT callback, routing, serialization, project media and preferences
NOT AFFECTED. Transport uses existing Application/actions. No Undo/persistence for
transient key state; schema 14 and preferences v7 unchanged. Studio UI-only bridge,
shared transport unchanged; Live-specific UI DEFERRED. Styling/geometry unchanged.

Regression: posted native keydown/up with real fixture child focus, repeat guard,
native text input, modifiers, modal and unrelated-window guards; existing editor lifecycle/Pin/preset/DPI and main
Space tests retained. Local Release/CTest and packaged GUI smoke required.
No installed TH-U/Nuro special checks, no GitHub code publication or Actions.
2026-10-08: user accepted fix1 («Это заработало») and confirmed ordinary VST3
editors work. Built-in EQ follow-up is accepted in fix2. The opaque text/separate
UI-thread limitations above remain; acceptance does not broaden synthetic evidence.
