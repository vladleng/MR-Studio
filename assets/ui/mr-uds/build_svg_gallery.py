"""Build the complete offline SVG gallery for MR Studio.

Pure stdlib; no network and no modification to vector resources.
Run: python assets/ui/mr-uds/build_svg_gallery.py

Includes all MR UDS SVGs, transport SVGs, SVG previews and docs/design SVGs
from the checked-out repository. The browser never fetches JSON over file:///.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
TEMPLATE = ROOT / "previews" / "concept_pack.template.html"
OUTPUT = ROOT / "previews" / "concept_pack.html"


def build_gallery() -> int:
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    sizes = {"assets/ui/mr-uds/" + asset["file"]: asset.get("size", [0, 0])
             for asset in manifest["assets"]}
    candidates = []
    for folder in (ROOT / "svg", ROOT / "previews",
                   REPO / "assets" / "ui" / "icons" / "transport",
                   REPO / "docs" / "design"):
        if folder.exists():
            candidates.extend(folder.rglob("*.svg"))
    unique = sorted(set(candidates), key=lambda p: p.relative_to(REPO).as_posix().lower())
    rows = []
    for path in unique:
        repo_path = path.relative_to(REPO).as_posix()
        link = Path(os.path.relpath(path, OUTPUT.parent)).as_posix()
        size = sizes.get(repo_path, [0, 0])
        rows.append([link, size[0], size[1]])
    template = TEMPLATE.read_text(encoding="utf-8")
    token = "/*__ASSET_ROWS__*/"
    if template.count(token) != 1:
        raise ValueError("Expected exactly one asset list placeholder")
    payload = json.dumps(rows, ensure_ascii=False, separators=(",", ":"))
    OUTPUT.write_text(template.replace(token, payload), encoding="utf-8")
    print(f"MR Studio SVG gallery: {len(rows)} entries -> {OUTPUT}")
    return len(rows)


if __name__ == "__main__":
    build_gallery()
