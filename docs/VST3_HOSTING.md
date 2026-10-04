# Windows VST3 effect hosting — 0.1l / Stage 3b

Local source `95c719fcd0893652f92a93beb9b20cb538cd84b9`. User acceptance pending. Code/builds remain local;
GitHub issues/docs only. Native inserts and hardware routing use the same shared
processing contract; no separate Live engine is introduced.

## Workflow
In Mixer open a track, bus or Master Inserts. Scan VST3… selects a folder (for example
`C:\Program Files\Common Files\VST3`) or a directory-style `.vst3` bundle.
Scanning runs asynchronously; each module is loaded in a separate hidden
`mrs_vst3_scan.exe` child with a 10-second timeout. Scan output is the module's audio
effect class metadata, not an instrument/MIDI implementation. Cached candidates
appear by plugin name/vendor in Add. The cache and scan log are in
`%LOCALAPPDATA%\MoonRiverStudio`; changed/deleted modules are reconciled on rescan.
Failed/crashed/timed-out modules retain a fingerprint and are skipped until their
files change or the cache/index are removed. Copy the helper beside the main EXE.

Add/remove/reorder/unload and VST3 slot bypass require Pause/Stop. A newly added
VST3 is prepared before its project reference is committed; missing/unsupported
effects produce a specific error. The first audio bus uses the host's mono/stereo
layout; effects must support it. Additional buses are inactive, MIDI/instruments,
sidechain assignment and surround workflows belong to later work. Native windows
attach through IPlugView/IPlugFrame; plugins without a native editor can use the
generic parameter selector and normalized 0–1 field. Hidden parameters are excluded
from the selector; read-only/nonautomatable parameters cannot be edited there.
Plugin-window typing/shortcuts are dispatched to the plugin window.

Live generic parameter changes and editor performEdit callbacks feed bounded
preallocated queues. Track/bus chains run in blocks of up to 64 frames, without
adding buffering latency; older prepared one-frame test graphs remain supported.
Input is deinterleaved into SDK bus buffers and output reinterleaved. Tempo,
quarter-note position, sample position and playing state are supplied to processors.
No audio-thread SDK state capture, DLL load/unload, UI calls or host allocations.
Third-party processors can have their own allocation/CPU behavior.

Opaque component/controller state and parameter overrides are saved after
Pause/Stop with callbacks quiescent. Existing plugin state is retained when adding
or reordering adjacent inserts. Project v9 reads v1–v8. Missing plugins remain
represented in saved projects; open/edit offline, remove/recover the effect and
reconnect the device. Plugin/editor changes mark the project dirty; native/generic
host parameters use Undo. Opaque plugin-internal editor gestures use the plugin's
own undo behavior rather than automatic host command history.

## Crash / isolation strategy (3b boundary)
Scanning is out of process and timeout-bounded, with cached rejection. Audio and
editors run in process in this slice. A failed process return or caught Windows SEH
process fault disables that instance and passes its dry input; the insert status
reports the fault. This is best-effort containment, not process isolation or a
guarantee against memory corruption, hanging process calls or faults during other
SDK lifecycle calls. Full runtime isolation/recovery/load hardening belongs to 3e.
Dynamic bus/reload/latency restart requests are currently rejected; Pause/Stop and
reload the effect after such mode changes. Reported latency is queried when the
instance is prepared/activated; PDC and parallel-route alignment belong to 3e.

## Budgets
Windows x64, 32-bit float audio. Up to 4096 declared parameters/instance and 256
distinct changed parameters per transaction/block, with 16 points/parameter in the
VST3 host queue. Saved component/controller blobs each have a 1 MiB limit; the core
snapshot also has a 16 MiB overall limit. Chains retain existing 8/channel and
32/project limits. Limits fail explicitly. No full #16 performance matrix claimed.

## Local SDK/build
Official [Steinberg SDK](https://github.com/steinbergmedia/vst3sdk), cached locally at
`build/_deps/vst3sdk`, root commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`
(SDK 3.8.1), with pinned base/pluginterfaces/public.sdk/cmake submodules. MIT license
copies are included in the package. See the official
[loading contract](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/VST%2BModule%2BArchitecture/Loading.html).
`-DMRS_BUILD_VST3=ON -DMRS_VST3_SDK_ROOT=<cached SDK path>` is explicit and configure
never downloads the SDK. `MRS_BUILD_VST3=OFF` retains the native DSP build. No tool
or third-party plugin was installed. ASIO and PortAudio use the existing cached
offline dependencies; all configure/build/tests are local, no GitHub Actions.

## Validation
85/85 CTest including dynamic test VST3 probe/processing/bypass, stereo separation,
opaque state roundtrip, actual editor callback, live desktop parameters/Undo,
state retention across adjacent inserts, rejection without device interruption,
realtime host allocation check and isolated scanner crash/hang/cache tests.
GUI smoke passed. Installed Blue Cat Gain 3 Stereo additionally passed actual
processing, gain automation, opaque-state reopen and native editor attachment;
2099 declared parameters handled; reported latency 0 samples. This is software
render validation; intended ASIO hardware and further plugins await user acceptance.
