; Linux ELF64 (статичний, без libc): write(1,...) + exit(0)
bits 64
org 0x400000
ehdr:
    db 0x7F, 'E', 'L', 'F', 2, 1, 1, 0
    times 8 db 0
    dw 2, 0x3E
    dd 1
    dq _start
    dq phdr - $$
    dq 0
    dd 0
    dw 64, 56, 1, 0, 0, 0
phdr:
    dd 1, 7
    dq 0, $$, $$
    dq filesz, filesz, 0x1000
_start:
    mov eax, 1
    mov edi, 1
    lea rsi, [rel msg]
    mov edx, msglen
    syscall
    mov eax, 60
    xor edi, edi
    syscall
msg: db "Hello from a Linux ELF64 program running on Huy OS!", 10
msglen equ $ - msg
filesz equ $ - $$
