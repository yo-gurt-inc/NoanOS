# Compiler and tools
ASM      = nasm
CC       = i686-linux-gnu-gcc
LD       = i686-linux-gnu-ld
OBJCOPY  = i686-linux-gnu-objcopy

# Directories
BUILD_DIR    = build/obj
IMG_DIR      = build/img
BIN_DIR      = build/bin
BOOT_SRC_DIR = boot
USER_DIR     = user
TOOLS_DIR    = tools

# musl-linked ELF programs (drop .c files in rootfs_src/)
MUSL         = /root/NoanOS/musl-noan
MUSL_CC      = $(CC) -m32 -march=i686 -static -nostdlib \
               -I$(MUSL)/include -Wl,-Ttext-segment=0x200000 \
               $(MUSL)/lib/crt1.o $(MUSL)/lib/crti.o
MUSL_LIBS    = $(MUSL)/lib/libc.a $(MUSL)/lib/crtn.o -lgcc -lgcc_eh
ROOTFS_SRCS   = $(wildcard rootfs_src/*.c)
ROOTFS_BINS   = $(patsubst rootfs_src/%.c,rootfs/bin/%,$(ROOTFS_SRCS))
NOSTDLIB_SRCS = $(wildcard rootfs_src/nostdlib/*.c)
NOSTDLIB_BINS = $(patsubst rootfs_src/nostdlib/%.c,rootfs/bin/%,$(NOSTDLIB_SRCS))
NOSTDLIB_SRCS = $(wildcard rootfs_src/nostdlib/*.c)
NOSTDLIB_BINS = $(patsubst rootfs_src/nostdlib/%.c,rootfs/bin/%,$(NOSTDLIB_SRCS))

# Flags
CFLAGS   = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32 -march=i686 -fno-pie -fno-stack-protector -nostdlib -nostartfiles
LDFLAGS  = -T $(BOOT_SRC_DIR)/linker.ld --oformat elf32-i386 -m elf_i386
ASMFLAGS = -f elf32

# Source files
BOOT_SRC   = $(BOOT_SRC_DIR)/boot.asm
KERNEL_ASM = kernel.asm interrupts.asm
KERNEL_C   = kernel/core/kernel.c kernel/core/malloc.c kernel/core/memory_map.c kernel/core/panic.c kernel/core/initrd.c kernel/core/installer.c \
             kernel/cpu/gdt.c kernel/cpu/idt.c kernel/cpu/task.c kernel/cpu/syscall.c kernel/cpu/paging.c \
             kernel/io/kprint.c kernel/io/terminal.c kernel/io/keyboard.c kernel/io/framebuffer.c kernel/io/serial.c \
             kernel/storage/ata.c kernel/storage/fat32.c kernel/storage/fat32_fat.c kernel/storage/fat32_path.c kernel/storage/fat32_dir.c kernel/storage/fat32_file.c kernel/storage/elf.c kernel/storage/noan.c \
             kernel/system/timer.c kernel/system/rtc.c kernel/system/power.c
SH_C       = userspace/shell/shell.c \
             userspace/shell/commands/commands.c userspace/shell/commands/sh_uptime.c userspace/shell/commands/sh_ls.c userspace/shell/commands/sh_cd.c \
             userspace/shell/commands/sh_pwd.c userspace/shell/commands/sh_mkdir.c userspace/shell/commands/sh_rm.c userspace/shell/commands/sh_cp.c \
             userspace/shell/commands/sh_mv.c userspace/shell/commands/sh_touch.c userspace/shell/commands/sh_cat.c userspace/shell/commands/sh_echo.c \
             userspace/shell/commands/sh_clear.c userspace/shell/commands/sh_help.c userspace/shell/commands/sh_reboot.c userspace/shell/commands/sh_shutdown.c \
             userspace/shell/commands/sh_vim.c userspace/shell/commands/sh_panic.c userspace/shell/commands/sh_date.c userspace/shell/commands/sh_mem.c \
             userspace/shell/commands/sh_disks.c userspace/shell/commands/sh_stat.c

# User sources
USER_SRC  = $(wildcard $(USER_DIR)/*.c)
USER_BINS = $(patsubst $(USER_DIR)/%.c,$(BIN_DIR)/%,$(USER_SRC))

# Output files
BOOT_BIN   = $(BUILD_DIR)/boot.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
INITRD_BIN = $(BUILD_DIR)/initrd.bin

# Object files
KERNEL_ASM_O = $(patsubst %.asm,$(BUILD_DIR)/%_asm.o,$(KERNEL_ASM))
KERNEL_C_O   = $(addprefix $(BUILD_DIR)/,$(notdir $(KERNEL_C:.c=.o)))
SH_C_O       = $(addprefix $(BUILD_DIR)/,$(notdir $(SH_C:.c=.o)))

vpath %.asm $(BOOT_SRC_DIR)
vpath %.c kernel/core kernel/cpu kernel/io kernel/storage kernel/system userspace/shell userspace/shell/commands

DISK_IMG = $(IMG_DIR)/disk.img
HDD_IMG  = $(IMG_DIR)/hdd.img

# Progress tracking
TOTAL_FILES := $(words $(KERNEL_C) $(SH_C) $(KERNEL_ASM) $(USER_SRC))
CURRENT     := 0

# Default target
all: $(DISK_IMG) $(HDD_IMG)
	@printf "\033[0;32mBuild complete!\033[0m\n"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(IMG_DIR):
	mkdir -p $(IMG_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BOOT_BIN): $(BOOT_SRC) | $(BUILD_DIR)
	@printf "\033[0;33m[ASM]\033[0m Assembling \033[0;36mboot.asm\033[0m\n"
	@$(ASM) -f bin $(BOOT_SRC) -o $(BOOT_BIN)

$(BUILD_DIR)/%_asm.o: %.asm | $(BUILD_DIR)
	$(eval CURRENT := $(shell echo $$(($(CURRENT)+1))))
	@printf "\033[0;33m[%2d/$(TOTAL_FILES)]\033[0m Assembling \033[0;36m$(<F)\033[0m\n" $(CURRENT)
	@$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(eval CURRENT := $(shell echo $$(($(CURRENT)+1))))
	@printf "\033[0;32m[%2d/$(TOTAL_FILES)]\033[0m Compiling \033[0;36m$(<F)\033[0m\n" $(CURRENT)
	@$(CC) $(CFLAGS) -I kernel/include -I userspace/include -c $< -o $@

$(KERNEL_ELF): $(KERNEL_ASM_O) $(KERNEL_C_O)
	@printf "\033[0;35m[LD]\033[0m Linking kernel...\n"
	@$(LD) $(LDFLAGS) $^ -o $@

$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD_DIR)
	@printf "\033[0;35m[BIN]\033[0m Converting to binary...\n"
	@$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

$(BIN_DIR)/%: $(USER_DIR)/%.c | $(BIN_DIR) $(BUILD_DIR)
	$(eval CURRENT := $(shell echo $$(($(CURRENT)+1))))
	@printf "\033[0;34m[%2d/$(TOTAL_FILES)]\033[0m Building user program: \033[0;36m$*\033[0m\n" $(CURRENT)
	@$(CC) $(CFLAGS) -I kernel/include -c $< -o $(BUILD_DIR)/$*.o
	@$(LD) -Ttext 0xA00000 --entry _start --oformat binary -m elf_i386 $(BUILD_DIR)/$*.o -o $(BUILD_DIR)/$*.flat
	@python3 $(TOOLS_DIR)/mknoan.py $(BUILD_DIR)/$*.flat $(BIN_DIR)/$*

rootfs/bin:
	@mkdir -p rootfs/bin

rootfs/bin/%: rootfs_src/%.c | rootfs/bin
	@printf "\033[0;36m[MUSL]\033[0m Compiling \033[0;36m$*\033[0m\n"
	@$(MUSL_CC) $< $(MUSL_LIBS) -o $@

rootfs/bin/%: rootfs_src/nostdlib/%.c | rootfs/bin
	@printf "\033[0;36m[ELF]\033[0m Compiling \033[0;36m$*\033[0m\n"
	@$(CC) -m32 -march=i686 -static -nostdlib -nostartfiles \
		-Wl,-Ttext-segment=0x200000 $< -o $@
ROOTFS_FILES = $(shell find rootfs -type f 2>/dev/null | grep -v README)

$(INITRD_BIN): $(SH_C_O) $(USER_BINS) $(ROOTFS_FILES) $(ROOTFS_BINS) $(NOSTDLIB_BINS) | $(BUILD_DIR)
	@printf "\033[0;35m[LD]\033[0m Linking shell...\n"
	@$(LD) -Ttext 0x800000 --entry _start --oformat binary -m elf_i386 $(SH_C_O) -o $(BUILD_DIR)/shell.bin
	@printf "\033[0;33m[INITRD]\033[0m Creating Noan Archive (NARC)...\n"
	@python3 $(TOOLS_DIR)/mkinitrd.py $(INITRD_BIN) $(BUILD_DIR)/shell.bin $(BIN_DIR)

$(DISK_IMG): $(BOOT_BIN) $(KERNEL_BIN) $(INITRD_BIN) | $(IMG_DIR)
	@printf "\033[0;33m[IMG]\033[0m Building disk image...\n"
	@rm -f $(DISK_IMG)
	@dd if=/dev/zero of=$(DISK_IMG) bs=512 count=20480 2>/dev/null
	@dd if=$(BOOT_BIN)   of=$(DISK_IMG) conv=notrunc bs=512 seek=0 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$(DISK_IMG) conv=notrunc bs=512 seek=1 2>/dev/null
	@dd if=$(INITRD_BIN) of=$(DISK_IMG) conv=notrunc bs=512 seek=121 2>/dev/null
	@printf "LIVE" | dd of=$(DISK_IMG) bs=1 seek=130560 conv=notrunc 2>/dev/null

$(HDD_IMG): | $(IMG_DIR)
	@printf "\033[0;33m[IMG]\033[0m Creating blank HDD image...\n"
	@dd if=/dev/zero of=$(HDD_IMG) bs=512 count=20480 2>/dev/null
	@printf "\033[0;33m[IMG]\033[0m Formatting HDD as FAT32...\n"
	@MTOOLSRC=/dev/null mformat -i $(HDD_IMG) \
		-v "SIMPLE OS " \
		-N 12345678 \
		-R 128 \
		-L 160 \
		-c 8 \
		-F ::
	@python3 -c "import struct; f=open('$(HDD_IMG)','r+b'); f.seek(510); f.write(b'\x00\x00'); f.close()"
	@printf "\033[0;33m[IMG]\033[0m Populating HDD from rootfs/...\n"
	@if [ -d rootfs ]; then \
		find rootfs -mindepth 1 -type d | sort | while read dir; do \
			fat_path=$$(echo "$$dir" | sed 's|^rootfs||'); \
			MTOOLSRC=/dev/null mmd -i $(HDD_IMG) "::$$fat_path" 2>/dev/null || true; \
		done; \
		find rootfs -type f ! -name 'README' | while read f; do \
			fat_path=$$(echo "$$f" | sed 's|^rootfs||'); \
			MTOOLSRC=/dev/null mcopy -i $(HDD_IMG) "$$f" "::$$fat_path" && \
			printf "  \033[0;36m%s\033[0m\n" "$$fat_path" || \
			printf "  \033[0;31mFailed:\033[0m $$f\n"; \
		done; \
	fi

run: $(DISK_IMG) $(HDD_IMG)
	pkill -f "qemu-system-i[3]86" || true
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG),index=0 -drive format=raw,file=$(HDD_IMG),index=1 \
		-serial file:serial.log &
	@sleep 3
	@echo "=== serial.log ===" && cat serial.log

run-installed: $(HDD_IMG)
	pkill -f "qemu-system-i[3]86" || true
	qemu-system-i386 -drive format=raw,file=$(HDD_IMG),index=0

clean:
	@printf "\033[0;31m[CLEAN]\033[0m Removing build artifacts...\n"
	@rm -rf $(BUILD_DIR) $(IMG_DIR) $(BIN_DIR)

.PHONY: all run run-installed clean
