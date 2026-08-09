NoanOS I/O Optimizations — Summary

Goal
Make file reading and loading significantly faster across the project (kernel FAT32, installer, host tools).

High-level changes
- Cluster-buffered reads: read full clusters (sectors_per_cluster) in one ata_read_sectors call (fat32_file.c, elf.c, fat32_dir.c).
- FAT-sector cache: single cached FAT sector buffer reduces repeated FAT reads/writes (fat32_fat.c).
- Persistent buffers: sector and cluster buffers allocated at fat32_init and reused (fat32.c).
- Read-ahead: simple READAHEAD_CLUSTERS (default 1) prefetches the next cluster for sequential reads (fat32_file.c).
- Installer batching: INSTALL_BATCH_SECTORS default 64 (installer.c) to reduce PIO operations when copying boot/kernel/initrd.
- Tools batching: host-side image scripts updated to use larger batched I/O where applicable (tools/make-hdd.py).

Benchmarks (host-side)
- tools/bench_read.py: host image read microbench (see tools/bench_results.txt)
- tools/installer_copy_bench.py: host-side 700-sector copy test
  - batch=8 -> 0.004188s (81.6 MB/s)
  - batch=64 -> 0.002281s (149.9 MB/s)
  - ~1.8× throughput improvement for batching 8→64 on host

Notes & caveats
- Host-side numbers reflect host FS and page-cache; run QEMU/in-guest to measure device-level PIO performance (recommended).
- 1000× is an aggressive target; cluster buffering + FAT caching + batching typically yields 5–100× on emulated PIO; DMA/virtio needed for higher gains.
- Memory tradeoffs: caching whole FAT or larger read-ahead increases RAM usage. READAHEAD_CLUSTERS and INSTALL_BATCH_SECTORS are compile-time tunables.

How to measure (recommended)
1. Host quick: python3 tools/bench_read.py and tools/installer_copy_bench.py
2. Device-like: build and run in QEMU (make -C src run or make -C src run-installed) and time installer "Copying 700 sectors" output in serial.log.
3. For repeatability: drop host caches or use O_DIRECT, run multiple trials, compare means.

Files changed (high level)
- src/kernel/storage/fat32.c, fat32_fat.c, fat32_file.c, fat32_dir.c
- src/kernel/storage/elf.c
- src/kernel/core/installer.c
- src/tools/make-hdd.py, tools/bench_read.py, tools/installer_copy_bench.py

Next steps
- Further speedups: implement DMA/virtio, cache full FAT in RAM (if memory allows), direct disk->physical-page streaming to avoid intermediate copies.
- Tune READAHEAD_CLUSTERS and INSTALL_BATCH_SECTORS per target hardware.

If you want, I can now boot QEMU and run an in-guest ELF/load or installer benchmark and attach the measured serial.log and numbers.
