"""Host IPC fault fixture. Never shipped or selectable by the production worker."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import time


def send(kind, sequence, **fields):
    data = json.dumps({"version": 1, "type": kind, "sequence": str(sequence), **fields}).encode()
    sys.stdout.buffer.write(struct.pack("<I", len(data)) + data)
    sys.stdout.buffer.flush()


send("hello", 1)
size = struct.unpack("<I", sys.stdin.buffer.read(4))[0]
req = json.loads(sys.stdin.buffer.read(size))
mode = req["source_revision"]
if mode == "stderr_flood":
    sys.stderr.buffer.write(b"x" * (1024 * 1024))
    sys.stderr.buffer.flush()
if mode == "crash":
    sys.exit(3)
if mode in ("hang", "cancel"):
    time.sleep(60)
if mode == "flood":
    sys.stdout.buffer.write(b"\xff" * 70000)
    sys.stdout.buffer.flush()
    time.sleep(1)
if mode == "wrong_job":
    send("progress", 2, job_id="wrong", phase="inference")
    sys.exit(0)
if mode == "missing_model":
    send("error", 2, job_id=req["job_id"], code="model_missing")
    sys.exit(0)

if mode in ("progress", "bad_progress"):
    send("progress", 2, job_id=req["job_id"], phase="inference", torch_cpu_capability="AVX2 (fixture)", torch_intraop_threads=int(req["threads"]), completed="1" if mode == "progress" else "3", total="2", current_pass="1", passes="1")
    time.sleep(0.1)

result = Path.cwd() / "result"
result.mkdir()
frames = int(req["source_frames"])
rate = int(req["project_rate"])
manifest = {"version": 1, "job_id": req["job_id"], "session_id": req["session_id"],
            "source_revision": mode, "source_sha256": req["source_sha256"], "source_rate": str(rate),
            "source_frames": str(frames), "source_start": req["source_start"],
            "timeline_start": req["timeline_start"], "project_rate": str(rate),
            "configured_threads": int(req["threads"]), "backend_version": "0.48.0", "model": "official-htdemucs-derived-2-v1",
            "effective_device": "cpu", "outputs": []}
for stem in ("vocals", "instrumental"):
    path = result / (stem + ".wav")
    audio = struct.pack("<f", float("nan") if mode == "nan" else 1.25) * frames * 2
    header = b"RIFF" + struct.pack("<I", 36 + len(audio)) + b"WAVEfmt "
    header += struct.pack("<IHHIIHH", 16, 3, 2, rate, rate * 8, 8, 32)
    path.write_bytes(header + b"data" + struct.pack("<I", len(audio)) + audio)
    manifest["outputs"].append({"stem": stem, "path": path.name,
        "sha256": "0" * 64 if mode == "bad_hash" else hashlib.sha256(path.read_bytes()).hexdigest(),
        "bytes": str(path.stat().st_size), "frames": str(frames), "rate": str(rate),
        "channels": 2, "origin": "0", "format": "WAV_FLOAT32"})
if mode == "traversal":
    manifest["outputs"][0]["path"] = "../source.wav"
if mode == "duplicate":
    manifest["outputs"][1] = manifest["outputs"][0]
if mode == "stale":
    manifest["session_id"] = "old_session"
(result / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
send("result", 3 if mode in ("progress", "bad_progress") else 2, job_id=req["job_id"], manifest=manifest)
