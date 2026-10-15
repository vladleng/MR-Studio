# MR Cab browser state — 0.4b upd1 / #196

Date: 2026-10-15. User reported that closing/reopening the plugin retained the IR
but reset Full Combo/model/variant menus and filters; clarified that audio controls
were not affected. Status: **READY WITH MANUAL CHECK**, user acceptance pending.
Source: `eb6d735290b502d9f0440a2bfbe78a2fddecb68d` (local only).

The browser was reconstructed with All IR, empty facets/search and disabled toggles
on every editor opening. These values existed only inside the component.

The fix stores role, six selected facet values, search, Favorites only and Unavailable
as optional `cabBrowserViews` in existing ViewSettings JSON. Keys are insert IDs,
so different instances have independent views. This is a local UI preference,
not project/audio state, and does not create project Undo entries or change presets.
Up to256 cached views are retained; decoded strings/counts are bounded.
Older preferences without the new optional field continue loading.

Values are restored by text after the asynchronous catalog is available, rather
than by temporary menu item numbers. Explicit500ms and Any Length remain selected
instead of being overwritten by the200ms initial default. Pending choices survive
closing while the first scan is not ready. User changes update the cache immediately;
disk publication uses the existing settings writer after300ms of inactivity,
with a final flush on editor retirement/close. No WAV reads/writes or scans are added.

Code changes: Settings.h (bounded view metadata), CabBrowser.h (restore/cache/flush),
Editors.cpp (regression smoke), version.hpp (0.4b upd1 Cab Browser State).
DSP, loader, project model/schema and selected IR identity are unchanged.

## Verification and delivery

- Release JUCE GUI build PASS using existing asio-local target/toolchain.
- Targeted CTest **4/4 PASS4.39s**: Cab editor, Reverb editor/browser, J1/settings.
- Regression invokes the close-button handler, reopens the editor, checks exact role,
  model/capture/length/search/toggles, repeats close/open four times, round-trips
  preferences through a temporary file, handles older preferences, delays the scan
  in a fresh Application and checks a separate instance does not inherit the view.
- 500ms and explicit Any Length restore; IR/controls/project revision/insert generation
  remain unchanged. All fixtures are synthetic, with isolated temporary settings.
- Packaged Cab smoke **exit0**;13files/12SHA256 entries verified.
- No full CTest for this UI-only correction. Base0.4b149/149 belongs to the previous
  candidate; it is not presented as a new upd1 run. No new hardware/listening claims.

Package: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.4b-upd1-Cab-Browser-State-JUCE-ASIO-Windows-local`.
GUI SHA256: `6B42193E08117682EA096331D068ACA32633790DDED4910F2FAF95C35F484C8C`.
Existing MR Studio shortcut points to upd1; icon retained. Base0.4b remains intact,
with its manifest hashes checked. Symbols: `build/asio-local/deliveries/0.4b-upd1-cab-browser`.
Logs: `build/asio-local/validation/cab-browser-upd1`.

Restart MR Studio from the shortcut, select the desired filters once, then close
and reopen MR Cab to confirm. Old unsaved filter choices cannot be recovered;
existing Full Combo mapping does not need to be applied again.
Vendor WAV, user projects and actual user preferences were not used as test fixtures.
Source/build/package remain local; GitHub receives documentation/issue updates only.
#196 remains open awaiting user acceptance; #185 remains accepted/closed.
