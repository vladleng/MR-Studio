# Native effect editor audit and unification

Date: 2026-10-15. Candidate: 0.4b upd3 Native Editors.
Status: ACCEPTED by user, 2026-10-15. Historical automated/manual validation limits below remain unchanged. Local source: ab66a2e34da2f8443df8bd5fef587b585ebffe22.

## Contract and scope

User requested an audit of all native effects against mr-plugin-development,
particularly double opening, and a common implementation of the editor rules.
Opening an insert, loading an MR preset and closing/reopening its editor are the
entry points. First show must use the final frame, owner and keyboard hook.
Repeated open raises the existing window. Compatible preset loads preserve the
window, content, native HWND, position and size and refresh parameters in place.
Audio parameters continue to belong to the project/preset and shared Undo model;
browser preferences and the chosen EQ band belong to local per-insert UI settings.
Vendor WAVs remain untouched. No new Cab roadmap stage or issue closure is implied.

| Layer | Impact |
|---|---|
| JUCE UI/window/notifications | Affected: common opening/preset path for every native effect |
| Local preferences | Affected: optional bounded nativeEditorViews, max 256 inserts |
| Model/commands/Undo | Existing implementation reused; no new audio state |
| Engine/RT | No DSP/callback changes; existing preset transaction and shutdown preserved |
| Project/preset serialization | Unchanged; old preferences without the optional field load |
| Studio/Live | Studio desktop shell only; shared DSP remains unchanged |
| Tests/docs | Affected: native window regression plus Cab/Reverb/VST3 neighbours |

## Inventory and findings

All eight native InsertKind values use FxPanel and EditorWindow in
apps/studio-juce/src/Editors.cpp. VST3 is a separate processor-owned editor path.

| Effect | Processor | Controls and findings |
|---|---|---|
| Gain | mrs.gain | Whole signal multiplier, UI in dB; immediate gain updates, no DSP smoothing added |
| High pass | mrs.highpass | Frequency/Q; unused generic Gain is disabled and no longer committed |
| Low pass | mrs.lowpass | Frequency/Q; unused generic Gain is disabled and no longer committed |
| EQ (single band) | mrs.eq | Peak-band gain in dB, frequency/Q; not an output gain |
| Channel EQ | mrs.channel-eq | Five bands, enabled/frequency/Q/band gain; smoothed DSP; selected band was lost on close |
| Legacy Cab IR | mrs.cab-ir | Existing embedded IR path inspected; no new legacy compatibility work |
| MR Reverb | mr_reverb | Wet pre-delay/tone/Mix, input gate/threshold; Options/search/folder were lost on close |
| MR Cab | mrs.cab-v2 | IR -> wet cuts -> wet gain/polarity -> Mix; previous Cab fixes retained |

Sources: core/processing/src/model.cpp (insert_graph), native.cpp, reverb.cpp,
cab_ir.cpp, apps/studio-desktop/src/application.cpp (load_insert_preset,
set_inserts, runtime owner), JUCE Editors.cpp, ReverbBrowser and CabBrowser.
These are local code findings, not a claim of a new exhaustive DSP/ASIO safety audit.

The opening defect was shared: non-Cab FxPanel windows were shown in the constructor
before theme/title/resizable flags/owner/hooks were final. Reverb then changed frame
flags on a visible window, potentially recreating the peer and invalidating its hook.
Only Cab had a preset-preserving exception; every other native effect was explicitly
closed and reopened on preset load. Both exceptions are replaced by the common native
host-only path. External VST3 editors still retire before graph replacement, because
their controller lifetime differs from a model-backed native FxPanel.

The all-native regression also reproduced a real close/remove lifetime crash:
a queued Direct2D paint reached an old FxPanel after its insert had been removed.
FxPanel::current threw through Windows dispatch, leading to terminate/abort
(0xc0000409). A local debugger stack identified Editors.cpp paint/index, not DSP.
Retired panels now stop querying the model; paint tolerates a disappearing insert,
and closing cancels any unfinished preview before retiring hooks/browser state.
The native test deliberately replaces inserts between editor lifetimes and pumps
messages, covering this path as well as preset changes after timer delivery.

Optional nativeEditorViews stores EQ band, Reverb Options, search and relative folder
by stable insert ID; bounded values and old-file compatibility are maintained.
Search is debounced by 300ms with immediate cache updates and a close flush; folders
restore after async indexing without silently replacing a pending selection.
Cab retains its separate richer browser view from upd1. The preset file chooser now
checks project-store lifetime before applying an asynchronous result.

The existing preset decoder's bypass policy is preserved: MR Cab/MR Reverb restore
their saved bypass; older native effects retain the current slot bypass. Tests use
actual parameter changes and assert that existing policy explicitly. Unifying this
separate sound/preset contract was not needed to fix window lifetime.

Reverb also allowed the clicked row to disagree with the active IR after a rejected
load. Refresh now derives selection from active key/path every time, including when
a replacement loader reuses revision numbers. Filtered-out/missing active sources
are not represented by an unrelated highlighted row. Requested errors remain visible.

## Validation and delivery

Release JUCE/ASIO configure/build PASS. Targeted CTest **7/7 PASS, 47.28s**:
juce_native_editors, juce_cab_editor, juce_reverb_editor, juce_reverb_browser,
juce_j1_smoke, juce_j2_smoke and juce_j3_smoke. No full CTest for this UI fix.
The all-native test covers all eight kinds, actual alternating parameters, unchanged
HWND/content/bounds and zero hide/show across presets after timers, repeat open,
owned z-order without global topmost, close/reopen, EQ band settings-file roundtrip,
independent insert state and old preferences. Reverb covers repeated close/reopen
with search/folder/Options, controls/Undo and search Space; existing Cab source,
browser persistence and external VST3 lifetime/keyboard regressions remain green.
Reverb layout snapshot inspected at 100%; existing smoke also exports 150/200%.
The earlier failing all-native run exposed the queued-paint crash described above;
the final candidate and packaged native smoke both pass after that correction.

Local package: MR-Studio-0.4b-upd3-Native-Editors-JUCE-ASIO-Windows-local under the
MR Studio project Builds folder. 13 files / 12 SHA256 entries verified, packaged
native-editor smoke exit0. GUI SHA256:
3E0DA57C3CE74FFF4656B8FD8F381705883F9D14A6E01DD50D338D7442C068BD.
Existing desktop shortcut updated with its icon retained; previous upd2 package
hashes verified unchanged. Symbols: build/asio-local/deliveries/0.4b-upd3-native-editors.
Logs: build/asio-local/validation/native-editors-upd3 and native-editors-build.log.
The project mr-plugin-development skill also records the queued-paint lesson and
passes quick_validate.

Manual checklist supplied before acceptance: restart via the shortcut, open native effects, switch
presets, close/reopen, click Studio behind an editor and assess initial flashing.
Physical perception of flashing, mixed-DPI and actual ASIO/listening are NOT RUN;
automated HWND assertions are not those checks. No new exhaustive RT/DSP claim.
Code/build/package stay local; GitHub publication is documentation/issues only, no code push
or Actions. User acceptance closes #196 only; no transition to another Cab/Rig stage.
