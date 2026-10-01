# NoanOS
Hobby x86 bare-metal OS in C/ASM with FAT32, ATA, ELF loading, musl userspace, and a shell. Python scripts build the disk images.

## Layout (all source is under src/)
- src/boot/        : boot.asm, interrupts.asm, kernel.asm, linker.ld
- src/kernel/core/    : kernel.c, pmm.c, malloc.c, initrd.c, installer.c, panic.c, usermem.c, memory_map.c
- src/kernel/cpu/     : gdt, idt, paging, syscall, task
- src/kernel/io/      : framebuffer, keyboard, serial, kprint, terminal
- src/kernel/storage/ : ata, elf, fat32*.c (dir/fat/file/path), noan.c
- src/kernel/system/  : power, rtc, timer
- src/kernel/include/ : headers, mirrors the dirs above
- src/userspace/shell/ : shell.c, installer.c, commands/sh_*.c (one file per command)
- src/user/        : small userland programs (cat, ls, rm...) + libnoan.h
- src/rootfs_src/  : test programs compiled into rootfs
- src/tools/       : make-hdd.py, mkinitrd.py, mknoan.py, run_tests.py
- src/Makefile, src/build.sh : build entry points
- libnoan_cxx_*.c  : C++ stubs (repo root)

## Never read
- musl-noan/, musl-hello/, musl-noan-cxx-shim/ (vendored musl headers/libs, ignore unless I name a file)
- bin/, build/, src/build/, src/rootfs/bin/, *.img, *.bin, *.o, *.a, *.elf, *.log

## Rules
- Don't scan the repo. Grep/glob first, then read only the needed files, in ranges if large.
- Shell commands live in their own sh_*.c file; edit only that file plus commands.c to register.
- If unsure which file to edit, ask me.
- Before opening any file, grep SYMBOLS.txt (format file:line name) and read only that line range; regenerate it after adding functions

## Build / test
- Build: cd src && ./build.sh   (or make)
- Test: python3 src/tools/run_tests.py
