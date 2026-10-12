# Local development and build policy

Правило2026-10-12: GitHub — основной источник информации; документы/issues синхронизировать
после содержательной работы/приёмки, не только при релизах. Source/build/tests/packages
локально. Docs-only commit поверх актуальногоmain с [skip ci], без code push/merge/Actions.
Исторические local-only/only-at-release правила ниже заменены этой записью.


## Current 0.3f/#83 — 2026-10-12

Stopped tempo-map editor, Core staged transaction and Engine TimingPlan preserve
WAV samples/MIDI ticks/sample playhead/DSP instances. Callback splits at tempo/loop
boundaries; channel/instrument/Master host context agrees. Schema20/archive1 unchanged.
Release/full129 PASS146.00s including GUI6 and new desktop_tempo/VST3/MIDI/click tests.
Logs validation/tempo83 retain earlier compile/test-fixture/GUI geometry/callback-count
failures. Final tests verify frame budgets/exact PCM/no host RT allocations; no broad
rerun of installed TH-U/Nuro diagnostics. Screenshots100/150 inspected. Packaged diagnostics/J3 PASSexit0;
actual ASIO/physical DPI NOT RUN, #83 acceptance pending, #80 remains open.
Preserve accepted0.3e upd2 and older packages; persistent shortcut now0.3f/icon retained/target verified.
No push/merge/Actions. GitHub #83 final delivery update confirmed after initial transport failures; open pending acceptance.

## Current 0.3e upd2/#81 — 2026-10-12

User follow-up10 points: taller/translucent sections/font14/radius3/gap3;
body drag metadata only, inline double-click rename on sections/markers,
marker alpha0.72 and right-click anchored palette. Marker optionalRGB persists
with schema20 and migration1–19; archive1 unchanged. Full local rebuild for
changed Core layout before tests. Release/full128 PASS164.54s; final GUI6
PASS76.10s after grid/font correction. Logs validation/sections81-upd2; package next.
Delivered source `c871edc801133e204066c9edf0c73edfe3d44a17`, packaged diagnostics/J3 exit0.
Package `MR-Studio-0.3e-upd2-Sections-Markers-JUCE-ASIO-Windows-local`;13 files/12 hashes reverified.
GUI SHA256 `3F2C6A54C90ECA6EC8BD45A73436AC4BF8F4471AA72D860B3C74468ACD861BA2`.
Symbols deliveries/0.3e-upd2-sections-markers; fresh sections/markers/menu inspected.
MR shortcutupd2/icon retained/target verified, older packages unchanged.
#81 accepted/closed2026-10-12: «Отлично принимаю! Что дальше?»
Acceptance-only documentation update: no rebuild, package/manifest/shortcut unchanged.
Next recommended #80/0.3f not started; hardware ASIO/physical DPI not separately measured.
No push/merge/Actions.

Delivered source `eb2f30e53406d87402803b6b1ab0aaa353992df7`; package `MR-Studio-0.3f-Tempo-Map-JUCE-ASIO-Windows-local`.
GUI SHA256 `8591719AB45ABF2E1893AE9F04D139A57EA21E8ABB816FBAC353B7AC9A46FDA7`;13 files/12 hashes reverified.
Symbols deliveries/0.3f-tempo-map; accepted packages preserved.

## Previous 0.3e upd1/#81 — 2026-10-12

User corrections: right-click popup at cursor, palette squares at bottom,
editable name, Backspace delete, name-only caption and aligned border.
Source `4ce08e0825476ac101cc1dc5f1be38e492d474e1`, Release/focused2 PASS22.02s;
final full128 PASS128.38s including GUI6. First diagnostics/J3 Space focus
failure retained; isolated baseline/current and full re-run PASS without
product keyboard edits. Packaged diagnostics/J3 PASSexit0/clean shutdown.
Fresh previews inspected;13 files/12 hashes reverified. Package `MR-Studio-0.3e-upd1-Section-Menu-JUCE-ASIO-Windows-local`,
GUI SHA256 `6473418C246143FF8539D9DDF3C56C09B453FB26999EFAEF581657D8D95F1AF4`.
Logs validation/sections81-upd1, symbols deliveries/0.3e-upd1-section-menu.
Same MR shortcut updated/icon retained/target verified; older packages preserved.
No schema/callback/DSP changes, #81 acceptance pending, #80 remains open.
Hardware ASIO/physical DPI not separately measured. Local code/build/docs,
GitHub issues only; no push/merge/Actions.

## Previous 0.3e/#81 — 2026-10-12

Direct user request #81 ahead of #80. Sections palette/RGB/precise bounds/edge
preview + minimum create prerequisite; #80 stays open. Source baseline0073982,
Source61c3fbd, Release/full128 PASS140.94s, final GUI6 PASS72.26s,
packaged J3 PASSexit0. Compact13/12 hashes verified; fresh100/150/200 previews
inspected. Package MR-Studio-0.3e-Section-Colors-Bounds-JUCE-ASIO-Windows-local.
Logs validation/sections81, symbols deliveries/0.3e-sections81, MR shortcut0.3e
updated/icon retained. #81 acceptance pending; old packages unchanged.
Preserve accepted0.3d upd3/older packages; update same shortcut after delivery.

## Accepted current package — 2026-10-12

0.3d upd3/#149 accepted: «Да, все ок, принимаю»; issue closed completed.
Package/manifest/shortcut unchanged. Source1c0aba5, Release/full127 PASS146.99s,
packaged J3 PASS and real MIDI7605/1989 roundtrip/ManualDevice PASS retained.
Documentation-only acceptance: no rebuild. Next recommended #80/0.3e not started.

## Follow-up 0.3d upd3 — 2026-10-12

upd2/#149 failed user acceptance: long mono SMF rejected and actual VSTi blank
drop no-op. Fix the per-clip/playback/deserialize4096 budget to use unchanged
project32768 total; interest payload only, validate coordinates on drop (JUCE
findTarget supplies target-local cursor afterwards). Release and real user
MIDI7605/1989 import/SaveOpen/ManualDevice PASS; full127/package next.
Delivered source1c0aba5, Release/full127/127 PASS146.99s and packaged J3 PASS.
Compact13/12 hashes rechecked; current MR shortcut upd3/icon retained.
Package MR-Studio-0.3d-upd3-MIDI-Drop-Fix-JUCE-ASIO-Windows-local.
Logs validation/midi-drop149-fix, symbols deliveries/0.3d-upd3-midi-drop-fix;
old packages and original user MIDI unchanged. #149 acceptance pending.

## Current work — 2026-10-12

0.3d upd1/#148 accepted: «Хорошо, пока оставим так. Принимаю»; issue closed.
Current work 0.3d upd2/#149: VSTi/MIDI drag/drop. Validate local Release,
MIDI import, hosting/cache and GUI; create fresh compact package and update
persistent shortcut after checks. Preserve upd1 and all older packages.
Delivered source f455466; full127/127 PASS150.96s, final GUI6/6 PASS41.49s,
packaged J3 exit0 and fresh100/150 MIDI shadow previews inspected. Compact13/12
hashes rechecked, shortcut updated/icon retained. #149 user acceptance pending.
Package MR-Studio-0.3d-upd2-MIDI-Instrument-Drop-JUCE-ASIO-Windows-local;
logs validation/midi-drop149, symbols deliveries/0.3d-upd2-midi-instrument-drop.

## User-facing packages — 2026-10-10

Active **0.3d upd1/#148**, menu/browser font14/width1.04 and Home card fill.
Release/GUI6/6 PASS; packaged checks next, accepted 0.3d/#86 retained. Update desktop shortcut
only after validation; schema19/DSP/Undo unchanged.
Delivered source `36d7794`, packaged J3 PASS, fresh100/150 previews inspected;
compact13 files/12 hashes verified:
`Builds/MR-Studio-0.3d-upd1-Browser-Cards-JUCE-ASIO-Windows-local`.
Symbols `build/asio-local/deliveries/0.3d-upd1-browser-font-cards`, desktop
shortcut updated. Visual #148 acceptance pending; physical mixed-DPI not measured.

Active **0.3d / #86**, explicit user request 2026-10-12. Shared navigation UI
and non-RT recording/count guard; schema19/archive1 unchanged. Build/validation
Release/CTest126/126 PASS, packaged checks next; logs `validation/navigation86`,
preserve accepted upd14/older packages.
Delivered source `8e833de`; packaged J3 PASS, fresh100/150 previews inspected.
Compact13 files / 12 hashes verified: `Builds/MR-Studio-0.3d-Navigation-JUCE-ASIO-Windows-local`.
Symbols `build/asio-local/deliveries/0.3d-navigation`, desktop shortcut updated.
#86 user acceptance pending; physical ASIO/mixed-DPI not separately measured.

Active **0.3c upd14**, #147 open. Close project/channel menus, FX role schema19.
Release/CTest126/126 PASS; packaged checks next, logs `validation/upd14` in build.
Preserve older packages and update persistent desktop shortcut on delivery.
Delivered source `d46be8e`, Release/CTest126/126 and packaged J3 PASS;
100/150 previews inspected, compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd14-Channel-Menus-JUCE-ASIO-Windows-local`.
Desktop shortcut updated to upd14; icon retained. User acceptance #147 pending.

Current: **0.3c upd13**, #146 open, button text14/compact13 logical, width+4%; mixer buttons unchanged12/scale1.
Source `04f97e6`; Release/GUI6/6 and packaged J3 PASS; home100/150 previews
inspected. Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd13-Button-Font-JUCE-ASIO-Windows-local`.
Logs `build/asio-local/validation/button-font`, symbols
`build/asio-local/deliveries/0.3c-upd13-button-font`. Visual acceptance pending;
physical mixed-DPI not measured. Preserve upd12 and older packages.

Current work: **0.3c upd12**, #146 open, popup menu font sizing.
Theme/preset menu font matches VST3 Segoe UI12; row minimum22px avoids V4
automatic glyph shrink. Source `5b11598`; Release/GUI CTest6/6 and packaged J3
PASS. Fresh default/preset menu software100/150 previews inspected.
Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd12-Menu-Font-JUCE-ASIO-Windows-local`.
Logs/previews `build/asio-local/validation/menu-font`, symbols
`build/asio-local/deliveries/0.3c-upd12-menu-font`. User visual acceptance
pending; preserve older packages. Physical mixed-DPI not measured.
Upd11/#145 accepted on 2026-10-11: «Это принимаю», issue closed completed.

Current follow-up: **0.3c upd11**, #145 open, dark nested IR browsers.
Only TreeView/ListBox fills #212830 inside retained #2D353E IR card.
Source `8f5e56e`; Release/Reverb GUI CTest2/2 and packaged browser/editor PASS;
software100/150 previews inspected. Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd11-IR-Browser-JUCE-ASIO-Windows-local`.
Logs `build/asio-local/validation/ir-browser-surfaces`, symbols
`build/asio-local/deliveries/0.3c-upd11-ir-browser`. Visual acceptance pending;
physical mixed-DPI not measured. Preserve older packages.

Current follow-up: **0.3c upd10**, #145 open, reference theme contrast.
Source `b042800`; Release/GUI CTest6/6 PASS; packaged J3/Reverb browser PASS.
Fresh software100/150 previews inspected. Compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd10-Theme-Contrast-JUCE-ASIO-Windows-local`.
Logs/previews `build/asio-local/validation/theme-contrast`, symbols
`build/asio-local/deliveries/0.3c-upd10-theme-contrast`. User visual acceptance
pending. Preserve upd9 and all older packages.
Only neutral fills and Arrange/IR browser canvas; DSP/schema18 unchanged.

Current visual work: **0.3c upd9**, #145 open, reference theme. Release/GUI
CTest6/6 PASS; packaged J3 PASS, software100/150 previews inspected.
Source `fd07877`; compact13 files / 12 hashes verified:
`Builds/MR-Studio-0.3c-upd9-Reference-Theme-JUCE-ASIO-Windows-local`.
Logs/previews `build/asio-local/validation/reference-theme`, symbols
`build/asio-local/deliveries/0.3c-upd9-reference-theme`. User visual acceptance
pending. [UI_REFERENCE_UPD9.md](UI_REFERENCE_UPD9.md).
Only UI/fonts/paint; schema18 and DSP unchanged. Preserve all older packages.

Accepted on 2026-10-11: **0.3c upd8**, #137 closed completed,
insert loading and Send power bypass. Source `0866533`; Release/CTest126/126 and
packaged J3 fixture smoke PASS, software100/150 previews inspected. Compact13 files:
`Builds/MR-Studio-0.3c-upd8-Inserts-Send-Bypass-JUCE-ASIO-Windows-local`.
Logs/previews `build/asio-local/validation/inserts-send-bypass`, symbols
`build/asio-local/deliveries/0.3c-upd8-inserts-send-bypass`. Physical ASIO/plugin
load timings and physical mixed-DPI were not separately measured. User accepted:
«Тесты прошли приемку». Existing package/manifest unchanged.
Project schema18 stores Send bypass; old sends default enabled. Details:
[INSERT_LOADING_SEND_BYPASS_UPD8.md](INSERT_LOADING_SEND_BYPASS_UPD8.md).
The new request resumes only this scope, not #25/Stage5.

Current accepted UI additions: **0.3c upd7**, #136 closed completed (context menu,
browser, insert/send divider), source `4185cc5`. Release/CTest126/126 and packaged
J3 fixture smoke PASS. Compact13-file package:
`Builds/MR-Studio-0.3c-upd7-Mixer-UI-JUCE-ASIO-Windows-local`.
Logs/previews: repository `build/asio-local/validation/mixer-ui`; symbols
`build/asio-local/deliveries/0.3c-upd7-mixer-ui`. User accepted on 2026-10-11:
«Принимай, с обновлениями пока все». Updates stopped until a new user request;
do not start #25/Stage5 automatically. Package/manifest unchanged, no rebuild
for acceptance. Preserve accepted upd6 and older packages.

User accepted general fixes: **0.3c upd6**, #135 closed completed (I/R,
mixer inserts), source `1e82e99`. Release/full CTest126/126 and packaged smoke
PASS. Compact13-file delivery: `Builds/MR-Studio-0.3c-upd6-Mixer-IR-Inserts-JUCE-ASIO-Windows-local`.
Logs/previews: repository `build/asio-local/validation/mixer-ir`; matching symbols:
`build/asio-local/deliveries/0.3c-upd6-mixer-ir`. Actual ASIO/input/record audition
and physical mixed-DPI were not separately measured; user confirmed
«Все работает, закрывай» on 2026-10-11. Existing package/manifest unchanged.
User accepted **0.3c upd5**, #134 closed completed, source `9d1a8ee`.
Preserve accepted upd5/upd4 and prior packages.

Current accepted Input gate / Mix addition uses **0.3c upd4**,
source `f97fba5`. Compact13-file package; preserve all prior deliveries. Details
and compatibility: [REVERB_GATE_MIX.md](REVERB_GATE_MIX.md).
User accepted it on 2026-10-11: #132/#128 closed completed. Acceptance does
not rewrite the existing package/manifest or trigger a new build/publication.

Active Reverb CPU follow-up in #132 uses **0.3c upd3**, source `3c6903c`.
Keep upd1/upd2 packages intact; local validation and symbols remain separate.

The user-authorized Reverb CPU correction uses **0.3c upd2**; this is a fix
within the same addition, not advancement to #132 or the next version letter.
The original 0.3c upd1 package remains intact.

After the user's feedback about excessive pictures in the build folder, distribute
a compact runtime package: application, required runtime helpers, licenses,
short README and SHA256 manifest. Embedded JUCE resources do not need loose copies.
GUI previews, smoke fixtures, test executables/logs, diagnostics smoke output,
source/context docs and matching debug symbols stay in the local build/validation
area, not in the user-facing package. Preserve matching symbols by delivery version.
Run packaged smoke with a separate working directory to keep its generated output
out of the package. Keep prior deliveries intact.

The user assigned the first Chord Track candidate the version **0.3a**.
On 2026-10-10 the user clarified: **each new sub-issue advances the version letter**.
#79 = 0.3a (its refinements used upd1); #82 = **0.3b**, although the accepted
package was mistakenly labelled 0.3a upd2. Keep that existing package and hashes
intact as historical evidence. Next #85 = **0.3c**; fixes within the same sub-issue
may use upd suffixes. Record acceptance separately from the binary version string.

The earlier **0.2g upd11** label for #79 was a packaging/version mistake, not a
separately accepted update. Do not infer future version numbers from roadmap labels.

On 2026-10-10 the user paused #25 progression for MR Reverb #128 and explicitly
assigned **0.3c upd1**. This overrides automatic version-letter progression for this
addition. The first checkpoint is DSP #129; a complete user-facing Reverb build
requires the browser/state/editor work in #130/#131 and subsequent #132 validation.

User instructions, recorded 2026-10-05:
- Development, source commits, configure/build/tests and packages stay local on Windows.
- Updated 2026-10-08: maintain documentation locally during development; synchronize
  GitHub documentation only at explicitly requested releases. Do not publish docs
  after every fix or acceptance. GitHub issues remain the current work plan and
  ongoing record of implementation, verification, limitations and user acceptance.
  Push source or merge only on the user's explicit request. No GitHub Actions.
- No mobile work in the active JUCE migration.
- Nuro Audio project-state restore is user accepted. Do not run special installed
  Nuro compatibility/state checks on every build. Repeat only when the user requests
  it or reports a new Nuro issue. Keep ordinary synthetic VST3 regression tests.
- TH-U project-state/sound restore is user accepted on 2026-10-05. Do not run
  special installed TH-U editor/state/DSP comparisons on routine builds. Repeat
  only on an explicit request or a newly reported TH-U-specific issue. Keep ordinary
  synthetic plugin regression coverage; new generic preset workflow tests use fixtures.


2026-10-11: **0.3c upd13 принят** — «Принимаю,тест пройден».
#146 closed completed. Пакет, SHA256 manifest и исходники не изменены при приёмке;
без пересборки/push/merge/Actions. Дальнейшие изменения только по новому запросу.


## Persistent desktop shortcut — 2026-10-12

User requested a shortcut with an MR Studio image. `Desktop/MR Studio.lnk`
points to accepted upd13; working directory is the package, no arguments.
Stable icon in chat workspace `Launchers/MR Studio.ico` (seven resolutions).
Update the same shortcut target when delivering subsequent local packages;
preserve older version folders. Do not rebuild or modify packages for a shortcut.


2026-10-12: **0.3c upd14 принят** — «Отлично, принимаю». #147 closed completed.
Existing package/manifest/shortcut unchanged for acceptance; no rebuild/push/merge/Actions.
