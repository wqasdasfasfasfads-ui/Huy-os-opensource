; Huy OS 0.3 - вхід ядра: 32-біт -> long mode (64-біт), потім kmain на C
[bits 32]
section .text.entry
global _start
global irq_stubs
global exc_stub
extern kmain
extern irq_handler
extern sys_handler
global run_prog
global prog_exit
global syscall_entry
global payload
extern __bss_start
extern __bss_end

_start:
    cli
    cld
    ; таблиці сторінок: 0x70000 PML4, 0x71000 PDPT, 0x72000..0x75000 4 x PD (4 ГБ, сторінки по 2 МБ)
    mov edi, 0x70000
    mov ecx, 6 * 1024
    xor eax, eax
    rep stosd
    mov dword [0x70000], 0x71003
    mov dword [0x71000], 0x72003
    mov dword [0x71008], 0x73003
    mov dword [0x71010], 0x74003
    mov dword [0x71018], 0x75003
    mov edi, 0x72000
    mov eax, 0x83
    mov ecx, 2048
.pd:
    mov [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    add eax, 0x200000
    loop .pd

    mov eax, cr4
    or eax, 0x20              ; PAE
    mov cr4, eax
    mov eax, 0x70000
    mov cr3, eax
    mov ecx, 0xC0000080       ; EFER
    rdmsr
    or eax, 0x100             ; LME
    wrmsr
    lgdt [gdt64_desc]
    mov eax, cr0
    or eax, 0x80000000        ; PG -> long mode активний
    mov cr0, eax
    push dword 0x08
    push dword lm_entry
    retf

[bits 64]
lm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kmain
.hang:
    cli
    hlt
    jmp .hang

%macro IRQSTUB 1
irq%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    mov edi, %1
    cld
    call irq_handler
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro
IRQSTUB 0
IRQSTUB 1
IRQSTUB 2
IRQSTUB 3
IRQSTUB 4
IRQSTUB 5
IRQSTUB 6
IRQSTUB 7
IRQSTUB 8
IRQSTUB 9
IRQSTUB 10
IRQSTUB 11
IRQSTUB 12
IRQSTUB 13
IRQSTUB 14
IRQSTUB 15

; --- syscall (Linux x86-64 ABI): rax=номер, rdi,rsi,rdx=аргументи. Працюємо в ring 0 на стеку програми ---
syscall_entry:
    sub rsp, 128              ; не чіпаємо red zone програми
    push rcx
    push r11
    push rdi
    push rsi
    push rdx
    push r8
    push r9
    push r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    cld
    call sys_handler
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi
    pop r11
    pop rcx
    add rsp, 128
    push r11
    popfq
    jmp rcx

; run_prog(entry=rdi, sp=rsi, mode=rdx): mode 0 = ELF (jmp, rsp -> argc), 1 = PE (call, MS x64 ABI)
run_prog:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rel saved_rsp], rsp
    mov rsp, rsi
    mov rax, rdi
    test edx, edx
    jnz .pe
    xor edx, edx
    xor ebp, ebp
    jmp rax
.pe:
    and rsp, -16
    sub rsp, 32
    call rax
prog_exit:
    mov rsp, [rel saved_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

exc_stub:
    cli
.h: hlt
    jmp .h

align 8
irq_stubs:
    dq irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
    dq irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF     ; 0x08: 64-bit code
    dq 0x00CF92000000FFFF     ; 0x10: data
gdt64_desc:
    dw gdt64_desc - gdt64 - 1
    dd gdt64

section .data
align 8
saved_rsp: dq 0
align 8
payload:
    incbin "files.bin"
