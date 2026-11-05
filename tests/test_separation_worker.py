"""Offline contract tests; synthetic engine is injected only in this test module."""
import importlib.util
import io
import os
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

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

    def test_thread_override_requires_explicit_matching_cpu_opt_in(self):
        with patch.dict(os.environ, {}, clear=True):
            for threads in ("1", "2", "3", "4"):
                self.assertEqual(worker.requested_threads(dict(self.req, threads=threads)), int(threads))
            for threads in ("8", "16", "0", "17"):
                with self.assertRaises(worker.Failure):
                    worker.requested_threads(dict(self.req, threads=threads))
        for threads in ("1", "2", "4", "8", "16"):
            with patch.dict(os.environ, {"MRS_SEPARATION_BENCHMARK_THREADS": threads}):
                self.assertEqual(worker.requested_threads(dict(self.req, threads=threads)), int(threads))
                for changes in ({"threads": "3"}, {"device": "auto"}, {"device": "cuda"}):
                    with self.assertRaises(worker.Failure):
                        worker.requested_threads(dict(self.req, threads=threads) | changes)
        for override in ("0", "3", "32", "08", "garbage"):
            with patch.dict(os.environ, {"MRS_SEPARATION_BENCHMARK_THREADS": override}):
                with self.assertRaises(worker.Failure):
                    worker.requested_threads(self.req)

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

    def test_profiles_mono_rates_and_exact_outputs(self):
        for model, profile in worker.PROFILES.items():
            for rate, channels in ((44100, 1), (96000, 2)):
                with self.subTest(model=model, rate=rate, channels=channels):
                    sf.write(str(self.source), np.ones((rate // 2, channels), dtype=np.float32) * 2, rate, subtype="FLOAT")
                    req = dict(self.req, model=model, source_start="0", source_frames=str(rate // 10), source_sha256=worker.digest(self.source), project_rate="48000")
                    def engine(snapshot, out, *_args):
                        audio, sr = sf.read(str(snapshot), dtype="float32", always_2d=True)
                        self.assertEqual(audio.shape, (4410, 2))
                        for label in profile["native_stems"]:
                            sf.write(str(out / (label.lower() + ".wav")), audio, sr, subtype="FLOAT")
                        return "cpu"
                    work = self.root / (model + str(rate)); work.mkdir()
                    result = worker.execute(req, work, lambda *_a, **_k: None, lambda: None, engine)
                    self.assertEqual([o["stem"] for o in result["outputs"]], [s.lower() for s in profile.get("default_stems", profile["stems"])])
                    for output in result["outputs"]:
                        info = sf.info(str(work / "result" / output["path"]))
                        self.assertEqual((info.frames, info.samplerate, info.channels), (4800, 48000, 2))
                    self.assertEqual(result["source_channels"], channels)

    def test_unsupported_model_and_settings_fail_before_output(self):
        for settings, code in (({"model": "invented-6-stems"}, "unsupported_model"),
                               ({"model": "mdx-kara2-2-v1", "chunk": "small"}, "invalid_input"),
                               ({"chunk": "unbounded"}, "invalid_input")):
            req = dict(self.req, **settings)
            with self.assertRaises(worker.Failure) as error:
                worker.execute(req, self.root / "absent", lambda *_a, **_k: None, lambda: None, self.fake)
            self.assertEqual(error.exception.code, code)
            self.assertFalse((self.root / "absent").exists())

    def test_progress_counts_after_iteration_and_separates_passes(self):
        events = []
        reporter = worker.ChunkProgress(lambda phase, **kw: events.append((phase, kw)), lambda: None, 2)
        iterator = reporter.wrap(lambda values: values)([10, 20])
        self.assertEqual(next(iterator), 10)
        self.assertEqual(events[-1][1]["completed"], "0")
        self.assertEqual(list(iterator), [20])
        self.assertEqual(events[-1][1], {"completed": "2", "total": "2", "current_pass": "1", "passes": "2"})
        self.assertEqual(list(reporter.wrap(lambda values: values)([30])), [30])
        self.assertEqual(events[-1][1]["current_pass"], "2")
        self.assertEqual(events[-1][1]["completed"], "1")

    def test_ft_progress_covers_all_four_experts_for_one_kept_stem(self):
        profile = worker.PROFILES["official-htdemucs-ft-4-v1"]
        self.assertEqual(worker.inference_passes(profile), 4)
        events = []
        reporter = worker.ChunkProgress(lambda phase, **kw: events.append(kw), lambda: None,
                                        worker.inference_passes(profile))
        for _ in range(4):
            self.assertEqual(list(reporter.wrap(lambda values: values)([1, 2])), [1, 2])
        self.assertEqual(events[-1], {"completed": "2", "total": "2", "current_pass": "4", "passes": "4"})
        with self.assertRaises(worker.Failure):
            list(reporter.wrap(lambda values: values)([1]))
        for model in ("official-htdemucs-4-v1", "htdemucs-6s"):
            self.assertEqual(worker.inference_passes(worker.PROFILES[model]), 1)
        self.assertNotIn("mvsep-mega-53-v1", worker.PROFILES)

    def test_ft_bass_selection_publishes_only_bass(self):
        self.req.update(model="official-htdemucs-ft-4-v1", stems=["Bass"])
        result = self.run_job()
        self.assertEqual(result["selected_stems"], ["Bass"])
        self.assertEqual([o["stem"] for o in result["outputs"]], ["bass"])
        self.assertEqual(sorted(p.name for p in (self.root / "job/result").iterdir()),
                         ["bass.wav", "manifest.json"])

    def test_progress_cancellation_never_counts_unfinished_chunk(self):
        calls = 0
        def check():
            nonlocal calls
            calls += 1
            if calls == 2:
                raise worker.Failure("cancelled")
        events = []
        reporter = worker.ChunkProgress(lambda _phase, **kw: events.append(kw), check, 1)
        iterator = reporter.wrap(lambda values: values)([10])
        self.assertEqual(next(iterator), 10)
        with self.assertRaises(worker.Failure):
            next(iterator)
        self.assertEqual(events[-1]["completed"], "0")

    def test_selected_output_only_and_manifest(self):
        self.req["stems"] = ["Vocals"]
        result = self.run_job()
        self.assertEqual([f["stem"] for f in result["outputs"]], ["vocals"])
        self.assertEqual(result["selected_stems"], ["Vocals"])
        self.assertEqual(result["available_stems"], ["Vocals", "Instrumental"])
        self.assertFalse((self.root / "job/result/instrumental.wav").exists())

    def test_empty_unknown_duplicate_or_unbounded_selection_rejected(self):
        for selected in ([], ["Bass"], ["Vocals", "Vocals"], "Vocals", ["Vocals"] * 54):
            with self.subTest(selection=selected), self.assertRaises(worker.Failure) as error:
                worker.execute(dict(self.req, stems=selected), self.root / "absent", lambda *_a, **_k: None, lambda: None, self.fake)
            self.assertEqual(error.exception.code, "invalid_stems")
        self.assertFalse((self.root / "absent").exists())

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

    def test_cancel_poller_handles_partial_frames(self):
        read_fd, write_fd = os.pipe()
        self.addCleanup(os.close, write_fd)
        stream = os.fdopen(read_fd, "rb", buffering=0)
        self.addCleanup(stream.close)
        finished = threading.Event()
        result = []
        thread = threading.Thread(target=lambda: result.append(worker.poll_frame(stream, finished)))
        thread.start()
        frame = io.BytesIO()
        worker.write_frame(frame, {"type": "cancel", "job_id": "job"})
        data = frame.getvalue()
        os.write(write_fd, data[:2])
        time.sleep(0.05)
        os.write(write_fd, data[2:7])
        time.sleep(0.05)
        os.write(write_fd, data[7:])
        thread.join(timeout=2)
        finished.set()
        thread.join(timeout=2)
        self.assertEqual(result, [{"type": "cancel", "job_id": "job"}])

    def test_cancel_poller_stops_without_input(self):
        read_fd, write_fd = os.pipe()
        self.addCleanup(os.close, write_fd)
        stream = os.fdopen(read_fd, "rb", buffering=0)
        self.addCleanup(stream.close)
        finished = threading.Event()
        result = []
        thread = threading.Thread(target=lambda: result.append(worker.poll_frame(stream, finished)))
        thread.start()
        finished.set()
        thread.join(timeout=2)
        self.assertEqual(result, [None])

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
