# Pinned stretch dependencies

`stretch-lock.json` records immutable upstream commits and SHA256 for every
vendored file. These are unmodified public headers and licenses, not copied DSP
inside MR Core. CMake checks all listed hashes on every configure, including fully
disconnected builds. Runtime packaging requires both MIT license files.

- Signalsmith Stretch: `a670068d9aeb64913331d5cc29337b19a457a7df` (API 1.3.2).
- Signalsmith Linear: `de55e6a50ffcf6f8f43f649692d94691c7025151` (0.6.4, the version
  requested by this Stretch revision's upstream CMake).
- FFT provider: portable built-in Signalsmith Linear. No Accelerate, IPP, PFFFT,
  CMSIS-DSP or XSIMD backend/build downloads. Upstream CMake is not executed.

Offline builds need only this checkout: no external dependency cache is required.
Public headers are covered by the normal source checkout; no SDK/user media is
part of this directory. Full upstream notices are in each `LICENSE.txt` and are
copied verbatim to Windows runtime packages.

Update procedure: fetch the proposed exact commit from each recorded official
origin into an isolated scratch checkout; verify its commit and licenses, compare
headers/API/transitive requirements, copy the same root/include/platform header
layout plus license, and regenerate the file SHA256 map. Review both code and
hash changes together. Configure offline, run stretch contract/render/RT tests
and both supported CI profiles; repeat #62 benchmarks/listening before changing
the default. Never update a checksum merely to accept an unexplained mismatch.
Optional FFT providers require separate review of licenses, reproducibility and
measured benefit. Hashes attest the reviewed snapshot, not independent authorship.
