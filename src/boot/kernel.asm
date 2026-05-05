[BITS 32]
global _start           ; ELF entry point
extern kmain           ; C function

section .text

; Export interrupt handlers for C code
 
    
_start:                ; This will be at 0x100000 when loaded
    ; Diagnostic 'K'
    mov word [0xB8000], 0x0F4B ; 'K' on black, bright white
    
    ; Set up stack if needed
    mov esp, 0x9C00    ; Or wherever you want your kernel stack
    
    ; Push the initrd address passed in ESI from the bootloader
    push esi
    ; Push the boot drive passed in EAX from the bootloader
    push eax
    
    ; Call your C main function
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
    mov ax, 0x2B        ; Index of TSS (5 * 8 = 40 = 0x28) | RPL 3 = 0x2B
    ltr ax
    ret

