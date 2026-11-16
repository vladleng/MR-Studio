"""Regression checks for CI component routing (no network or build)."""
import ast
from fnmatch import fnmatchcase
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


def changed_paths(workflow, event):
    """Read one inline YAML paths list without a PyYAML runtime dependency."""
    lines = (WORKFLOWS / workflow).read_text(encoding="utf-8").splitlines()
    event_line = f"  {event}:"
    start = lines.index(event_line)
    for line in lines[start + 1:]:
        if line and not line.startswith(" "):
            break
        if line.startswith("    paths: "):
            return ast.literal_eval(line.removeprefix("    paths: "))
    raise AssertionError(f"{workflow}: missing {event} paths filter")


class LiveCiRoutingTests(unittest.TestCase):
    def test_live_and_shared_audio_changes_do_not_dispatch_transcription(self):
        unrelated = (
            "apps/studio-juce/src/LiveInterface.cpp",
            "core/audio/src/engine.cpp",
            "core/audio/include/mrs/audio.hpp",
            "CMakeLists.txt",
            "tests/audio_tests.cpp",
            "cmake/Stretch.cmake",
        )
        for workflow in ("transcription-contracts.yml", "transcription-runtime.yml"):
            for event in ("push", "pull_request"):
                with self.subTest(workflow=workflow, event=event):
                    patterns = changed_paths(workflow, event)
                    self.assertFalse(
                        [path for path in unrelated
                         if any(fnmatchcase(path, pattern) for pattern in patterns)],
                        f"{workflow} is over-triggered by MR Live/shared audio changes",
                    )

    def test_transcription_changes_keep_component_checks(self):
        direct = {
            "transcription-contracts.yml": (
                "apps/studio-desktop/src/transcription_controller.cpp",
                "tools/transcription/worker.py",
                "cmake/TranscriptionProfile.cmake",
                "tests/python_transcription_tests.cpp",
            ),
            "transcription-runtime.yml": (
                "apps/studio-juce/src/TranscriptionRuntime.cpp",
                "tools/transcription/requirements-cpu.txt",
                "cmake/TranscriptionProfile.cmake",
                "tools/transcription_runtime_check.cpp",
            ),
        }
        for workflow, files in direct.items():
            for event in ("push", "pull_request"):
                patterns = changed_paths(workflow, event)
                for path in files:
                    with self.subTest(workflow=workflow, event=event, path=path):
                        self.assertTrue(any(fnmatchcase(path, pattern)
                                            for pattern in patterns))

    def test_fast_core_does_not_duplicate_transcription_contracts(self):
        core = (WORKFLOWS / "core-contracts.yml").read_text(encoding="utf-8")
        for target in (
            "mrs_audio_to_midi_worker_tests",
            "mrs_python_transcription_tests",
            "mrs_transcription_controller_tests",
            "mrs_separation_assets_tests",
            "desktop_transcription_controller",
            "desktop_audio_to_midi_worker",
            "desktop_python_transcription",
            "desktop_separation_assets",
        ):
            with self.subTest(target=target):
                self.assertNotIn(target, core)


if __name__ == "__main__":
    unittest.main()
