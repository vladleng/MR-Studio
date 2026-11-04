#!/usr/bin/env python3
"""Explicit developer-only conversion of the accepted SW Fixed6 checkpoint.

Requires torch, PyYAML and safetensors in a development environment. These
dependencies must never become part of the native user runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    catalogue = json.loads((Path(__file__).parents[1] / "mr_separation_profiles.json").read_text())
    # Source names/checksums remain owned by the accepted production catalogue.
    profiles = catalogue if isinstance(catalogue, list) else catalogue["profiles"]
    profile = next(p for p in profiles if p["id"] == "bs-roformer-sw-6-v1")
    verified = {}
    for entry in profile["files"]:
        path = args.source / entry["name"]
        if digest(path) != entry["sha256"]:
            raise ValueError(f"Source SHA256 mismatch: {path.name}")
        verified[path.suffix] = path
    if args.output.exists():
        raise FileExistsError("Conversion output must be a new directory")
    # No checkpoint deserialization before both source hashes are verified.
    import torch
    import yaml
    from safetensors.torch import save_file

    class ConfigLoader(yaml.SafeLoader):
        pass
    ConfigLoader.add_constructor("tag:yaml.org,2002:python/tuple",
                                 lambda loader, node: loader.construct_sequence(node))
    config = yaml.load(verified[".yaml"].read_text(), Loader=ConfigLoader)
    audio, model = config["audio"], config["model"]
    roles = config["training"]["instruments"]
    if roles != ["bass", "drums", "other", "vocals", "guitar", "piano"] or model["num_stems"] != 6:
        raise ValueError("Unexpected independent SW heads")
    state = torch.load(verified[".ckpt"], map_location="cpu", weights_only=True)
    for key in ("state_dict", "model", "model_state_dict"):
        if isinstance(state, dict) and isinstance(state.get(key), dict):
            state = state[key]
            break
    tensors = {k: v.detach().cpu().contiguous() for k, v in state.items()
               if isinstance(v, torch.Tensor) and not k.endswith(".rotary_embed.freqs")}
    if not tensors or any(v.dtype != torch.float32 for v in tensors.values()):
        raise ValueError("Expected unquantized Float32 checkpoint tensors")
    exported = {"model_type": "bs_roformer", "sample_rate": audio["sample_rate"],
                "stereo": model["stereo"], "chunk_size": model["stft_hop_length"] * (256 - 1),
                "batch_size": 1, "num_overlap": 2, "normalize": False, "instruments": roles}
    for name in ("dim", "depth", "num_stems", "time_transformer_depth", "freq_transformer_depth",
                 "linear_transformer_depth", "freqs_per_bands", "dim_head", "heads",
                 "stft_normalized", "mask_estimator_depth", "skip_connection"):
        exported[name] = model.get(name, 0 if name == "linear_transformer_depth" else False)
    for source, target in (("stft_n_fft", "n_fft"), ("stft_hop_length", "hop_length"),
                           ("stft_win_length", "win_length")):
        exported[target] = model[source]
    exported["mlp_expansion_factor"] = model.get("mlp_expansion_factor", 4)
    args.output.mkdir(parents=True)
    save_file(tensors, str(args.output / "model.safetensors"))
    (args.output / "config.json").write_text(json.dumps(exported, indent=2) + "\n")
    provenance = {"profile": profile["id"], "sources": profile["files"],
                  "precision": "f32", "segment_size": 256, "overlap": 2,
                  "output_sha256": digest(args.output / "model.safetensors"),
                  "torch": torch.__version__, "admission": "NOT TESTED"}
    (args.output / "conversion.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"Converted {len(tensors)} Float32 tensors; native parity NOT TESTED")


if __name__ == "__main__":
    main()
