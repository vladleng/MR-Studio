"""Offline contract tests; synthetic engine is injected only in this test module."""
import importlib.util
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

import numpy as np
import soundfile as sf

SPEC = importlib.util.spec_from_file_location("mr_worker", Path(__file__).parents[1] / "tools/separation/worker.py")
worker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(worker)


class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="mr-separation-test-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / "исходник test.wav"
        self.data = np.ones((10000, 2), dtype=np.float32) * 2.0
        sf.write(str(self.source), self.data, 48000, subtype="FLOAT")
        self.req = {"version": 1, "type": "submit", "job_id": "job", "session_id": "session",
                    "source_revision": "rev1", "source_path": str(self.source), "source_sha256": worker.digest(self.source),
                    "source_start": "100", "source_frames": "4800", "timeline_start": "98765",
                    "project_rate": "48000", "threads": "1", "memory_bytes": str(1024**3),
                    "device": "cpu", "model_dir": str(self.root)}

    def fake(self, snapshot, out, _model, device, threads, emit, check):
        data, rate = sf.read(str(snapshot), dtype="float32", always_2d=True)
        self.assertEqual(rate, 44100)
        self.assertEqual(data.shape, (4410, 2))
        self.assertEqual(device, "cpu")
        self.assertEqual(threads, 1)
        check()
        for stem, factor in (("vocals", 1), ("bass", 2), ("drums", 3), ("other", 4)):
            sf.write(str(out / (stem + ".wav")), data * factor, rate, subtype="FLOAT")
        return "cpu"

    def run_job(self, engine=None):
        work = self.root / "job"
        work.mkdir()
        return worker.execute(self.req, work, lambda *_args, **_kw: None, lambda: None, engine or self.fake)

    def test_roundtrip_geometry_gain_manifest(self):
        source_hash = worker.digest(self.source)
        manifest = self.run_job()
        self.assertEqual(manifest["timeline_start"], "98765")
        self.assertEqual(manifest["source_start"], "100")
        self.assertEqual(manifest["source_frames"], "4800")
        self.assertEqual(manifest["effective_device"], "cpu")
        self.assertEqual(manifest["derivation"], "instrumental=bass+drums+other")
        for output in manifest["outputs"]:
            path = self.root / "job/result" / output["path"]
            self.assertEqual(output["sha256"], worker.digest(path))
            info = sf.info(str(path))
            self.assertEqual((info.frames, info.channels, info.samplerate, info.subtype), (4800, 2, 48000, "FLOAT"))
        vocals, _ = sf.read(str(self.root / "job/result/vocals.wav"))
        instrumental, _ = sf.read(str(self.root / "job/result/instrumental.wav"))
        self.assertGreater(float(vocals[100:-100].max()), 1.9)
        np.testing.assert_allclose(instrumental, vocals * 9, atol=0.0001)
        self.assertEqual(worker.digest(self.source), source_hash)
        self.assertFalse((self.root / "job/result.part").exists())
        self.assertEqual(json.loads((self.root / "job/result/manifest.json").read_text()), manifest)

    def test_source_revision_hash_and_bounds(self):
        self.req["source_sha256"] = "0" * 64
        with self.assertRaisesRegex(worker.Failure, "") as error:
            self.run_job()
        self.assertEqual(error.exception.code, "stale_source")

    def test_cancel_before_import_or_output(self):
        work = self.root / "job"
        work.mkdir()
        def cancel():
            raise worker.Failure("cancelled")
        with self.assertRaises(worker.Failure) as error:
            worker.execute(self.req, work, lambda *_: None, cancel, self.fake)
        self.assertEqual(error.exception.code, "cancelled")
        self.assertEqual(list(work.iterdir()), [])

    def test_bad_native_geometry_never_publishes(self):
        def wrong(*args):
            device = self.fake(*args)
            sf.write(str(args[1] / "vocals.wav"), np.zeros((10, 2)), 44100, subtype="FLOAT")
            return device
        with self.assertRaises(worker.Failure) as error:
            self.run_job(wrong)
        self.assertEqual(error.exception.code, "invalid_output")
        self.assertFalse((self.root / "job/result").exists())

    def test_missing_model_without_fetch(self):
        with self.assertRaises(worker.Failure) as error:
            self.run_job(worker.infer_htdemucs)
        self.assertEqual(error.exception.code, "model_missing")

    def test_out_of_bounds(self):
        self.req["source_start"] = "9900"
        with self.assertRaises(worker.Failure) as error:
            self.run_job()
        self.assertEqual(error.exception.code, "invalid_input")

    def test_resource_preflight(self):
        self.req["memory_bytes"] = "1"
        with self.assertRaises(worker.Failure) as error:
            self.run_job()
        self.assertEqual(error.exception.code, "invalid_input")

    def test_frame_limits_and_types(self):
        for raw in (struct.pack("<I", 65537), struct.pack("<I", 1) + b"[", struct.pack("<I", 2) + b"[]", b"\x02"):
            with self.assertRaises(worker.Failure):
                worker.read_frame(io.BytesIO(raw))
        out = io.BytesIO()
        worker.write_frame(out, {"version": 1, "job_id": "тест"})
        self.assertEqual(worker.read_frame(io.BytesIO(out.getvalue()))["job_id"], "тест")

    def test_counter_rejects_noncanonical_values(self):
        for value in (1, "-1", "01", "1.0", "١", "9" * 21):
            with self.assertRaises(worker.Failure):
                worker.counter(value)

    def test_network_guard_denies_before_connect(self):
        script = "import runpy,socket; w=runpy.run_path(" + repr(str(Path(worker.__file__))) + "); w['block_network'](); socket.create_connection(('127.0.0.1', 1))"
        result = subprocess.run([sys.executable, "-I", "-c", script], capture_output=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"network_disabled", result.stderr)


if __name__ == "__main__":
    unittest.main()
