global long_mode_start
extern kmain
extern boot_magic
extern boot_info

section .text
bits 64

long_mode_start:
    ; Boot into the kernel shell using the standard VGA-compatible path.
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rax, [boot_magic]
    mov rdi, rax
    mov rax, [boot_info]
    mov rsi, rax
    call kmain

.halt:
    hlt
    jmp .halt
