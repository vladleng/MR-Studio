"""Deterministic synthetic FLOAT32 snapshot for native CPU geometry/timing tests."""
import argparse
import array
import math
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=1)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("Use 1..300 seconds")
    if args.output.exists():
        parser.error("Do not overwrite existing audio")
    rate = 44100
    pcm = array.array("f")
    for index in range(rate * args.seconds):
        t = index / rate
        phase = t % 0.5
        envelope = math.exp(-8 * phase)
        bass = 0.2 * envelope * math.sin(2 * math.pi * 110 * t)
        tone = 0.04 * math.sin(2 * math.pi * 440 * t)
        pcm.extend((bass + tone, 0.9 * (bass + tone) + 0.01 * envelope * math.sin(2 * math.pi * 880 * t)))
    if sys.byteorder != "little":
        pcm.byteswap()
    data = pcm.tobytes()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", len(data) + 36,
                                 b"WAVE", b"fmt ", 16, 3, 2, rate, rate * 8, 8, 32, b"data", len(data)))
        stream.write(data)


if __name__ == "__main__":
    main()
