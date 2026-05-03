BITS 16
ORG 0x7C00

; FAT32 BPB Header (Required for filesystem compatibility)
jmp short start
nop

oem_name            db "SIMPLEOS"
bytes_per_sector    dw 512
sectors_per_cluster db 1
reserved_sectors    dw 128      ; Reserve space for bootloader + kernel
num_fats            db 2
root_entry_count    dw 0
total_sectors_16    dw 0
media_type          db 0xF8
fat_size_16         dw 0
sectors_per_track   dw 18
num_heads           dw 2
hidden_sectors      dd 0
total_sectors_32    dd 20480    ; 10MB default

; FAT32 Extended BPB
sectors_per_fat_32  dd 160
ext_flags           dw 0
fs_version          dw 0
root_cluster        dd 2
fs_info             dw 1
backup_boot_sector  dw 6
times 12 db 0                   ; Reserved
drive_number        db 0x80
reserved1           db 0
boot_signature      db 0x29
volume_id           dd 0x12345678
volume_label        db "SIMPLE OS  "
fs_type             db "FAT32   "

code_offset     equ 0x08
data_offset     equ 0x10
kernel_segment  equ 0x1000  ; ES
kernel_sectors  equ 64      ; # of sectors to read
kernel_offset   equ 0x0000  ; Offset into segment
kernel_start_addr equ 0x100000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    ; If we booted from 0x80 (Live Disk), check if 0x81 (HDD) is bootable
    cmp dl, 0x80
    jne .skip_smart_boot

    ; Try to read HDD (0x81) boot sector
    mov dl, 0x81
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 1
    mov dh, 0
    mov bx, 0x9000      ; Safe temporary address
    int 0x13
    jc .skip_smart_boot ; Error reading HDD

    cmp word [bx + 510], 0xAA55
    jne .skip_smart_boot ; HDD not bootable

    ; HDD is bootable! Give the user a choice
    mov si, smart_boot_msg
    call print_string_16

    ; Wait ~3 seconds for 'I' key
    mov ah, 0x00
    int 0x1A            ; Get system ticks
    mov bx, dx
    add bx, 54          ; ~3 seconds

.wait_loop:
    mov ah, 0x01
    int 0x16            ; Check for key
    jnz .key_pressed
    
    mov ah, 0x00
    int 0x1A
    cmp dx, bx
    jl .wait_loop
    jmp .do_chainload   ; Timeout, boot HDD

.key_pressed:
    mov ah, 0x00
    int 0x16            ; Get key
    cmp al, 'i'
    je .skip_smart_boot
    cmp al, 'I'
    je .skip_smart_boot

.do_chainload:
    mov dl, 0x81
    jmp 0x0000:0x9000   ; Jump to the loaded HDD bootloader

.skip_smart_boot:
    mov dl, [boot_drive]

    ; Diagnostic: 'B'
    mov ah, 0x0E
    mov al, 'B'
    int 0x10

    mov ax, kernel_segment
    mov es, ax
    mov bx, kernel_offset

    mov ah, 0x02
    mov al, kernel_sectors
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Load GDT and enter PM
    lgdt [gdt_descriptor]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp code_offset:PModemain

disk_error:
    mov si, msg
.next:
    lodsb
    or al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .next

print_string_16:
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

[BITS 32]
PModemain:
    mov ax, data_offset
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x9C00

    ; A20
    in al, 0x92
    or al, 2
    out 0x92, al

    mov esi, 0x10000                    ; Source
    mov edi, kernel_start_addr          ; Destination (0x100000)
    mov ecx, kernel_sectors * 512 / 4   ; Size in dwords
    rep movsd                           ; Copy
    
    xor eax, eax
    mov al, [boot_drive]                ; Pass boot drive number
    jmp kernel_start_addr

; Data
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

msg db "Err", 0
smart_boot_msg db "Booting HDD... (I: Installer)", 13, 10, 0
boot_drive db 0

times 510 - ($-$$) db 0
dw 0xAA55
