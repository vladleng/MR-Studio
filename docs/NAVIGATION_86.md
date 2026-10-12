# Musical navigation — #86 / 0.3d

Accepted 2026-10-12: «Билд проверку прошел». #86 closed completed. Existing
0.3d package/manifest unchanged for acceptance. Cosmetic follow-up #148/upd1
does not reopen navigation scope.

2026-10-12. User explicitly resumed #25 with #86 after accepted 0.3c upd14.
Source baseline `56d36c4`; active branch unchanged. Other musical editors remain
out of scope. Project schema19/archive1 unchanged by navigation.

UI entry points: footer Sections/Markers selector changes previous/next arrows;
Transport menu has previous/next markers and Go to section/marker lists. Click
Section/Marker current→next captions to open the same ID-based list. Section band
click seeks its start without Snap influencing target selection. Marker list
selection seeks the chosen ID; marker context has explicit Go to selected marker.
Marker editing/double-click and coincident selection remain available.

Shared MusicalTimeline handles previous/next/ID/loop. Application supplies a
non-RT navigation guard to that shared service, so direct consumers cannot bypass
recording/count-in protection. UI disables navigation during recording and rejects
active edit gestures. Popup callbacks retain IDs and store identity, revalidate on
use and refuse changed/deleted targets without seek. No name-based identification.

Previous inside a section returns its start; at a start it chooses the previous
strict start. Gaps use nearest strict preceding/following anchor. No wrap. Marker
ties share a sample location; next/previous skip same-tick duplicates. Selecting
either coincident ID reaches that position. Queue full/preparation failure throws
an explicit result; displayed context/playhead remain engine-confirmed. Seek does
not change play/pause or run LiveAction metadata. Existing loop-section is explicit,
uses current timing map and stays runtime transport state.

UI mode/selection are transient, not project data or Undo operations. Seek itself
does not dirty/serialize the project. Model/persistence/Undo algorithms and schema
are unaffected; edit/Undo/Open retain IDs used by fresh navigation lists.
Callback/DSP algorithms are unaffected. Existing prime_streams + prepared_seek
queue is reused; all guards/list building occur on the control/UI thread. Engine
state remains the source of confirmed context, not optimistic navigation state.

Release/CTest126/126 PASS (119.47s): edge/gap/tie/stale/guard/queue failure tests,
playing/paused/count-in GUI scenarios. Packaged smoke and fresh
software100/150 previews. Only fixtures; no user settings/projects changed.
Separate compact 0.3d package and desktop shortcut update after validation.
No push/merge/Actions; #86 stays open until user acceptance.

Delivered: source `8e833de06d6d330d34f6d192996b30577fd986d9`, Release/full
CTest126/126 PASS (119.47s), packaged J3 fixture smoke PASS (exit0). Fresh
software100/150 navigation/footer previews inspected in
`build/asio-local/validation/navigation86/packaged-smoke`; queue-full preview
deliberately shows the tested error, with confirmed current context unchanged.
Compact13 files / 12 payload hashes verified:
`Builds/MR-Studio-0.3d-Navigation-JUCE-ASIO-Windows-local`.
GUI SHA256 `0DA1A7C6BD262AC99AA1E985964D2BAC418CC37CE5E5F389991C3972C0AA7A2F`.
Symbols `build/asio-local/deliveries/0.3d-navigation`. Persistent desktop shortcut
updated to0.3d, retained icon/verified target; accepted upd14 GUI hash unchanged.
READY WITH MANUAL CHECK; physical ASIO/mixed-DPI not separately measured.
