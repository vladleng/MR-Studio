# Native separation migration (#351)

Accepted baseline: Studio 0.9.6-Upd1, source `22b183a6d1dbc60ab8a106f643b34495f5752776`.
The production Python worker and model cache remain intact until native parity is accepted.
This document describes a developer spike, not a released Studio 0.9.7 runtime.

## Reproducible CPU baseline

`tools/separation/native/baseline-lock.json` pins audio.cpp v0.9.3, commit
`41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48`, the published Windows/Linux C API
and CLI archives (sizes and full SHA256), and a revision-pinned F16 HTDemucs6 GGUF.
The converted package identifies checkpoint `5c90dfd2`, the same checkpoint used
by the accepted Python catalogue. Conversion provenance and numerical parity
still require comparison against the original checkpoint; package naming alone
does not prove parity.

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
| HTDemucs6 (`5c90dfd2`) | C API Windows/Linux CPU: 1s and 60s, two calls per session, six canonical outputs with exact geometry. Original-checkpoint A/B and 3–5min tests remain pending. | SPIKE ONLY |
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
This run does not exercise the custom SW patch. Native Studio integration,
quality acceptance and any speed claim remain pending.

`spike-evidence.json` records the Linux reports, package/patch hashes and reference
comparison. The raw 1s SW bass test repeats deterministically (same WAV SHA256).
Against the pinned PyTorch head it has cosine 0.99109 on their common 44032 frames;
the native API returns 44100 frames. The near-silent synthetic Bass peak is only
5.6e-6, so this is not evidence of musical quality. The `mdxc` frontend preserves
the reference's valid ISTFT extent before overlap/add. No native admission or
speed improvement is established by these measurements.

`compare_sw_chunk.py <clean-reference-root> <original-directory> <input.wav>
<native-evidence> <new-comparison-directory> [--role bass] [--mdxc]` reproduces
the raw-head or model-plus-frontend comparison. It requires pinned audio-separator source
`4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e`, torch 2.8.0 CPU, numpy 2.2.6,
soundfile 0.13.1, PyYAML 6.0.3, einops 0.8.2, beartype 0.18.5,
rotary-embedding-torch 0.6.5 and packaging 26.3 in the developer environment.
The custom Windows CI job builds the patched source, converts the exact original
checkpoint and probes Bass/Guitar independently. Its artifacts contain synthetic
examples/reports only; weights and development Python are not shipped.
