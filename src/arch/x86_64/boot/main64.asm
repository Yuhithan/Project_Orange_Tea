global long_mode_start
extern kmain
extern boot_magic
extern boot_info

section .text
bits 64

long_mode_start:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, [boot_magic]
    mov rsi, [boot_info]
    call kmain

.halt:
    hlt
    jmp .halt