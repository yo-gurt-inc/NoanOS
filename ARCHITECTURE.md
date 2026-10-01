# NoanOS Architecture (read this instead of exploring)

32-bit x86 hobby OS. C + NASM. Paths below are relative to `src/` unless noted.
Verified against the source.

## Boot flow
1. `boot/boot.asm`: 16-bit MBR at 0x7C00. Loads kernel (segment 0x1000, 120 sectors) and initrd (segment 0x2000, 512 sectors), checks initrd magic `NARC`. If an HDD is present it prompts "Press I for Installer". Enters protected mode at `PModemain`, kernel ends up at 0x100000, initrd at 0x200000.
2. `boot/kernel.asm`: `_start` saves boot drive, calls `kmain`. Also defines `gdt_flush`, `tss_flush`.
3. `kernel/core/kernel.c`: `kmain(boot_drive, initrd_addr)`. Init order: serial -> kprint -> malloc (0x400000, 4MB) -> gdt -> idt -> paging -> syscall -> task -> timer(100Hz) -> keyboard -> ata -> `sti` -> `initrd_unpack`.
   Then reads marker sector 700 (`INSTALLER_MARKER_SECTOR`) on the boot disk: `LIVE` = installer menu (`installer_start`), `AUTO` = `installer_auto` then reboot (test boot 1), `TEST` = run `runtests` then clear marker and start shell (test boot 2), anything else = normal boot.
   Normal boot: `fat32_init(boot_disk)` -> `noan_load("shell")` from disk, falling back to `initrd_get_entry()` -> `task_create` + `task_yield`.
4. `userspace/shell/shell.c`: `_start` -> `shell_main`, the interactive shell.

## Interrupts
- `boot/interrupts.asm`: ISR/IRQ stubs -> `common_stub` -> C handlers `isr_handler`, `irq_handler`, `syscall_handler` (all in `kernel/cpu/`).
- `cpu/idt.c`: IDT setup, `pic_remap`, `irq_install_handler(irq, fn)`. Handlers take and return `esp` (task switch happens by returning a different esp).
- IRQ users: `system/timer.c` (`timer_callback`), `io/keyboard.c` (`keyboard_handler`).

## Memory
- `core/pmm.c`: bitmap physical allocator, 32MB in 4KB blocks (`pmm_alloc`/`pmm_free`).
- `core/malloc.c`: kernel heap (`kmalloc`/`kfree`, free list).
- `cpu/paging.c`: per-process page dirs, `paging_clone_dir_cow` (fork), `paging_map_page`, `paging_load_dir`.
- `core/usermem.c`: safe user<->kernel copies (`copy_from_user`, `copy_to_user`, `is_user_addr_valid`). User space is 0x08000000-0xC0000000.
- `core/memory_map.c`: BIOS memory map detection/printing.

## Processes (`cpu/task.c`, header `include/cpu/task.h`)
- `process_t`: registers, `page_dir`, `fds[MAX_FDS=16]`, `cwd`, `brk_end`, `is_elf`, linked list `next`. `MAX_PROCESSES=16`.
- `task_create`, `task_fork`, `task_exec`, `task_switch`, `task_yield`, `task_kill_current`.
- `is_elf == 1`: Linux i386 ABI (musl binaries). `is_elf == 0`: native NoanOS ABI.

## Syscalls (`cpu/syscall.c`, numbers in `include/cpu/syscall.h`)
- `syscall_handler(esp)` is the dispatcher. If the current process has `is_elf`, it routes to `linux_syscall()` (Linux i386 numbers, `LINUX_SYS_*` defined at top of syscall.c). Otherwise it handles native `SYS_*` numbers.
- Native syscalls are thin wrappers over kernel/fat32/terminal functions (`SYS_LS` -> `fat32_ls`, etc.).
- Adding a native syscall: add `SYS_X` in `include/cpu/syscall.h`, handle in `syscall_handler`, add wrapper in `userspace/include/shell/noan.h` (and `user/libnoan.h` if used by user programs).
- Adding a Linux syscall: add `LINUX_SYS_X`, handle in `linux_syscall`.

## Storage
- `storage/ata.c`: PIO ATA driver, up to 4 drives (`ata_read_sectors`, `ata_write_sectors`).
- FAT32 is split by concern: `fat32.c` (init/format/getters), `fat32_fat.c` (FAT table, clusters), `fat32_path.c` (8.3 + LFN names, entry lookup/create, path resolution), `fat32_dir.c` (ls, cd, mkdir), `fat32_file.c` (cat, read, echo, touch, rm, cp, mv, stat). Private helpers are prefixed `_fat32_`.
- Call chain: syscall -> `fat32_*` (file/dir) -> `_fat32_find_entry` / `_fat32_find_full_path_file` (path) -> `_fat32_get_fat_entry` / `_fat32_set_fat_entry` (fat) -> `ata_*_sectors`.
- `storage/noan.c`: native binary loader. Format = `noan_header_t` (magic `NOAN`, entry_point, code_size, data_size). `noan_load`, `noan_execute`.
- `storage/elf.c`: `elf_load_file(path)` returns a `process_t*` for Linux ELF binaries (PT_LOAD segments).

## I/O
- `io/terminal.c`: VGA text mode 80x25, ANSI escape parsing, optional mirror to serial (`terminal_set_serial_mirror`).
- `io/serial.c`: COM1 output. Used by tests and crash dumps.
- `io/keyboard.c`: scancode -> ASCII, 256-byte ring buffer, shift/ctrl/caps state, `keyboard_getchar`, `keyboard_flush`.
- `io/framebuffer.c`: VBE framebuffer helpers. `io/kprint.c`: kernel printing (`kprint`, `kprint_hex`, `kprint_dec`).

## Core misc
- `core/initrd.c`: unpacks the `NARC` archive (shell + user programs) in memory.
- `core/installer.c`: kernel-side installer (copies OS to HDD, marker sector 700). Userspace twin: `userspace/shell/installer.c`.
- `core/panic.c`, `system/power.c` (reboot/shutdown), `system/rtc.c`, `system/timer.c`.

## Userspace
- `userspace/shell/commands/commands.c`: `commands[]` table (name, handler, help) and `execute_command(s)`. One file per command: `sh_<name>.c`. To add a command: new `sh_x.c`, declare in `userspace/include/shell/commands.h`, register in the `commands[]` table.
- `userspace/include/shell/noan.h`: inline syscall wrappers (`noan_*`).
- `user/*.c` (cat, ls, echo, ...): standalone native programs using `user/libnoan.h`. `user/runtests.c`: in-OS test runner.
- `rootfs_src/*.c`: test programs (musl-linked). `rootfs/bin/`: built outputs, never read.

## Build and tools
- Toolchain: `nasm`, `i686-linux-gnu-gcc/ld/objcopy` (cross), gcc-15 paths hardcoded in Makefile for C++. Python 3 for tools, `qemu-system-i386` for run/test.
- `build.sh`: `make clean` + `make -j8`, checks initrd size; `./build.sh run` also runs QEMU.
- Make targets: `all` (builds `build/img/disk.img` + `hdd.img`), `run`, `run-installed`, `test`, `clean`. Intermediates in `build/obj`, images in `build/img`.
- musl programs: any `rootfs_src/*.c` / `*.cpp` is auto-built to `rootfs/bin/` (static, linked at 0x08000000 against `../musl-noan`). `rootfs_src/nostdlib/*.c` are raw no-libc programs.
- Adding a kernel `.c` file: it must be added to the `KERNEL_C` list in `Makefile` (no wildcard). Same for shell commands in `SH_C`.
- `tools/mkinitrd.py` packs initrd; `tools/mknoan.py` wraps a raw binary in a NOAN header; `tools/make-hdd.py` builds the FAT32 `hdd.img` (RESERVED=704 sectors, 2 FATs, 8 sectors/cluster); `tools/run_tests.py` boots the image headless and checks the serial log against expected output.

## Repo root (outside src/)
- `musl-noan/` = musl sysroot (headers + libs). Vendored, never read.
- `libnoan_cxx_stubs.c`, `libnoan_cxx_lfs_stubs.c` = stubs for C++ runtime (pthread, 64-bit file funcs, arc4random).

## Gotchas
- `syscall_handler` and the scheduler print debug lines to serial on every call, so `serial*.log` files get huge. Never read them.
- Duplicate names exist: `installer_start`, `reboot`, `wait_for_key` (kernel vs userspace installer), `_start` (every user program), `main` (every rootfs_src program), `strcmp/strcpy` (local copies in shell). Grep with the path to disambiguate.
- Static inline helpers in headers (`noan.h`, `libnoan.h`, `io.h`, `syscall.h`) are not in SYMBOLS.txt; grep them directly.
- `KNOWN_FAULTY = {test, malloc}` in `tools/run_tests.py`: those test programs are expected to fail for now.

