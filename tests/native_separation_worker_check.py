"""Real native worker IPC regression. Developer/CI Python only, no Python inference."""
import argparse
import array
import hashlib
import json
import math
import os
from unittest.mock import patch
from pathlib import Path
import queue
import struct
import subprocess
import threading
import time
import uuid
import wave


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def exact(stream, size):
    data = bytearray()
    while len(data) < size:
        chunk = stream.read(size-len(data))
        if not chunk:
            raise EOFError("Worker pipe closed")
        data.extend(chunk)
    return bytes(data)


class Client:
    def __init__(self, executable, work):
        self.process = subprocess.Popen([str(executable)], cwd=work, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.events, self.log = queue.Queue(), bytearray()
        self.sequence = 0

        def reader():
            try:
                while True:
                    size, = struct.unpack("<I", exact(self.process.stdout, 4))
                    if not 0 < size <= 65536:
                        raise AssertionError("Unframed native-library stdout or oversized message")
                    event = json.loads(exact(self.process.stdout, size).decode("utf-8"))
                    self.events.put(event)
            except Exception as error:
                self.events.put(error)

        def diagnostic():
            while chunk := self.process.stderr.read(4096):
                self.log.extend(chunk)
                del self.log[:-262144]

        self.reader = threading.Thread(target=reader, daemon=True)
        self.diagnostic = threading.Thread(target=diagnostic, daemon=True)
        self.reader.start()
        self.diagnostic.start()
        hello = self.next(10)
        assert hello["type"] == "hello"

    def next(self, timeout=30):
        event = self.events.get(timeout=timeout)
        if isinstance(event, Exception):
            raise AssertionError(f"Native wire failed: {event}; {self.log.decode(errors='replace')}")
        assert event["version"] == 1 and isinstance(event["sequence"], str)
        number = int(event["sequence"])
        assert number > self.sequence
        self.sequence = number
        return event

    def send(self, obj, fragmented=False):
        body = json.dumps(obj, separators=(",", ":"), allow_nan=False).encode()
        frame = struct.pack("<I", len(body)) + body
        if fragmented:
            self.process.stdin.write(frame[:2])
            self.process.stdin.flush()
            time.sleep(0.1)
            frame = frame[2:]
        self.process.stdin.write(frame)
        self.process.stdin.flush()

    def finish(self):
        code = self.process.wait(timeout=10)
        self.process.stdin.close()
        self.reader.join(timeout=2)
        self.diagnostic.join(timeout=2)
        self.process.stdout.close()
        self.process.stderr.close()
        return code

    def kill(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)


def fixture(path, rate=44100, channels=1, bits=16, floating=False):
    # Mono PCM16 ensures the existing Core WAV decoder, range and mono policy
    # are exercised, rather than only the probe's IEEE Float32 fixture reader.
    samples = bytearray()
    for frame in range(rate*2):
        t = frame / rate
        value = 0.2 * math.exp(-8*(t % 0.5)) * math.sin(2*math.pi*110*t)
        for channel in range(channels):
            sample = value if channel == 0 else value*0.9 + 0.015*math.sin(2*math.pi*440*t)
            if floating:
                samples.extend(struct.pack("<f", sample))
            else:
                integer = round(sample*((1 << (bits-1))-1))
                samples.extend(integer.to_bytes(bits//8, "little", signed=True))
    if floating:
        path.write_bytes(struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", len(samples)+36, b"WAVE", b"fmt ", 16,
                                     3, channels, rate, rate*channels*4, channels*4, 32, b"data", len(samples))+samples)
        return
    with wave.open(str(path), "wb") as stream:
        stream.setparams((channels, bits//8, rate, 0, "NONE", "not compressed"))
        stream.writeframes(samples)


def float_wav(path, rate=44100):
    data = path.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    assert struct.unpack_from("<I", data, 4)[0] + 8 == len(data)
    assert data[12:16] == b"fmt " and struct.unpack_from("<HHI", data, 20) == (3, 2, rate)
    assert data[36:40] == b"data" and struct.unpack_from("<I", data, 40)[0] == len(data)-44
    samples = array.array("f")
    samples.frombytes(data[44:])
    import sys
    if sys.byteorder != "little":
        samples.byteswap()
    assert all(math.isfinite(value) for value in samples)
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("worker", type=Path)
    parser.add_argument("ht6_dir", type=Path)
    parser.add_argument("sw_dir", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.worker = args.worker.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    source = (args.output / "контрабас source.wav").resolve()
    fixture(source)
    original_hash = sha(source)
    results = []
    base = {"version": 1, "type": "submit", "session_id": "native-regression", "source_revision": "1",
            "source_path": str(source), "source_sha256": original_hash, "source_start": "22050",
            "source_frames": "44100", "timeline_start": "12345", "project_rate": "48000",
            "device": "cpu", "memory_bytes": str(4*1024**3), "timeout_ms": "120000", "threads": "2",
            "model": "htdemucs-6s", "model_dir": str(args.ht6_dir.resolve()), "stems": ["Bass"], "chunk": "default"}

    def run(name, changes=None, expected=None, cancel=False, benchmark=""):
        work = args.output / ("job-" + name)
        work.mkdir()
        with patch.dict(os.environ, {"MRS_SEPARATION_BENCHMARK_THREADS": benchmark}):
            client = Client(args.worker, work)
        request = dict(base, job_id=str(uuid.uuid4()))
        request.update(changes or {})
        input_source = Path(request["source_path"])
        input_hash = sha(input_source)
        if changes and "source_path" in changes and "source_sha256" not in changes:
            request["source_sha256"] = input_hash
        source_format, source_channels, source_rate = struct.unpack_from("<HHI", input_source.read_bytes(), 20)
        source_bits, = struct.unpack_from("<H", input_source.read_bytes(), 34)
        expected_frames = (int(request["source_frames"])*int(request["project_rate"]) + source_rate//2)//source_rate
        trace = []
        sent_cancel = False
        try:
            client.send(request, fragmented=True)
            deadline = time.monotonic() + 120
            while True:
                event = client.next(min(30, max(0.1, deadline-time.monotonic())))
                assert event["job_id"] == request["job_id"]
                trace.append(event)
                if cancel and not sent_cancel and event["type"] == "progress" and event.get("completed") == "0":
                    client.send({"type": "cancel", "job_id": request["job_id"]}, fragmented=True)
                    sent_cancel = True
                if event["type"] in ("error", "result"):
                    terminal = event
                    break
                assert time.monotonic() < deadline, "Native job deadline"
            code = client.finish()
            if expected:
                assert terminal["type"] == "error" and terminal["code"] == expected, terminal
                assert code != 0 and not (work / "result").exists() and not (work / "result.part").exists()
            else:
                assert terminal["type"] == "result" and code == 0, (terminal, client.log.decode(errors="replace"))
                manifest = terminal["manifest"]
                saved = json.loads((work / "result/manifest.json").read_text())
                assert saved == manifest and manifest["backend"] == "audio.cpp"
                assert manifest["configured_threads"] == int(request["threads"])
                assert manifest["benchmark_override"] == bool(benchmark)
                assert manifest["source_sha256"] == input_hash and manifest["source_start"] == request["source_start"]
                assert manifest["source_frames"] == request["source_frames"] and manifest["timeline_start"] == request["timeline_start"]
                assert manifest["source_channels"] == source_channels and manifest["source_rate"] == str(source_rate)
                assert manifest["project_rate"] == request["project_rate"] and manifest["mono_policy"] == "duplicate_to_stereo"
                snapshot = float_wav(work/"snapshot.wav")
                model_frames = (int(request["source_frames"])*44100 + source_rate//2)//source_rate
                assert len(snapshot) == model_frames*2 and manifest["snapshot_sha256"] == sha(work/"snapshot.wav")
                if source_rate == 44100 and source_format == 1 and source_channels == 1 and source_bits == 16:
                    with wave.open(str(input_source), "rb") as stream:
                        stream.setpos(int(request["source_start"]))
                        raw = array.array("h")
                        raw.frombytes(stream.readframes(int(request["source_frames"])))
                    import sys
                    if sys.byteorder != "little":
                        raw.byteswap()
                    expected_snapshot = array.array("f", (sample/32768 for sample in raw for _ in range(2)))
                    assert snapshot == expected_snapshot, "Snapshot must contain only the selected range, duplicated to stereo"
                assert {item["stem"] for item in manifest["outputs"]} == {role.lower() for role in request["stems"]}
                assert {p.name for p in (work/"result").iterdir()} == {"manifest.json"} | {role.lower()+".wav" for role in request["stems"]}
                for entry in manifest["outputs"]:
                    path = work / "result" / entry["path"]
                    assert entry["sha256"] == sha(path) and int(entry["bytes"]) == path.stat().st_size
                    assert entry["frames"] == str(expected_frames) and entry["rate"] == request["project_rate"] and entry["origin"] == "0"
                    assert len(float_wav(path, int(request["project_rate"]))) == expected_frames*2
                progress = [event for event in trace if event.get("completed") is not None]
                assert progress and any(event["completed"] == event["total"] for event in progress)
                for current_pass in {event["current_pass"] for event in progress}:
                    values = [event for event in progress if event["current_pass"] == current_pass]
                    assert values[0]["completed"] == "0" and values[-1]["completed"] == values[-1]["total"]
                    assert [int(event["completed"]) for event in values] == sorted(int(event["completed"]) for event in values)
            assert not cancel or sent_cancel
            assert sha(source) == original_hash and sha(input_source) == input_hash
            (work/"wire.json").write_text(json.dumps(trace, indent=2)+"\n")
            results.append({"case": name, "status": "PASS", "terminal": terminal["type"], "events": len(trace)})
        finally:
            client.kill()

    run("threads-default-cap", {"threads": "8"}, "invalid_input")
    run("threads-invalid-override", {"threads": "3"}, "invalid_input", benchmark="3")
    run("threads-mismatch", {"threads": "8"}, "invalid_input", benchmark="16")
    run("threads-cpu-only", {"threads": "16", "device": "auto"}, "invalid_input", benchmark="16")
    # Functional IPC check only: runner CPU capacity is not a scaling result.
    run("threads-16-opt-in", {"threads": "16"}, benchmark="16")
    run("ht6-partial-mono", {"stems": ["Bass", "Vocals"]})
    run("sw-two-heads", {"model": "bs-roformer-sw-6-v1", "model_dir": str(args.sw_dir.resolve()), "stems": ["Bass", "Guitar"]})
    for rate, target, channels, bits, floating, stems in (
            (48000, 96000, 2, 24, False, ["Vocals", "Drums", "Bass", "Guitar", "Piano", "Other"]),
            (88200, 48000, 1, 16, False, ["Bass"]),
            (96000, 44100, 2, 32, True, ["Bass"])):
        candidate = (args.output / f"source-{rate}.wav").resolve()
        fixture(candidate, rate, channels, bits, floating)
        run(f"rate-{rate}-to-{target}", {"source_path": str(candidate), "source_start": str(rate//4+5),
                                       "source_frames": str(rate+17), "project_rate": str(target), "stems": stems})
    run("stale-source", {"source_sha256": "0"*64}, "stale_source")
    run("unsupported-rate", {"project_rate": "12345"}, "unsupported_input")
    run("unsupported-model", {"model": "not-installed"}, "unsupported_model")
    run("duplicate-stems", {"stems": ["Bass", "Bass"]}, "invalid_stems")
    wrong = args.output / "wrong-model"
    wrong.mkdir()
    (wrong/"htdemucs-6stems-f16.gguf").write_bytes(b"not a valid checkpoint")
    run("model-integrity", {"model_dir": str(wrong.resolve())}, "integrity_failure")
    run("cancel-inference", expected="cancelled", cancel=True)
    (args.output/"report.json").write_text(json.dumps({"scope": "real native worker IPC, development slice", "cases": results,
                                                    "source_unchanged": sha(source) == original_hash}, indent=2)+"\n")
    print(f"PASS {len(results)} native worker IPC/model/range/selection/error/cancel cases", flush=True)


if __name__ == "__main__":
    main()
