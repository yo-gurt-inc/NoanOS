[BITS 32]

%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli
    push dword 0        ; Dummy error code
    push dword %1       ; Interrupt number
    jmp common_stub
%endmacro

%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli
    push dword %1       ; Interrupt number (error code already pushed by CPU)
    jmp common_stub
%endmacro

%macro IRQ 2
  global irq%1
  irq%1:
    cli
    push dword 0        ; Dummy error code
    push dword %2       ; Interrupt number
    jmp common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

global syscall_stub
syscall_stub:
    cli
    push dword 0        ; Dummy error code
    push dword 128      ; 0x80
    jmp common_stub

extern isr_handler
extern irq_handler
extern syscall_handler

common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Pass pointer to registers struct as Argument 1
    
    mov eax, [esp + 40] ; Get int_no (shifted by 4 because of push esp)
    cmp eax, 128
    je .do_syscall
    cmp eax, 32
    jae .do_irq
    
    call isr_handler
    jmp .cleanup
    
.do_irq:
    call irq_handler
    jmp .cleanup
    
.do_syscall:
    call syscall_handler

.cleanup:
    add esp, 4          ; Clean up the 'push esp' argument
    mov esp, eax        ; Switch to the returned stack pointer (points to 'ds')

.done:
    pop eax             ; Pops DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8          ; Clean up int_no and err_code
    sti
    iret

global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret
