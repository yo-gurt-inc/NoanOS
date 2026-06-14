BITS 16
ORG 0x7C00

; FAT32 BPB Header
jmp short start
nop
oem_name            db "SIMPLEOS"
bytes_per_sector    dw 512
sectors_per_cluster db 8
reserved_sectors    dw 128
num_fats            db 2
root_entry_count    dw 0
total_sectors_16    dw 0
media_type          db 0xF8
fat_size_16         dw 0
sectors_per_track   dw 32
num_heads           dw 64
hidden_sectors      dd 0
total_sectors_32    dd 20480    ; 10MB

; FAT32 Extended BPB
sectors_per_fat_32  dd 160
ext_flags           dw 0
fs_version          dw 0
root_cluster        dd 2
fs_info             dw 1
backup_boot_sector  dw 6
times 12 db 0
drive_number        db 0x80
reserved1           db 0
boot_signature      db 0x29
volume_id           dd 0x12345678
volume_label        db "SIMPLE OS  "
fs_type             db "FAT32   "

code_offset       equ 0x08
data_offset       equ 0x10
kernel_segment    equ 0x1000
kernel_sectors    equ 120
kernel_start_addr equ 0x100000

initrd_segment    equ 0x2000
initrd_sectors    equ 512
initrd_start_addr equ 0x200000
initrd_magic      equ 0x4352414E

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; Check if we should offer Smart Boot (if we booted from 0x80 and 0x81 exists)
    cmp dl, 0x80
    jne .skip_smart
    
    ; Try to read sector 0 of 0x81 (HDD)
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 1
    mov dh, 0
    mov dl, 0x81
    mov bx, 0x9000
    int 0x13
    jc .skip_smart
    
    cmp word [0x91FE], 0xAA55
    jne .skip_smart
    
    ; HDD found! Ask user
    mov si, msg_smart
    call print_string
    
    ; Wait for 'i' for installer, otherwise boot HDD
    mov ah, 0
    int 0x16
    cmp al, 'i'
    je .skip_smart
    cmp al, 'I'
    je .skip_smart
    
    ; Boot HDD
    mov dl, 0x81
    jmp 0x0000:0x9000

.skip_smart:
    mov dl, [boot_drive]
    xor ax, ax
    int 0x13            ; Reset

    ; Load Kernel
    mov eax, 1
    mov cx, kernel_sectors
    mov bx, kernel_segment
    call read_lba

    ; Load Initrd
    mov eax, 121
    mov cx, initrd_sectors
    mov bx, initrd_segment
    call read_lba
    ; Verify Magic
    mov ax, initrd_segment
    mov es, ax
    mov eax, [es:0x0000]
    cmp eax, initrd_magic
    jne initrd_corrupt



initrd_magic_ok:

    ; Enter Protected Mode
    lgdt [gdt_descriptor]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp code_offset:PModemain

read_lba:
.loop:
    pusha
    mov [dap_lba], eax
    mov [dap_segment], bx
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    popa
    inc eax
    add bx, 0x20
    loop .loop
    ret

print_string:
    mov ah, 0x0E
.l: lodsb
    test al, al
    jz .d
    int 0x10
    jmp .l
.d: ret

disk_error: mov al, 'E'
            jmp print_err
initrd_corrupt: mov al, 'M'
print_err:
    mov ah, 0x0E
    int 0x10
    hlt

[BITS 32]
PModemain:
    mov ax, data_offset
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x9C00
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Copy Kernel
    mov esi, 0x10000
    mov edi, kernel_start_addr
    mov ecx, kernel_sectors * 512 / 4
    rep movsd
    
    ; Copy Initrd
    mov esi, 0x20000
    mov edi, initrd_start_addr
    mov ecx, initrd_sectors * 512 / 4
    rep movsd

    movzx eax, byte [boot_drive] ; Pass drive cleanly in EAX
    mov esi, initrd_start_addr   ; Pass initrd addr in ESI
    jmp kernel_start_addr

gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

align 8
dap:
    db 0x10, 0
    dw 1                ; 1 sector
    dw 0                ; Offset
dap_segment dw 0        ; Segment
dap_lba     dq 0        ; LBA

boot_drive  db 0
msg_smart   db "HDD found! Press 'I' for Installer, or any key to boot HDD.", 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
