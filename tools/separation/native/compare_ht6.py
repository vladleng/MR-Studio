#!/usr/bin/env python3
"""Developer CPU comparison against the unchanged pinned Demucs implementation.

No downloader, resampling, project/IPC handling or subjective quality test.
"""
import argparse
from fractions import Fraction
import importlib
import json
from pathlib import Path
import subprocess
import sys
import time
import types

from convert_sw_fixed import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_root", type=Path)
    parser.add_argument("original", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("native_evidence", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--repeats", type=int, choices=[1, 2], default=2)
    args = parser.parse_args()
    revision = "4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e"
    actual = subprocess.check_output(["git", "-C", str(args.reference_root), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(args.reference_root), "status", "--porcelain", "--untracked-files=no"], text=True)
    if actual != revision or dirty:
        raise ValueError("Expected clean pinned audio-separator reference")
    profiles = json.loads((Path(__file__).parents[1] / "mr_separation_profiles.json").read_text())
    profile = next(p for p in profiles if p["id"] == "htdemucs-6s")
    source = next(f for f in profile["files"] if f["name"].endswith(".th"))
    if digest(args.original) != source["sha256"]:
        raise ValueError("Original checkpoint SHA256 mismatch")
    native_report = json.loads((args.native_evidence / "report.json").read_text())
    if native_report["input_sha256"] != digest(args.input) or native_report["threads"] != 4:
        raise ValueError("Input/native benchmark settings mismatch")
    lock = json.loads(Path(__file__).with_name("baseline-lock.json").read_text())
    if native_report["model_sha256"] != lock["model"]["sha256"]:
        raise ValueError("Unexpected native HT6 package")
    if args.output.exists():
        raise FileExistsError("Evidence output must be new")
    import numpy as np
    import soundfile as sf
    import torch

    # Bypass high-level downloader initialization; use unchanged model and apply
    # modules, including real julius/diffq dependencies. No inference stubs.
    for name in ("audio_separator", "audio_separator.separator", "audio_separator.separator.uvr_lib_v5"):
        module = types.ModuleType(name)
        module.__path__ = [str(args.reference_root.resolve().joinpath(*name.split(".")))]
        sys.modules[name] = module
    module = types.ModuleType("demucs")
    module.__path__ = [str(args.reference_root.resolve() / "audio_separator/separator/uvr_lib_v5/demucs")]
    sys.modules["demucs"] = module
    HTDemucs = importlib.import_module("demucs.htdemucs").HTDemucs
    apply_model = importlib.import_module("demucs.apply").apply_model
    torch.set_num_threads(4)
    torch.set_num_interop_threads(1)
    torch.manual_seed(0)
    loaded = time.perf_counter()
    with torch.serialization.safe_globals([HTDemucs, Fraction]):
        checkpoint = torch.load(args.original, map_location="cpu", weights_only=True)
    model = HTDemucs(*checkpoint["args"], **checkpoint["kwargs"]).eval()
    model.load_state_dict(checkpoint["state"], strict=True)
    load_ms = (time.perf_counter()-loaded)*1000
    audio, rate = sf.read(args.input, dtype="float32", always_2d=True)
    if rate != 44100 or audio.shape[1] != 2 or not 1 < len(audio) <= 44100*300 or not np.isfinite(audio).all():
        raise ValueError("Expected bounded stereo 44.1kHz Float32 input")
    mixture = torch.from_numpy(audio.T.copy())
    reference = mixture.mean(0)
    mean, std = reference.mean(), reference.std()
    if not torch.isfinite(std):
        std = torch.zeros_like(std)
    mixture = (mixture-mean)/std.clamp_min(torch.finfo(mixture.dtype).eps)
    runs = []
    for _ in range(args.repeats):
        started = time.perf_counter()
        with torch.inference_mode():
            separated = apply_model(model, mixture[None], shifts=0, split=True,
                                    overlap=0.25, progress=False, device="cpu")[0]
            separated = separated.cpu()*std+mean
        inference_ms = (time.perf_counter()-started)*1000
        rows = []
        for role, predicted in zip(model.sources, separated.numpy()):
            native, native_rate = sf.read(args.native_evidence / "run0" / f"{role}.wav", dtype="float32", always_2d=True)
            a, b = native.astype(np.float64), predicted.T.astype(np.float64)
            if native_rate != rate or a.shape != b.shape or not np.isfinite(b).all():
                raise ValueError("Output geometry/non-finite sample mismatch")
            norm = float(np.sqrt(np.sum(a*a)*np.sum(b*b)))
            rows.append({"role": role, "frames": len(a),
                         "waveform_cosine": float(np.sum(a*b)/norm) if norm else None,
                         "mae": float(np.abs(a-b).mean()), "max_error": float(np.abs(a-b).max()),
                         "reference_peak": float(np.abs(b).max())})
        runs.append({"model_and_chunking_ms": inference_ms, "outputs": rows})
    if digest(args.input) != native_report["input_sha256"]:
        raise ValueError("Source changed during comparison")
    report = {"scope": "same-checkpoint model plus Demucs frontend, not complete Windows worker",
              "reference_revision": revision, "python": sys.version, "torch": torch.__version__,
              "threads": 4, "shifts": 0, "overlap": 0.25, "seed": 0,
              "input_sha256": native_report["input_sha256"], "original_sha256": source["sha256"],
              "native_package_sha256": native_report["model_sha256"], "frames": len(audio),
              "model_load_ms": load_ms, "runs": runs, "input_unchanged": True,
              "limitations": "Synthetic input, dev Python3.12; no subjective quality or user Ryzen benchmark"}
    args.output.mkdir(parents=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
