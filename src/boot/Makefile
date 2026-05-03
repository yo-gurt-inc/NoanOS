# Compiler and tools
ASM = nasm
CC = gcc
LD = ld
OBJCOPY = objcopy

# Directories
BUILD_DIR = build
BIN_DIR = bin

# Flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32 -march=i686 -fno-pie -fno-stack-protector -nostdlib -nostartfiles
LDFLAGS = -T linker.ld --oformat elf32-i386 -m elf_i386
ASMFLAGS = -f elf32

# Source files
BOOT_SRC = boot.asm
KERNEL_ASM = kernel.asm interrupts.asm
KERNEL_C = ../kernel/kernel.c ../kernel/kprint.c ../kernel/terminal.c ../kernel/idt.c ../kernel/keyboard.c ../kernel/shell.c ../kernel/malloc.c ../kernel/ata.c ../kernel/fat32.c ../kernel/installer.c ../kernel/timer.c ../kernel/rtc.c ../kernel/power.c ../kernel/gdt.c ../kernel/task.c ../kernel/syscall.c ../kernel/elf.c

# Output files
BOOT_BIN = $(BIN_DIR)/boot.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BIN_DIR)/kernel.bin

# Object files
KERNEL_ASM_O = $(patsubst %.asm,$(BUILD_DIR)/%_asm.o,$(KERNEL_ASM))
KERNEL_C_O = $(patsubst ../kernel/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C))

DISK_IMG = $(BIN_DIR)/disk.img
HDD_IMG = $(BIN_DIR)/hdd.img

# Default target
all: $(DISK_IMG) $(HDD_IMG)

# Create necessary directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build bootloader binary
$(BOOT_BIN): $(BOOT_SRC) | $(BIN_DIR)
	$(ASM) -f bin $(BOOT_SRC) -o $(BOOT_BIN)

# Build kernel assembly object files
$(BUILD_DIR)/%_asm.o: %.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Build kernel C object files
$(BUILD_DIR)/%.o: ../kernel/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I ../kernel -c $< -o $@
	@echo "Built C object: $@"

# Link kernel ELF
$(KERNEL_ELF): $(KERNEL_ASM_O) $(KERNEL_C_O)
	$(LD) $(LDFLAGS) $^ -o $@

# Convert ELF to flat binary
$(KERNEL_BIN): $(KERNEL_ELF) | $(BIN_DIR)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

# Create bootable disk image (Live Disk)
$(DISK_IMG): $(BOOT_BIN) $(KERNEL_BIN) | $(BIN_DIR)
	rm -f $(DISK_IMG)
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=256
	dd if=$(BOOT_BIN)   of=$(DISK_IMG) conv=notrunc bs=512 seek=0
	dd if=$(KERNEL_BIN) of=$(DISK_IMG) conv=notrunc bs=512 seek=1
	printf "LIVE" | dd of=$(DISK_IMG) bs=1 seek=130560 conv=notrunc

# Create a blank 10MB secondary hard drive for installation
$(HDD_IMG): | $(BIN_DIR)
	dd if=/dev/zero of=$(HDD_IMG) bs=1M count=10

# Run in QEMU (Live Disk is index 0, HDD is index 1)
run: $(DISK_IMG) $(HDD_IMG)
	pkill -f "qemu-system-i[3]86" || true
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG),index=0 -drive format=raw,file=$(HDD_IMG),index=1 -vnc :0 -k en-us

# Run in QEMU (Boot from installed HDD)
run-installed: $(HDD_IMG)
	pkill -f "qemu-system-i[3]86" || true
	qemu-system-i386 -drive format=raw,file=$(HDD_IMG),index=0,media=disk -vnc :0 -k en-us

# Run with GDB debug info
debug: $(DISK_IMG) $(HDD_IMG)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG),index=0,media=disk -drive format=raw,file=$(HDD_IMG),index=1,media=disk -s -S

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all run run-installed debug clean
