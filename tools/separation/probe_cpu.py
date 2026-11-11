"""Developer-only CPU runtime probe. No models, downloads or speed benchmark."""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
import platform


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    import torch
    import onnxruntime as ort
    from worker import cpu_runtime_info, onnx_thread_limit
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    sample = torch.ones((16, 16), dtype=torch.float32)
    assert torch.equal(sample @ sample, sample * 16), 'CPU matrix multiplication failed'
    info = cpu_runtime_info(torch) | {'scope': 'CPU library functional probe; not model performance or per-operator ISA proof',
                                      'platform': platform.platform(), 'torch_build': torch.__config__.show()}
    info['avx2_dispatch_probe'] = {'status': 'NOT TESTED', 'reason': 'host runtime did not report AVX2/AVX512 support'}
    if info['torch_cpu_capability'] in ('AVX2', 'AVX512'):
        # Explicit override is confined to this fresh developer subprocess.
        # Do not set it in Studio or infer the ISA of every model operator.
        code = """import json, math, torch
assert torch.backends.cpu.get_cpu_capability() == 'AVX2'
torch.set_num_threads(1)
x = torch.ones(64, dtype=torch.float32)
assert torch.allclose(torch.sin(x), torch.full_like(x, math.sin(1)), atol=1e-6)
print(json.dumps({'status': 'PASS', 'torch_cpu_capability': torch.backends.cpu.get_cpu_capability(), 'elementwise_check': 'sin float32'}))
"""
        child = subprocess.run([sys.executable, '-I', '-c', code],
                               env=os.environ | {'ATEN_CPU_CAPABILITY': 'avx2'},
                               capture_output=True, text=True, check=True, timeout=60)
        info['avx2_dispatch_probe'] = json.loads(child.stdout)
    original = ort.SessionOptions
    with onnx_thread_limit(ort, 8):
        options = ort.SessionOptions()
        assert options.intra_op_num_threads == 8 and options.inter_op_num_threads == 1
    assert ort.SessionOptions is original
    info['onnxruntime_version'] = ort.__version__
    info['onnxruntime_build'] = ort.get_build_info()
    info['onnx_pool_options_probe'] = 'PASS; functional configuration only, not scaling'
    info['source'] = os.environ.get('GITHUB_SHA', '')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(info, indent=2))


if __name__ == '__main__':
    main()
