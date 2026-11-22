; Huy OS 0.3 - завантажувач: ядро в 0x10000 (далі 32->64 біт робить entry.asm), шрифт BIOS в 0x6000, VBE 1024x768 в 0x5000
[bits 16]
[org 0x7C00]

    jmp 0x0000:start
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [drive], dl

%ifndef ISO
    mov si, dap
    mov dl, [drive]
    mov ah, 0x42
    int 0x13
    jc fail
%endif

    ; копіюємо 8x16 шрифт з BIOS у 0x6000
    mov ax, 0x1130
    mov bh, 6
    int 0x10
    push ds
    push es
    pop ds
    mov si, bp
    xor ax, ax
    mov es, ax
    mov di, 0x6000
    mov cx, 2048
    cld
    rep movsw
    pop ds

    ; шукаємо в списку VBE режим 1024x768 (спершу 32 біти, потім 24)
    mov ax, 0x4F00
    mov di, 0x4000
    int 0x10
    cmp ax, 0x004F
    jne .fallback
    mov ax, [0x400E]
    mov [curoff], ax
.next:
    mov ax, [0x4010]
    mov fs, ax
    mov si, [curoff]
    fs lodsw
    mov [curoff], si
    cmp ax, 0xFFFF
    je .nextpass
    mov [mode], ax
    mov cx, ax
    mov ax, 0x4F01
    mov di, 0x5000
    int 0x10
    cmp ax, 0x004F
    jne .next
    mov ax, [0x5000]
    and ax, 0x90
    cmp ax, 0x90
    jne .next
    cmp word [0x5012], 1024
    jne .next
    cmp word [0x5014], 768
    jne .next
    mov al, [want]
    cmp [0x5019], al
    jne .next
    mov bx, [mode]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    je .done
    jmp .next
.nextpass:
    cmp byte [want], 24
    je .fallback
    mov byte [want], 24
    mov ax, [0x400E]
    mov [curoff], ax
    jmp .next
.fallback:
    mov ax, 0x4F01
    mov cx, 0x118
    mov di, 0x5000
    int 0x10
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne fail
.done:

    in al, 0x92
    or al, 2
    out 0x92, al
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

fail:
    mov si, msg_err
.p: lodsb
    test al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .p

[bits 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
%ifdef ISO
    ; BIOS завантажив boot+ядро в 0x7C00; переносимо ядро в 0x10000 (назад, бо області перекриваються)
    std
    mov esi, 0x7E00 + KSECTORS * 512 - 4
    mov edi, 0x10000 + KSECTORS * 512 - 4
    mov ecx, KSECTORS * 128
    rep movsd
    cld
%endif
    jmp 0x10000          ; 32-бітний вхід ядра: вмикає paging і long mode

[bits 16]
drive:   db 0
want:    db 32
curoff:  dw 0
mode:    dw 0
msg_err: db "Huy OS: pomylka diska abo VBE", 0

align 4
dap:
    db 0x10, 0
    dw KSECTORS
    dw 0x0000, 0x1000
    dq 1

align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_desc:
    dw gdt_desc - gdt - 1
    dd gdt

times 510 - ($ - $$) db 0
dw 0xAA55
