"""MR Studio local separation sidecar. No network, downloads or project mutation.

Launched explicitly by SeparationRunner using an isolated Python runtime. stdin /
stdout carry bounded uint32-LE framed JSON; all library output goes to stderr.
"""
import contextlib
import hashlib
import importlib.metadata
import io
import json
import math
import os
from pathlib import Path
import socket
import struct
import sys
import threading
import time

if os.name == "nt":
    import ctypes
    from ctypes import wintypes
    import msvcrt
    _peek_pipe = ctypes.WinDLL("kernel32", use_last_error=True).PeekNamedPipe
    _peek_pipe.argtypes = (wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                          ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD))
    _peek_pipe.restype = wintypes.BOOL
else:
    import select

VERSION = 1
BACKEND_VERSION = "0.48.0"
PROFILES = {p["id"]: p for p in json.loads(Path(__file__).with_name("mr_separation_profiles.json").read_text(encoding="utf-8"))}
DEFAULT_MODEL = "official-htdemucs-derived-2-v1"
MODEL_FILES = {f["name"]: f["sha256"] for f in PROFILES[DEFAULT_MODEL]["files"]}
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


def poll_frame(stream, finished):
    """Interruptible pipe reader; never hold a Windows CRT fd lock waiting for input.

    NumPy's native startup can query CRT stdio state. A simultaneous blocking
    stdin read on Windows holds that descriptor lock across ReadFile.
    """
    buffer = bytearray()
    while not finished.wait(0.02):
        if os.name == "nt":
            available = wintypes.DWORD()
            if not _peek_pipe(msvcrt.get_osfhandle(stream.fileno()), None, 0, None, ctypes.byref(available), None):
                raise Failure("protocol_error")
            count = min(available.value, 4096)
            if not count:
                continue
        else:
            if not select.select([stream.fileno()], [], [], 0)[0]:
                continue
            count = 4096
        chunk = os.read(stream.fileno(), count)
        if not chunk:
            raise Failure("protocol_error")
        buffer.extend(chunk)
        if len(buffer) > MAX_FRAME + 4:
            raise Failure("protocol_error")
        if len(buffer) >= 4:
            size = struct.unpack("<I", buffer[:4])[0]
            if not 0 < size <= MAX_FRAME:
                raise Failure("protocol_error")
            if len(buffer) >= 4 + size:
                return read_frame(io.BytesIO(buffer[:4 + size]))
    return None


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


def profile_for(req):
    model = req.get("model", DEFAULT_MODEL)
    if not isinstance(model, str) or model not in PROFILES:
        raise Failure("unsupported_model")
    if req.get("chunk", "default") not in PROFILES[model]["chunks"]:
        raise Failure("invalid_input")
    return PROFILES[model]


class ChunkProgress:
    """Counts completed backend iterations; emitting is bounded to 4 Hz."""
    def __init__(self, emit, check, passes):
        self.emit, self.check, self.passes = emit, check, passes
        self.current_pass = 0

    def wrap(self, original):
        def tracked(iterable, *args, **kwargs):
            total = kwargs.get("total") or len(iterable)
            self.current_pass += 1
            current = self.current_pass
            if not 0 < total <= 1000000 or current > self.passes:
                raise Failure("invalid_output")
            def report(done):
                self.emit("inference", completed=str(done), total=str(total),
                          current_pass=str(current), passes=str(self.passes))
            report(0)
            last = time.monotonic()
            for done, item in enumerate(original(iterable, *args, **kwargs), 1):
                self.check()
                yield item
                self.check()
                now = time.monotonic()
                if now - last >= 0.25 or done == total:
                    report(done)
                    last = now
        return tracked


def infer_profile(snapshot, output_dir, model_dir, requested_device, threads, emit, check, profile, chunk):
    """Fixed, hash-verified closure; no backend catalogue or metadata downloads."""
    for artifact in profile["files"]:
        path = model_dir / artifact["name"]
        if not path.is_file():
            raise Failure("model_missing")
        if path.is_symlink() or path.stat().st_size != artifact["bytes"] or digest(path) != artifact["sha256"]:
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
            if torch.cuda.is_available() and profile["architecture"] != "MDX":
                probe = torch.ones(1, device="cuda") + 1
                probe.cpu()
                free, _total = torch.cuda.mem_get_info()
                if free < profile["min_cuda_bytes"]:
                    raise Failure("resource_limit")
                effective = "cuda"
        except (RuntimeError, MemoryError):
            effective = "cpu"
        if requested_device == "cuda" and effective != "cuda":
            raise Failure("unsupported_device")
    check()
    emit("loading", device=effective)
    import onnxruntime as ort
    ort.disable_telemetry_events()
    from audio_separator.separator import Separator
    from audio_separator.separator.common_separator import CommonSeparator
    from audio_separator.separator.uvr_lib_v5 import spec_utils

    class OfflineSeparator(Separator):
        def setup_accelerated_inferencing_device(self):
            self.torch_device = torch.device(effective)
            self.torch_device_cpu = torch.device("cpu")
            self.torch_device_mps = None
            self.onnx_execution_provider = ["CPUExecutionProvider"]

        def download_model_files(self, filename):
            if filename != profile["model_file"]:
                raise Failure("unsupported_model")
            path = str(model_dir / filename)
            config = next((str(model_dir / f["name"]) for f in profile["files"] if f["name"].endswith(".yaml")), None)
            return filename, profile["architecture"], profile["id"], path, config

        def load_model_data_using_hash(self, _path):
            if profile["architecture"] != "MDX":
                raise Failure("unsupported_model")
            return dict(profile["model_data"])

    def write_float(self, stem_path, stem_source):
        data = np.asarray(stem_source, dtype=np.float32)
        if data.ndim != 2 or data.shape[1] != 2 or not np.isfinite(data).all():
            raise Failure("invalid_output")
        sf.write(str(output_dir / Path(stem_path).name), data, 44100, format="WAV", subtype="FLOAT")

    CommonSeparator.write_audio = write_float
    # All sources share the snapshot gain; do not independently normalize stems.
    spec_utils.normalize = lambda wave, **_kwargs: wave
    os.environ.pop("AUDIO_SEPARATOR_MODEL_DIR", None)
    separator = OfflineSeparator(
        model_file_dir=str(model_dir), output_dir=str(output_dir), output_format="WAV",
        sample_rate=44100, use_soundfile=True, log_level=40,
        demucs_params={"segment_size": 4 if chunk == "small" else "Default", "shifts": 0, "overlap": 0.25, "segments_enabled": True},
        mdxc_params={"segment_size": 128 if chunk == "small" else 256, "override_model_segment_size": True, "batch_size": 1, "overlap": 2, "pitch_shift": 0},
        mdx_params={"hop_length": 1024, "segment_size": 256, "overlap": 0.25, "batch_size": 1, "enable_denoise": False},
    )
    separator.load_model(profile["model_file"])
    check()
    emit("inference", device=effective)
    import types
    if profile["architecture"] == "Demucs":
        module = importlib.import_module("audio_separator.separator.uvr_lib_v5.demucs.apply")
    else:
        module = importlib.import_module("audio_separator.separator.architectures." + ("mdx_separator" if profile["architecture"] == "MDX" else "mdxc_separator"))
    original = module.tqdm
    reporter = ChunkProgress(emit, check, 2 if profile["architecture"] == "MDX" else 1)
    module.tqdm = types.SimpleNamespace(tqdm=reporter.wrap(original.tqdm)) if profile["architecture"] == "Demucs" else reporter.wrap(original)
    try:
        separator.separate(str(snapshot), {stem: stem.lower() for stem in profile["native_stems"]})
    finally:
        module.tqdm = original
    return effective


def infer_htdemucs(snapshot, output_dir, model_dir, requested_device, threads, emit, check):
    return infer_profile(snapshot, output_dir, model_dir, requested_device, threads, emit, check, PROFILES[DEFAULT_MODEL], "default")


def execute(req, work, emit, check, engine=None):
    profile = profile_for(req)
    emit("validating")
    source, start, frames, origin, target_rate, threads = validate_request(req)
    check()
    for key in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS"):
        os.environ[key] = str(threads)
    # Managed runtimes are immutable; JIT caches belong to this disposable job.
    os.environ["NUMBA_CACHE_DIR"] = str(work / "numba-cache")
    # Imports are deliberately lazy: normal DAW/runner construction needs none.
    emit("import_numpy")
    import numpy as np
    emit("import_soundfile")
    import soundfile as sf
    emit("import_scipy")
    from scipy.signal import resample_poly
    emit("preparing")
    info = sf.info(str(source))
    if info.channels not in (1, 2) or info.samplerate not in (44100, 48000, 88200, 96000):
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
    if info.channels == 1:
        data = np.repeat(data, 2, axis=1)
    model_frames = (frames * 44100 + info.samplerate // 2) // info.samplerate
    data = resample(data, info.samplerate, 44100, model_frames)
    snapshot = work / "snapshot.wav"
    sf.write(str(snapshot), data, 44100, format="WAV", subtype="FLOAT")
    del data
    check()
    native = work / "native"
    native.mkdir()
    if engine is None:
        effective = infer_profile(snapshot, native, Path(req["model_dir"]).resolve(), req["device"], threads, emit, check, profile, req.get("chunk", "default"))
    else:
        effective = engine(snapshot, native, Path(req["model_dir"]).resolve(), req["device"], threads, emit, check)
    check()
    emit("verifying", device=effective)
    stems = {}
    for name in (stem.lower() for stem in profile["native_stems"]):
        path = native / (name + ".wav")
        if path.is_symlink():
            raise Failure("invalid_output")
        audio, rate = sf.read(str(path), dtype="float32", always_2d=True)
        if rate != 44100 or audio.shape != (model_frames, 2) or not np.isfinite(audio).all():
            raise Failure("invalid_output")
        stems[name] = audio
    if profile["id"] == DEFAULT_MODEL:
        stems["instrumental"] = stems["bass"] + stems["drums"] + stems["other"]
    if profile["id"] == "mel-roformer-kim-2-v1":
        stems["instrumental"] = stems.pop("other")
    outputs = []
    pending = work / "result.part"
    pending.mkdir()
    for label in profile["stems"]:
        name = label.lower()
        audio = stems[name]
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
                "snapshot_sha256": digest(snapshot), "model": profile["id"],
                "weights": {f["name"]: f["sha256"] for f in profile["files"]}, "backend": "python-audio-separator", "backend_version": BACKEND_VERSION,
                "requested_device": req["device"], "effective_device": effective, "precision": "fp32",
                "gain_policy": "unnormalized_float", "derivation": "instrumental=bass+drums+other" if profile["id"] == DEFAULT_MODEL else "model-native/complement",
                "chunk": req.get("chunk", "default"), "source_channels": info.channels, "mono_policy": "duplicate_to_stereo", "model_rate": 44100,
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
    if os.environ.get("MR_SEPARATION_DIAGNOSTICS") == "1":
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
        # Unbuffered request avoids prefetching a queued cancel into a separate
        # BufferedReader while the poller observes the underlying pipe handle.
        input_stream = sys.stdin.buffer.raw
        req = read_frame(input_stream)
        deadline = time.monotonic() + counter(req.get("timeout_ms"), 100, 24 * 3600000) / 1000
        def check():
            if cancelled.is_set():
                raise Failure("cancelled")
            if time.monotonic() > deadline:
                raise Failure("timeout")
        def read_cancel():
            try:
                command = poll_frame(input_stream, finished)
                if command is None:
                    return
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
