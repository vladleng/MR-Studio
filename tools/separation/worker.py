"""MR Studio local separation sidecar. No network, downloads or project mutation.

Launched explicitly by SeparationRunner using an isolated Python runtime. stdin /
stdout carry bounded uint32-LE framed JSON; all library output goes to stderr.
"""
import contextlib
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import socket
import struct
import sys
import threading
import time

VERSION = 1
BACKEND_VERSION = "0.48.0"
MODEL_FILES = {
    "htdemucs.yaml": "239c445d0b14454d541ad8bd9bb271c9e536d267e8a4625208744cbb2e7bb66c",
    "955717e8-8726e21a.th": "8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4",
}
MAX_FRAME = 65536


class Failure(Exception):
    def __init__(self, code):
        self.code = code


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_frame(stream):
    def read_exact(n):
        data = bytearray()
        while len(data) < n:
            chunk = stream.read(n - len(data))
            if not chunk:
                raise Failure("protocol_error")
            data.extend(chunk)
        return bytes(data)
    size = struct.unpack("<I", read_exact(4))[0]
    if not 0 < size <= MAX_FRAME:
        raise Failure("protocol_error")
    try:
        value = json.loads(read_exact(size).decode("utf-8"))
    except (ValueError, UnicodeError):
        raise Failure("protocol_error") from None
    if not isinstance(value, dict):
        raise Failure("protocol_error")
    return value


def write_frame(stream, value):
    payload = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":")).encode()
    if len(payload) > MAX_FRAME:
        raise Failure("protocol_error")
    stream.write(struct.pack("<I", len(payload)) + payload)
    stream.flush()


def counter(value, minimum=0, maximum=2**53):
    if not isinstance(value, str) or not value.isascii() or not value.isdecimal():
        raise Failure("invalid_input")
    if len(value) > 20 or (len(value) > 1 and value[0] == "0"):
        raise Failure("invalid_input")
    number = int(value)
    if not minimum <= number <= maximum:
        raise Failure("invalid_input")
    return number


def block_network():
    # Install before importing any ML package. This process-local download guard
    # is not an OS sandbox for arbitrary code; Windows host also disallows children.
    def audit(event, _args):
        if event in ("socket.connect", "socket.connect_ex", "socket.getaddrinfo", "socket.bind"):
            raise Failure("network_disabled")
    sys.addaudithook(audit)
    def deny(*_args, **_kwargs):
        raise Failure("network_disabled")
    socket.create_connection = deny


def validate_request(req):
    if req.get("version") != VERSION or req.get("type") != "submit":
        raise Failure("protocol_error")
    for key in ("job_id", "session_id", "source_revision"):
        value = req.get(key)
        if not isinstance(value, str) or not 1 <= len(value) <= 128:
            raise Failure("invalid_input")
    if req.get("device") not in ("cpu", "cuda", "auto"):
        raise Failure("unsupported_device")
    start = counter(req.get("source_start"))
    frames = counter(req.get("source_frames"), 1, 96000 * 600)
    origin = counter(req.get("timeline_start"))
    rate = counter(req.get("project_rate"), 8000, 96000)
    if rate not in (8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000):
        raise Failure("unsupported_input")
    threads = counter(req.get("threads"), 1, 4)
    source = Path(req["source_path"]).resolve(strict=True)
    if not source.is_file() or source.stat().st_size > 512 * 1024**2:
        raise Failure("resource_limit")
    if req.get("source_sha256") != digest(source):
        raise Failure("stale_source")
    return source, start, frames, origin, rate, threads


def infer_htdemucs(snapshot, output_dir, model_dir, requested_device, threads, emit, check):
    """The only admitted technical profile; exact weights verified before unpickling."""
    for name, expected in MODEL_FILES.items():
        path = model_dir / name
        if not path.is_file():
            raise Failure("model_missing")
        if path.is_symlink() or digest(path) != expected:
            raise Failure("integrity_failure")
    if importlib.metadata.version("audio-separator") != BACKEND_VERSION:
        raise Failure("unsupported_runtime")
    import numpy as np
    import soundfile as sf
    import torch
    torch.set_num_threads(threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(0)
    effective = "cpu"
    if requested_device != "cpu":
        try:
            if torch.cuda.is_available():
                # Probe actual allocation and an operation, not just device presence.
                probe = torch.ones(1, device="cuda") + 1
                probe.cpu()
                effective = "cuda"
        except (RuntimeError, MemoryError):
            effective = "cpu"
        if requested_device == "cuda" and effective != "cuda":
            raise Failure("unsupported_device")
    check()
    emit("loading", device=effective)
    # No model catalogue/UVR metadata requests: this fixed adapter resolves the
    # complete official Demucs closure itself, avoiding upstream discovery fetches.
    from audio_separator.separator import Separator
    from audio_separator.separator.common_separator import CommonSeparator

    class OfflineSeparator(Separator):
        def setup_accelerated_inferencing_device(self):
            self.torch_device = torch.device(effective)
            self.torch_device_cpu = torch.device("cpu")
            self.torch_device_mps = None
            self.onnx_execution_provider = ["CPUExecutionProvider"]

        def download_model_files(self, filename):
            if filename != "htdemucs.yaml":
                raise Failure("unsupported_model")
            path = str(model_dir / filename)
            return filename, "Demucs", "official-htdemucs", path, path

    def write_float(self, stem_path, stem_source):
        # Bypass CommonSeparator's independent per-stem normalisation/clipping.
        data = np.asarray(stem_source, dtype=np.float32)
        if data.ndim != 2 or data.shape[1] != 2 or not np.isfinite(data).all():
            raise Failure("invalid_output")
        path = output_dir / Path(stem_path).name
        sf.write(str(path), data, 44100, format="WAV", subtype="FLOAT")

    CommonSeparator.write_audio = write_float
    os.environ.pop("AUDIO_SEPARATOR_MODEL_DIR", None)
    separator = OfflineSeparator(
        model_file_dir=str(model_dir), output_dir=str(output_dir), output_format="WAV",
        sample_rate=44100, use_soundfile=True, log_level=40,
        demucs_params={"segment_size": "Default", "shifts": 0, "overlap": 0.25, "segments_enabled": True},
    )
    separator.load_model("htdemucs.yaml")
    check()
    emit("inference", device=effective)
    separator.separate(str(snapshot), {"Vocals": "vocals", "Bass": "bass", "Drums": "drums", "Other": "other"})
    return effective


def execute(req, work, emit, check, engine=infer_htdemucs):
    emit("validating")
    source, start, frames, origin, target_rate, threads = validate_request(req)
    check()
    for key in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS"):
        os.environ[key] = str(threads)
    # Imports are deliberately lazy: normal DAW/runner construction needs none.
    emit("import_numpy")
    import numpy as np
    emit("import_soundfile")
    import soundfile as sf
    emit("import_scipy")
    from scipy.signal import resample_poly
    emit("preparing")
    info = sf.info(str(source))
    if info.channels != 2 or info.samplerate not in (44100, 48000, 88200, 96000):
        raise Failure("unsupported_input")
    if start + frames > info.frames or frames > info.samplerate * 600:
        raise Failure("invalid_input")
    # All large buffers fit a conservative preflight budget; OS process limit is
    # enforced independently by the Windows host Job Object.
    if frames * 2 * 4 * 32 > counter(req.get("memory_bytes"), 256 * 1024**2, 16 * 1024**3):
        raise Failure("resource_limit")
    target_frames = (frames * target_rate + info.samplerate // 2) // info.samplerate
    with sf.SoundFile(str(source)) as f:
        f.seek(start)
        data = f.read(frames, dtype="float32", always_2d=True)
    if len(data) != frames or not np.isfinite(data).all() or req["source_sha256"] != digest(source):
        raise Failure("stale_source")
    # Snapshot fixes the source identity before ML work; original is never written.
    def resample(data, src_rate, dst_rate, length):
        if src_rate != dst_rate:
            gcd = math.gcd(src_rate, dst_rate)
            data = resample_poly(data, dst_rate // gcd, src_rate // gcd, axis=0)
        if len(data) < length:
            raise Failure("invalid_output")
        return np.ascontiguousarray(data[:length], dtype=np.float32)
    model_frames = (frames * 44100 + info.samplerate // 2) // info.samplerate
    data = resample(data, info.samplerate, 44100, model_frames)
    snapshot = work / "snapshot.wav"
    sf.write(str(snapshot), data, 44100, format="WAV", subtype="FLOAT")
    del data
    check()
    native = work / "native"
    native.mkdir()
    effective = engine(snapshot, native, Path(req["model_dir"]).resolve(), req["device"], threads, emit, check)
    check()
    emit("verifying", device=effective)
    stems = {}
    for name in ("vocals", "bass", "drums", "other"):
        path = native / (name + ".wav")
        if path.is_symlink():
            raise Failure("invalid_output")
        audio, rate = sf.read(str(path), dtype="float32", always_2d=True)
        if rate != 44100 or audio.shape != (model_frames, 2) or not np.isfinite(audio).all():
            raise Failure("invalid_output")
        stems[name] = audio
    instrument = stems["bass"] + stems["drums"] + stems["other"]
    outputs = []
    pending = work / "result.part"
    pending.mkdir()
    for name, audio in (("vocals", stems["vocals"]), ("instrumental", instrument)):
        check()
        audio = resample(audio, 44100, target_rate, target_frames)
        if not np.isfinite(audio).all():
            raise Failure("invalid_output")
        path = pending / (name + ".wav")
        sf.write(str(path), audio, target_rate, format="WAV", subtype="FLOAT")
        outputs.append({"stem": name, "path": path.name, "sha256": digest(path),
                        "bytes": str(path.stat().st_size), "frames": str(target_frames),
                        "rate": str(target_rate), "channels": 2, "format": "WAV_FLOAT32", "origin": "0"})
    manifest = {"version": VERSION, "job_id": req["job_id"], "session_id": req["session_id"],
                "source_revision": req["source_revision"], "source_sha256": req["source_sha256"],
                "source_start": str(start), "source_frames": str(frames), "source_rate": str(info.samplerate),
                "timeline_start": str(origin), "project_rate": str(target_rate),
                "snapshot_sha256": digest(snapshot), "model": "official-htdemucs-derived-2-v1",
                "weights": MODEL_FILES, "backend": "python-audio-separator", "backend_version": BACKEND_VERSION,
                "requested_device": req["device"], "effective_device": effective, "precision": "fp32",
                "gain_policy": "unnormalized_float", "derivation": "instrumental=bass+drums+other",
                "shifts": 0, "seed": 0, "overlap": 0.25, "outputs": outputs}
    (pending / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=True, allow_nan=False), encoding="utf-8")
    check()
    pending.rename(work / "result")
    return manifest


def main():
    # Preserve a private protocol fd, redirect even native-library stdout writes.
    wire = os.fdopen(os.dup(sys.stdout.fileno()), "wb", buffering=0)
    os.dup2(sys.stderr.fileno(), sys.stdout.fileno())
    # A bounded host log tail makes a blocked native import diagnosable without
    # extending its deadline. No traceback or local paths enter protocol errors.
    import faulthandler
    faulthandler.dump_traceback_later(60, repeat=False)
    block_network()
    lock = threading.Lock()
    cancelled = threading.Event()
    finished = threading.Event()
    seq = 0
    req = {}
    def send(kind, **fields):
        nonlocal seq
        with lock:
            seq += 1
            write_frame(wire, {"version": VERSION, "type": kind, "sequence": str(seq),
                               "job_id": req.get("job_id", ""), **fields})
    send("hello")
    try:
        req = read_frame(sys.stdin.buffer)
        deadline = time.monotonic() + counter(req.get("timeout_ms"), 100, 24 * 3600000) / 1000
        def check():
            if cancelled.is_set():
                raise Failure("cancelled")
            if time.monotonic() > deadline:
                raise Failure("timeout")
        def read_cancel():
            try:
                command = read_frame(sys.stdin.buffer)
                if command.get("type") == "cancel" and command.get("job_id") == req.get("job_id"):
                    cancelled.set()
                else:
                    cancelled.set()
            except (Failure, OSError):
                cancelled.set()
        def heartbeat():
            while not finished.wait(2):
                try:
                    send("heartbeat")
                except (BrokenPipeError, OSError):
                    cancelled.set()
                    return
        threading.Thread(target=read_cancel, daemon=True).start()
        threading.Thread(target=heartbeat, daemon=True).start()
        with contextlib.redirect_stdout(sys.stderr):
            manifest = execute(req, Path.cwd(), lambda phase, **kw: send("progress", phase=phase, **kw), check)
        check()
        send("result", manifest=manifest)
    except Failure as error:
        send("error", code=error.code)
    except (FileNotFoundError, KeyError, ValueError, TypeError):
        send("error", code="invalid_input")
    except (ImportError, importlib.metadata.PackageNotFoundError):
        send("error", code="runtime_missing")
    except MemoryError:
        send("error", code="resource_limit")
    except Exception as error:
        # No source paths or arbitrary backend strings cross into UI diagnostics.
        code = "resource_limit" if "out of memory" in str(error).lower() else "backend_failure"
        send("error", code=code)
    finally:
        finished.set()
        faulthandler.cancel_dump_traceback_later()


if __name__ == "__main__":
    main()
