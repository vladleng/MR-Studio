"""Explicit CI/dev installation only. The worker never calls this downloader.

User-authorised development smoke; license review deferred, no redistributed weights.
"""
import argparse
import hashlib
from pathlib import Path
import urllib.request

FILES = {
    "htdemucs.yaml": (
        "https://github.com/nomadkaraoke/python-audio-separator/releases/download/model-configs/htdemucs.yaml",
        "239c445d0b14454d541ad8bd9bb271c9e536d267e8a4625208744cbb2e7bb66c", 1024 * 1024),
    "955717e8-8726e21a.th": (
        "https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/955717e8-8726e21a.th",
        "8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4", 1024**3),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    for name, (url, expected, maximum) in FILES.items():
        target = args.directory / name
        if target.exists():
            raise SystemExit("Use a new directory; installer does not replace files")
        part = target.with_suffix(target.suffix + ".part")
        try:
            h = hashlib.sha256()
            count = 0
            with urllib.request.urlopen(url, timeout=120) as response, part.open("xb") as out:
                while chunk := response.read(1024 * 1024):
                    count += len(chunk)
                    if count > maximum:
                        raise RuntimeError("Download exceeds size limit")
                    h.update(chunk)
                    out.write(chunk)
            if h.hexdigest() != expected:
                raise RuntimeError("Model checksum mismatch")
            part.rename(target)
            print(name, count, expected)
        finally:
            part.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
