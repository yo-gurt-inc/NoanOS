[BITS 32]
global _start           ; ELF entry point
extern kmain           ; C function

section .text

; Export interrupt handlers for C code
 
    
_start:
    mov word [0xB8000], 0x0F4B ; 'K' on black, bright white
    
    mov esp, 0x9C00

    ; Save boot args before we clobber registers
    mov [boot_drive_save], eax
    mov [initrd_save], esi

    ; Zero BSS
    extern bss_start
    extern bss_end
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Restore and call kmain(boot_drive, initrd_addr)
    mov eax, [boot_drive_save]
    mov esi, [initrd_save]
    push esi
    push eax
    call kmain
    
    ; Halt if kmain returns
    cli
    hlt
    jmp $

global gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

global tss_flush
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

section .data
boot_drive_save dd 0
initrd_save     dd 0

