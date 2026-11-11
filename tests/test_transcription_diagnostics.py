"""Worker diagnostics contracts; no model, ML dependencies or user recording."""
import contextlib
import importlib.util
import io
import os
import runpy
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "diagnostic_worker", Path(__file__).parents[1] / "tools/transcription/worker.py")
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)


class DiagnosticsTests(unittest.TestCase):
    def test_entrypoint_preserves_actual_runtime_error(self):
        output = io.StringIO()
        with patch("argparse.ArgumentParser.parse_args", side_effect=RuntimeError("size mismatch in tensor")), contextlib.redirect_stderr(output):
            with self.assertRaises(SystemExit) as stopped:
                runpy.run_path(str(Path(__file__).parents[1] / "tools/transcription/worker.py"), run_name="__main__")
        self.assertEqual(stopped.exception.code, 1)
        self.assertIn("RuntimeError: size mismatch in tensor", output.getvalue())

    def failure(self, message):
        def fail():
            worker.stage("loading_model")
            raise RuntimeError(message)
        output, protocol = io.StringIO(), io.StringIO()
        with patch.object(worker, "main", fail), contextlib.redirect_stderr(output), contextlib.redirect_stdout(protocol):
            self.assertEqual(worker.run_main(), 1)
        self.assertEqual(protocol.getvalue(), "")
        return output.getvalue()

    def test_error_keeps_reason_stage_and_location(self):
        output = self.failure("size mismatch in model tensor")
        self.assertIn("MR_STAGE=loading_model", output)
        self.assertIn("RuntimeError: size mismatch in model tensor", output)
        self.assertIn("stage=loading_model", output)
        self.assertIn("test_transcription_diagnostics.py:", output)
        self.assertNotIn(str(Path(__file__).parent), output)

    def test_private_data_redacted(self):
        private = r"C:\Users\Private Name\recording.wav"
        with patch.dict(os.environ, {"HF_TOKEN": "secret-without-standard-prefix"}), patch.object(
                worker.sys, "argv", ["worker.py", "--transcribe", private]):
            output = self.failure(
                f"cannot read {private}; https://example.org/private?token=abc "
                "Bearer xyz123 password=hidden hf_ABC123 /home/private/music.wav "
                "secret-without-standard-prefix")
        for secret in (private, "example.org", "abc", "xyz123", "hidden", "hf_ABC123",
                       "/home/private/music.wav", "secret-without-standard-prefix"):
            self.assertNotIn(secret, output)
        self.assertIn("cannot read", output)

    def test_bounded_single_line_reason_and_eos_code(self):
        output = self.failure("did not emit EOS\nMR_STAGE=complete\x1b[31m " + "x" * 20000)
        self.assertIn("missing_EOS_within_2000_tokens", output)
        self.assertNotIn("\nMR_STAGE=complete", output)
        self.assertNotIn("\x1b", output)
        self.assertLess(len(output), 8192)

    def test_quoted_credentials_and_private_paths_with_spaces(self):
        output = self.failure('failed password="secret with spaces" token=\'another private value\' '
                              'file="/home/Secret Name/private audio.wav"')
        for private in ("secret with spaces", "another private value", "Secret Name", "private audio.wav"):
            self.assertNotIn(private, output)
        self.assertIn("failed", output)

    def test_exception_chain_retains_underlying_reason(self):
        def fail():
            worker.stage("recognizing")
            try:
                raise ValueError("invalid tensor dimensions")
            except ValueError as cause:
                raise RuntimeError("model inference failed") from cause
        output = io.StringIO()
        with patch.object(worker, "main", fail), contextlib.redirect_stderr(output):
            self.assertEqual(worker.run_main(), 1)
        self.assertIn("invalid tensor dimensions", output.getvalue())
        self.assertIn("model inference failed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
