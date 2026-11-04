"""Developer-only explicit fetch of the accepted SW conversion inputs."""
import argparse
import json
from pathlib import Path

from fetch_baseline import fetch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    profiles = json.loads((Path(__file__).parents[1] / "mr_separation_profiles.json").read_text())
    profile = next(p for p in profiles if p["id"] == "bs-roformer-sw-6-v1")
    for entry in profile["files"]:
        fetch(args.root, entry)


if __name__ == "__main__":
    main()
