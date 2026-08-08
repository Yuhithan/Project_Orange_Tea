global long_mode_start
extern kmain
extern boot_magic
extern boot_info

section .text
bits 64

long_mode_start:
    ; Shell-only kernel entry. Graphics initialization is intentionally not
    ; performed here; kmain starts the VGA shell after the CPU handoff.
    ; load null into all data segment registers
    mov ax , 0
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
