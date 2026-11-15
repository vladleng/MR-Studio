"""The same Git text must produce the same evidence on Windows and Linux."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import ci_evidence


class EvidenceLineEndings(unittest.TestCase):
    def test_checkout_line_endings_do_not_change_definition(self):
        previous = Path.cwd()
        with tempfile.TemporaryDirectory() as folder:
            try:
                os.chdir(folder)
                workflow = Path('.github/workflows/probe.yml')
                workflow.parent.mkdir(parents=True)
                recorder = Path(folder, 'recorder.py')
                with patch.object(ci_evidence, '__file__', str(recorder)):
                    workflow.write_bytes(b'name: probe\njobs: {}\n')
                    recorder.write_bytes(b'# recorder\n')
                    linux = ci_evidence.definition_hash('probe.yml')
                    workflow.write_bytes(b'name: probe\r\njobs: {}\r\n')
                    recorder.write_bytes(b'# recorder\r\n')
                    self.assertEqual(linux, ci_evidence.definition_hash('probe.yml'),
                                     'Identical Git text must survive Windows CRLF checkout')
                    workflow.write_bytes(b'name: changed\r\njobs: {}\r\n')
                    self.assertNotEqual(linux, ci_evidence.definition_hash('probe.yml'))
            finally:
                os.chdir(previous)


if __name__ == '__main__':
    unittest.main()
