"""Record successful checks against the actual checkout and workflow definition."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def _normalized_bytes(path):
    # Invariant to Git checkout line endings (core.autocrlf): identical workflow
    # definitions must hash identically on Windows (CRLF) and Linux (LF) runners,
    # otherwise the release preflight rejects valid gui/core evidence after a full
    # successful candidate build.
    return path.read_bytes().replace(b'\r\n', b'\n').replace(b'\r', b'\n')


def definition_hash(workflow):
    return hashlib.sha256(_normalized_bytes(Path('.github/workflows', workflow))
                          + _normalized_bytes(Path(__file__))).hexdigest()


def record(workflow, checks):
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    Path('ci-evidence.json').write_text(json.dumps(dict(
        schema=1, sha=sha, workflow=workflow, definition=definition_hash(workflow), checks=checks)))


if __name__ == '__main__':
    record(sys.argv[1], sys.argv[2:])
