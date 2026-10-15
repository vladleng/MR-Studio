# MR Cab window lifecycle — 0.4b upd2 / #196

Date: 2026-10-15. User noticed an apparent double opening and a reopen on preset
changes, and asked where Gain sits. Status: READY WITH MANUAL CHECK; acceptance pending.
Local source: `1c2ca1807f75e39f466e173e94b29bd11e4082ef`.

First-open cause: Cab's DocumentWindow was shown before title/resizable flags/limits
were finalised. Changing native frame flags can recreate a visible Windows peer.
Cab now constructs hidden, configures its frame, applies its native owner, attaches
the Space bridge to the final peer, then shows once. Other plugin types retain
their existing opening behaviour.

Preset cause: native-host preset loading explicitly closed all editors and reopened
the target. Cab now retains its existing host UI while the unchanged backend preset
transaction runs, synchronises controls in place and acknowledges the new insert
generation before the next Desktop timer. External plugin editors still detach and
their native parents retire before a possible graph replacement; the retained Cab
browser obtains the current loader from Application, not from an old graph lease.
When no Cab window exists, preset loading still opens one normally.

The browser also compares actual loaded relative path/digest when refreshing its
active row. A new loader may reuse active_revision1; that must not suppress a source
change after a preset load. Cached browser filters from upd1 remain intact.

Gain is unchanged DSP: IR convolution → wet Low/High cut → wet Gain/Polarity → Mix.
Dry bypasses this Gain. At100% Mix this is Cab output gain. A tooltip now explains it.
No input gain/drive or DSP algorithm changes were introduced.

## Validation and package

Release JUCE GUI build PASS. Targeted CTest **5/5 PASS6.05s**: Cab editor,
Reverb editor/browser, J1 and synthetic VST3 J2 smoke. No full CTest for this UI follow-up;
base0.4b149/149 remains historical, not a new upd2 result.

Cab regression loads real synthetic MR preset files with different source-rate WAV
identities and Mix values, then switches back. It checks the same window/content,
native HWND, bounds and zero Component visibility transitions, plus updated controls
and correct active IR row. Existing owner/z-order and upd1 browser persistence tests pass.
Physical perception of startup flashing and hardware/listening remain user checks.

Package: `C:/Users/Vladislav/Documents/ChatGPT Projects/MR Studio/Builds/MR-Studio-0.4b-upd2-Cab-Window-JUCE-ASIO-Windows-local`.
13files/12SHA256 entries verified; packaged Cab smoke **exit0**.
GUI SHA256 `22E968638CF68DFAEEA6F2AD1A8296272E8B10CF49F0FA0682C56B466024C786`.
MR Studio shortcut→upd2, icon retained; upd1 preserved and hashes rechecked.
Symbols `build/asio-local/deliveries/0.4b-upd2-cab-window`;
logs `build/asio-local/validation/cab-window-upd2`.
Vendor WAV and user projects unchanged; synthetic isolated fixtures only.
Code/build/package local, GitHub documentation/issues only. #196 awaits acceptance.
