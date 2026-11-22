; Windows PE64 .exe (kernel32: GetStdHandle, WriteFile, ExitProcess)
bits 64
IB equ 0x400000
dos: dw 'MZ'
    times 58 db 0
    dd pe - $$
pe: db 'P', 'E', 0, 0
    dw 0x8664, 1
    dd 0, 0, 0
    dw opt_end - opt, 0x0002
opt:
    dw 0x20B
    db 14, 0
    dd 0x200, 0, 0
    dd entry - $$
    dd 0x200
    dq IB
    dd 0x200, 0x200
    dw 6, 0, 0, 0, 6, 0
    dd 0
    dd 0x400
    dd 0x200
    dd 0
    dw 3, 0
    dq 0x100000, 0x1000, 0x100000, 0x1000
    dd 0
    dd 16
    dq 0
    dd imp_dir - $$, imp_dir_end - imp_dir
    times 14 dq 0
opt_end:
    db '.text', 0, 0, 0
    dd 0x200, 0x200, 0x200, 0x200
    dd 0, 0
    dw 0, 0
    dd 0xE0000060
    times 0x200 - ($ - $$) db 0
entry:
    sub rsp, 40
    mov ecx, -11
    call [rel iat_GetStdHandle]
    mov rcx, rax
    lea rdx, [rel msg]
    mov r8d, msglen
    lea r9, [rel written]
    mov qword [rsp + 32], 0
    call [rel iat_WriteFile]
    xor ecx, ecx
    call [rel iat_ExitProcess]
msg: db "Hello from a Windows PE64 .exe running on Huy OS!", 13, 10
msglen equ $ - msg
written: dd 0
align 8
imp_dir:
    dd int_tbl - $$, 0, 0, dll_name - $$, iat - $$
    times 5 dd 0
imp_dir_end:
int_tbl: dq hn1 - $$, hn2 - $$, hn3 - $$, 0
iat:
iat_GetStdHandle: dq hn1 - $$
iat_WriteFile: dq hn2 - $$
iat_ExitProcess: dq hn3 - $$
    dq 0
hn1: dw 0
    db 'GetStdHandle', 0
hn2: dw 0
    db 'WriteFile', 0
hn3: dw 0
    db 'ExitProcess', 0
dll_name: db 'KERNEL32.dll', 0
times 0x400 - ($ - $$) db 0
