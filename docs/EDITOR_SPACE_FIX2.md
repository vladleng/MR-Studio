# 0.2g fix2 — audio effect and parameter editors

User accepted fix1: «Это заработало», and requested the same behaviour for audio
plugin editors. Native VST3 audio effects already use PluginPanel's bridge, which
does not distinguish instruments/effects. FxPanel fallback/native effect windows
and the explicit VST3 Parameters window previously had no bridge; fix2 attaches
the same scoped handler to these windows as well. Closing/retiring them removes
registration and increments an attachment generation to cancel queued commands.

Space invokes the existing shared main Play/Stop policy, with repeat/modifier/
modal/text guards unchanged. No second transport or duplicate native registration.
UI/lifetime/actions/tests/docs affected; DSP/RT/model/project media/persistence/
Undo not affected. Schema 14/preferences v7 unchanged; no transient keys persisted.
Studio UI-only change; Live UI deferred. No layout/theme changes.

Regression uses an actual JUCE audio-effect parameter window: posted Space starts
once despite repeat, next Space stops, text entry keeps Space, retirement cancels
pending action. Existing native VST3 effect/instrument/Pin/editor lifecycle tests
remain. Opaque custom text widgets without OS caret and separate plugin UI threads
remain limitations of [fix1's bridge](EDITOR_SPACE_FIX1.md), not hardware PASS.
User clarified the failure was in the built-in equalizer; ordinary VST3 editors
already work. Fix2 targets the shared built-in FxPanel, not a VST3-specific fault.
2026-10-08: user accepted fix2: «Тест пройден». Built-in EQ accepted in the
reported workflow; this does not certify every custom text widget or plugin UI thread.
118/118 CTest PASS (78.83 s), three packaged J3+diagnostics smokes exit 0,
packaged crash tests PASS. No special TH-U/Nuro DSP tests.
Code/packages local, no merge/release/Actions requested for this fix.
