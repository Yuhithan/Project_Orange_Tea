global port_inb
global port_outb

port_inb:
    mov dx, di
    in al, dx
    movzx rax, al
    ret
port_outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret