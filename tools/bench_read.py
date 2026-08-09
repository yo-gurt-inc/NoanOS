#!/usr/bin/env python3
"""
Simple host-side read benchmark for disk image files.
Writes results to tools/bench_results.txt
"""
import os
import sys
import time
from statistics import mean

CANDIDATES = [
    'bin/hdd.img',
    'bin/disk.img',
    'src/build/img/hdd.img',
    'src/build/img/disk.img',
]

def find_image():
    for p in CANDIDATES:
        if os.path.exists(p):
            return p
    for root, dirs, files in os.walk('.'):
        for f in files:
            if f.endswith('.img'):
                return os.path.join(root, f)
    return None

def bench(path, sizes, total_bytes=100*1024*1024):
    results = {}
    file_size = os.path.getsize(path)
    with open(path, 'rb', buffering=0) as f:
        for size in sizes:
            iters = max(1, int(total_bytes // size))
            latencies = []
            bytes_read = 0
            start = time.perf_counter()
            for i in range(iters):
                # spread reads across file
                offset = (i * size) % max(1, file_size - size + 1)
                f.seek(offset)
                t0 = time.perf_counter()
                data = f.read(size)
                t1 = time.perf_counter()
                if not data:
                    break
                bytes_read += len(data)
                latencies.append((t1 - t0) * 1000.0)  # ms
            end = time.perf_counter()
            total_time = end - start
            throughput_mb_s = (bytes_read / (1024*1024)) / total_time if total_time > 0 else 0.0
            avg_lat = mean(latencies) if latencies else 0.0
            p95 = sorted(latencies)[max(0, int(len(latencies)*0.95)-1)] if latencies else 0.0
            results[size] = {
                'bytes_read': bytes_read,
                'total_time_s': total_time,
                'throughput_mb_s': throughput_mb_s,
                'avg_latency_ms': avg_lat,
                'p95_latency_ms': p95,
                'reads': len(latencies),
            }
    return results

def write_report(path, results, out_path='tools/bench_results.txt'):
    lines = []
    lines.append(f'Benchmark of image: {path}')
    lines.append('Sizes: small=1KiB, medium=64KiB, large=1MiB')
    lines.append('')
    for size in sorted(results.keys()):
        r = results[size]
        lines.append(f'Chunk size: {size} bytes')
        lines.append(f'  reads: {r["reads"]}')
        lines.append(f'  bytes_read: {r["bytes_read"]}')
        lines.append(f'  total_time_s: {r["total_time_s"]:.6f}')
        lines.append(f'  throughput_mb_s: {r["throughput_mb_s"]:.3f}')
        lines.append(f'  avg_latency_ms: {r["avg_latency_ms"]:.3f}')
        lines.append(f'  p95_latency_ms: {r["p95_latency_ms"]:.3f}')
        lines.append('')
    text = '\n'.join(lines)
    with open(out_path, 'w') as f:
        f.write(text)
    print(text)

def main():
    img = find_image()
    if img is None:
        print('No .img found in repository. Create a test file? Using /tmp/test.img of 200MB')
        img = '/tmp/test.img'
        if not os.path.exists(img):
            with open(img, 'wb') as f:
                f.truncate(200 * 1024 * 1024)
    sizes = [1024, 64*1024, 1024*1024]
    try:
        print('Image used:', img, 'size:', os.path.getsize(img))
    except Exception as e:
        print('Error statting image:', e)
        sys.exit(1)
    results = bench(img, sizes)
    write_report(img, results)

if __name__ == '__main__':
    main()
