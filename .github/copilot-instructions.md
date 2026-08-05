Copilot instructions for NoanOS

Build, test, and lint commands

- Primary build: from repository root run:
  - make -C src/boot
    - produces: bin/boot.bin, bin/kernel.bin, bin/disk.img, bin/hdd.img and build/* object files
  - Clean: make -C src/boot clean
- Run in QEMU (from repo root):
  - make -C src/boot run    # boots the live disk and a 10MB HDD image
  - make -C src/boot run-installed  # boots from HDD image
  - make -C src/boot debug  # qemu -s -S for gdb attach
- Single-object / single-file compile (example):
  - mkdir -p build && \
    gcc -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32 -march=i686 -fno-pie -fno-stack-protector -nostdlib -nostartfiles -I src/kernel -c src/kernel/terminal.c -o build/terminal.o
  - Use the Makefile in src/boot for correct flags and linking; prefer make -C src/boot.
- Tests / lints: There are no automated tests or linters configured in the repo.

High-level architecture (big picture)

- Boot stage (src/boot): NASM boot sector + small loader (boot.asm) that:
  - Provides a minimal FAT32 BPB for live disk compatibility
  - Loads a fixed number of kernel sectors into 0x100000 and jumps to the kernel entry
  - Exposes the BIOS boot drive in DL (passed to kernel as boot_drive)
  - Uses linker.ld to place kernel sections at early physical addresses before objcopy to flat binary
- Kernel (src/kernel): a 32-bit, freestanding i386 kernel written in C + some assembly
  - core entry: kmain(u32 boot_drive) (src/kernel/kernel.c)
  - subsystems present: GDT/TSS, IDT/interrupt handlers, PIT/timer, basic preemptive-like task switching (int 0x20), system calls (int 0x80), simple kmalloc/kfree heap, ATA driver, FAT32 implementation, a simple shell and an installer
  - Task model: process_t and task scheduler in task.c; task_create(entry, flags) — flags bit 0 == user mode (see task_create usage)
  - User-mode support: syscall interface via int 0x80; ELF loader (elf.c) allocates fresh physical frames per-process and maps them into the process's own page directory at whatever vaddr the ELF requests. User binaries are linked at 0x08000000 (per-process virtual address, not the identity-mapped region).
- Disk images (bin/): boot.bin, kernel.bin, disk.img (live disk), hdd.img (10MB installed HDD). The Makefile composes disk.img by writing the boot sector and kernel.bin to sectors.

Key conventions and repository-specific patterns

- CPU/ABI and toolchain
  - Target: i386 (elf32), GCC invoked with -m32 -march=i686 and freestanding flags. Assembly code built with NASM (ASM = nasm). Linker uses custom linker.ld and ld flags: -T linker.ld --oformat elf32-i386 -m elf_i386.
  - Kernel converted to a flat binary with objcopy: objcopy -O binary kernel.elf kernel.bin
- Build directory layout (Makefile expectations)
  - The Makefile in src/boot assumes it runs from src/boot; it references C sources as ../kernel/*.c and includes headers with -I ../kernel. Prefer invoking make -C src/boot from repo root.
- Entry/boot semantics
  - Bootloader writes the BIOS drive number into a byte (boot_drive) and eventually jumps to the kernel's physical load address. Kernel entry point is kmain(u32 boot_drive).
  - Live disk detection: the boot image writes "LIVE" at a specific offset (sector 255). The kernel checks sector 255 for "LIVE" to decide whether to run the installer or mount the installed FS.
- Task / mode conventions
  - task_create(entry, flags): pass 1 in flags to request user-mode (ring 3) stack/segments (see task.c stack setup).
  - Syscalls: performed via int 0x80. See include/syscall.h for syscall numbers; userspace stub functions in shell.c show syscall0/1 usage patterns.
- Memory management
  - kmalloc / kfree: simple free-list allocator; initialized in kmain with malloc_init(start, size). Heap is located at a static address in kmain (0x200000 in current code).
- Filesystem and storage
  - Simple FAT32 implementation in fat32.c with minimal BPB assumptions (designed for small images). Use fat32_* helpers (ls, cd, cat, mkdir, touch, echo, rm) from shell.
- Debugging / running
  - QEMU is invoked by the Makefile. For GDB attach, run make -C src/boot debug which starts qemu with -s -S; attach gdb to the listening port and load kernel symbols from build/kernel.elf.

Important files to consult quickly

- src/boot/Makefile  — build + qemu/run targets and the exact CFLAGS/LD/ASM flags used
- src/boot/boot.asm   — bootloader and PM transition logic, how kernel is loaded and DL passed
- src/boot/linker.ld  — linker layout for the kernel
- src/kernel/kernel.c — kernel entry (kmain) and init sequence
- src/kernel/task.c, include/task.h  — task model & user-vs-kernel setup
- src/kernel/syscall.c, include/syscall.h — syscall interface usage patterns
- src/kernel/fat32.c, src/kernel/ata.c — storage and filesystem primitives
- src/kernel/malloc.c — simple kmalloc/kfree semantics

If this file already exists

- No pre-existing .github/copilot-instructions.md was found; this file is created to be the canonical quick-reference for Copilot sessions.

Summary

- Added a concise Copilot instructions file describing how to build, run, and debug the OS; the big-picture architecture; and repository-specific conventions (toolchain, boot semantics, task/syscall patterns, and FAT32 assumptions).

If you'd like adjustments (more detail on syscall numbers, example gdb workflow, or adding quick reference snippets for common edits), say which area to expand.
