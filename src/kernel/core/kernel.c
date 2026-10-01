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
    serial_puts("[kernel] drive_idx=");
    serial_dec(drive_idx);
    serial_puts("\n");
    ata_drive_t* boot_disk = ata_get_drive(drive_idx);
    serial_puts("[kernel] boot_disk=");
    serial_dec(boot_disk ? 1 : 0);
    serial_puts(" exists=");
    serial_dec(boot_disk ? boot_disk->exists : 0);
    serial_puts("\n");

    /* Read the boot-mode marker: "LIVE" -> installer menu, "AUTO" -> install
     * without any input then reboot (make test boot 1), "TEST" -> run every
     * binary under /bin (make test boot 2), anything else -> normal boot. */
    int mode_auto = 0;
    int mode_test = 0;
    if (boot_disk && boot_disk->exists) {
        u8* buf = (u8*)kmalloc(512);
        serial_puts("[kernel] reading marker sector\n");
        ata_read_sectors(boot_disk, INSTALLER_MARKER_SECTOR, 1, (u16*)buf);
        serial_puts("[kernel] marker sector read OK\n");

        if (buf[0] == 'L' && buf[1] == 'I' && buf[2] == 'V' && buf[3] == 'E') {
            is_live = 1;
        } else if (buf[0] == 'A' && buf[1] == 'U' && buf[2] == 'T' && buf[3] == 'O') {
            mode_auto = 1;
        } else if (buf[0] == 'T' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 'T') {
            mode_test = 1;
        }
        kfree(buf);
    }
    serial_puts("[kernel] is_live=");
    serial_dec(is_live);
    serial_puts(" auto=");
    serial_dec(mode_auto);
    serial_puts(" test=");
    serial_dec(mode_test);
    serial_puts("\n");

    if (mode_auto) {
        /* Test-mode boot 1: mirror screen output to serial, auto-install to
         * the first non-boot drive, then reboot into the installed OS. */
        task_switch_quiet = 1;   /* per-switch chatter throttles the guest */
        terminal_set_serial_mirror(1);
        serial_puts("[MODE=AUTO]\n");
        installer_auto(boot_drive);
        serial_puts("[AUTOINSTALL] FAILED, halting\n");
        while(1) { asm volatile("hlt"); }
    }

    if (mode_test) {
        /* Test-mode boot 2: mirror screen output to serial and run the test
         * runner instead of the shell. */
        task_switch_quiet = 1;   /* per-switch chatter throttles the guest */
        terminal_set_serial_mirror(1);
        serial_puts("[MODE=TEST]\n");
        if (boot_disk && boot_disk->exists) {
            serial_puts("[kernel] fat32_init\n");
            fat32_init(boot_disk);
            serial_puts("[kernel] fat32_init OK\n");
        }
        void* runner_entry = (void*)noan_load("runtests");
        serial_puts("[kernel] noan_load runtests result=");
        serial_dec((u32)runner_entry);
        serial_puts("\n");
        if (runner_entry) {
            serial_puts("[kernel] task_create(runtests)\n");
            task_create(runner_entry, 0x1, 0);
            serial_puts("[kernel] task_yield\n");
            task_yield();
            serial_puts("[kernel] back from yield\n");
        } else {
            kprint("runtests not found on disk!\n");
        }

        /* The sweep is done (or the runner was missing). Clear the TEST
         * marker so the next boot starts normally, then hand over to the
         * installed shell - ending on a usable prompt instead of halting on
         * the summary screen. */
        if (boot_disk && boot_disk->exists) {
            u8* zbuf = (u8*)kmalloc(512);
            if (zbuf) {
                for (int z = 0; z < 512; z++) zbuf[z] = 0;
                ata_write_sectors(boot_disk, INSTALLER_MARKER_SECTOR, 1, (u16*)zbuf);
                kfree(zbuf);
                serial_puts("[TESTMODE] marker cleared\n");
            }
        }
        kprint("=== Test sweep finished - starting shell ===\n");
        void* shell_entry = (void*)noan_load("shell");
        serial_puts("[kernel] noan_load shell result=");
        serial_dec((u32)shell_entry);
        serial_puts("\n");
        if (shell_entry) {
            task_create(shell_entry, 0x1, 0);
            task_yield();
            serial_puts("[kernel] back from yield\n");
        }
        while(1) { asm volatile("hlt"); }
    }

    if (is_live) {
        if (installer_start(boot_drive)) {
            while(1) { asm volatile("hlt"); }
        }
    } else {
        if (boot_disk && boot_disk->exists) {
            serial_puts("[kernel] fat32_init\n");
            fat32_init(boot_disk);
            serial_puts("[kernel] fat32_init OK\n");
        }
    }
    
    void* shell_entry_addr = 0;
    if (!is_live) {
        serial_puts("[kernel] noan_load shell\n");
        shell_entry_addr = (void*)noan_load("shell");
        serial_puts("[kernel] noan_load result=");
        serial_dec((u32)shell_entry_addr);
        serial_puts("\n");
        if (shell_entry_addr == 0) {
            shell_entry_addr = initrd_get_entry();
            serial_puts("[kernel] using initrd entry\n");
        }
    } else {
        shell_entry_addr = initrd_get_entry();
    }

    serial_puts("[kernel] shell_entry=");
    serial_dec((u32)shell_entry_addr);
    serial_puts("\n");
    if (shell_entry_addr) {
        serial_puts("[kernel] task_create\n");
        task_create(shell_entry_addr, 0x1, 0);
        serial_puts("[kernel] task_yield\n");
        task_yield();
        serial_puts("[kernel] back from yield\n");
    } else {
        panic("No shell entry point");
    }
    
    while(1) {
        asm volatile("hlt");
    }
}
