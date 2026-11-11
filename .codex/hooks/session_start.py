#!/usr/bin/env python3
"""Provide a small, read-only startup note to Codex (no network, no builds)."""

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CONTEXT = ROOT / "docs" / "SESSION_CONTEXT.md"
MAX_CHARS = 4500


def current_branch() -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(ROOT), "branch", "--show-current"],
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
        )
        return result.stdout.strip() if result.returncode == 0 else ""
    except (OSError, subprocess.TimeoutExpired):
        return ""


def main() -> int:
    try:
        note = CONTEXT.read_text(encoding="utf-8").strip()
    except OSError:
        return 0  # Missing context must not block a Codex session.

    if not note:
        return 0
    if len(note) > MAX_CHARS:
        note = note[:MAX_CHARS].rsplit("\n", 1)[0]
        note += "\n[Context truncated: see docs/SESSION_CONTEXT.md]"

    branch = current_branch()
    print("MR Studio | session quick context (not a status report)")
    if branch:
        print(f"Current checkout branch: {branch}")
    print(note)
    return 0


if __name__ == "__main__":
    sys.exit(main())
