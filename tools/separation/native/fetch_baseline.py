"""Developer/CI-only fetch of the pinned native spike; never shipped to users."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import tarfile
import tempfile
import urllib.request
import zipfile


def valid(path, item):
    if not path.is_file() or path.stat().st_size != item["bytes"]:
        return False
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() == item["sha256"]


def fetch(root, item):
    dest = root / item.get("name", Path(item.get("path", "")).name)
    if dest.exists():
        if not valid(dest, item):
            raise RuntimeError("Existing cache failed integrity: " + dest.name)
        print("Verified cache:", dest.name, flush=True)
        return dest
    with tempfile.NamedTemporaryFile(dir=root, prefix=".part-", delete=False) as out:
        partial = Path(out.name)
        try:
            with urllib.request.urlopen(item["url"], timeout=60) as response:
                if not response.geturl().startswith("https://"):
                    raise RuntimeError("HTTPS required")
                count = 0
                while block := response.read(1024 * 1024):
                    count += len(block)
                    if count > item["bytes"]:
                        raise RuntimeError("Download byte limit")
                    out.write(block)
            out.close()
            if not valid(partial, item):
                raise RuntimeError("Download integrity: " + dest.name)
            partial.rename(dest)
        finally:
            partial.unlink(missing_ok=True)
    print("Verified download:", dest.name, flush=True)
    return dest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    lock = json.loads(Path(__file__).with_name("baseline-lock.json").read_text())
    target = "windows" if platform.system() == "Windows" else "ubuntu"
    sdk = None
    for item in lock["artifacts"]:
        if "-" + target + "-" not in item["name"]:
            continue
        archive = fetch(args.root, item)
        folder = args.root / ("sdk" if "-lib-" in item["name"] else "cli")
        folder.mkdir(exist_ok=True)
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as package:
                for member in package.infolist():
                    path = (folder / member.filename).resolve()
                    if not path.is_relative_to(folder.resolve()):
                        raise RuntimeError("Unsafe archive path")
                package.extractall(folder)
        else:
            with tarfile.open(archive) as package:
                package.extractall(folder, filter="data")
        if "-lib-" in item["name"]:
            sdk = folder
    model = fetch(args.root, lock["model"])
    print(json.dumps({"sdk_root": str(sdk), "model": str(model),
                      "upstream_commit": lock["upstream"]["commit"]}), flush=True)


if __name__ == "__main__":
    main()
