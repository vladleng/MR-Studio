# ASIO device layer — dependencies / spike

Decision for #16: the MRS-owned render engine is C++20. PortAudio supplies a
replaceable ASIO device adapter; it is not the DAW engine or plugin host.

PortAudio is pinned to commit `873e3c83fbe2f57ebcf59083e627a3f8fa051ffe`.
Build uses only its ASIO host on Windows x64. WASAPI/MME/DirectSound/WDMKS/JACK
are disabled in this prototype. The host opens the installed vendor driver,
not a custom/virtual ASIO driver. No fallback may label itself ASIO.

Primary API references:
- https://github.com/PortAudio/portaudio/blob/873e3c83fbe2f57ebcf59083e627a3f8fa051ffe/include/pa_asio.h
- https://github.com/PortAudio/portaudio/blob/873e3c83fbe2f57ebcf59083e627a3f8fa051ffe/include/portaudio.h
- https://github.com/PortAudio/portaudio/blob/873e3c83fbe2f57ebcf59083e627a3f8fa051ffe/src/hostapi/asio/pa_asio.cpp
- https://github.com/PortAudio/portaudio/blob/873e3c83fbe2f57ebcf59083e627a3f8fa051ffe/LICENSE.txt
- https://www.steinberg.net/developers/
- https://www.steinberg.net/asiosdk

Why this path: existing ASIO channel selectors/names, vendor control panel,
driver buffer capabilities, format conversion and lifecycle. It allows MRS
render/transport tests to remain independent of SDK/device availability and
does not bind the project to a UI framework. Direct SDK hosting remains an
alternative if benchmark evidence requires replacing the adapter.

Native buffer: suggested latency requests the desired native period; user
frames are unspecified, avoiding an additional fixed user-buffer layer.
Observed callback frames and reported stream latency are recorded. Driver
fallback is a REVIEW, not an assumed performance pass.

The official ASIO SDK is downloaded into the build directory by MRS CMake,
or supplied with `-DMRS_ASIO_SDK_ZIP=...`. It is pinned by SHA256:
`d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca`.
A changed upstream archive fails explicitly; review its version/license and
update the pin deliberately rather than silently compiling a new SDK.
No SDK source is committed to MRS or included in the checker package.
PortAudio's license notice and the obtained SDK license notice accompany
the developer checker.

The MRS product license remains undecided. These builds are development/
hardware evaluation artifacts. Before a public product release, select and
document the applicable ASIO SDK license path and satisfy it, along with
the PortAudio notice. ASIO's current SDK offers proprietary/GPL alternatives;
using the device wrapper does not erase SDK obligations. This is the existing
release check in AUDIO_ENGINE.md §17, not a completed licensing decision.
