# Native separation migration (#351)

Accepted baseline: Studio 0.9.6-Upd1, source `22b183a6d1dbc60ab8a106f643b34495f5752776`.
The accepted Python worker and model cache remain intact. The user requested a temporary
Studio 0.9.7 A/B build with an explicit Python/PyTorch or audio.cpp/GGML choice.
The final backend and removal of the other implementation follow user tests; native
replacement is no longer predetermined. This source is not a released runtime until CI passes.

## Temporary Studio A/B

The backend selector in Separate into Stems defaults to Python and is locked during
a job. Each ProjectInput captures the backend; the existing ProjectJob/Runner/asset
commands remain shared. There is no automatic fallback or second DAW engine.
Native model files live below the existing Separation root in `native-cpu/models`,
with independent locks and managed inventories. Python installations remain intact.
Native runtime is bundled in this test build, not installed with pip. Native HTDemucs6
can be downloaded through Model Manager; SW Fixed6 imports the exact GGUF from the
separate matching build artifact. Expected package hash and size are compiled into
the app/worker during CI. Wrong packages are rejected before publication.
Other native catalogue entries are visibly unsupported; Python retains them.
Results retain backend, weight precision and host wall time in their provenance;
wall time includes startup/model loading and output verification, excluding later
project asset copy. Compare identical clip ranges, roles and chunk settings.
Native CPU may be slower; user quality/performance acceptance is pending.

`[ci native-studio]` on the task branch runs the native checks, then Studio-only
ARA2/ASIO build, targeted separation/model/project/persistence checks and the
canonical Windows package verifier. The separate weights artifact is optional
and never bundled into the base runtime. MR Live is OFF; main merge is separate.

## Reproducible CPU baseline

`tools/separation/native/baseline-lock.json` pins audio.cpp v0.9.3, commit
`41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48`, the published Windows/Linux C API
and CLI archives (sizes and full SHA256), and a revision-pinned F16 HTDemucs6 GGUF.
The converted package identifies checkpoint `5c90dfd2`, the same checkpoint used
by the accepted Python catalogue. `verify_ht6_weights.py` verifies both full source
hashes and proves all 525 Float16 tensor byte sequences, complete tensor set,
model args/kwargs and canonical roles match the original checkpoint exactly.
The original checkpoint already uses Float16; this package adds no rounding.
This proves weight/config identity, not full-worker inference or subjective quality.

Run the explicit developer fetch, then compile the independent C++ probe:

```text
python tools/separation/native/fetch_baseline.py <cache>
cmake -S tools/separation/native -B <build> -DMRS_AUDIOCPP_SDK_ROOT=<cache>/sdk
cmake --build <build> --config Release --parallel 2
python tools/separation/native/make_fixture.py <input.wav> --seconds 60
mrs_native_separation_probe <model.gguf> <input.wav> <new-evidence-directory> 4 2
```

The fetch and fixture scripts are developer/CI tools only, not a new user Python
runtime. The probe calls the native C ABI directly, checks ABI compatibility,
requires six unique canonical outputs, exact frames/rate/channel count, finite
samples and an unchanged source hash, and writes unclipped FLOAT32 WAV files.
The same model/session handles are reused for two calls. `report.json` separates
model loading, session creation, cold/warm inference and overall wall time and
records peak process RSS. It does not report SDR or claim subjective quality.
The fixture is synthetic, not a recording or a listening benchmark.

The Windows workflow builds only this probe. No Studio/JUCE/Live/ASIO target,
weights or native SDK is uploaded as a user application. Reports and short
synthetic examples are retained for seven days. A real 60s/3–5min Python/native
comparison at matched settings remains a separate migration gate.

## Exact-profile compatibility at the pinned revision

| Accepted profile | Source-backed finding | Admission status |
| --- | --- | --- |
| HTDemucs derived2 / native4 (`955717e8`) | Single-model Demucs family exists; derived Instrumental needs the existing Bass+Drums+Other rule. Exact GGUF conversion and output comparison pending. | NOT TESTED |
| HTDemucs6 (`5c90dfd2`) | C API Windows/Linux CPU: 1s and 60s, two calls per session, six canonical outputs with exact geometry. Original tensors/config identical; synthetic60s reference comparison below. Full worker and 3–5min tests pending. | SPIKE ONLY |
| HTDemucs FT4 | `src/models/demucs/session.cpp` explicitly rejects manifests with more than one submodel. The accepted FT bag contains four specialized checkpoints. | REQUIRES NATIVE ORCHESTRATION |
| BS-RoFormer Viperx ep317 | Single-output RoFormer exists; published ep368 packages are not substitutes for ep317. Exact ep317 conversion/config/parity pending. | NOT TESTED |
| BS-RoFormer SW Fixed6 | `src/models/roformer/runtime.cpp` rejects `num_stems != 1`, binds only `mask_estimators.0`; session publishes vocals plus a derived residual. | REQUIRES NATIVE PORT |
| SCNet XL IHF4 | No SCNet family/spec/source found in the pinned tree or current upstream family list. | REQUIRES NATIVE PORT |
| Mel-Band Kimberley2 | General Mel-Band path exists; an MLX-labelled package does not prove parity with our exact checkpoint. | NOT TESTED |
| MDX KARA2 | No MDX family/spec/source found in the pinned tree/current family list. An ONNX file is not directly a GGUF package. | REQUIRES NATIVE PORT |

None of these findings authorize removing a model or silently substituting
another checkpoint. Ports of accepted models are required work in #351, not
optional new-model work in #340. SW Fixed6 and SCNet are especially important
because of the accepted user's bass-quality observations.

## Worker integration constraints

- Keep the C ABI inside an isolated native process. Reuse the existing bounded
  Job/IPC/manifest, Model Manager and Project Model; no inference in DAW/ASIO.
- The published offline C ABI has no progress/cancellation callback. Final
  integration must retain truthful progress and cancellation (process isolation
  permits forced termination); do not invent chunk counts or a completed state.
- Native FT can potentially run the individual one-hot experts and keep the
  corresponding roles. This needs real conversion and A/B checks before admission.
- GGUF quantization, chunk/overlap, normalization, thread counts and input ranges
  must be recorded independently; changed precision/settings are not proof that
  the framework itself is faster.
- Local CPU first. CUDA/Vulkan are local capabilities only when tested; Cloud
  remains deferred in #339. Keep existing projects/old result records readable.
- Remove Python/PyTorch from the production package only after the accepted
  scenarios work natively or the user explicitly accepts documented replacements.

Upstream pointers are at the pinned commit:
[C API](https://github.com/0xShug0/audio.cpp/blob/41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48/include/audiocpp.h),
[Demucs session](https://github.com/0xShug0/audio.cpp/blob/41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48/src/models/demucs/session.cpp),
[RoFormer runtime](https://github.com/0xShug0/audio.cpp/blob/41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48/src/models/roformer/runtime.cpp).

## SW Fixed6 development port

`patches/audio-cpp-sw-heads.patch` under `tools/separation/native` applies to the
exact upstream commit above. It adds a checked one-based `bs_roformer.stem_index`
and exact graph `bs_roformer.chunk_size`, loads the selected independent mask
head, and publishes its canonical role. Single-head vocal/residual behavior stays
on the original branch. Only BS multi-head models are enabled by this patch.
It is a development port, not an admitted production model.

Reproduction (separate clean upstream checkout; CMake 3.24+):

```sh
git checkout --detach 41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48
git apply --check /path/to/MR-Studio/tools/separation/native/patches/audio-cpp-sw-heads.patch
git apply /path/to/MR-Studio/tools/separation/native/patches/audio-cpp-sw-heads.patch
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAUDIOCPP_BUILD_C_API=ON \
  -DAUDIOCPP_MODEL_SET=custom -DAUDIOCPP_MODELS=demucs,roformer \
  -DAUDIOCPP_BUILD_NATIVE_MODEL_MANAGER=OFF -DAUDIOCPP_DEPLOYMENT_BUILD=ON \
  -DAUDIOCPP_VERSION=v0.9.3-mrs1 -DENGINE_ENABLE_NATIVE_CPU=OFF \
  -DENGINE_ENABLE_CUDA=OFF -DENGINE_ENABLE_VULKAN=OFF \
  -DENGINE_ENABLE_METAL=OFF -DENGINE_ENABLE_HIP=OFF \
  -DENGINE_BUILD_TESTS=OFF -DENGINE_BUILD_MODEL_TESTS=OFF
cmake --build build --config Release --target audiocpp audiocpp_gguf --parallel 2
```

In a developer environment with torch 2.8.0 CPU, PyYAML 6.0.3 and safetensors
0.5.3, run `convert_sw_fixed.py <verified-original-directory> <new-directory>`.
It validates both original catalogue hashes before checkpoint deserialization,
exports unquantized tensors, role order, conversion provenance and the accepted
worker's segment 256 / overlap 2 settings. No dependencies from this tool belong
in the user package. Pack using the patched converter:

```sh
audiocpp_gguf --input weights=<converted>/model.safetensors --root <converted> \
  --output <new-model.gguf> --type orig --family bs_roformer \
  --model-spec model_specs/bs_roformer.json
```

Compile the probe against this custom C API/header (CMake cache variables
`AUDIOCPP_INCLUDE` and `AUDIOCPP_LIBRARY` may specify the build locations).
The optional last probe argument selects a raw SW role, for example
`probe model.gguf input-1s.wav new-evidence 4 2 bass`. This mode requires an input
of 1025–130560 frames and uses an exact-size graph with native overlap 1.
It measures one raw model invocation; it is **not** the complete worker.

The accepted MDXC frontend uses Hamming overlap/add, segment size 256 (130560
frames for SW) or 128 (65024), and a final full chunk aligned to the input tail.
Pinned audio.cpp instead uses reflected border padding and linear fade windows.
The probe's additional final argument `mdxc` enables this scheduling/windowing
outside the C API through `sw_frontend.hpp`, calls exact-size raw chunks with
native overlap 1 and reports completed chunks. It also restricts each prediction
to the reference ISTFT valid extent before accumulation. Its C++ test checks
independent NumPy window goldens, short/exact/tail schedules, bounds and a shorter
ISTFT result. Production IPC/cancel integration and full-worker parity remain
pending. Do not switch the production worker based on a development probe.

Official-SDK Windows baseline: [run 41762034298](https://github.com/vladleng/MR-Studio/actions/runs/41762034298),
source `3c408ee1b72ac2b2cb0957aaf0bb108289066a76`; 1s/60s repeats PASS.
This run does not exercise the custom SW patch. The custom SW Windows port passed
[run41765773225](https://github.com/vladleng/MR-Studio/actions/runs/41765773225)
at source `5c5f3be3ded33aba1c158e56c1e5bb541aa5c85f`: raw Bass/Guitar1s repeats,
Bass4s overlapping chunks/repeats, and frontend unit test.
Native Studio integration,
quality acceptance and any speed claim remain pending.

`spike-evidence.json` records the Linux reports, package/patch hashes and reference
comparison. The raw 1s SW bass test repeats deterministically (same WAV SHA256).
Against the pinned PyTorch head it has cosine 0.99109 on their common 44032 frames;
the native API returns 44100 frames. The near-silent synthetic Bass peak is only
5.6e-6, so this is not evidence of musical quality. The `mdxc` frontend preserves
the reference's valid ISTFT extent before overlap/add. No native admission or
speed improvement is established by these measurements.

The 4s SW model-plus-frontend run exercises two overlapping full chunks and two
calls on the same session: both return exactly 176400 stereo frames and identical
WAV hashes. The independent PyTorch model plus reference scheduling also returns
176400 frames; waveform cosine is 0.97883 on this near-silent synthetic Bass
(reference peak 1.23e-5). This remains a development comparison, not quality parity.

`compare_sw_chunk.py <clean-reference-root> <original-directory> <input.wav>
<native-evidence> <new-comparison-directory> [--role bass] [--mdxc]` reproduces
the raw-head or model-plus-frontend comparison. It requires pinned audio-separator source
`4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e`, torch 2.8.0 CPU, numpy 2.2.6,
soundfile 0.13.1, PyYAML 6.0.3, einops 0.8.2, beartype 0.18.5,
rotary-embedding-torch 0.6.5 and packaging 26.3 in the developer environment.
The custom Windows CI job builds the patched source, converts the exact original
checkpoint and probes Bass/Guitar independently. Its artifacts contain synthetic
examples/reports only; weights and development Python are not shipped.

## Original HTDemucs6 comparison

`verify_ht6_weights.py <original.th> <pinned.gguf> <new-report.json>` requires
developer torch 2.8.0 CPU/numpy 2.2.6. It uses `weights_only=True`, two explicitly
allowed class symbols and an inert class marker; no Demucs architecture is
constructed for this weight/config check. Both source hashes are checked first.

`compare_ht6.py <clean-reference-root> <original.th> <input.wav> <native-evidence>
<new-output-directory>` loads the unchanged pinned audio-separator Demucs model
and `apply_model` with real dependencies (julius 0.2.8, diffq-fixed 0.2.4,
tqdm 4.70.1 in addition to the reference dependencies above). No inference stubs
are used. Two calls share one model; CPU threads4, shifts0, seed0, overlap0.25,
original segment7.8s, unnormalized Float32 output. The comparison covers model,
normalization and chunking; host IPC/resampling/project operations are excluded.

On the identical synthetic60s input, all six native outputs have exactly2646000
stereo frames, source hash unchanged, waveform cosine >=0.99999999998;
largest absolute difference is2.36e-6. This is strong numerical evidence on this
fixture, not musical/listening acceptance. Development reference Python is3.12.14,
not the accepted Windows embedded3.11.9 runtime.

| Linux CPU4 threads, same checkpoint/input | Cold call | Warm call |
| --- | ---: | ---: |
| Native C API, normalization included | 113.73s | 108.77s |
| PyTorch model plus chunking, input normalization excluded | 34.32s | 32.71s |

The Linux environment has a cgroup quota of two CPUs while both runtimes request
four threads; these results describe that constrained configuration. A matched
two-thread check is needed before attributing the entire gap to the framework.
Timings exclude imports, model load, source/model hashing and project work;
normalization accounts for a small scope difference. These measurements show
**no CPU speedup** from this pinned native implementation. They are not a user
Ryzen benchmark. CPU optimization and matched complete-worker/music tests remain
required; do not claim migration is ready or faster from these results.

A matched two-thread 1s check still shows a gap: native9.58/9.17s versus
PyTorch3.78/2.78s. The cgroup/thread mismatch alone does not explain it. AVX2 is
enabled in the custom build. BLAS is disabled; simply enabling the standalone
GGML BLAS backend is insufficient because this model path uses CPU host graph
plans without a BLAS scheduler. No BLAS acceleration is implemented or claimed.

The native180s input also passed:7938000 stereo frames for each of six roles,
finite samples, unchanged source,289.50s inference; detailed RSS/hashes in evidence.
This longer geometry/resource check has no matched Python/music acceptance yet.

## Isolated worker development slice

`MRS_BUILD_NATIVE_SEPARATION_WORKER=ON` explicitly builds a separate
`mrs_native_separation_worker` executable. It uses the existing Core WAV decoder,
the pinned C API and the existing uint32-LE/64KiB JSON job/manifest contract. It
does not create another DAW audio engine. There is no Studio install/copy rule.
Production Model Manager continues to select the accepted worker until migration
acceptance. `Configuration::nativeWorker` is an explicit migration/test override,
with no automatic fallback; it is not a permanent user-facing backend switch.

Apply **both** `audio-cpp-sw-heads.patch` and `audio-cpp-demucs-progress.patch` to
the pinned upstream checkout before building its C API. The second patch adds
MR extension v1 `mrs_audiocpp_request_set_chunk_progress`: synchronous real Demucs
chunk counts, returning zero to cancel between chunks. It is a custom extension,
not an upstream ABI0.2 feature. The C API regression uses a real HT6 model to
check start/end cancellation, resetting the callback and reusing the same session
without changing its output. Linux PASS; the new Windows extension/host run is
pending. SW progress comes from actual completed outer frontend chunks.

Example developer build (SQLite is required by the existing root configure):

```sh
cmake -S . -B build/native-worker -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DMRS_BUILD_DESKTOP=OFF -DMRS_BUILD_ASIO=OFF -DMRS_BUILD_VST3=OFF \
  -DMRS_BUILD_ARA=OFF -DMRS_BUILD_JUCE_UI=OFF -DMRS_BUILD_LIVE=OFF \
  -DMRS_BUILD_NATIVE_SEPARATION_WORKER=ON \
  -DMRS_AUDIOCPP_SOURCE_ROOT=<patched-audio.cpp> \
  -DMRS_AUDIOCPP_NATIVE_LIBRARY=<custom-C-API-library>
cmake --build build/native-worker --config Release \
  --target mrs_native_separation_worker mrs_native_ipc_test --parallel 2
```

The worker verifies the package's full SHA before loading, runs inference off the
DAW process, sends heartbeats and real chunk progress, checks cancellation/deadline,
and publishes only selected unclipped Float32 outputs through an atomic
`result.part`→`result` rename. It records original source hashes/range, timeline
offset, legacy checkpoint provenance and the actual native package hash. Work
files are owned by the existing Runner/Job Object; no original/model deletion.
JSON checks cover duplicate keys, UTF-8/NUL, finite numbers, decimal counter
precision, depth/node/frame bounds. No library stdout is allowed onto the wire.

Local real-model IPC regression passed8 cases: partial PCM16 mono clip with
timeline offset and two HT6 outputs; two SW heads in the same loaded model;
stale source, unsupported rate/model, duplicate stems, corrupt model, fragmented
cancel. Input unchanged; only requested WAV files published. Test harness
`tests/native_separation_worker_check.py` uses standard-library Python as a
client, **not** for inference. Windows CI also checks the actual existing Runner
and its Job Object without configuring a Python runtime, plus the previous fake
worker regressions. No Studio GUI, MR Live or physical ASIO tests are invoked.

Current admission limits are explicit: only exact HT6 and SW Fixed6 candidates,
source44.1/48/88.2/96kHz and the existing eight project rates8–96kHz,
mono/stereo Core-supported PCM16/24/32 or Float32 WAV; HT6 default segment only;
SW default/small and inputs over1024 model frames. Other rates/profiles are
rejected; no silent conversion or replacement. The remaining accepted catalogue
and full quality/performance acceptance are still required before activation.
Multiple SW selections currently run independent head sessions sequentially;
shared-encoder optimization and matched quality/performance tests remain pending.

GGUF container metadata may differ between producer platforms. Conversion writes
JSON sidecars with explicit LF; this does not promise byte-identical containers
across compilers. The canonical Linux development SW package has the documented
SHA. For an explicitly verified local conversion, the builder can bake its exact
hash through `MRS_NATIVE_SW_PACKAGE_SHA256`; the runtime request cannot override
it. CI converts only after checking the accepted original hashes and bakes that
produced package hash into the worker. This is not an unverified runtime fallback
or a replacement checkpoint. Production distribution still needs a canonical
published package/catalogue pin.

## Native source/project rate conversion

`native_resample.hpp` ports the accepted Float32 `scipy.signal.resample_poly`
policy: reduced rational ratio, symmetric Kaiser5 low-pass FIR, constant-zero
exterior, centered phase, Float32 coefficient scaling/accumulation and the same
nearest-frame trim. The existing DAW import resampler has a different filter and
is intentionally unchanged. Cancellation is checked during filter preparation
and output blocks. No Python/NumPy/SciPy is needed in the native worker.

Nineteen synthetic goldens from numpy2.2.6/scipy1.15.3 cover supported ratio
shapes, mono/stereo, gain above1, silence, centered/edge impulses, identity bit
preservation and frame rounding. Linux max absolute error is0 on these vectors;
the portable test tolerance is2e-6. Bounds/non-finite input and cancellation are
also checked. `tests/generate_native_resample_fixture.py` is a developer-only
generator; checked-in vectors contain no recorded/user audio. Windows SRC
validation is still pending the next source build.

The complete Linux worker now passes11 real-model IPC cases, adding partial
48kHz PCM24 stereo→96kHz all-six outputs, 88.2kHz mono→48kHz and96kHz Float32
stereo→44.1kHz to the earlier subset/error/cancel cases. Tests verify snapshot
frames/hash, exact source-range samples for identity conversion, project output
frames/rate, unchanged sources and selected-file publication. The actual Windows
Runner test also uses its standard48kHz project rate once this step is built.
