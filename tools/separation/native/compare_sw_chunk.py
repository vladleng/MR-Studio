#!/usr/bin/env python3
"""Developer-only raw SW head comparison, not an end-to-end worker benchmark."""
import argparse
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
    parser.add_argument("--role", choices=["bass", "drums", "other", "vocals", "guitar", "piano"], default="bass")
    parser.add_argument("--mdxc", action="store_true", help="Compare reference Hamming/tail schedule, not one raw head")
    args = parser.parse_args()
    revision = "4dd3477c0a8f5bfb0fb053c6ed444bb935cb150e"
    actual = subprocess.check_output(["git", "-C", str(args.reference_root), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(args.reference_root), "status", "--porcelain", "--untracked-files=no"], text=True)
    if actual != revision or dirty:
        raise ValueError("Reference must be the clean pinned audio-separator 0.48.0 source")
    catalogue = json.loads((Path(__file__).parents[1] / "mr_separation_profiles.json").read_text())
    profile = next(p for p in catalogue if p["id"] == "bs-roformer-sw-6-v1")
    for entry in profile["files"]:
        if digest(args.original / entry["name"]) != entry["sha256"]:
            raise ValueError("Original source hash mismatch")
    native_report = json.loads((args.native_evidence / "report.json").read_text())
    if native_report["input_sha256"] != digest(args.input) or native_report["threads"] != 4:
        raise ValueError("Native/input benchmark settings mismatch")
    if args.output.exists():
        raise FileExistsError("Comparison output must be new")
    import numpy as np
    import soundfile as sf
    import torch
    import yaml

    # Namespace paths bypass only the high-level downloader __init__; all model,
    # device and rotary implementations are loaded unchanged from pinned source.
    for name in ("audio_separator", "audio_separator.separator", "audio_separator.separator.uvr_lib_v5"):
        module = types.ModuleType(name)
        module.__path__ = [str(args.reference_root.resolve().joinpath(*name.split(".")))]
        sys.modules[name] = module
    BSRoformer = importlib.import_module("audio_separator.separator.uvr_lib_v5.roformer.bs_roformer").BSRoformer
    torch.set_num_threads(4)
    config = yaml.load((args.original / "BS-Rofo-SW-Fixed.yaml").read_text(), Loader=yaml.FullLoader)
    model = BSRoformer(**config["model"]).eval()
    model.load_state_dict(torch.load(args.original / profile["model_file"], map_location="cpu", weights_only=True), strict=True)
    index = config["training"]["instruments"].index(args.role)
    model.mask_estimators = torch.nn.ModuleList([model.mask_estimators[index]])
    model.num_stems = 1
    audio, rate = sf.read(args.input, dtype="float32", always_2d=True)
    if rate != 44100 or audio.shape[1] != 2 or not 1024 < len(audio) <= (44100*300 if args.mdxc else 130560):
        raise ValueError("Expected one stereo SW chunk")
    if args.mdxc and native_report.get("frontend") != "mdxc_hamming_tail_aligned":
        raise ValueError("Native frontend mode mismatch")
    started = time.perf_counter()
    with torch.inference_mode():
        if args.mdxc:
            # Reproduce pinned MDXC scheduling and Float32 accumulation independently
            # from the C++ implementation; high-level export/resampling stays outside this test.
            chunk, step = 130560, 65280
            starts = []
            for offset in range(0, len(audio), step):
                if offset + chunk >= len(audio):
                    tail = max(len(audio) - chunk, 0)
                    if not starts or starts[-1] != tail:
                        starts.append(tail)
                    break
                starts.append(offset)
            reference = np.zeros_like(audio)
            counter = np.zeros((len(audio), 1), dtype=np.float32)
            window = np.hamming(chunk).astype(np.float32)[:, None]
            for offset in starts:
                piece = audio[offset:offset+chunk]
                predicted = model(torch.from_numpy(piece.T.copy())[None])[0].numpy().T
                valid = min(len(piece), len(predicted), chunk)
                reference[offset:offset+valid] += predicted[:valid] * window[:valid]
                counter[offset:offset+valid] += window[:valid]
            reference /= np.maximum(counter, np.float32(1e-10))
        else:
            reference = model(torch.from_numpy(audio.T.copy())[None])[0].numpy().T
    inference_ms = (time.perf_counter() - started) * 1000
    native, native_rate = sf.read(args.native_evidence / "run0" / f"{args.role}.wav", dtype="float32", always_2d=True)
    if native_rate != rate or native.shape != audio.shape or not np.isfinite(reference).all() or not np.isfinite(native).all():
        raise ValueError("Invalid output geometry or samples")
    frames = min(len(native), len(reference))
    a, b = native[:frames].astype(np.float64), reference[:frames].astype(np.float64)
    norm = float(np.sqrt(np.sum(a*a) * np.sum(b*b)))
    report = {"scope": "selected model plus reference frontend" if args.mdxc else "raw selected head; not complete worker or subjective quality",
              "reference_revision": revision, "python": sys.version, "torch": torch.__version__,
              "threads": 4, "role": args.role, "input_sha256": digest(args.input),
              "native_model_sha256": native_report["model_sha256"],
              "frames_reference": len(reference), "frames_native": len(native),
              "compared_frames": frames, "reference_inference_ms": inference_ms,
              "waveform_cosine": float(np.sum(a*b)/norm) if norm else None,
              "mae": float(np.abs(a-b).mean()), "max_error": float(np.abs(a-b).max()),
              "reference_peak": float(np.abs(b).max()), "admission": "NOT ACCEPTED"}
    args.output.mkdir(parents=True)
    sf.write(args.output / f"{args.role}.wav", reference, rate, subtype="FLOAT")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
