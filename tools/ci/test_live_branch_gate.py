"""Legacy Live branch must not schedule targets absent from its product tree."""
import unittest
from pathlib import Path
class LiveBranchTests(unittest.TestCase):
    def test_core_targets_exist(self):
        text=Path('.github/workflows/core-contracts.yml').read_text()
        self.assertNotIn('mrs_python_transcription_tests', text)
        self.assertIn('mrs_live_preload_tests', text)
        self.assertIn('mrs_audio_tests', text)
    def test_live_scope_only(self):
        text=Path('.github/workflows/audio-engine.yml').read_text()
        self.assertIn('options: [live]', text)
        self.assertIn('supports Live-only acceptance', text)
    def test_absent_runtime_is_not_scheduled(self):
        self.assertFalse(Path('.github/workflows/transcription-runtime.yml').exists())
        self.assertFalse(Path('.github/workflows/transcription-contracts.yml').exists())
