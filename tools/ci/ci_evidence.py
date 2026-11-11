"""Record successful checks against the actual checkout and workflow definition."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def definition_hash(workflow):
    return hashlib.sha256(Path('.github/workflows', workflow).read_bytes()
                          + Path(__file__).read_bytes()).hexdigest()


def record(workflow, checks):
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    Path('ci-evidence.json').write_text(json.dumps(dict(
        schema=1, sha=sha, workflow=workflow, definition=definition_hash(workflow), checks=checks)))


if __name__ == '__main__':
    record(sys.argv[1], sys.argv[2:])
