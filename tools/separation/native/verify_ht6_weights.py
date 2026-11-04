#!/usr/bin/env python3
"""Developer-only tensor/config identity check: original HT6 checkpoint vs pinned GGUF.

Does not execute the Demucs model or prove inference/quality parity.
"""
import argparse
from fractions import Fraction
import json
import math
from pathlib import Path
import struct

from convert_sw_fixed import digest


class HTDemucs:
    """Inert symbol for the checkpoint's class metadata, never instantiated."""


class Gguf:
    def __init__(self, path):
        self.stream = path.open("rb")
        if self.stream.read(4) != b"GGUF" or self.number("I") != 3:
            raise ValueError("Expected GGUF v3")
        count, metadata = self.number("Q"), self.number("Q")
        if count != 525 or metadata > 1000:
            raise ValueError("Unexpected HT6 package geometry")
        self.meta = {}
        for _ in range(metadata):
            key = self.string()
            if key in self.meta:
                raise ValueError("Duplicate GGUF metadata")
            self.meta[key] = self.value(self.number("I"))
        self.tensors = []
        for _ in range(count):
            name, rank = self.string(), self.number("I")
            if not 1 <= rank <= 4:
                raise ValueError("Unexpected HT6 tensor rank")
            shape = [self.number("Q") for _ in range(rank)]
            self.tensors.append((name, shape, self.number("I"), self.number("Q")))
        alignment = self.meta.get("general.alignment", 32)
        if alignment not in (32, 64):
            raise ValueError("Unexpected alignment")
        self.start = (self.stream.tell() + alignment - 1) // alignment * alignment

    def number(self, kind):
        return struct.unpack("<" + kind, self.stream.read(struct.calcsize("<" + kind)))[0]

    def string(self):
        size = self.number("Q")
        if size > 1024*1024:
            raise ValueError("GGUF metadata string limit")
        return self.stream.read(size).decode("utf-8")

    def value(self, kind):
        if kind == 8:
            return self.string()
        if kind == 9:
            item, count = self.number("I"), self.number("Q")
            if item == 9 or count > 1024*1024:
                raise ValueError("GGUF metadata array limit")
            return [self.value(item) for _ in range(count)]
        return self.number({0:"B", 1:"b", 2:"H", 3:"h", 4:"I", 5:"i", 6:"f", 7:"?", 10:"Q", 11:"q", 12:"d"}[kind])


def normalized(value):
    if isinstance(value, Fraction):
        return float(value)
    if isinstance(value, (tuple, list)):
        return [normalized(item) for item in value]
    if isinstance(value, dict):
        return {str(k): normalized(v) for k, v in value.items()}
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("gguf", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    profiles = json.loads((Path(__file__).parents[1] / "mr_separation_profiles.json").read_text())
    profile = next(p for p in profiles if p["id"] == "htdemucs-6s")
    source = next(f for f in profile["files"] if f["name"].endswith(".th"))
    lock = json.loads(Path(__file__).with_name("baseline-lock.json").read_text())
    if digest(args.original) != source["sha256"] or digest(args.gguf) != lock["model"]["sha256"]:
        raise ValueError("Source/package full SHA256 mismatch")
    if args.report.exists():
        raise FileExistsError("Evidence report must be new")
    import torch
    # Only resolve the two class symbols found in the verified checkpoint.
    # No architecture import, model construction or unrestricted pickle load.
    with torch.serialization.safe_globals([(HTDemucs, "demucs.htdemucs.HTDemucs"), Fraction]):
        original = torch.load(args.original, map_location="cpu", weights_only=True)
    if original["klass"] is not HTDemucs:
        raise ValueError("Unexpected checkpoint class")
    package = Gguf(args.gguf)
    verified = set()
    try:
        for name, shape, kind, offset in package.tensors:
            if name.startswith("_audiocpp."):
                name = package.meta["audiocpp.tensor_names"][int(name.split(".")[1])]
            if not name.startswith("submodel_weights/"):
                raise ValueError("Unexpected tensor namespace")
            key = name.removeprefix("submodel_weights/")
            tensor = original["state"][key]
            if key in verified or kind != 1 or tensor.dtype != torch.float16 or shape != list(reversed(tensor.shape)):
                raise ValueError(f"Tensor shape/type mismatch: {key}")
            package.stream.seek(package.start + offset)
            if package.stream.read(math.prod(shape)*2) != tensor.contiguous().numpy().tobytes():
                raise ValueError(f"Tensor bytes differ: {key}")
            verified.add(key)
        if verified != set(original["state"]):
            raise ValueError("Tensor set mismatch")
        meta = package.meta
        names, offsets, data = (meta["audiocpp.embedded_files."+key] for key in ("names", "offsets", "data"))
        index = names.index("5c90dfd2/config.json")
        end = offsets[index+1] if index+1 < len(offsets) else len(data)
        config = json.loads(bytes(data[offsets[index]:end]))
        if config["kwargs"] != normalized(original["kwargs"]) or config["args"] != normalized(original["args"]):
            raise ValueError("Original/GGUF model configuration differs")
        if config["signature"] != "5c90dfd2" or config["sources"] != original["kwargs"]["sources"]:
            raise ValueError("Checkpoint/role mapping differs")
    finally:
        package.stream.close()
    report = {"scope": "weight/config identity only; not inference parity", "status": "PASS",
              "source_sha256": source["sha256"], "gguf_sha256": lock["model"]["sha256"],
              "exact_tensors": len(verified), "original_dtype": "float16", "gguf_dtype": "float16",
              "additional_weight_rounding": False, "args_kwargs_equal": True,
              "sources": config["sources"], "torch": torch.__version__}
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
