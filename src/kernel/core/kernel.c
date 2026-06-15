#include "io/kprint.h"
#include "io/terminal.h"
#include "cpu/idt.h"
#include "io/keyboard.h"

#include "core/malloc.h"
#include "storage/ata.h"
#include "storage/fat32.h"
#include "core/installer.h"
#include "system/timer.h"
#include "cpu/gdt.h"
#include "cpu/task.h"
#include "cpu/syscall.h"
#include "cpu/paging.h"
#include "io/serial.h"
#include "core/initrd.h"
#include "core/panic.h"
#include "storage/noan.h"

void kmain(u32 boot_drive_raw, u32 initrd_addr) {
    u32 boot_drive = boot_drive_raw & 0xFF;
    serial_init();
    serial_puts("[kernel] boot drive=");
    serial_dec(boot_drive);
    serial_puts("\n");
    kprint_init();

    malloc_init(0x400000, 4 * 1024 * 1024);
    serial_puts("[kernel] malloc OK\n");
    
    gdt_init();
    serial_puts("[kernel] gdt OK\n");
    idt_init();
    serial_puts("[kernel] idt OK\n");
    paging_init();
    serial_puts("[kernel] paging OK\n");
    syscall_init();
    serial_puts("[kernel] syscall OK\n");
    task_init();
    serial_puts("[kernel] task OK\n");
    /* Set the idle/kernel task to use the kernel page directory */
    get_current_process()->page_dir = kernel_page_dir;
    timer_init(100);
    serial_puts("[kernel] timer OK\n");
    keyboard_init();
    serial_puts("[kernel] keyboard OK\n");
    ata_init();
    serial_puts("[kernel] ata OK\n");
    asm volatile("sti");
    serial_puts("[kernel] sti OK\n");

    if (initrd_unpack(initrd_addr) < 0) {
        panic("INITRD not found or corrupt");
    }
    serial_puts("[kernel] initrd OK\n");

    int is_live = 0;
    int drive_idx = (boot_drive >= 0x80) ? (boot_drive - 0x80) : 0;
    ata_drive_t* boot_disk = ata_get_drive(drive_idx);
    
    if (boot_disk && boot_disk->exists) {
        u8* buf = (u8*)kmalloc(512);
        ata_read_sectors(boot_disk, 255, 1, (u16*)buf);
        
        if (buf[0] == 'L' && buf[1] == 'I' && buf[2] == 'V' && buf[3] == 'E') {
            is_live = 1;
        }
        kfree(buf);
    }

    if (is_live) {
        if (installer_start(boot_drive)) {
            while(1) { asm volatile("hlt"); }
        }
    } else {
        if (boot_disk && boot_disk->exists) {
            fat32_init(boot_disk);
        }
    }
    
    void* shell_entry_addr = 0;
    if (!is_live) {
        shell_entry_addr = (void*)noan_load("shell");
        if (shell_entry_addr == 0) {
            shell_entry_addr = initrd_get_entry();
        }
    } else {
        shell_entry_addr = initrd_get_entry();
    }

    if (shell_entry_addr) {
        task_create(shell_entry_addr, 0x1, 0);
        task_yield();
    } else {
        panic("No shell entry point");
    }
    
    while(1) {
        asm volatile("hlt");
    }
}
