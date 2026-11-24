bits 64

section .text

extern putchar_

global x86_64_div64_32
x86_64_div64_32:
    mov r8, rdx
    mov r9, rcx
    mov esi, esi
    mov rax, rdi
    xor edx, edx
    div rsi
    mov [r8], rax
    mov [r9], edx
    ret

global x86_64_Video_WriteCharTeletype
x86_64_Video_WriteCharTeletype:
    sub rsp, 8
    call putchar_
    add rsp, 8
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
