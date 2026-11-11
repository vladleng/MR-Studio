"""Unit tests for the release preflight gate (stdlib only, no network)."""

import unittest
from verify_release_gate import GateError, verify, gui_affecting_path

SHA = "a" * 40


def run(name, id=1, sha=SHA, status="completed", conclusion="success", version=1):
    return {
        "path": f".github/workflows/{name}@refs/heads/feature",
        "id": id,
        "head_sha": sha,
        "status": status,
        "conclusion": conclusion,
        "run_number": version,
        "run_attempt": 1,
    }


def core_jobs(ok=True):
    return [{"conclusion": "success", "steps": [{"name": "Core contract test suite", "conclusion": "success" if ok else "skipped"}]}]


def jobs_for_run(run_id):
    if run_id == 2:
        return [{"conclusion": "success", "steps": [{"name": "Compile JUCE GUI sources using actual MSVC /WX flags", "conclusion": "success"}]}]
    return core_jobs()


class VerifyReleaseGateTest(unittest.TestCase):
    def test_core_pass(self):
        self.assertEqual(len(verify(SHA, "studio", [run("core-contracts.yml")], lambda _: core_jobs())), 1)

    def test_missing_core_blocks(self):
        with self.assertRaisesRegex(GateError, "Missing core-contracts.yml"):
            verify(SHA, "studio", [], lambda _: core_jobs())

    def test_other_sha_cannot_qualify(self):
        with self.assertRaises(GateError):
            verify(SHA, "studio", [run("core-contracts.yml", sha="b"*40)], lambda _: core_jobs())

    def test_pending_blocks(self):
        with self.assertRaisesRegex(GateError, "in_progress"):
            verify(SHA, "studio", [run("core-contracts.yml", status="in_progress", conclusion=None)], lambda _: core_jobs())

    def test_failed_blocks(self):
        with self.assertRaisesRegex(GateError, "failure"):
            verify(SHA, "studio", [run("core-contracts.yml", conclusion="failure")], lambda _: core_jobs())

    def test_skipped_core_test_blocks(self):
        with self.assertRaisesRegex(GateError, "without executing"):
            verify(SHA, "studio", [run("core-contracts.yml")], lambda _: core_jobs(False))

    def test_optional_windows_contract_failure_blocks(self):
        runs = [run("core-contracts.yml"), run("transcription-runtime.yml", 3, conclusion="failure")]
        with self.assertRaisesRegex(GateError, "transcription-runtime.yml"):
            verify(SHA, "studio", runs, lambda _: core_jobs())

    def test_optional_windows_contract_success(self):
        runs = [run("core-contracts.yml"), run("transcription-runtime.yml", 3)]
        self.assertEqual(len(verify(SHA, "studio", runs, lambda _: core_jobs())), 2)

    def test_release_scoped_separation_requires_contract(self):
        with self.assertRaisesRegex(GateError, "Missing separation.yml"):
            verify(SHA, "separation", [run("core-contracts.yml")], lambda _: core_jobs())

    def test_separation_contract_pass(self):
        runs = [run("core-contracts.yml"), run("separation.yml", 3)]
        self.assertEqual(len(verify(SHA, "separation-ui", runs, lambda _: core_jobs())), 2)

    def test_latest_failed_run_blocks(self):
        runs = [run("core-contracts.yml", version=1), run("core-contracts.yml", version=2, id=2, conclusion="failure")]
        with self.assertRaises(GateError):
            verify(SHA, "studio", runs, lambda _: core_jobs())

    def test_release_workflow_does_not_block_on_itself(self):
        runs = [run("core-contracts.yml"), run("audio-engine.yml", conclusion="failure")]
        self.assertEqual(len(verify(SHA, "studio", runs, lambda _: core_jobs())), 1)

    def test_new_component_contract_blocks_if_failed(self):
        runs = [run("core-contracts.yml"), run("transcription-contracts.yml", conclusion="failure")]
        with self.assertRaisesRegex(GateError, "transcription-contracts.yml"):
            verify(SHA, "studio", runs, lambda _: core_jobs())

    def test_new_component_contract_pass_required(self):
        runs = [run("core-contracts.yml"), run("transcription-contracts.yml")]
        self.assertEqual(len(verify(SHA, "studio", runs, lambda _: core_jobs())), 2)

    def test_gui_path_detection(self):
        for path in (
            "apps/studio-juce/src/TranscriptionView.cpp", "apps/studio-juce/src/Desktop.h",
            "apps/studio-desktop/include/mrs/desktop.hpp", "assets/ui/mr-uds/icons/guitar.svg",
            "cmake/Juce.cmake", "CMakeLists.txt", "tools/ci/sdk-lock.json",
            ".github/workflows/windows-juce-gui-compile.yml",
        ):
            with self.subTest(path=path):
                self.assertTrue(gui_affecting_path(path))
        for path in ("docs/README.md", "core/src/ai.cpp", "tools/transcription/worker.py"):
            with self.subTest(path=path):
                self.assertFalse(gui_affecting_path(path))

    def test_missing_gui_compile_blocks(self):
        with self.assertRaisesRegex(GateError, "Missing windows-juce-gui-compile.yml"):
            verify(SHA, "studio", [run("core-contracts.yml")], jobs_for_run, gui_required=True)

    def test_gui_compile_success_allows_release(self):
        runs = [run("core-contracts.yml"), run("windows-juce-gui-compile.yml", id=2)]
        self.assertEqual(len(verify(SHA, "studio", runs, jobs_for_run, gui_required=True)), 2)

    def test_gui_compile_failure_blocks(self):
        runs = [run("core-contracts.yml"), run("windows-juce-gui-compile.yml", id=2, conclusion="failure")]
        with self.assertRaisesRegex(GateError, "windows-juce-gui-compile.yml"):
            verify(SHA, "studio", runs, jobs_for_run, gui_required=True)

    def test_gui_compile_without_actual_compile_step_blocks(self):
        runs = [run("core-contracts.yml"), run("windows-juce-gui-compile.yml", id=3)]
        with self.assertRaisesRegex(GateError, "without executing"):
            verify(SHA, "profile", runs, jobs_for_run, gui_required=True)

    def test_gui_compile_wrong_sha_blocks(self):
        runs = [run("core-contracts.yml"), run("windows-juce-gui-compile.yml", id=2, sha="b" * 40)]
        with self.assertRaisesRegex(GateError, "Missing windows-juce-gui-compile.yml"):
            verify(SHA, "studio", runs, jobs_for_run, gui_required=True)

    def test_no_gui_changes_does_not_require_gui_workflow(self):
        self.assertEqual(len(verify(SHA, "studio", [run("core-contracts.yml")], jobs_for_run, gui_required=False)), 1)

    def test_bad_sha_blocks(self):
        with self.assertRaisesRegex(GateError, "Invalid commit SHA"):
            verify("bad", "studio", [run("core-contracts.yml")], lambda _: core_jobs())


if __name__ == "__main__":
    unittest.main()
