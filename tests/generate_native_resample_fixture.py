"""Generate synthetic separation SRC goldens using the accepted SciPy versions."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
import scipy
from scipy.signal import resample_poly


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError("Generate into a new file and review the fixture diff")
    assert np.__version__ == "2.2.6" and scipy.__version__ == "1.15.3"
    specs = [(44100, 48000, 31), (48000, 44100, 31), (44100, 96000, 47), (96000, 44100, 47),
             (8000, 44100, 19), (44100, 8000, 127), (32000, 44100, 61), (88200, 44100, 63),
             (44100, 88200, 47), (22050, 44100, 53), (44100, 16000, 67), (48000, 96000, 31),
             (44100, 44100, 64), (96000, 8000, 127)]
    cases = []

    def add(name, source_rate, target_rate, values):
        values = np.asarray(values, dtype=np.float32)
        divisor = math.gcd(source_rate, target_rate)
        count = (len(values)*target_rate + source_rate//2)//source_rate
        expected = resample_poly(values, target_rate//divisor, source_rate//divisor, axis=0)[:count]
        assert len(expected) == count
        cases.append({"name": name, "source_rate": source_rate, "target_rate": target_rate,
                      "channels": values.shape[1], "input": values.reshape(-1).tolist(),
                      "expected": expected.reshape(-1).tolist()})

    for index, (source, target, frames) in enumerate(specs):
        time = np.arange(frames, dtype=np.float64)
        left = 0.9*np.sin(0.31*time) + 0.45*np.cos(0.79*time) + (time % 5 - 2)*0.04
        right = -0.7*left + 0.13*np.sin(0.57*time)
        values = np.stack((left, right), axis=1).astype(np.float32)
        if source == target:
            values[0, 0] = -0.0
        add(f"ratio-{index}", source, target, values)
    for position in (0, 63, 126):
        impulse = np.zeros((127, 2), dtype=np.float32)
        impulse[position] = [1.5, -0.7]
        add(f"impulse-{position}", 48000, 44100, impulse)
    add("constant-mono", 96000, 44100, np.full((127, 1), 0.125, dtype=np.float32))
    add("silence", 44100, 96000, np.zeros((31, 2), dtype=np.float32))
    payload = {"reference": "scipy.signal.resample_poly default Kaiser5/constant-zero, Float32, nearest-frame trim",
               "numpy": np.__version__, "scipy": scipy.__version__, "cases": cases}
    args.output.write_text(json.dumps(payload, indent=2, allow_nan=False)+"\n", newline="\n")
    print(f"Generated {len(cases)} synthetic SRC reference cases")


if __name__ == "__main__":
    main()
