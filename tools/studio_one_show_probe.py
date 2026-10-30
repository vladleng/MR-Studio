#!/usr/bin/env python3
"""Read-only, value-redacted schema probe for Studio One / Studio Pro .show files.

This is a compatibility investigation tool, NOT an importer. It never extracts
files or uploads project content. Only XML element paths, attribute NAMES and
archive entry types are reported; XML text/attribute values are never emitted.
"""
from __future__ import annotations

import argparse
import io
import json
from pathlib import Path, PurePosixPath
import sys
import xml.etree.ElementTree as ET
import zipfile

MAX_ARCHIVE_BYTES = 512 * 1024 * 1024
MAX_MEMBERS = 4096
MAX_XML_MEMBERS = 32
MAX_XML_BYTES = 8 * 1024 * 1024
MAX_TOTAL_UNCOMPRESSED = 1024 * 1024 * 1024
MAX_XML_NODES = 100_000
MAX_XML_DEPTH = 48
MAX_SCHEMA_PATHS = 400


class ProbeError(ValueError):
    """Input is unsupported, unsafe or exceeds bounded inspection limits."""


def _safe_member(name: str) -> bool:
    if not name or len(name) > 1024 or "\x00" in name or "\\" in name:
        return False
    path = PurePosixPath(name)
    return not path.is_absolute() and ":" not in path.parts[0] and all(
        part not in ("..", ".") for part in name.rstrip("/").split("/")
    )


def _xml_name(tag: str) -> str:
    return tag.split("}", 1)[-1] if tag.startswith("{") else tag


def _schema(xml_bytes: bytes) -> dict:
    # ElementTree does not fetch external entities; no file/network resolution.
    # The bounded input and bounded traversal also protect against oversized XML.
    paths: dict[str, dict] = {}
    path_stack: list[str] = []
    nodes = 0
    try:
        for event, elem in ET.iterparse(io.BytesIO(xml_bytes), events=("start", "end")):
            if event == "start":
                nodes += 1
                if nodes > MAX_XML_NODES or len(path_stack) >= MAX_XML_DEPTH:
                    raise ProbeError("XML exceeds complexity/depth limits")
                tag = _xml_name(elem.tag)
                path_stack.append(tag)
                path = "/".join(path_stack[:10])
                if path not in paths:
                    if len(paths) >= MAX_SCHEMA_PATHS:
                        raise ProbeError("XML exceeds schema path limit")
                    paths[path] = {"count": 0, "attributes": set()}
                paths[path]["count"] += 1
                paths[path]["attributes"].update(_xml_name(key) for key in elem.attrib)
            else:
                path_stack.pop()
                elem.clear()
    except ET.ParseError as exc:
        raise ProbeError("Invalid XML document") from exc
    return {
        "root": next(iter(paths)).split("/")[0] if paths else "",
        "nodes": nodes,
        "paths": [
            {"path": name, "count": entry["count"], "attributes": sorted(entry["attributes"])}
            for name, entry in sorted(paths.items())
        ],
    }


def inspect(path: Path) -> dict:
    if not path.is_file():
        raise ProbeError("Source file does not exist")
    if path.suffix.casefold() != ".show":
        raise ProbeError("Expected Studio One .show extension")
    if path.stat().st_size > MAX_ARCHIVE_BYTES:
        raise ProbeError("Archive exceeds 512 MiB inspection limit")
    if not zipfile.is_zipfile(path):
        raise ProbeError("Not a ZIP container; internal .show format must be established separately")

    report = {
        "tool": "mr-live-show-probe",
        "schema_version": 1,
        "status": "inspection-only",
        "archive_entries": 0,
        "xml_documents": [],
        "other_extensions": {},
        "warnings": [],
    }
    try:
        with zipfile.ZipFile(path, "r") as archive:
            members = archive.infolist()
            if len(members) > MAX_MEMBERS:
                raise ProbeError("Too many archive entries")
            report["archive_entries"] = len(members)
            total = 0
            xml_count = 0
            for item in members:
                if not _safe_member(item.filename):
                    raise ProbeError("Unsafe ZIP entry name")
                if item.flag_bits & 0x1:
                    raise ProbeError("Encrypted ZIP entries are unsupported")
                total += item.file_size
                if total > MAX_TOTAL_UNCOMPRESSED:
                    raise ProbeError("Archive declares excessive uncompressed data")
                if item.is_dir():
                    continue
                suffix = PurePosixPath(item.filename).suffix.casefold()
                if suffix != ".xml":
                    report["other_extensions"][suffix or "<none>"] = (
                        report["other_extensions"].get(suffix or "<none>", 0) + 1
                    )
                    continue
                xml_count += 1
                if xml_count > MAX_XML_MEMBERS:
                    raise ProbeError("Too many XML documents to inspect safely")
                # Never read media/plug-in binaries, even if they are present.
                if item.file_size > MAX_XML_BYTES:
                    report["warnings"].append("XML document omitted: exceeds 8 MiB")
                    continue
                with archive.open(item, "r") as stream:
                    raw = stream.read(MAX_XML_BYTES + 1)
                if len(raw) > MAX_XML_BYTES:
                    raise ProbeError("XML document expanded beyond its declared bound")
                report["xml_documents"].append({
                    "archive_path": item.filename,
                    "schema": _schema(raw),
                })
    except (zipfile.BadZipFile, OSError, RuntimeError) as exc:
        raise ProbeError("Cannot inspect ZIP member or corrupt archive") from exc
    if not report["xml_documents"]:
        report["warnings"].append(
            "No readable XML schema detected; actual Show format remains unverified"
        )
    report["other_extensions"] = dict(sorted(report["other_extensions"].items()))
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("show", type=Path, help="Path to original .show (read-only)")
    parser.add_argument("-o", "--output", type=Path, help="Optional local JSON report destination")
    arguments = parser.parse_args(argv)
    try:
        result = inspect(arguments.show)
        data = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if arguments.output:
            if arguments.output.resolve() == arguments.show.resolve():
                raise ProbeError("Cannot overwrite the source .show")
            arguments.output.write_text(data, encoding="utf-8")
            print(f"Local, value-redacted report written: {arguments.output}")
        else:
            sys.stdout.write(data)
        return 0
    except (ProbeError, OSError) as exc:
        print(f"MR Live probe: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
