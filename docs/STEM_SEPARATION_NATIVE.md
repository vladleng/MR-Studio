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
| HTDemucs6 (`5c90dfd2`) | C API real Linux CPU run returns Drums/Bass/Other/Vocals/Guitar/Piano with exact geometry. Windows and long/reference tests tracked separately. | SPIKE ONLY |
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
